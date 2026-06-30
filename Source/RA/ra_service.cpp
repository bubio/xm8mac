#include "ra_service.h"

#include "rc_error.h"

#include <cctype>
#include <cstring>

namespace Xm8Ra {

namespace {

bool IsMd5Hex(const std::string& hash)
{
	if (hash.size() != 32) {
		return false;
	}
	for (const char c : hash) {
		if (!std::isxdigit(static_cast<unsigned char>(c))) {
			return false;
		}
	}
	return true;
}

} // namespace

RaService::RaService(RaServiceOptions options)
	: http_client_(std::move(options.http_client)),
	  credentials_(options.ra_root)
{
	if (http_client_ == nullptr) {
		SetFailed(RC_INVALID_STATE, "HTTP client is required");
		return;
	}

	http_bridge_.reset(new RaRcClientHttpBridge(http_client_.get()));
	client_ = rc_client_create(
		options.read_memory != nullptr ? options.read_memory : ReadNoMemory,
		RaRcClientHttpBridge::ServerCall);
	if (client_ == nullptr) {
		SetFailed(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY));
		return;
	}

	rc_client_set_userdata(client_, http_bridge_.get());
	rc_client_set_hardcore_enabled(client_, 0);
}

RaService::~RaService()
{
	Shutdown();
}

bool RaService::IsReady() const
{
	return client_ != nullptr && http_bridge_ != nullptr;
}

bool RaService::BeginLoginWithPassword(const std::string& username,
	const std::string& password, std::string *error)
{
	if (!IsReady()) {
		if (error != nullptr) {
			*error = "RA service is not ready";
		}
		return false;
	}

	login_ = RaLoginSnapshot();
	login_.state = RaLoginState::LoginPending;
	login_.username = username;
	login_kind_ = LoginKind::Password;

	rc_client_async_handle_t *handle = rc_client_begin_login_with_password(
		client_, username.c_str(), password.c_str(), LoginCallback, this);
	if (handle == nullptr && login_.state == RaLoginState::LoginPending) {
		SetFailed(RC_INVALID_STATE, "Login did not start");
		if (error != nullptr) {
			*error = login_.message;
		}
		return false;
	}

	return true;
}

bool RaService::BeginLoginWithSavedToken(std::string *error)
{
	if (!IsReady()) {
		if (error != nullptr) {
			*error = "RA service is not ready";
		}
		return false;
	}

	RaCredentials credentials;
	if (!credentials_.Load(&credentials, error)) {
		login_ = RaLoginSnapshot();
		login_.state = RaLoginState::LoggedOut;
		return false;
	}

	login_ = RaLoginSnapshot();
	login_.state = RaLoginState::LoginPending;
	login_.username = credentials.username;
	login_kind_ = LoginKind::SavedToken;

	rc_client_async_handle_t *handle = rc_client_begin_login_with_token(
		client_, credentials.username.c_str(), credentials.token.c_str(),
		LoginCallback, this);
	credentials_.ClearSecret(&credentials);
	if (handle == nullptr && login_.state == RaLoginState::LoginPending) {
		SetFailed(RC_INVALID_STATE, "Saved token login did not start");
		DeleteCredentialsForRejectedToken();
		if (error != nullptr) {
			*error = login_.message;
		}
		return false;
	}

	return true;
}

bool RaService::BeginLoadGameByHash(const std::string& hash, std::string *error)
{
	if (!IsReady()) {
		if (error != nullptr) {
			*error = "RA service is not ready";
		}
		return false;
	}
	if (login_.state != RaLoginState::LoggedIn) {
		if (error != nullptr) {
			*error = "RA login is required before loading a game";
		}
		return false;
	}
	if (!IsMd5Hex(hash)) {
		if (error != nullptr) {
			*error = "RA game hash must be a 32-character MD5 hex string";
		}
		return false;
	}
	if (game_session_.state == RaGameSessionState::LoadPending) {
		if (error != nullptr) {
			*error = "RA game load is already pending";
		}
		return false;
	}

	http_bridge_->AdvanceGeneration();
	game_session_ = RaGameSessionSnapshot();
	game_session_.state = RaGameSessionState::LoadPending;
	game_session_.hash = hash;
	game_session_.load_state = rc_client_get_load_game_state(client_);

	rc_client_async_handle_t *handle = rc_client_begin_load_game(
		client_, hash.c_str(), LoadGameCallback, this);
	game_session_.load_state = rc_client_get_load_game_state(client_);
	if (handle == nullptr &&
		game_session_.state == RaGameSessionState::LoadPending) {
		DisableGameSession(RC_INVALID_STATE, "RA game load did not start");
		if (error != nullptr) {
			*error = game_session_.message;
		}
		return false;
	}

	return true;
}

void RaService::DrainHttp()
{
	if (http_bridge_ != nullptr) {
		http_bridge_->DrainCompleted();
		if (client_ != nullptr) {
			game_session_.load_state = rc_client_get_load_game_state(client_);
		}
	}
}

void RaService::UnloadGame()
{
	if (client_ != nullptr) {
		rc_client_unload_game(client_);
	}
	if (http_bridge_ != nullptr) {
		http_bridge_->AdvanceGeneration();
	}
	game_session_ = RaGameSessionSnapshot();
}

void RaService::Logout()
{
	UnloadGame();
	if (client_ != nullptr) {
		rc_client_logout(client_);
	}

	std::string ignored_error;
	credentials_.Delete(&ignored_error);

	login_ = RaLoginSnapshot();
	login_.state = RaLoginState::LoggedOut;
	login_kind_ = LoginKind::None;
}

void RaService::Shutdown()
{
	if (shutdown_) {
		return;
	}
	shutdown_ = true;

	if (http_bridge_ != nullptr) {
		http_bridge_->CancelAll();
		http_bridge_->DrainCompleted();
	}

	if (client_ != nullptr) {
		rc_client_set_userdata(client_, nullptr);
		rc_client_destroy(client_);
		client_ = nullptr;
	}

	if (http_bridge_ != nullptr) {
		http_bridge_->AbortAllWithoutCallbacks();
		http_bridge_.reset();
	}
	http_client_.reset();
	login_kind_ = LoginKind::None;
}

RaLoginSnapshot RaService::LoginSnapshot() const
{
	return login_;
}

RaGameSessionSnapshot RaService::GameSessionSnapshot() const
{
	return game_session_;
}

size_t RaService::PendingHttpCount() const
{
	return http_bridge_ != nullptr ? http_bridge_->PendingCount() : 0;
}

uint64_t RaService::LastIssuedRequestId() const
{
	return http_bridge_ != nullptr ? http_bridge_->LastIssuedRequestId() : 0;
}

const RaHttpClient *RaService::HttpClientForTesting() const
{
	return http_client_.get();
}

RaHttpClient *RaService::HttpClientForTesting()
{
	return http_client_.get();
}

uint32_t RC_CCONV RaService::ReadNoMemory(uint32_t, uint8_t *buffer,
	uint32_t num_bytes, rc_client_t *)
{
	if (buffer != nullptr && num_bytes != 0) {
		std::memset(buffer, 0, num_bytes);
	}
	return 0;
}

void RC_CCONV RaService::LoginCallback(int result, const char *error_message,
	rc_client_t *, void *userdata)
{
	RaService *service = static_cast<RaService *>(userdata);
	if (service != nullptr) {
		service->HandleLoginCallback(result, error_message);
	}
}

void RC_CCONV RaService::LoadGameCallback(int result,
	const char *error_message, rc_client_t *, void *userdata)
{
	RaService *service = static_cast<RaService *>(userdata);
	if (service != nullptr) {
		service->HandleLoadGameCallback(result, error_message);
	}
}

void RaService::HandleLoginCallback(int result, const char *error_message)
{
	login_.result = result;
	login_.message = error_message != nullptr ? error_message : "";

	if (result != RC_OK) {
		SetFailed(result, login_.message);
		if (login_kind_ == LoginKind::SavedToken) {
			DeleteCredentialsForRejectedToken();
		}
		login_kind_ = LoginKind::None;
		return;
	}

	const rc_client_user_t *user = rc_client_get_user_info(client_);
	if (user == nullptr || user->username == nullptr || user->token == nullptr) {
		SetFailed(RC_MISSING_VALUE, "Login succeeded without user credentials");
		if (login_kind_ == LoginKind::SavedToken) {
			DeleteCredentialsForRejectedToken();
		}
		login_kind_ = LoginKind::None;
		return;
	}

	login_.state = RaLoginState::LoggedIn;
	login_.username = user->username;
	login_.display_name = user->display_name != nullptr ?
		user->display_name : user->username;

	RaCredentials credentials;
	credentials.username = user->username;
	credentials.token = user->token;
	std::string ignored_error;
	credentials_.Save(credentials, &ignored_error);
	credentials_.ClearSecret(&credentials);

	login_kind_ = LoginKind::None;
}

void RaService::HandleLoadGameCallback(int result, const char *error_message)
{
	game_session_.result = result;
	game_session_.message = error_message != nullptr ? error_message : "";
	game_session_.load_state = rc_client_get_load_game_state(client_);

	if (result != RC_OK) {
		DisableGameSession(result, game_session_.message);
		return;
	}

	const rc_client_game_t *game = rc_client_get_game_info(client_);
	if (game == nullptr || game->id == 0) {
		DisableGameSession(RC_NO_GAME_LOADED, "RA game information is unavailable");
		return;
	}

	game_session_.state = RaGameSessionState::Loaded;
	game_session_.disabled_for_session = false;
	game_session_.game_id = game->id;
	game_session_.console_id = game->console_id;
	game_session_.title = game->title != nullptr ? game->title : "";
	game_session_.hash = game->hash != nullptr ? game->hash : game_session_.hash;
	game_session_.badge_url = game->badge_url != nullptr ? game->badge_url : "";
}

void RaService::SetFailed(int result, const std::string& message)
{
	login_.state = RaLoginState::Failed;
	login_.result = result;
	login_.message = message;
}

void RaService::DisableGameSession(int result, const std::string& message)
{
	if (client_ != nullptr) {
		rc_client_unload_game(client_);
	}
	game_session_.state = RaGameSessionState::DisabledForSession;
	game_session_.result = result;
	game_session_.message = message;
	game_session_.load_state = client_ != nullptr ?
		rc_client_get_load_game_state(client_) : 0;
	game_session_.disabled_for_session = true;
}

void RaService::DeleteCredentialsForRejectedToken()
{
	std::string ignored_error;
	credentials_.Delete(&ignored_error);
	login_.credentials_deleted = true;
}

} // namespace Xm8Ra
