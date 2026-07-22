#include "ra_library_fixture_seed.h"

#include "Fixtures/d88_fixture.h"
#include "ra_file_util.h"
#include "ra_library.h"
#include "ra_media_store.h"
#include "sqlite3.h"


namespace RaLibraryFixtureSeed {
namespace {

constexpr int64_t kMultiDiskRaGameId = 8801001;
constexpr int64_t kSingleD88RaGameId = 8801002;

std::string JoinPath(const std::string& base, const char *child)
{
	if (!base.empty() && (base.back() == '/' || base.back() == '\\')) {
		return base + child;
	}
	return base + "/" + child;
}

bool MakeDirectoryTree(const std::string& path, std::string *error)
{
	return Xm8Ra::EnsureRaDirectoryTree(path, error);
}

bool StepDone(sqlite3 *db, sqlite3_stmt *stmt, std::string *error)
{
	const int rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		if (error != nullptr) {
			*error = sqlite3_errmsg(db);
		}
		sqlite3_finalize(stmt);
		return false;
	}
	sqlite3_finalize(stmt);
	return true;
}

bool SeedProgress(const std::string& database_path, int64_t ra_game_id,
	int core_total, int core_unlocked, int hardcore_unlocked,
	int points_total, int points_unlocked, std::string *error)
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

	sqlite3_stmt *stmt = nullptr;
	const char *sql =
		"INSERT OR REPLACE INTO progress(username, ra_game_id, core_total,"
		" core_unlocked, hardcore_unlocked, points_total, points_unlocked,"
		" synced_at) VALUES('fixture', ?, ?, ?, ?, ?, ?, 1710000000)";
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		if (error != nullptr) {
			*error = sqlite3_errmsg(db);
		}
		sqlite3_close(db);
		return false;
	}
	sqlite3_bind_int64(stmt, 1, ra_game_id);
	sqlite3_bind_int(stmt, 2, core_total);
	sqlite3_bind_int(stmt, 3, core_unlocked);
	sqlite3_bind_int(stmt, 4, hardcore_unlocked);
	sqlite3_bind_int(stmt, 5, points_total);
	sqlite3_bind_int(stmt, 6, points_unlocked);
	const bool ok = StepDone(db, stmt, error);
	sqlite3_close(db);
	return ok;
}

SeededMedia MakeMedia(const std::string& source_path,
	const Xm8Ra::ImportedMedia& imported)
{
	SeededMedia media;
	media.source_path = source_path;
	media.working_path = imported.working_path;
	media.md5 = imported.record.md5;
	return media;
}

} // namespace

bool Seed(const std::string& ra_root, SeedResult *result, std::string *error)
{
	if (ra_root.empty()) {
		if (error != nullptr) {
			*error = "RA root is empty";
		}
		return false;
	}
	if (result == nullptr) {
		if (error != nullptr) {
			*error = "invalid seed result";
		}
		return false;
	}

	SeedResult seeded;
	seeded.ra_root = ra_root;
	seeded.source_dir = JoinPath(ra_root, "dev-fixtures/source");

	if (!MakeDirectoryTree(seeded.source_dir, error)) {
		return false;
	}
	if (!D88Fixture::GenerateStandardSet(seeded.source_dir, error)) {
		return false;
	}

	Xm8Ra::RaLibrary library;
	if (!library.Open(ra_root, error)) {
		return false;
	}
	Xm8Ra::RaMediaStore store(&library);

	const std::string pair_path = JoinPath(seeded.source_dir, "pair.m3u");
	const std::string single_path = JoinPath(seeded.source_dir, "single.d88");
	const std::string second_path = JoinPath(seeded.source_dir, "second.d88");
	const std::string multi_path = JoinPath(seeded.source_dir, "multi.d88");

	Xm8Ra::ImportedPlaylist playlist;
	if (!store.ImportM3U(pair_path, &playlist, error)) {
		return false;
	}
	if (playlist.media.size() < 2) {
		if (error != nullptr) {
			*error = "seed playlist did not import two media entries";
		}
		return false;
	}
	if (!library.MarkGameIdentified(playlist.game_id, kMultiDiskRaGameId,
		"RA Test Multi Disk",
		"https://media.retroachievements.org/Images/088001.png", error)) {
		return false;
	}
	Xm8Ra::LaunchProfile profile;
	if (!library.LoadLaunchProfile(playlist.game_id, &profile, error)) {
		return false;
	}
	profile.drives[1].assigned = true;
	profile.drives[1].media_md5 = playlist.media[1].record.md5;
	profile.drives[1].bank_index = 0;
	profile.drives[1].is_ra_anchor = false;
	if (!library.SaveLaunchProfile(profile, error)) {
		return false;
	}
	if (!SeedProgress(library.DatabasePath(), kMultiDiskRaGameId,
		12, 5, 2, 120, 50, error)) {
		return false;
	}

	SeededGame multi_disk;
	multi_disk.game_id = playlist.game_id;
	multi_disk.ra_game_id = kMultiDiskRaGameId;
	multi_disk.title = "RA Test Multi Disk";
	multi_disk.media.push_back(MakeMedia(single_path, playlist.media[0]));
	multi_disk.media.push_back(MakeMedia(second_path, playlist.media[1]));
	seeded.games.push_back(multi_disk);

	Xm8Ra::ImportedMedia single;
	if (!store.ImportDesktopD88(multi_path, &single, error)) {
		return false;
	}
	if (!library.MarkGameIdentified(single.record.game_id, kSingleD88RaGameId,
		"RA Test Game",
		"https://media.retroachievements.org/Images/088002.png", error)) {
		return false;
	}
	if (!SeedProgress(library.DatabasePath(), kSingleD88RaGameId,
		8, 1, 1, 80, 10, error)) {
		return false;
	}

	SeededGame single_game;
	single_game.game_id = single.record.game_id;
	single_game.ra_game_id = kSingleD88RaGameId;
	single_game.title = "RA Test Game";
	single_game.media.push_back(MakeMedia(multi_path, single));
	seeded.games.push_back(single_game);

	*result = seeded;
	return true;
}

} // namespace RaLibraryFixtureSeed
