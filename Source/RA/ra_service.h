#ifndef XM8_RA_SERVICE_H
#define XM8_RA_SERVICE_H

#include "ra_credentials.h"
#include "ra_http_client.h"
#include "ra_rc_client_http.h"

#include "rc_client.h"

#include <memory>
#include <string>
#include <vector>

namespace Xm8Ra {

typedef uint32_t (*RaHostReadMemoryFunc)(uint32_t address, uint8_t *buffer,
	uint32_t num_bytes, void *userdata);

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

enum class RaEventType {
	None,
	AchievementTriggered,
	LeaderboardStarted,
	LeaderboardFailed,
	LeaderboardSubmitted,
	AchievementChallengeIndicatorShow,
	AchievementChallengeIndicatorHide,
	AchievementProgressIndicatorShow,
	AchievementProgressIndicatorHide,
	AchievementProgressIndicatorUpdate,
	LeaderboardTrackerShow,
	LeaderboardTrackerHide,
	LeaderboardTrackerUpdate,
	LeaderboardScoreboard,
	ResetRequested,
	GameCompleted,
	ServerError,
	Disconnected,
	Reconnected,
	SubsetCompleted,
	RichPresenceChanged,
};

struct RaAchievementEvent {
	uint32_t id = 0;
	uint32_t points = 0;
	uint8_t state = 0;
	uint8_t category = 0;
	uint8_t bucket = 0;
	uint8_t unlocked = 0;
	uint8_t type = 0;
	float measured_percent = 0.0f;
	std::string title;
	std::string description;
	std::string measured_progress;
	std::string badge_url;
	std::string badge_locked_url;
};

struct RaAchievementListItem {
	uint32_t id = 0;
	uint32_t points = 0;
	uint8_t state = 0;
	uint8_t category = 0;
	uint8_t bucket = 0;
	uint8_t unlocked = 0;
	uint8_t type = 0;
	float measured_percent = 0.0f;
	float rarity = 0.0f;
	float rarity_hardcore = 0.0f;
	std::string title;
	std::string description;
	std::string measured_progress;
	std::string badge_url;
	std::string badge_locked_url;
	std::string bucket_label;
};

struct RaAchievementListSnapshot {
	bool game_loaded = false;
	bool has_achievements = false;
	std::string game_title;
	std::vector<RaAchievementListItem> achievements;
};

struct RaLeaderboardEvent {
	uint32_t id = 0;
	uint8_t state = 0;
	uint8_t format = 0;
	bool lower_is_better = false;
	std::string title;
	std::string description;
	std::string tracker_value;
	std::string display;
};

struct RaLeaderboardListItem {
	uint32_t id = 0;
	uint8_t state = 0;
	uint8_t format = 0;
	bool lower_is_better = false;
	std::string title;
	std::string description;
	std::string tracker_value;
	std::string bucket_label;
};

struct RaLeaderboardListSnapshot {
	bool game_loaded = false;
	bool has_leaderboards = false;
	std::string game_title;
	std::vector<RaLeaderboardListItem> leaderboards;
};

struct RaLeaderboardScoreboardEntry {
	uint32_t rank = 0;
	std::string username;
	std::string score;
};

struct RaLeaderboardScoreboardEvent {
	uint32_t leaderboard_id = 0;
	uint32_t new_rank = 0;
	uint32_t num_entries = 0;
	std::string submitted_score;
	std::string best_score;
	std::vector<RaLeaderboardScoreboardEntry> top_entries;
};

enum class RaLeaderboardEntriesState {
	None,
	FetchPending,
	Loaded,
	Failed,
};

struct RaLeaderboardEntryItem {
	uint32_t rank = 0;
	uint32_t index = 0;
	std::string username;
	std::string display;
};

struct RaLeaderboardEntriesSnapshot {
	RaLeaderboardEntriesState state = RaLeaderboardEntriesState::None;
	int result = 0;
	std::string message;
	uint32_t leaderboard_id = 0;
	uint32_t total_entries = 0;
	int32_t user_index = -1;
	std::vector<RaLeaderboardEntryItem> entries;
};

struct RaServerErrorEvent {
	int result = 0;
	uint32_t related_id = 0;
	std::string api;
	std::string message;
};

struct RaSubsetEvent {
	uint32_t id = 0;
	uint32_t num_achievements = 0;
	uint32_t num_leaderboards = 0;
	std::string title;
	std::string badge_url;
};

struct RaEvent {
	RaEventType type = RaEventType::None;
	uint32_t raw_type = 0;
	RaAchievementEvent achievement;
	RaLeaderboardEvent leaderboard;
	RaLeaderboardScoreboardEvent scoreboard;
	RaServerErrorEvent server_error;
	RaSubsetEvent subset;
	std::string rich_presence;
};

struct RaServiceOptions {
	std::string ra_root;
	std::unique_ptr<RaHttpClient> http_client;
	std::unique_ptr<RaCredentialsStore> credentials_store;
	rc_client_read_memory_func_t read_memory = nullptr;
	RaHostReadMemoryFunc host_read_memory = nullptr;
	void *host_read_memory_userdata = nullptr;
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
	bool BeginFetchLeaderboardEntries(uint32_t leaderboard_id,
		uint32_t first_entry, uint32_t count, std::string *error);
	void DrainHttp();
	bool DoFrame();
	bool Idle();
	bool IsProcessingRequired() const;
	std::vector<RaEvent> TakeEvents();
	void UnloadGame();
	void Logout();
	void Shutdown();

	RaLoginSnapshot LoginSnapshot() const;
	RaGameSessionSnapshot GameSessionSnapshot() const;
	RaAchievementListSnapshot AchievementListSnapshot() const;
	RaLeaderboardListSnapshot LeaderboardListSnapshot() const;
	RaLeaderboardEntriesSnapshot LeaderboardEntriesSnapshot() const;
	size_t PendingHttpCount() const;
	uint64_t LastIssuedRequestId() const;
	const RaHttpClient *HttpClientForTesting() const;
	RaHttpClient *HttpClientForTesting();
	void QueueEventForTesting(const rc_client_event_t *event);

private:
	enum class LoginKind {
		None,
		Password,
		SavedToken,
	};

	static uint32_t RC_CCONV ReadNoMemory(uint32_t address, uint8_t *buffer,
		uint32_t num_bytes, rc_client_t *client);
	static uint32_t RC_CCONV ReadHostMemory(uint32_t address, uint8_t *buffer,
		uint32_t num_bytes, rc_client_t *client);
	static void RC_CCONV LoginCallback(int result, const char *error_message,
		rc_client_t *client, void *userdata);
	static void RC_CCONV LoadGameCallback(int result, const char *error_message,
		rc_client_t *client, void *userdata);
	static void RC_CCONV LeaderboardEntriesCallback(int result,
		const char *error_message, rc_client_leaderboard_entry_list_t *list,
		rc_client_t *client, void *userdata);
	static void RC_CCONV ClientEventHandler(const rc_client_event_t *event,
		rc_client_t *client);
	static void RC_CCONV ServerCall(const rc_api_request_t *request,
		rc_client_server_callback_t callback, void *callback_data,
		rc_client_t *client);

	void HandleLoginCallback(int result, const char *error_message);
	void HandleLoadGameCallback(int result, const char *error_message);
	void HandleLeaderboardEntriesCallback(int result, const char *error_message,
		rc_client_leaderboard_entry_list_t *list);
	void HandleClientEvent(const rc_client_event_t *event);
	void UpdateRichPresenceEvent();
	void SetFailed(int result, const std::string& message);
	void DisableGameSession(int result, const std::string& message);
	void DeleteCredentialsForRejectedToken();
	void AbortLoginInProgress();
	void AbortLeaderboardEntriesInProgress();

	std::unique_ptr<RaHttpClient> http_client_;
	std::unique_ptr<RaCredentialsStore> credentials_;
	std::unique_ptr<RaRcClientHttpBridge> http_bridge_;
	rc_client_t *client_ = nullptr;
	RaHostReadMemoryFunc host_read_memory_ = nullptr;
	void *host_read_memory_userdata_ = nullptr;
	LoginKind login_kind_ = LoginKind::None;
	rc_client_async_handle_t *login_async_handle_ = nullptr;
	rc_client_async_handle_t *leaderboard_entries_async_handle_ = nullptr;
	RaLoginSnapshot login_;
	RaGameSessionSnapshot game_session_;
	RaLeaderboardEntriesSnapshot leaderboard_entries_;
	std::vector<RaEvent> events_;
	std::string rich_presence_;
	bool shutdown_ = false;
};

} // namespace Xm8Ra

#endif
