#include "Fixtures/d88_fixture.h"
#include "ra_library.h"
#include "ra_media_store.h"
#include "ra_paths.h"
#include "sqlite3.h"

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <sys/stat.h>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace {

int failures = 0;

void Check(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		failures++;
	}
}

std::string JoinPath(const std::string& dir, const char *name)
{
	if (!dir.empty() && dir.back() == '/') {
		return dir + name;
	}
	return dir + "/" + name;
}

bool PathExists(const std::string& path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0;
}

bool MakeDirectoryTree(const std::string& path, std::string *error)
{
	std::string current;
	size_t index = 0;
	if (!path.empty() && path[0] == '/') {
		current = "/";
		index = 1;
	}

	while (index <= path.size()) {
		const size_t slash = path.find('/', index);
		const std::string part = path.substr(index,
			slash == std::string::npos ? std::string::npos : slash - index);
		if (!part.empty()) {
			if (!current.empty() && current.back() != '/') {
				current += '/';
			}
			current += part;
			if (mkdir(current.c_str(), 0755) != 0 && errno != EEXIST) {
				if (error != nullptr) {
					*error = std::strerror(errno);
				}
				return false;
			}
		}
		if (slash == std::string::npos) {
			break;
		}
		index = slash + 1;
	}
	return true;
}

void RemoveTree(const std::string& path)
{
	struct stat st;
	if (lstat(path.c_str(), &st) != 0) {
		return;
	}

	if (S_ISDIR(st.st_mode)) {
		DIR *dir = opendir(path.c_str());
		if (dir != nullptr) {
			while (dirent *entry = readdir(dir)) {
				const std::string name = entry->d_name;
				if (name == "." || name == "..") {
					continue;
				}
				RemoveTree(JoinPath(path, name.c_str()));
			}
			closedir(dir);
		}
		rmdir(path.c_str());
	}
	else {
		unlink(path.c_str());
	}
}

std::vector<char> ReadFile(const std::string& path)
{
	std::ifstream stream(path, std::ios::binary);
	return std::vector<char>(std::istreambuf_iterator<char>(stream),
		std::istreambuf_iterator<char>());
}

bool AppendByte(const std::string& path, char value)
{
	std::ofstream stream(path, std::ios::binary | std::ios::app);
	if (!stream.is_open()) {
		return false;
	}
	stream.put(value);
	return stream.good();
}

bool WriteTextFile(const std::string& path, const char *text)
{
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream.is_open()) {
		return false;
	}
	stream << text;
	return stream.good();
}

bool CopyFileBytes(const std::string& source, const std::string& destination)
{
	std::ifstream input(source, std::ios::binary);
	std::ofstream output(destination, std::ios::binary | std::ios::trunc);
	if (!input.is_open() || !output.is_open()) {
		return false;
	}
	output << input.rdbuf();
	return input.good() && output.good();
}

bool ExecSql(const std::string& database_path, const char *sql,
	std::string *error)
{
	sqlite3 *db = nullptr;
	if (sqlite3_open_v2(database_path.c_str(), &db,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK) {
		if (error != nullptr) {
			*error = db != nullptr ? sqlite3_errmsg(db) : "open sqlite failed";
		}
		if (db != nullptr) {
			sqlite3_close(db);
		}
		return false;
	}
	char *message = nullptr;
	const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &message);
	if (rc != SQLITE_OK) {
		if (error != nullptr) {
			*error = message != nullptr ? message : sqlite3_errmsg(db);
		}
		sqlite3_free(message);
		sqlite3_close(db);
		return false;
	}
	sqlite3_close(db);
	return true;
}

int QueryInt(const std::string& database_path, const char *sql)
{
	sqlite3 *db = nullptr;
	if (sqlite3_open_v2(database_path.c_str(), &db, SQLITE_OPEN_READONLY,
		nullptr) != SQLITE_OK) {
		if (db != nullptr) {
			sqlite3_close(db);
		}
		return -1;
	}
	sqlite3_stmt *stmt = nullptr;
	int value = -1;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK &&
		sqlite3_step(stmt) == SQLITE_ROW) {
		value = sqlite3_column_int(stmt, 0);
	}
	if (stmt != nullptr) {
		sqlite3_finalize(stmt);
	}
	sqlite3_close(db);
	return value;
}

std::string QueryText(const std::string& database_path, const char *sql)
{
	sqlite3 *db = nullptr;
	if (sqlite3_open_v2(database_path.c_str(), &db, SQLITE_OPEN_READONLY,
		nullptr) != SQLITE_OK) {
		if (db != nullptr) {
			sqlite3_close(db);
		}
		return "";
	}
	sqlite3_stmt *stmt = nullptr;
	std::string value;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK &&
		sqlite3_step(stmt) == SQLITE_ROW) {
		const unsigned char *text = sqlite3_column_text(stmt, 0);
		if (text != nullptr) {
			value = reinterpret_cast<const char *>(text);
		}
	}
	if (stmt != nullptr) {
		sqlite3_finalize(stmt);
	}
	sqlite3_close(db);
	return value;
}

bool DirectoryHasPrefix(const std::string& path, const char *prefix)
{
	DIR *dir = opendir(path.c_str());
	if (dir == nullptr) {
		return false;
	}
	const std::string prefix_string = prefix;
	bool found = false;
	while (dirent *entry = readdir(dir)) {
		const std::string name = entry->d_name;
		if (name.compare(0, prefix_string.size(), prefix_string) == 0) {
			found = true;
			break;
		}
	}
	closedir(dir);
	return found;
}

} // namespace

int main()
{
	const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
	const char *temporary = std::getenv(
#ifdef _WIN32
		"TEMP"
#else
		"TMPDIR"
#endif
	);
	const std::string base = std::string(temporary != nullptr ? temporary :
#ifdef _WIN32
		"."
#else
		"/tmp"
#endif
	) + "/xm8-ra-library-store-" + std::to_string(unique);
	const std::string source_dir = JoinPath(base, "source");
	const std::string ra_root = JoinPath(base, "ra");

	std::string error;
	Check(MakeDirectoryTree(base, &error), "create test root");
	if (!error.empty()) {
		std::cerr << error << '\n';
	}
	Check(Xm8Ra::RootFromSettingDir((base + "/").c_str()) == ra_root,
		"RA root is under setting directory");
	Check(Xm8Ra::RootFromSettingDir("") == "",
		"empty setting directory has no RA root");
	Check(D88Fixture::GenerateStandardSet(source_dir, &error),
		"generate fixture set");
	if (!error.empty()) {
		std::cerr << error << '\n';
	}

	Xm8Ra::RaLibrary library;
	Check(library.Open(ra_root, &error), "open RA library");
	if (!error.empty()) {
		std::cerr << error << '\n';
	}
	Check(PathExists(JoinPath(ra_root, "library.sqlite3")),
		"library DB exists");

	Xm8Ra::RaSettings settings;
	Check(library.LoadSettings(&settings, &error), "load default RA settings");
	Check(!settings.enabled, "RA settings default disabled");
	Check(settings.last_mode == Xm8Ra::kRaModeSoftcore,
		"RA settings default softcore");
	settings.enabled = true;
	settings.last_mode = Xm8Ra::kRaModeHardcore;
	settings.unofficial_enabled = true;
	settings.notification_seconds = 8;
	settings.image_cache_limit_mib = 256;
	Check(library.SaveSettings(settings, &error), "save RA settings");

	Xm8Ra::RaMediaStore store(&library);
	Xm8Ra::ImportedMedia first;
	const std::string single = JoinPath(source_dir, "single.d88");
	const auto original_single = ReadFile(single);
	Check(store.ImportDesktopD88(single, &first, &error), "import single D88");
	if (!error.empty()) {
		std::cerr << error << '\n';
	}
	Check(first.record.md5 == "5c50ca4f9e3a7afbe4d6666e8974949d",
		"single md5 registered");
	Check(first.record.game_id > 0, "game id assigned");
	Check(first.copied, "first import created working copy");
	Check(ReadFile(first.working_path) == original_single,
		"working copy equals original at creation");
	Check(ReadFile(single) == original_single, "original remains unchanged");
	Check(first.working_path.find("/ra/media/") != std::string::npos,
		"working copy is under RA media root");

	std::vector<Xm8Ra::RaLibraryGameListItem> games;
	Check(library.ListGames(&games, &error), "list games after first import");
	Check(games.empty(), "unidentified import is hidden from RA library");
	Check(library.MarkGameIdentified(first.record.game_id, 1234,
		"RA Single Game", "https://media.example/badge.png", &error),
		"mark first game identified");
	Check(library.ListGames(&games, &error),
		"list games after RA identification");
	Check(games.size() == 1, "library list has identified game");
	if (!games.empty()) {
		Check(games[0].game_id == first.record.game_id,
			"library list game id");
		Check(games[0].ra_game_id == 1234, "library list RA game id");
		Check(games[0].title == "RA Single Game",
			"library list uses RA title");
		Check(games[0].badge_url == "https://media.example/badge.png",
			"library list badge URL");
		Check(games[0].media_count == 1,
			"library list media count");
		Check(games[0].health_state == Xm8Ra::kRaMediaHealthOk,
			"library list health");
		Check(!games[0].has_progress,
			"library list progress initially absent");
	}

	Xm8Ra::ImportedMedia duplicate;
	Check(store.ImportDesktopD88(single, &duplicate, &error),
		"duplicate import");
	Check(duplicate.record.md5 == first.record.md5,
		"duplicate md5 reused");
	Check(duplicate.record.game_id == first.record.game_id,
		"duplicate game reused");
	Check(!duplicate.copied, "duplicate did not overwrite working copy");
	Check(ReadFile(duplicate.working_path) == original_single,
		"duplicate working copy unchanged");

	Xm8Ra::MediaHealthStatus health;
	Check(store.CheckMediaHealth(first.record.md5, &health, &error),
		"check healthy media");
	Check(health.health_state == Xm8Ra::kRaMediaHealthOk,
		"healthy media state");
	Check(health.source_exists, "healthy media source exists");
	Check(health.working_exists, "healthy media working copy exists");
	Check(health.working_probe_ok, "healthy media working copy probes");

	Check(unlink(first.working_path.c_str()) == 0,
		"remove working copy for health check");
	Check(store.CheckMediaHealth(first.record.md5, &health, &error),
		"check missing working copy");
	Check(health.health_state == Xm8Ra::kRaMediaHealthWorkingMissing,
		"missing working copy health state");
	std::string reset_path;
	Check(store.ResetWorkingCopy(single, first.record.md5, &reset_path, &error),
		"restore missing working copy from matching original");
	Check(ReadFile(first.working_path) == original_single,
		"restored missing working copy bytes");

	Check(AppendByte(first.working_path, '\x24'),
		"simulate save data in working copy");
	Check(ReadFile(single) == original_single,
		"simulated save data does not change original");
	Check(store.ResetWorkingCopy(single, first.record.md5, &reset_path, &error),
		"reset working copy from matching original");
	Check(reset_path == first.working_path, "reset targets same working path");
	Check(ReadFile(first.working_path) == original_single,
		"reset working copy restores original bytes");

	Xm8Ra::ImportedPlaylist playlist;
	Check(store.ImportM3U(JoinPath(source_dir, "pair.m3u"), &playlist, &error),
		"import M3U playlist");
	Check(playlist.media.size() == 2, "M3U imports two media entries");
	Check(playlist.game_id == first.record.game_id,
		"M3U reuses first media game");
	Check(playlist.anchor_md5 == first.record.md5,
		"M3U anchor is first media");
	if (playlist.media.size() == 2) {
		Check(playlist.media[0].record.game_id == playlist.game_id,
			"M3U first media game id");
		Check(playlist.media[1].record.game_id == playlist.game_id,
			"M3U second media game id");
		Check(playlist.media[1].record.md5 ==
			"ff400f51a2567419b3778691a905952e",
			"M3U second media md5");
		Check(playlist.media[1].working_path.find("/ra/media/") !=
			std::string::npos,
			"M3U second working copy is under RA media root");

		Xm8Ra::LaunchProfile profile;
		Check(library.LoadLaunchProfile(playlist.game_id, &profile, &error),
			"load default launch profile");
		Check(profile.drives[0].assigned,
			"default launch profile assigns drive 1");
		Check(profile.drives[0].media_md5 == playlist.anchor_md5,
			"default launch profile drive 1 uses anchor media");
		Check(profile.drives[0].is_ra_anchor,
			"default launch profile marks drive 1 as RA anchor");
		Check(!profile.drives[1].assigned,
			"default launch profile leaves drive 2 empty");

		profile.drives[1].assigned = true;
		profile.drives[1].media_md5 = playlist.media[1].record.md5;
		profile.drives[1].bank_index = 0;
		profile.drives[1].is_ra_anchor = false;
		Check(library.SaveLaunchProfile(profile, &error),
			"save two-drive launch profile");
		Xm8Ra::LaunchProfile saved_profile;
		Check(library.LoadLaunchProfile(playlist.game_id, &saved_profile,
			&error), "reload two-drive launch profile");
		Check(saved_profile.drives[1].assigned,
			"two-drive launch profile assigns drive 2");
		Check(saved_profile.drives[1].media_md5 ==
			playlist.media[1].record.md5,
			"two-drive launch profile drive 2 media");

		Xm8Ra::ResolvedLaunchProfile resolved;
		Check(store.ResolveLaunchProfile(playlist.game_id, &resolved,
			&error), "resolve two-drive launch profile");
		Check(resolved.drives[0].assigned && resolved.drives[1].assigned,
			"resolved launch profile has two drives");
		Check(resolved.drives[0].working_path.find("/ra/media/") !=
			std::string::npos,
			"resolved drive 1 uses RA working copy");
		Check(resolved.drives[1].working_path.find("/ra/media/") !=
			std::string::npos,
			"resolved drive 2 uses RA working copy");
		Check(resolved.drives[0].working_path != single,
			"resolved drive 1 does not use original path");
		Check(resolved.anchor_md5 == playlist.anchor_md5,
			"resolved launch profile exposes RA anchor");

		Check(library.ListGames(&games, &error),
			"list games after M3U import");
		Check(!games.empty() && games[0].media_count == 2,
			"library list counts grouped media");

		Check(unlink(resolved.drives[1].working_path.c_str()) == 0,
			"remove drive 2 working copy before resolve");
		Check(store.ResolveLaunchProfile(playlist.game_id, &resolved,
			&error), "resolve recreates missing working copy");
		Check(PathExists(resolved.drives[1].working_path),
			"resolved launch profile recreated drive 2 working copy");

		saved_profile.drives[0].is_ra_anchor = false;
		Check(!library.SaveLaunchProfile(saved_profile, &error),
			"reject launch profile without RA anchor");
	}

	Xm8Ra::ImportedMedia standalone_multi;
	Check(store.ImportDesktopD88(JoinPath(source_dir, "multi.d88"),
		&standalone_multi, &error), "import standalone multi-bank D88");
	Check(standalone_multi.record.game_id != first.record.game_id,
		"standalone D88 creates a separate game before merge");
	Check(library.MergeGameMedia(first.record.game_id,
		standalone_multi.record.game_id, &error),
		"merge standalone game media into existing game");
	Xm8Ra::MediaHealthRecord merged_record;
	Check(library.LoadMediaHealthRecord(standalone_multi.record.md5,
		&merged_record, &error), "load merged media record");
	Check(merged_record.game_id == first.record.game_id,
		"merged media belongs to target game");
	Xm8Ra::ResolvedLaunchProfile merged_profile;
	Check(store.ResolveLaunchProfile(first.record.game_id, &merged_profile,
		&error), "resolve launch profile after media merge");
	Check(merged_profile.anchor_md5 == first.record.md5,
		"media merge keeps target launch anchor");

	const std::string folder_dir = JoinPath(base, "folder-scan");
	const std::string nested_dir = JoinPath(folder_dir, "nested");
	Check(MakeDirectoryTree(nested_dir, &error), "create folder scan tree");
	Check(CopyFileBytes(JoinPath(source_dir, "single.d88"),
		JoinPath(nested_dir, "alpha.D88")), "copy uppercase D88");
	Check(CopyFileBytes(JoinPath(source_dir, "second.d88"),
		JoinPath(nested_dir, "beta.d88")), "copy second D88");
	Check(WriteTextFile(JoinPath(nested_dir, "group.M3U"),
		"# recursive import fixture\nalpha.D88#0\nbeta.d88#0\n"),
		"write uppercase M3U");
	Xm8Ra::ImportedFolder folder_import;
	Check(store.ImportFolderRecursive(folder_dir, &folder_import, &error),
		"recursive folder import");
	Check(folder_import.scanned_candidates == 3,
		"recursive folder scans D88 and M3U candidates");
	Check(folder_import.playlists.size() == 1,
		"recursive folder imports playlist");
	Check(folder_import.standalone_media.size() == 2,
		"recursive folder imports standalone D88 after playlist");
	if (!folder_import.playlists.empty()) {
		Check(folder_import.playlists[0].media.size() == 2,
			"recursive playlist has two media");
		Check(folder_import.playlists[0].anchor_md5 ==
			"5c50ca4f9e3a7afbe4d6666e8974949d",
			"recursive playlist anchor uses first D88");
	}

	Check(AppendByte(single, '\x55'), "modify source fixture");
	Check(!store.ResetWorkingCopy(single, first.record.md5, &reset_path, &error),
		"reset rejects modified original");
	Check(store.CheckMediaHealth(first.record.md5, &health, &error),
		"check modified original health");
	Check(health.health_state == Xm8Ra::kRaMediaHealthSourceChanged,
		"modified original health state");
	std::vector<Xm8Ra::RaMediaBankHash> local_hashes;
	Check(library.ListMediaBankHashes(&local_hashes, &error),
		"list bank hashes for library sync");
	Check(local_hashes.size() >= 4,
		"library sync sees single and multi-bank hashes");
	Xm8Ra::RaLibrarySyncPayload sync;
	sync.username = "tester";
	std::set<std::string> unique_sync_hashes;
	for (const Xm8Ra::RaMediaBankHash& local : local_hashes) {
		if (!unique_sync_hashes.insert(local.ra_hash).second) {
			continue;
		}
		Xm8Ra::RaLibraryHashMatch match;
		match.hash = local.ra_hash;
		match.ra_game_id = 1234;
		sync.hashes.push_back(match);
	}
	Xm8Ra::RaLibraryGameTitle sync_title;
	sync_title.ra_game_id = 1234;
	sync_title.title = "RA Synced Game";
	sync_title.badge_url = "https://media.example/synced.png";
	sync.titles.push_back(sync_title);
	Xm8Ra::RaLibraryProgress sync_progress;
	sync_progress.ra_game_id = 1234;
	sync_progress.core_total = 10;
	sync_progress.core_unlocked = 4;
	sync_progress.hardcore_unlocked = 2;
	sync.progress.push_back(sync_progress);
	const bool sync_ok = library.ApplyLibrarySync(sync, &error);
	Check(sync_ok, "apply complete library sync transaction");
	if (!sync_ok) {
		std::cerr << "library sync error: " << error << '\n';
	}
	Check(QueryInt(library.DatabasePath(),
		"SELECT COUNT(*) FROM sync_state WHERE sync_key ="
		" 'library:tester:pc8800'") == 1,
		"successful sync records completion");

	Xm8Ra::RaLibrarySyncPayload incomplete = sync;
	incomplete.username = "incomplete";
	incomplete.titles.clear();
	Check(!library.ApplyLibrarySync(incomplete, &error),
		"reject partial sync before database mutation");
	Check(QueryInt(library.DatabasePath(),
		"SELECT COUNT(*) FROM sync_state WHERE sync_key ="
		" 'library:incomplete:pc8800'") == 0,
		"partial sync does not advance completion state");

	Check(ExecSql(library.DatabasePath(),
		"CREATE TRIGGER fail_library_sync BEFORE INSERT ON progress"
		" WHEN NEW.username = 'rollback' BEGIN"
		" SELECT RAISE(ABORT, 'forced sync failure'); END;", &error),
		"install forced sync failure trigger");
	Xm8Ra::RaLibrarySyncPayload rollback_sync = sync;
	rollback_sync.username = "rollback";
	rollback_sync.titles[0].title = "Should Roll Back";
	Check(!library.ApplyLibrarySync(rollback_sync, &error),
		"database failure rolls back library sync");
	Check(QueryText(library.DatabasePath(),
		"SELECT title FROM games WHERE id = 1") == "RA Synced Game",
		"failed sync rolls back title update");
	Check(QueryInt(library.DatabasePath(),
		"SELECT COUNT(*) FROM sync_state WHERE sync_key ="
		" 'library:rollback:pc8800'") == 0,
		"failed transaction does not record completion");
	Check(ExecSql(library.DatabasePath(), "DROP TRIGGER fail_library_sync",
		&error), "remove forced sync failure trigger");

	Xm8Ra::RaLibrarySyncPayload other_user = sync;
	other_user.username = "other";
	other_user.progress[0].core_unlocked = 1;
	Check(library.ApplyLibrarySync(other_user, &error),
		"sync a second user independently");
	Check(QueryInt(library.DatabasePath(),
		"SELECT COUNT(*) FROM progress WHERE ra_game_id = 1234") == 2,
		"progress rows remain separated by username");
	std::vector<Xm8Ra::RaLibraryGameListItem> tester_games;
	std::vector<Xm8Ra::RaLibraryGameListItem> other_games;
	Check(library.ListGamesForUser("tester", &tester_games, &error),
		"list library progress for first user");
	Check(library.ListGamesForUser("other", &other_games, &error),
		"list library progress for second user");
	Check(!tester_games.empty() && tester_games[0].core_unlocked == 4,
		"first user sees only first user progress");
	Check(!other_games.empty() && other_games[0].core_unlocked == 1,
		"second user sees only second user progress");
	Check(library.ListGames(&games, &error),
		"list games with progress and health error");
	bool found_first_game = false;
	for (const Xm8Ra::RaLibraryGameListItem& item : games) {
		if (item.game_id == first.record.game_id) {
			found_first_game = true;
			Check(item.ra_game_id == 1234, "library list RA game id");
			Check(item.has_progress, "library list progress present");
			Check(item.core_total == 10 && item.core_unlocked == 4,
				"library list progress values");
			Check(item.hardcore_unlocked == 2,
				"library list hardcore progress");
			Check(item.points_total == 0 && item.points_unlocked == 0,
				"all-progress sync leaves unavailable points null");
			Check(item.health_state ==
				Xm8Ra::kRaMediaHealthSourceChanged,
				"library list reports health error");
		}
	}
	Check(found_first_game, "library list contains first game after progress");
	Check(library.MarkGamePlayed(first.record.game_id, &error),
		"mark game played");
	Check(library.ListGames(&games, &error),
		"list games after mark played");
	Check(!games.empty() && games[0].game_id == first.record.game_id,
		"recently played game sorts first");

	Xm8Ra::ImportedMedia modified;
	Check(store.ImportDesktopD88(single, &modified, &error),
		"modified source imports as new medium");
	Check(modified.record.md5 != first.record.md5,
		"modified source has distinct md5");
	Check(library.ListGames(&games, &error),
		"list games after unidentified modified import");
	bool found_modified_game = false;
	for (const Xm8Ra::RaLibraryGameListItem& item : games) {
		if (item.game_id == modified.record.game_id) {
			found_modified_game = true;
		}
	}
	Check(!found_modified_game,
		"unidentified modified import is hidden from RA library");
	Check(ReadFile(first.working_path) == original_single,
		"existing working save is not overwritten");

	library.Close();

	Xm8Ra::RaLibrary reopened;
	Check(reopened.Open(ra_root, &error), "reopen RA library");
	Xm8Ra::RaSettings persisted;
	Check(reopened.LoadSettings(&persisted, &error),
		"load persisted RA settings");
	Check(persisted.enabled, "RA enabled setting persisted");
	Check(persisted.last_mode == Xm8Ra::kRaModeHardcore,
		"RA hardcore setting persisted");
	Check(persisted.unofficial_enabled,
		"RA unofficial setting persisted");
	Check(persisted.notification_seconds == 8,
		"RA notification setting persisted");
	Check(persisted.image_cache_limit_mib == 256,
		"RA image cache setting persisted");
	Xm8Ra::RaSettings invalid = persisted;
	invalid.last_mode = 99;
	Check(!reopened.SaveSettings(invalid, &error),
		"reject invalid RA mode");
	reopened.Close();

	const std::string legacy_root = JoinPath(base, "legacy-ra");
	Xm8Ra::RaLibrary legacy_seed;
	Check(legacy_seed.Open(legacy_root, &error),
		"create library before v1 migration fixture");
	legacy_seed.Close();
	Check(ExecSql(JoinPath(legacy_root, "library.sqlite3"),
		"PRAGMA foreign_keys=OFF; BEGIN;"
		"CREATE TABLE media_banks_v1("
		" media_md5 TEXT NOT NULL REFERENCES media(md5) ON DELETE CASCADE,"
		" bank_index INTEGER NOT NULL, label TEXT NOT NULL,"
		" PRIMARY KEY(media_md5, bank_index), CHECK(bank_index >= 0));"
		"DROP TABLE media_banks;"
		"ALTER TABLE media_banks_v1 RENAME TO media_banks;"
		"UPDATE schema_meta SET schema_version = 1 WHERE singleton = 1;"
		"COMMIT; PRAGMA foreign_keys=ON;", &error),
		"downgrade empty fixture to schema v1");
	Xm8Ra::RaLibrary migrated;
	Check(migrated.Open(legacy_root, &error), "migrate schema v1 to v2");
	Check(QueryInt(migrated.DatabasePath(),
		"SELECT schema_version FROM schema_meta WHERE singleton = 1") == 2,
		"migration advances schema version");
	Check(QueryInt(migrated.DatabasePath(),
		"SELECT COUNT(*) FROM pragma_table_info('media_banks')"
		" WHERE name IN ('ra_hash','ra_game_id','identification_state')") == 3,
		"migration adds bank-level RA columns");
	migrated.Close();

	const std::string corrupt_root = JoinPath(base, "corrupt-ra");
	Check(MakeDirectoryTree(corrupt_root, &error), "create corrupt DB root");
	Check(WriteTextFile(JoinPath(corrupt_root, "library.sqlite3"),
		"this is not a sqlite database"),
		"write corrupt DB");
	Xm8Ra::RaLibrary recovered;
	Check(recovered.Open(corrupt_root, &error), "recover corrupt RA DB");
	Check(DirectoryHasPrefix(corrupt_root, "library.sqlite3.corrupt."),
		"corrupt DB quarantined");
	Check(recovered.LoadSettings(&settings, &error),
		"new settings exist after corrupt DB recovery");
	Check(!settings.enabled, "recovered DB settings default disabled");
	recovered.Close();

	const std::string empty_root = JoinPath(base, "empty-ra");
	Xm8Ra::RaLibrary empty_library;
	Check(empty_library.Open(empty_root, &error), "open empty library");
	Check(empty_library.ListGames(&games, &error), "list empty library");
	Check(games.empty(), "empty library has no games");
	empty_library.Close();

	RemoveTree(base);

	if (failures != 0) {
		std::cerr << failures << " test(s) failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "RA library/store tests passed\n";
	return EXIT_SUCCESS;
}
