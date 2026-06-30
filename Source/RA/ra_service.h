#ifndef XM8_RA_SERVICE_H
#define XM8_RA_SERVICE_H

#include "ra_credentials.h"
#include "ra_http_client.h"
#include "ra_rc_client_http.h"

#include "rc_client.h"

#include <memory>
#include <string>

namespace Xm8Ra {

enum class RaLoginState {
	LoggedOut,
	LoginPending,
	LoggedIn,
	Failed,
};

enum class RaGameSessionState {
	NoGame,
	LoadPending,
	Loaded,
	DisabledForSession,
};

struct RaLoginSnapshot {
	RaLoginState state = RaLoginState::LoggedOut;
	int result = 0;
	std::string message;
	std::string username;
	std::string display_name;
	bool credentials_deleted = false;
};

struct RaGameSessionSnapshot {
	RaGameSessionState state = RaGameSessionState::NoGame;
	int result = 0;
	int load_state = 0;
	std::string message;
	std::string hash;
	uint32_t game_id = 0;
	uint32_t console_id = 0;
	std::string title;
	std::string badge_url;
	bool disabled_for_session = false;
};

struct RaServiceOptions {
	std::string ra_root;
	std::unique_ptr<RaHttpClient> http_client;
	rc_client_read_memory_func_t read_memory = nullptr;
	std::string user_agent;
};

class RaService {
public:
	explicit RaService(RaServiceOptions options);
	~RaService();

	RaService(const RaService&) = delete;
	RaService& operator=(const RaService&) = delete;

	bool IsReady() const;
	bool BeginLoginWithPassword(const std::string& username,
		const std::string& password, std::string *error);
	bool BeginLoginWithSavedToken(std::string *error);
	bool BeginLoadGameByHash(const std::string& hash, std::string *error);
	void DrainHttp();
	void UnloadGame();
	void Logout();
	void Shutdown();

	RaLoginSnapshot LoginSnapshot() const;
	RaGameSessionSnapshot GameSessionSnapshot() const;
	size_t PendingHttpCount() const;
	uint64_t LastIssuedRequestId() const;
	const RaHttpClient *HttpClientForTesting() const;
	RaHttpClient *HttpClientForTesting();

private:
	enum class LoginKind {
		None,
		Password,
		SavedToken,
	};

	static uint32_t RC_CCONV ReadNoMemory(uint32_t address, uint8_t *buffer,
		uint32_t num_bytes, rc_client_t *client);
	static void RC_CCONV LoginCallback(int result, const char *error_message,
		rc_client_t *client, void *userdata);
	static void RC_CCONV LoadGameCallback(int result, const char *error_message,
		rc_client_t *client, void *userdata);

	void HandleLoginCallback(int result, const char *error_message);
	void HandleLoadGameCallback(int result, const char *error_message);
	void SetFailed(int result, const std::string& message);
	void DisableGameSession(int result, const std::string& message);
	void DeleteCredentialsForRejectedToken();

	std::unique_ptr<RaHttpClient> http_client_;
	std::unique_ptr<RaRcClientHttpBridge> http_bridge_;
	RaCredentialsStore credentials_;
	rc_client_t *client_ = nullptr;
	LoginKind login_kind_ = LoginKind::None;
	RaLoginSnapshot login_;
	RaGameSessionSnapshot game_session_;
	bool shutdown_ = false;
};

} // namespace Xm8Ra

#endif
