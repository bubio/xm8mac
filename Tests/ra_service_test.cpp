#include "ra_credentials.h"
#include "ra_http_fake.h"
#include "ra_service.h"

#include "rc_error.h"

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace {

int failures = 0;
const char *kKnownSupportedPc8800Hash =
	"d2bf8ef1abf7bb47e54da38a286f4ac8";

void Check(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		failures++;
	}
}

bool MakeDirectory(const std::string& path)
{
	return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
}

std::string TemporaryRoot(const char *name)
{
	const auto unique = std::chrono::steady_clock::now()
		.time_since_epoch().count();
	const char *temporary = std::getenv(
#ifdef _WIN32
		"TEMP"
#else
		"TMPDIR"
#endif
	);
	return std::string(temporary != nullptr ? temporary :
#ifdef _WIN32
		"."
#else
		"/tmp"
#endif
	) + "/" + name + "-" + std::to_string(unique);
}

std::unique_ptr<Xm8Ra::FakeRaHttpClient> MakeFakeHttp()
{
	return std::unique_ptr<Xm8Ra::FakeRaHttpClient>(
		new Xm8Ra::FakeRaHttpClient());
}

Xm8Ra::RaHttpResponse MakeJsonResponse(uint64_t request_id,
	const std::string& json)
{
	Xm8Ra::RaHttpResponse response;
	response.request_id = request_id;
	response.http_status = 200;
	response.transport_result = Xm8Ra::RaHttpTransportResult::Success;
	response.body.assign(json.begin(), json.end());
	return response;
}

std::string MinimalAchievementSetsJson()
{
	return "{\"Success\":true,\"GameId\":1234,\"Title\":\"Test Game\","
		"\"ConsoleId\":47,"
		"\"ImageIconUrl\":\"https://media.retroachievements.org/Images/012345.png\","
		"\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"\","
		"\"Sets\":[{\"AchievementSetId\":1234,\"GameId\":1234,"
		"\"Title\":\"Test Game\",\"Type\":\"core\","
		"\"ImageIconUrl\":\"https://media.retroachievements.org/Images/012345.png\","
		"\"Achievements\":[],\"Leaderboards\":[]}]}";
}

std::string FrameAchievementSetsJson()
{
	return "{\"Success\":true,\"GameId\":1234,\"Title\":\"Frame Test\","
		"\"ConsoleId\":47,\"ImageIconUrl\":\"\","
		"\"RichPresenceGameId\":0,\"RichPresencePatch\":\"\","
		"\"Sets\":[{\"AchievementSetId\":1234,\"GameId\":1234,"
		"\"Title\":\"Frame Test\",\"Type\":\"core\",\"ImageIconUrl\":\"\","
		"\"Achievements\":[{\"ID\":42,\"Title\":\"Frame Edge\","
		"\"Description\":\"Observe an intermediate frame\",\"Flags\":3,"
		"\"Points\":5,\"MemAddr\":\"0xH0000=1\",\"Author\":\"test\","
		"\"BadgeName\":\"00001\",\"Created\":1710000000,"
		"\"Modified\":1710000000,\"Type\":\"\",\"Rarity\":100.0,"
		"\"RarityHardcore\":100.0}],\"Leaderboards\":[]}]}";
}

std::string StartSessionJson()
{
	return "{\"Success\":true,\"Unlocks\":[],\"HardcoreUnlocks\":[],"
		"\"ServerNow\":1710000000}";
}

std::string LeaderboardInfoJson()
{
	return "{\"Success\":true,\"LeaderboardData\":{"
		"\"LBID\":77,\"LBFormat\":\"SCORE\",\"LowerIsBetter\":0,"
		"\"LBTitle\":\"Fastest Clear\",\"LBDesc\":\"Finish quickly\","
		"\"LBMem\":\"STA:0xH0000=1::CAN:0xH0001=0::SUB:0xH0002=1::VAL:0xH0003\","
		"\"GameID\":1234,\"LBAuthor\":\"dev\","
		"\"LBCreated\":\"2024-03-09 16:00:00\","
		"\"LBUpdated\":\"2024-03-09 16:00:01\","
		"\"TotalEntries\":10,"
		"\"Entries\":["
		"{\"User\":\"first\",\"Rank\":1,\"Index\":1,\"Score\":1000,"
		"\"DateSubmitted\":1710000002,\"AvatarUrl\":\"\"},"
		"{\"User\":\"player\",\"Rank\":2,\"Index\":2,\"Score\":900,"
		"\"DateSubmitted\":1710000003,\"AvatarUrl\":\"\"}"
		"]}}";
}

struct FrameMemory {
	uint8_t value = 0;
	uint32_t reads = 0;
};

uint32_t ReadFrameMemory(uint32_t address, uint8_t *buffer,
	uint32_t num_bytes, void *userdata)
{
	FrameMemory *memory = static_cast<FrameMemory *>(userdata);
	if (memory == nullptr || buffer == nullptr || num_bytes == 0 ||
		address != 0) {
		return 0;
	}
	memory->reads++;
	buffer[0] = memory->value;
	if (num_bytes > 1) {
		std::memset(buffer + 1, 0, num_bytes - 1);
	}
	return num_bytes;
}

} // namespace

int main()
{
	const std::string base = TemporaryRoot("xm8-ra-service");
	Check(MakeDirectory(base), "create service test root");
	Xm8Ra::RaPlatformCredentialsStore credential_store(base);

	{
		auto fake_http = MakeFakeHttp();
		Xm8Ra::FakeRaHttpClient *fake_http_raw = fake_http.get();
		Xm8Ra::RaServiceOptions options;
		options.ra_root = base;
		options.credentials_store =
			Xm8Ra::CreatePlatformRaCredentialsStore(base);
		options.http_client = std::move(fake_http);
		Xm8Ra::RaService service(std::move(options));

		std::string error;
		Check(service.IsReady(), "service is ready");
		Check(service.BeginLoginWithPassword("player", "secret-password",
			&error), "begin password login");
		Check(fake_http_raw->SentRequests().size() == 1,
			"password login sends HTTP request");
		Check(fake_http_raw->SentRequests()[0].post_data.find(
			"p=secret-password") != std::string::npos,
			"password login request contains password only in HTTP body");
		Check(service.LoginSnapshot().state == Xm8Ra::RaLoginState::LoginPending,
			"password login pending");

		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			"{\"Success\":true,\"User\":\"player\",\"Token\":\"saved-token\","
			"\"Score\":10,\"SoftcoreScore\":20,\"Messages\":0}"));
		service.DrainHttp();
		const Xm8Ra::RaLoginSnapshot snapshot = service.LoginSnapshot();
		Check(snapshot.state == Xm8Ra::RaLoginState::LoggedIn,
			"password login succeeds");
		Check(snapshot.username == "player", "logged in username captured");

		Xm8Ra::RaCredentials loaded;
		Check(credential_store.Load(&loaded, &error),
			"password login saves credentials");
		Check(loaded.username == "player" && loaded.token == "saved-token",
			"saved credentials use returned token");

		service.Shutdown();
		Check(service.PendingHttpCount() == 0, "shutdown clears pending HTTP");
	}

	{
		auto fake_http = MakeFakeHttp();
		Xm8Ra::FakeRaHttpClient *fake_http_raw = fake_http.get();
		Xm8Ra::RaServiceOptions options;
		options.ra_root = base;
		options.credentials_store =
			Xm8Ra::CreatePlatformRaCredentialsStore(base);
		options.http_client = std::move(fake_http);
		Xm8Ra::RaService service(std::move(options));

		std::string error;
		Check(service.BeginLoginWithSavedToken(&error),
			"begin saved token login");
		Check(fake_http_raw->SentRequests().size() == 1,
			"saved token login sends HTTP request");
		Check(fake_http_raw->SentRequests()[0].post_data.find(
			"t=saved-token") != std::string::npos,
			"saved token login request uses token");
		Check(fake_http_raw->SentRequests()[0].post_data.find(
			"p=") == std::string::npos,
			"saved token login does not send password field");

		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			"{\"Success\":false,\"Error\":\"Invalid token\","
			"\"Code\":\"invalid_credentials\"}"));
		service.DrainHttp();
		const Xm8Ra::RaLoginSnapshot snapshot = service.LoginSnapshot();
		Check(snapshot.state == Xm8Ra::RaLoginState::Failed,
			"saved token rejection fails login");
		Check(snapshot.credentials_deleted,
			"saved token rejection deletes credentials");

		Xm8Ra::RaCredentials loaded;
		Check(!credential_store.Load(&loaded, &error),
			"stored token removed after rejected token");
	}

	{
		Xm8Ra::RaCredentials credentials;
		credentials.username = "player";
		credentials.token = "pending-token";
		std::string error;
		Check(credential_store.Save(credentials, &error),
			"save credentials for pending token switch");

		auto fake_http = MakeFakeHttp();
		Xm8Ra::FakeRaHttpClient *fake_http_raw = fake_http.get();
		Xm8Ra::RaServiceOptions options;
		options.ra_root = base;
		options.credentials_store =
			Xm8Ra::CreatePlatformRaCredentialsStore(base);
		options.http_client = std::move(fake_http);
		Xm8Ra::RaService service(std::move(options));

		Check(service.BeginLoginWithSavedToken(&error),
			"begin saved token login before manual switch");
		Check(service.LoginSnapshot().state ==
			Xm8Ra::RaLoginState::LoginPending,
			"saved token login remains pending");
		Check(service.BeginLoginWithPassword("player", "password", &error),
			"password login aborts pending token login");
		Check(fake_http_raw->SentRequests().size() == 2,
			"manual login sends second HTTP request");
		Check(fake_http_raw->SentRequests()[1].post_data.find(
			"p=password") != std::string::npos,
			"manual switch request sends password");

		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			"{\"Success\":true,\"User\":\"player\",\"Token\":\"manual-token\","
			"\"Score\":10,\"SoftcoreScore\":20,\"Messages\":0}"));
		service.DrainHttp();
		Check(service.LoginSnapshot().state == Xm8Ra::RaLoginState::LoggedIn,
			"manual login succeeds after aborting token login");
	}

	{
		Xm8Ra::RaCredentials credentials;
		credentials.username = "player";
		credentials.token = "logout-token";
		std::string error;
		Check(credential_store.Save(credentials, &error),
			"save credentials for logout");

		auto fake_http = MakeFakeHttp();
		Xm8Ra::FakeRaHttpClient *fake_http_raw = fake_http.get();
		Xm8Ra::RaServiceOptions options;
		options.ra_root = base;
		options.credentials_store =
			Xm8Ra::CreatePlatformRaCredentialsStore(base);
		options.http_client = std::move(fake_http);
		Xm8Ra::RaService service(std::move(options));

		Check(service.BeginLoginWithSavedToken(&error),
			"begin saved token login for logout");
		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			"{\"Success\":true,\"User\":\"player\",\"Token\":\"logout-token\","
			"\"Score\":1,\"SoftcoreScore\":2,\"Messages\":0}"));
		service.DrainHttp();
		Check(service.LoginSnapshot().state == Xm8Ra::RaLoginState::LoggedIn,
			"saved token login succeeds before logout");

		service.Logout();
		Check(service.LoginSnapshot().state == Xm8Ra::RaLoginState::LoggedOut,
			"logout resets login state");
		Xm8Ra::RaCredentials loaded;
		Check(!credential_store.Load(&loaded, &error),
			"logout deletes saved token");
	}

	{
		Xm8Ra::RaCredentials credentials;
		credentials.username = "player";
		credentials.token = "resume-token";
		std::string error;
		Check(credential_store.Save(credentials, &error),
			"save credentials for resume login");

		auto fake_http = MakeFakeHttp();
		Xm8Ra::FakeRaHttpClient *fake_http_raw = fake_http.get();
		Xm8Ra::RaServiceOptions options;
		options.ra_root = base;
		options.credentials_store =
			Xm8Ra::CreatePlatformRaCredentialsStore(base);
		options.http_client = std::move(fake_http);
		Xm8Ra::RaService service(std::move(options));

		Check(service.BeginLoginWithSavedToken(&error),
			"begin saved token login before unload");
		service.UnloadGame();
		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			"{\"Success\":true,\"User\":\"player\",\"Token\":\"resume-token\","
			"\"Score\":1,\"SoftcoreScore\":2,\"Messages\":0}"));
		service.DrainHttp();
		Check(service.LoginSnapshot().state == Xm8Ra::RaLoginState::LoggedIn,
			"unload game does not discard pending login");
	}

	{
		Xm8Ra::RaCredentials credentials;
		credentials.username = "player";
		credentials.token = "load-token";
		std::string error;
		Check(credential_store.Save(credentials, &error),
			"save credentials for load game");

		auto fake_http = MakeFakeHttp();
		Xm8Ra::FakeRaHttpClient *fake_http_raw = fake_http.get();
		Xm8Ra::RaServiceOptions options;
		options.ra_root = base;
		options.credentials_store =
			Xm8Ra::CreatePlatformRaCredentialsStore(base);
		options.http_client = std::move(fake_http);
		Xm8Ra::RaService service(std::move(options));

		Check(service.BeginLoginWithSavedToken(&error),
			"begin saved token login for load game");
		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			"{\"Success\":true,\"User\":\"player\",\"Token\":\"load-token\","
			"\"Score\":1,\"SoftcoreScore\":2,\"Messages\":0}"));
		service.DrainHttp();
		Check(service.LoginSnapshot().state == Xm8Ra::RaLoginState::LoggedIn,
			"saved token login succeeds before load game");

		const std::string hash = kKnownSupportedPc8800Hash;
		Check(service.BeginLibrarySync({hash}, &error),
			"begin idle library sync");
		Check(service.LibrarySyncSnapshot().state ==
			Xm8Ra::RaLibrarySyncState::PendingHashes,
			"library sync starts with hash library");
		Check(fake_http_raw->SentRequests().back().post_data.find(
			"r=hashlibrary") != std::string::npos,
			"library sync requests PC-8800 hash library");
		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			std::string("{\"Success\":true,\"MD5List\":{\"") + hash +
			"\":1234,\"ffffffffffffffffffffffffffffffff\":9999}}"));
		service.DrainHttp();
		Check(service.LibrarySyncSnapshot().state ==
			Xm8Ra::RaLibrarySyncState::PendingTitles,
			"library sync advances to matched titles");
		Check(service.LibrarySyncSnapshot().hashes.size() == 1 &&
			service.LibrarySyncSnapshot().hashes[0].game_id == 1234,
			"hash library is filtered to local hashes");
		Check(fake_http_raw->SentRequests().back().post_data.find(
			"r=gameinfolist") != std::string::npos,
			"library sync requests matched game titles");
		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			"{\"Success\":true,\"Response\":[{\"ID\":1234,"
			"\"Title\":\"Synced Game\",\"ImageIcon\":\"/Images/1234.png\","
			"\"ImageUrl\":\"https://media.example/1234.png\"}]}"));
		service.DrainHttp();
		Check(service.LibrarySyncSnapshot().state ==
			Xm8Ra::RaLibrarySyncState::PendingProgress,
			"library sync advances to all progress");
		Check(fake_http_raw->SentRequests().back().post_data.find(
			"r=allprogress") != std::string::npos,
			"library sync requests all user progress");
		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			"{\"Success\":true,\"Response\":{\"1234\":{"
			"\"Achievements\":10,\"Unlocked\":4,"
			"\"UnlockedHardcore\":2}}}"));
		service.DrainHttp();
		const Xm8Ra::RaLibrarySyncSnapshot library_sync =
			service.LibrarySyncSnapshot();
		Check(library_sync.state == Xm8Ra::RaLibrarySyncState::Succeeded,
			"library sync succeeds only after all three responses");
		Check(library_sync.username == "player" &&
			library_sync.titles.size() == 1 &&
			library_sync.progress.size() == 1,
			"library sync copies username, title, and progress");
		Check(library_sync.progress[0].total == 10 &&
			library_sync.progress[0].unlocked == 4 &&
			library_sync.progress[0].hardcore_unlocked == 2,
			"library sync progress values are copied");
		service.ClearLibrarySyncResult();
		Check(service.LibrarySyncSnapshot().state ==
			Xm8Ra::RaLibrarySyncState::None,
			"completed library sync result can be consumed");

		Check(service.BeginLibrarySync({hash}, &error),
			"begin library sync that will partially fail");
		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			std::string("{\"Success\":true,\"MD5List\":{\"") + hash +
			"\":1234}}"));
		service.DrainHttp();
		const size_t requests_before_title_failure =
			fake_http_raw->SentRequests().size();
		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			"{\"Success\":false,\"Error\":\"Titles unavailable\"}"));
		service.DrainHttp();
		Check(service.LibrarySyncSnapshot().state ==
			Xm8Ra::RaLibrarySyncState::Failed,
			"partial library sync is reported as failed");
		Check(fake_http_raw->SentRequests().size() ==
			requests_before_title_failure,
			"failed title stage does not request progress");
		service.ClearLibrarySyncResult();

		Check(service.BeginLibrarySync({hash}, &error),
			"begin library sync before game start");
		const uint64_t pending_sync_request = service.LastIssuedRequestId();
		service.UnloadGame();
		fake_http_raw->Complete(MakeJsonResponse(pending_sync_request,
			std::string("{\"Success\":true,\"MD5List\":{\"") + hash +
			"\":1234}}"));
		service.DrainHttp();
		Check(service.LibrarySyncSnapshot().state ==
			Xm8Ra::RaLibrarySyncState::None,
			"game start aborts pending library sync and ignores late response");

		Check(service.BeginLoadGameByHash(hash, &error),
			"begin load game by hash");
		Check(service.GameSessionSnapshot().state ==
			Xm8Ra::RaGameSessionState::LoadPending,
			"load game enters pending state");
		Check(fake_http_raw->SentRequests().back().post_data.find(
			"r=achievementsets") != std::string::npos,
			"load game fetches achievement sets through rc_client");
		Check(fake_http_raw->SentRequests().back().post_data.find(
			std::string("m=") + kKnownSupportedPc8800Hash) != std::string::npos,
			"load game sends known supported PC-8800 hash");

		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			MinimalAchievementSetsJson()));
		service.DrainHttp();
		Check(fake_http_raw->SentRequests().back().post_data.find(
			"r=startsession") != std::string::npos,
			"load game starts RA session");
		Check(fake_http_raw->SentRequests().back().post_data.find(
			"g=1234") != std::string::npos,
			"start session uses identified game id");

		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			StartSessionJson()));
		service.DrainHttp();
		service.Idle();
		const Xm8Ra::RaGameSessionSnapshot snapshot =
			service.GameSessionSnapshot();
		Check(snapshot.state == Xm8Ra::RaGameSessionState::Loaded,
			"load game succeeds");
		Check(snapshot.game_id == 1234, "loaded game id captured");
		Check(snapshot.console_id == 47, "loaded console id captured");
		Check(snapshot.title == "Test Game", "loaded game title captured");
		Check(snapshot.hash == hash, "loaded game hash retained");
		Check(!snapshot.disabled_for_session,
			"successful load does not disable RA session");
		const Xm8Ra::RaAchievementListSnapshot achievements =
			service.AchievementListSnapshot();
		Check(achievements.game_loaded,
			"achievement list reports loaded game");
		Check(achievements.game_title == "Test Game",
			"achievement list captures game title");
		Check(!achievements.has_achievements,
			"empty achievement set reports no achievements");
		Check(achievements.achievements.empty(),
			"empty achievement set has no list items");

		const std::string alternate_media_hash =
			"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
		Check(service.BeginChangeMediaByHash(alternate_media_hash, &error),
			"begin media change by hash");
		Check(service.MediaChangeSnapshot().state ==
			Xm8Ra::RaMediaChangeState::Pending,
			"media change enters pending state");
		Check(fake_http_raw->SentRequests().back().post_data.find(
			"r=gameid") != std::string::npos,
			"media change resolves an unknown hash");
		Check(fake_http_raw->SentRequests().back().post_data.find(
			std::string("m=") + alternate_media_hash) != std::string::npos,
			"media change sends the requested hash");
		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			"{\"Success\":true,\"GameID\":1234}"));
		service.DrainHttp();
		Check(service.MediaChangeSnapshot().state ==
			Xm8Ra::RaMediaChangeState::Pending,
			"verified media hash is passed to rc_client");
		Check(fake_http_raw->SentRequests().back().post_data.find(
			"r=gameid") != std::string::npos,
			"rc_client resolves the verified media hash");
		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			"{\"Success\":true,\"GameID\":1234}"));
		service.DrainHttp();
		Check(service.MediaChangeSnapshot().state ==
			Xm8Ra::RaMediaChangeState::Succeeded,
			"media change succeeds after hash resolution");
		Check(service.GameSessionSnapshot().hash == alternate_media_hash,
			"successful media change updates active hash");
		service.ClearMediaChangeResult();
		Check(service.MediaChangeSnapshot().state ==
			Xm8Ra::RaMediaChangeState::None,
			"media change result can be consumed");

		const std::string failed_media_hash =
			"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
		Check(service.BeginChangeMediaByHash(failed_media_hash, &error),
			"begin media change that will fail");
		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			"{\"Success\":false,\"Error\":\"Unknown media\"}"));
		service.DrainHttp();
		Check(service.MediaChangeSnapshot().state ==
			Xm8Ra::RaMediaChangeState::Failed,
			"failed media hash resolution is reported");
		Check(service.GameSessionSnapshot().hash == alternate_media_hash,
			"failed media change preserves the active hash");
		service.ClearMediaChangeResult();

		const std::string other_game_media_hash =
			"dddddddddddddddddddddddddddddddd";
		Check(service.BeginChangeMediaByHash(other_game_media_hash, &error),
			"begin media change for a different RA game");
		const size_t requests_before_other_game =
			fake_http_raw->SentRequests().size();
		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			"{\"Success\":true,\"GameID\":9999}"));
		service.DrainHttp();
		Check(service.MediaChangeSnapshot().state ==
			Xm8Ra::RaMediaChangeState::Failed,
			"different RA Game ID is rejected before media change");
		Check(fake_http_raw->SentRequests().size() ==
			requests_before_other_game,
			"different-game hash is not passed to rc_client media change");
		Check(service.GameSessionSnapshot().hash == alternate_media_hash,
			"different-game media preserves active hash");
		service.ClearMediaChangeResult();

		Check(service.BeginChangeMediaByHash(hash, &error),
			"begin media rollback to a known hash");
		Check(service.MediaChangeSnapshot().state ==
			Xm8Ra::RaMediaChangeState::Succeeded,
			"known media rollback completes synchronously");
		Check(service.GameSessionSnapshot().hash == hash,
			"media rollback restores the previous hash");
		service.ClearMediaChangeResult();
		Check(!service.BeginChangeMediaByHash("not-a-md5", &error),
			"reject invalid media change hash");

		Check(service.BeginFetchLeaderboardEntries(77, 1, 5, &error),
			"begin leaderboard entry fetch");
		Check(fake_http_raw->SentRequests().back().post_data.find(
			"r=lbinfo") != std::string::npos,
			"leaderboard entry fetch uses lbinfo API");
		Check(fake_http_raw->SentRequests().back().post_data.find(
			"i=77") != std::string::npos,
			"leaderboard entry fetch sends leaderboard id");
		Check(fake_http_raw->SentRequests().back().post_data.find(
			"c=5") != std::string::npos,
			"leaderboard entry fetch sends entry count");
		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			LeaderboardInfoJson()));
		service.DrainHttp();
		const Xm8Ra::RaLeaderboardEntriesSnapshot leaderboard_entries =
			service.LeaderboardEntriesSnapshot();
		Check(leaderboard_entries.state ==
			Xm8Ra::RaLeaderboardEntriesState::Loaded,
			"leaderboard entries load succeeds");
		Check(leaderboard_entries.leaderboard_id == 77,
			"leaderboard entries retain leaderboard id");
		Check(leaderboard_entries.total_entries == 10,
			"leaderboard entries capture total entry count");
		Check(leaderboard_entries.entries.size() == 2,
			"leaderboard entries copy returned entries");
		Check(leaderboard_entries.entries[1].username == "player",
			"leaderboard entry username is copied");
		Check(leaderboard_entries.entries[1].display == "000900",
			"leaderboard entry display is formatted");

		const std::string pending_media_hash =
			"cccccccccccccccccccccccccccccccc";
		Check(service.BeginChangeMediaByHash(pending_media_hash, &error),
			"begin media change before unload");
		const uint64_t pending_media_request = service.LastIssuedRequestId();
		Check(service.MediaChangeSnapshot().state ==
			Xm8Ra::RaMediaChangeState::Pending,
			"media change remains pending before unload");
		service.UnloadGame();
		Check(service.MediaChangeSnapshot().state ==
			Xm8Ra::RaMediaChangeState::None,
			"unload aborts pending media change");
		fake_http_raw->Complete(MakeJsonResponse(pending_media_request,
			"{\"Success\":true,\"GameID\":1234}"));
		service.DrainHttp();
		Check(service.GameSessionSnapshot().state ==
			Xm8Ra::RaGameSessionState::NoGame,
			"unload game resets game session state");
		Check(service.BeginLoadGameByHash(
			"11111111111111111111111111111111", &error),
			"begin reload after unload");
		Check(fake_http_raw->SentRequests().back().post_data.find(
			"m=11111111111111111111111111111111") != std::string::npos,
			"reload uses new RA identification hash");
	}

	{
		auto fake_http = MakeFakeHttp();
		Xm8Ra::FakeRaHttpClient *fake_http_raw = fake_http.get();
		Xm8Ra::RaServiceOptions options;
		options.ra_root = base;
		options.credentials_store =
			Xm8Ra::CreatePlatformRaCredentialsStore(base);
		options.http_client = std::move(fake_http);
		Xm8Ra::RaService service(std::move(options));

		std::string error;
		Check(service.BeginLoginWithPassword("player", "secret", &error),
			"begin password login before failed load");
		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			"{\"Success\":true,\"User\":\"player\",\"Token\":\"failed-load\","
			"\"Score\":1,\"SoftcoreScore\":2,\"Messages\":0}"));
		service.DrainHttp();

		Check(!service.BeginLoadGameByHash("not-a-md5", &error),
			"reject invalid hash before RA request");
		Check(fake_http_raw->SentRequests().size() == 1,
			"invalid hash does not send HTTP request");

		Check(service.BeginLoadGameByHash(
			"fedcba9876543210fedcba9876543210", &error),
			"begin load game that will fail");
		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			"{\"Success\":false,\"Error\":\"Game not found\","
			"\"Code\":\"not_found\"}"));
		service.DrainHttp();
		const Xm8Ra::RaGameSessionSnapshot snapshot =
			service.GameSessionSnapshot();
		Check(snapshot.state == Xm8Ra::RaGameSessionState::DisabledForSession,
			"failed game load disables RA for session");
		Check(snapshot.disabled_for_session,
			"failed game load marks disabled flag");
		Check(!snapshot.message.empty(),
			"failed game load captures an explanatory message");
	}

	{
		auto fake_http = MakeFakeHttp();
		Xm8Ra::FakeRaHttpClient *fake_http_raw = fake_http.get();
		FrameMemory memory;
		Xm8Ra::RaServiceOptions options;
		options.ra_root = base;
		options.credentials_store =
			Xm8Ra::CreatePlatformRaCredentialsStore(base);
		options.http_client = std::move(fake_http);
		options.host_read_memory = ReadFrameMemory;
		options.host_read_memory_userdata = &memory;
		Xm8Ra::RaService service(std::move(options));

		std::string error;
		Check(service.BeginLoginWithPassword("player", "secret", &error),
			"begin login for frame evaluation");
		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			"{\"Success\":true,\"User\":\"player\",\"Token\":\"frame-token\","
			"\"Score\":1,\"SoftcoreScore\":2,\"Messages\":0}"));
		service.DrainHttp();
		Check(service.BeginLoadGameByHash(kKnownSupportedPc8800Hash, &error),
			"begin frame evaluation game load");
		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			FrameAchievementSetsJson()));
		service.DrainHttp();
		if (fake_http_raw->SentRequests().empty() ||
			fake_http_raw->SentRequests().back().post_data.find(
				"r=startsession") == std::string::npos) {
			std::cerr << "frame evaluation did not request startsession\n";
		}
		Check(!fake_http_raw->SentRequests().empty() &&
			fake_http_raw->SentRequests().back().post_data.find(
				"r=startsession") != std::string::npos,
			"frame evaluation starts an RA session");
		fake_http_raw->Complete(MakeJsonResponse(service.LastIssuedRequestId(),
			StartSessionJson()));
		service.DrainHttp();
		service.Idle();
		const Xm8Ra::RaGameSessionSnapshot frame_game =
			service.GameSessionSnapshot();
		if (frame_game.state != Xm8Ra::RaGameSessionState::Loaded) {
			std::cerr << "frame evaluation game failed to load: "
				<< frame_game.message << '\n';
		}
		Check(frame_game.state == Xm8Ra::RaGameSessionState::Loaded,
			"frame evaluation game is loaded");
		service.TakeEvents();

		memory.value = 0;
		Check(service.DoFrame(), "evaluate first completed frame");
		Check(service.TakeEvents().empty(),
			"achievement remains locked on first frame");
		memory.value = 1;
		Check(service.DoFrame(), "evaluate second completed frame");
		const std::vector<Xm8Ra::RaEvent> frame_events = service.TakeEvents();
		bool triggered = false;
		for (const Xm8Ra::RaEvent& event : frame_events) {
			if (event.type == Xm8Ra::RaEventType::AchievementTriggered &&
				event.achievement.id == 42) {
				triggered = true;
			}
		}
		Check(triggered,
			"memory change on the second frame triggers the achievement");
		Check(memory.reads >= 2,
			"each completed frame reads current emulated memory");
	}

	{
		auto fake_http = MakeFakeHttp();
		Xm8Ra::RaServiceOptions options;
		options.ra_root = base;
		options.credentials_store =
			Xm8Ra::CreatePlatformRaCredentialsStore(base);
		options.http_client = std::move(fake_http);
		Xm8Ra::RaService service(std::move(options));

		Check(!service.DoFrame(), "do frame is ignored before a game is loaded");
		Check(service.Idle(), "idle is allowed before a game is loaded");
		Check(service.TakeEvents().empty(),
			"idle before game load does not synthesize RA events");

		rc_client_achievement_t achievement = {};
		achievement.id = 42;
		achievement.points = 5;
		achievement.title = "Original Title";
		achievement.description = "Original Description";
		achievement.measured_percent = 50.0f;
		std::snprintf(achievement.measured_progress,
			sizeof(achievement.measured_progress), "%s", "1/2");
		achievement.badge_url = "https://media.example/ach.png";
		achievement.badge_locked_url = "https://media.example/lock.png";

		rc_client_event_t event = {};
		event.type = RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED;
		event.achievement = &achievement;
		service.QueueEventForTesting(&event);
		achievement.title = "Mutated Title";

		std::vector<Xm8Ra::RaEvent> events = service.TakeEvents();
		Check(events.size() == 1, "achievement event is queued");
		Check(events[0].type == Xm8Ra::RaEventType::AchievementTriggered,
			"achievement event type is mapped");
		Check(events[0].achievement.id == 42,
			"achievement event id is copied");
		Check(events[0].achievement.title == "Original Title",
			"achievement event title is copied away from rcheevos pointer");
		Check(events[0].achievement.measured_progress == "1/2",
			"achievement event measured progress is copied");
		Check(service.TakeEvents().empty(),
			"take events drains the event queue");

		event = {};
		event.type = RC_CLIENT_EVENT_DISCONNECTED;
		service.QueueEventForTesting(&event);
		event.type = RC_CLIENT_EVENT_RECONNECTED;
		service.QueueEventForTesting(&event);
		events = service.TakeEvents();
		Check(events.size() == 2,
			"connection state events are queued in order");
		Check(events[0].type == Xm8Ra::RaEventType::Disconnected,
			"disconnected event is mapped");
		Check(events[1].type == Xm8Ra::RaEventType::Reconnected,
			"reconnected event is mapped");
	}

	{
		auto fake_http = MakeFakeHttp();
		Xm8Ra::RaServiceOptions options;
		options.ra_root = base;
		options.credentials_store =
			Xm8Ra::CreatePlatformRaCredentialsStore(base);
		options.http_client = std::move(fake_http);
		Xm8Ra::RaService service(std::move(options));

		rc_client_leaderboard_scoreboard_entry_t top_entries[2] = {};
		top_entries[0].username = "first";
		top_entries[0].rank = 1;
		std::snprintf(top_entries[0].score, sizeof(top_entries[0].score),
			"%s", "1000");
		top_entries[1].username = "second";
		top_entries[1].rank = 2;
		std::snprintf(top_entries[1].score, sizeof(top_entries[1].score),
			"%s", "900");

		rc_client_leaderboard_scoreboard_t scoreboard = {};
		scoreboard.leaderboard_id = 77;
		scoreboard.new_rank = 2;
		scoreboard.num_entries = 10;
		std::snprintf(scoreboard.submitted_score,
			sizeof(scoreboard.submitted_score), "%s", "900");
		std::snprintf(scoreboard.best_score, sizeof(scoreboard.best_score),
			"%s", "900");
		scoreboard.top_entries = top_entries;
		scoreboard.num_top_entries = 2;

		rc_client_event_t event = {};
		event.type = RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD;
		event.leaderboard_scoreboard = &scoreboard;
		service.QueueEventForTesting(&event);
		top_entries[1].username = "mutated";

		const std::vector<Xm8Ra::RaEvent> events = service.TakeEvents();
		Check(events.size() == 1, "scoreboard event is queued");
		Check(events[0].type == Xm8Ra::RaEventType::LeaderboardScoreboard,
			"scoreboard event type is mapped");
		Check(events[0].scoreboard.leaderboard_id == 77,
			"scoreboard leaderboard id is copied");
		Check(events[0].scoreboard.top_entries.size() == 2,
			"scoreboard top entries are copied");
		Check(events[0].scoreboard.top_entries[1].username == "second",
			"scoreboard entry username is copied away from rcheevos pointer");
	}

	{
		auto fake_http = MakeFakeHttp();
		Xm8Ra::RaServiceOptions options;
		options.ra_root = base;
		options.credentials_store =
			Xm8Ra::CreatePlatformRaCredentialsStore(base);
		options.http_client = std::move(fake_http);
		Xm8Ra::RaService service(std::move(options));

		std::string error;
		Check(service.BeginLoginWithPassword("player", "pending-secret",
			&error), "begin pending login");
		Check(service.PendingHttpCount() == 1, "pending login has HTTP call");
		service.Shutdown();
		Check(service.PendingHttpCount() == 0,
			"shutdown drains canceled request");
	}

	credential_store.Delete(nullptr);
#ifndef _WIN32
	rmdir(base.c_str());
#endif

	if (failures != 0) {
		std::cerr << failures << " RA service test failure(s)\n";
		return 1;
	}

	std::cout << "RA service tests passed\n";
	return 0;
}
