#include "ra_service.h"

#include "rc_error.h"

#include <cctype>
#include <cstring>
#include <utility>

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

std::string SafeString(const char *value)
{
	return value != nullptr ? value : "";
}

RaEventType MapEventType(uint32_t type)
{
	switch (type) {
	case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
		return RaEventType::AchievementTriggered;
	case RC_CLIENT_EVENT_LEADERBOARD_STARTED:
		return RaEventType::LeaderboardStarted;
	case RC_CLIENT_EVENT_LEADERBOARD_FAILED:
		return RaEventType::LeaderboardFailed;
	case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
		return RaEventType::LeaderboardSubmitted;
	case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW:
		return RaEventType::AchievementChallengeIndicatorShow;
	case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE:
		return RaEventType::AchievementChallengeIndicatorHide;
	case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW:
		return RaEventType::AchievementProgressIndicatorShow;
	case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE:
		return RaEventType::AchievementProgressIndicatorHide;
	case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE:
		return RaEventType::AchievementProgressIndicatorUpdate;
	case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW:
		return RaEventType::LeaderboardTrackerShow;
	case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE:
		return RaEventType::LeaderboardTrackerHide;
	case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE:
		return RaEventType::LeaderboardTrackerUpdate;
	case RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD:
		return RaEventType::LeaderboardScoreboard;
	case RC_CLIENT_EVENT_RESET:
		return RaEventType::ResetRequested;
	case RC_CLIENT_EVENT_GAME_COMPLETED:
		return RaEventType::GameCompleted;
	case RC_CLIENT_EVENT_SERVER_ERROR:
		return RaEventType::ServerError;
	case RC_CLIENT_EVENT_DISCONNECTED:
		return RaEventType::Disconnected;
	case RC_CLIENT_EVENT_RECONNECTED:
		return RaEventType::Reconnected;
	case RC_CLIENT_EVENT_SUBSET_COMPLETED:
		return RaEventType::SubsetCompleted;
	default:
		return RaEventType::None;
	}
}

void CopyAchievement(RaAchievementEvent *target,
	const rc_client_achievement_t *achievement)
{
	if (target == nullptr || achievement == nullptr) {
		return;
	}

	target->id = achievement->id;
	target->points = achievement->points;
	target->state = achievement->state;
	target->category = achievement->category;
	target->bucket = achievement->bucket;
	target->unlocked = achievement->unlocked;
	target->type = achievement->type;
	target->measured_percent = achievement->measured_percent;
	target->title = SafeString(achievement->title);
	target->description = SafeString(achievement->description);
	target->measured_progress = achievement->measured_progress;
	target->badge_url = SafeString(achievement->badge_url);
	target->badge_locked_url = SafeString(achievement->badge_locked_url);
}

void CopyAchievementListItem(RaAchievementListItem *target,
	const rc_client_achievement_t *achievement, const char *bucket_label)
{
	if (target == nullptr || achievement == nullptr) {
		return;
	}

	target->id = achievement->id;
	target->points = achievement->points;
	target->state = achievement->state;
	target->category = achievement->category;
	target->bucket = achievement->bucket;
	target->unlocked = achievement->unlocked;
	target->type = achievement->type;
	target->measured_percent = achievement->measured_percent;
	target->rarity = achievement->rarity;
	target->rarity_hardcore = achievement->rarity_hardcore;
	target->title = SafeString(achievement->title);
	target->description = SafeString(achievement->description);
	target->measured_progress = achievement->measured_progress;
	target->badge_url = SafeString(achievement->badge_url);
	target->badge_locked_url = SafeString(achievement->badge_locked_url);
	target->bucket_label = SafeString(bucket_label);
}

void CopyLeaderboard(RaLeaderboardEvent *target,
	const rc_client_leaderboard_t *leaderboard)
{
	if (target == nullptr || leaderboard == nullptr) {
		return;
	}

	target->id = leaderboard->id;
	target->state = leaderboard->state;
	target->format = leaderboard->format;
	target->lower_is_better = leaderboard->lower_is_better != 0;
	target->title = SafeString(leaderboard->title);
	target->description = SafeString(leaderboard->description);
	target->tracker_value = SafeString(leaderboard->tracker_value);
}

void CopyLeaderboardTracker(RaLeaderboardEvent *target,
	const rc_client_leaderboard_tracker_t *tracker)
{
	if (target == nullptr || tracker == nullptr) {
		return;
	}

	target->id = tracker->id;
	target->display = tracker->display;
}

void CopyScoreboard(RaLeaderboardScoreboardEvent *target,
	const rc_client_leaderboard_scoreboard_t *scoreboard)
{
	if (target == nullptr || scoreboard == nullptr) {
		return;
	}

	target->leaderboard_id = scoreboard->leaderboard_id;
	target->new_rank = scoreboard->new_rank;
	target->num_entries = scoreboard->num_entries;
	target->submitted_score = scoreboard->submitted_score;
	target->best_score = scoreboard->best_score;
	if (scoreboard->top_entries == nullptr) {
		return;
	}
	for (uint32_t i = 0; i < scoreboard->num_top_entries; i++) {
		RaLeaderboardScoreboardEntry entry;
		entry.rank = scoreboard->top_entries[i].rank;
		entry.username = SafeString(scoreboard->top_entries[i].username);
		entry.score = scoreboard->top_entries[i].score;
		target->top_entries.push_back(entry);
	}
}

void CopyServerError(RaServerErrorEvent *target,
	const rc_client_server_error_t *server_error)
{
	if (target == nullptr || server_error == nullptr) {
		return;
	}

	target->result = server_error->result;
	target->related_id = server_error->related_id;
	target->api = SafeString(server_error->api);
	target->message = SafeString(server_error->error_message);
}

void CopySubset(RaSubsetEvent *target, const rc_client_subset_t *subset)
{
	if (target == nullptr || subset == nullptr) {
		return;
	}

	target->id = subset->id;
	target->num_achievements = subset->num_achievements;
	target->num_leaderboards = subset->num_leaderboards;
	target->title = SafeString(subset->title);
	target->badge_url = SafeString(subset->badge_url);
}

} // namespace

RaService::RaService(RaServiceOptions options)
	: http_client_(std::move(options.http_client)),
	  credentials_(std::move(options.credentials_store))
{
	if (http_client_ == nullptr) {
		SetFailed(RC_INVALID_STATE, "HTTP client is required");
		return;
	}
	if (credentials_ == nullptr) {
		SetFailed(RC_INVALID_STATE, "credentials store is required");
		return;
	}

	http_bridge_.reset(new RaRcClientHttpBridge(http_client_.get()));
	host_read_memory_ = options.host_read_memory;
	host_read_memory_userdata_ = options.host_read_memory_userdata;
	client_ = rc_client_create(
		host_read_memory_ != nullptr ? ReadHostMemory :
			(options.read_memory != nullptr ? options.read_memory : ReadNoMemory),
		ServerCall);
	if (client_ == nullptr) {
		SetFailed(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY));
		return;
	}

	rc_client_set_userdata(client_, this);
	rc_client_set_event_handler(client_, ClientEventHandler);
	rc_client_set_allow_background_memory_reads(client_, 0);
	rc_client_set_hardcore_enabled(client_, 0);
}

RaService::~RaService()
{
	Shutdown();
}

bool RaService::IsReady() const
{
	return client_ != nullptr && http_bridge_ != nullptr &&
		credentials_ != nullptr;
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

	AbortLoginInProgress();
	login_ = RaLoginSnapshot();
	login_.state = RaLoginState::LoginPending;
	login_.username = username;
	login_kind_ = LoginKind::Password;

	rc_client_async_handle_t *handle = rc_client_begin_login_with_password(
		client_, username.c_str(), password.c_str(), LoginCallback, this);
	if (handle == nullptr && login_.state == RaLoginState::LoginPending) {
		login_async_handle_ = nullptr;
		SetFailed(RC_INVALID_STATE, "Login did not start");
		if (error != nullptr) {
			*error = login_.message;
		}
		return false;
	}
	login_async_handle_ = handle;

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

	AbortLoginInProgress();
	RaCredentials credentials;
	if (!credentials_->Load(&credentials, error)) {
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
	credentials_->ClearSecret(&credentials);
	if (handle == nullptr && login_.state == RaLoginState::LoginPending) {
		login_async_handle_ = nullptr;
		SetFailed(RC_INVALID_STATE, "Saved token login did not start");
		DeleteCredentialsForRejectedToken();
		if (error != nullptr) {
			*error = login_.message;
		}
		return false;
	}
	login_async_handle_ = handle;

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

bool RaService::DoFrame()
{
	if (!IsReady() || game_session_.state != RaGameSessionState::Loaded) {
		return false;
	}

	rc_client_do_frame(client_);
	UpdateRichPresenceEvent();
	return true;
}

bool RaService::Idle()
{
	if (!IsReady()) {
		return false;
	}

	rc_client_idle(client_);
	UpdateRichPresenceEvent();
	return true;
}

bool RaService::IsProcessingRequired() const
{
	return client_ != nullptr && rc_client_is_processing_required(client_) != 0;
}

std::vector<RaEvent> RaService::TakeEvents()
{
	std::vector<RaEvent> events;
	events.swap(events_);
	return events;
}

void RaService::UnloadGame()
{
	if (client_ != nullptr) {
		rc_client_unload_game(client_);
	}
	game_session_ = RaGameSessionSnapshot();
	rich_presence_.clear();
}

void RaService::Logout()
{
	AbortLoginInProgress();
	UnloadGame();
	if (client_ != nullptr) {
		rc_client_logout(client_);
	}

	std::string ignored_error;
	credentials_->Delete(&ignored_error);

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

	AbortLoginInProgress();
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

RaAchievementListSnapshot RaService::AchievementListSnapshot() const
{
	RaAchievementListSnapshot snapshot;
	snapshot.game_loaded = game_session_.state == RaGameSessionState::Loaded;
	snapshot.game_title = game_session_.title;
	if (!snapshot.game_loaded || client_ == nullptr ||
		rc_client_has_achievements(client_) == 0) {
		return snapshot;
	}

	rc_client_achievement_list_t *list = rc_client_create_achievement_list(
		client_, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL,
		RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
	if (list == nullptr) {
		return snapshot;
	}

	for (uint32_t bucket_index = 0; bucket_index < list->num_buckets;
		++bucket_index) {
		const rc_client_achievement_bucket_t& bucket =
			list->buckets[bucket_index];
		for (uint32_t achievement_index = 0;
			achievement_index < bucket.num_achievements;
			++achievement_index) {
			RaAchievementListItem item;
			CopyAchievementListItem(&item,
				bucket.achievements[achievement_index], bucket.label);
			snapshot.achievements.push_back(item);
		}
	}

	rc_client_destroy_achievement_list(list);
	snapshot.has_achievements = !snapshot.achievements.empty();
	return snapshot;
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

void RaService::QueueEventForTesting(const rc_client_event_t *event)
{
	HandleClientEvent(event);
}

uint32_t RC_CCONV RaService::ReadNoMemory(uint32_t, uint8_t *buffer,
	uint32_t num_bytes, rc_client_t *)
{
	if (buffer != nullptr && num_bytes != 0) {
		std::memset(buffer, 0, num_bytes);
	}
	return 0;
}

uint32_t RC_CCONV RaService::ReadHostMemory(uint32_t address, uint8_t *buffer,
	uint32_t num_bytes, rc_client_t *client)
{
	RaService *service = client == nullptr ? nullptr :
		static_cast<RaService *>(rc_client_get_userdata(client));
	if (service == nullptr || service->host_read_memory_ == nullptr) {
		if (buffer != nullptr && num_bytes != 0) {
			std::memset(buffer, 0, num_bytes);
		}
		return 0;
	}
	return service->host_read_memory_(address, buffer, num_bytes,
		service->host_read_memory_userdata_);
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

void RC_CCONV RaService::ClientEventHandler(const rc_client_event_t *event,
	rc_client_t *client)
{
	RaService *service = client == nullptr ? nullptr :
		static_cast<RaService *>(rc_client_get_userdata(client));
	if (service != nullptr) {
		service->HandleClientEvent(event);
	}
}

void RC_CCONV RaService::ServerCall(const rc_api_request_t *request,
	rc_client_server_callback_t callback, void *callback_data,
	rc_client_t *client)
{
	RaService *service = client == nullptr ? nullptr :
		static_cast<RaService *>(rc_client_get_userdata(client));
	if (service == nullptr || service->http_bridge_ == nullptr) {
		if (callback != nullptr) {
			rc_api_server_response_t server_response = {};
			server_response.http_status_code = RC_API_SERVER_RESPONSE_CLIENT_ERROR;
			callback(&server_response, callback_data);
		}
		return;
	}

	service->http_bridge_->BeginServerCall(request, callback, callback_data);
}

void RaService::HandleLoginCallback(int result, const char *error_message)
{
	if (result == RC_ABORTED) {
		return;
	}
	login_async_handle_ = nullptr;
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
	if (credentials_ != nullptr) {
		credentials_->Save(credentials, &ignored_error);
		credentials_->ClearSecret(&credentials);
	}

	login_kind_ = LoginKind::None;
}

void RaService::HandleLoadGameCallback(int result, const char *error_message)
{
	if (result == RC_ABORTED) {
		return;
	}
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
	UpdateRichPresenceEvent();
}

void RaService::HandleClientEvent(const rc_client_event_t *event)
{
	if (event == nullptr) {
		return;
	}

	RaEvent copied;
	copied.raw_type = event->type;
	copied.type = MapEventType(event->type);
	CopyAchievement(&copied.achievement, event->achievement);
	CopyLeaderboard(&copied.leaderboard, event->leaderboard);
	CopyLeaderboardTracker(&copied.leaderboard, event->leaderboard_tracker);
	CopyScoreboard(&copied.scoreboard, event->leaderboard_scoreboard);
	CopyServerError(&copied.server_error, event->server_error);
	CopySubset(&copied.subset, event->subset);
	events_.push_back(copied);
}

void RaService::UpdateRichPresenceEvent()
{
	if (client_ == nullptr || game_session_.state != RaGameSessionState::Loaded ||
		rc_client_has_rich_presence(client_) == 0) {
		return;
	}

	char buffer[256];
	const size_t written = rc_client_get_rich_presence_message(
		client_, buffer, sizeof(buffer));
	buffer[sizeof(buffer) - 1] = '\0';
	std::string current = written > 0 ? buffer : "";
	if (current == rich_presence_) {
		return;
	}

	rich_presence_ = current;
	RaEvent event;
	event.type = RaEventType::RichPresenceChanged;
	event.rich_presence = rich_presence_;
	events_.push_back(event);
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
	rich_presence_.clear();
}

void RaService::DeleteCredentialsForRejectedToken()
{
	std::string ignored_error;
	if (credentials_ != nullptr) {
		credentials_->Delete(&ignored_error);
		login_.credentials_deleted = true;
	}
}

void RaService::AbortLoginInProgress()
{
	const bool reset_client_login =
		login_.state == RaLoginState::LoginPending ||
		login_.state == RaLoginState::LoggedIn;
	if (client_ != nullptr && login_async_handle_ != nullptr) {
		rc_client_abort_async(client_, login_async_handle_);
	}
	if (client_ != nullptr && reset_client_login) {
		rc_client_logout(client_);
	}
	login_async_handle_ = nullptr;
	if (login_.state == RaLoginState::LoginPending) {
		login_ = RaLoginSnapshot();
		login_.state = RaLoginState::LoggedOut;
	}
	login_kind_ = LoginKind::None;
}

} // namespace Xm8Ra
