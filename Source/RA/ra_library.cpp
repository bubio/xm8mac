#include "ra_library.h"

#include "sqlite3.h"

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <sys/stat.h>
#include <vector>

namespace Xm8Ra {
namespace {

constexpr int kSchemaVersion = 1;

std::string JoinPath(const std::string& base, const char *child)
{
	if (!base.empty() && base.back() == '/') {
		return base + child;
	}
	return base + "/" + child;
}

bool MakeDirectoryTree(const std::string& path, std::string *error)
{
	if (path.empty()) {
		return false;
	}

	std::string current;
	size_t index = 0;
	if (path[0] == '/') {
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

bool PathExists(const std::string& path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0;
}

bool RenameIfExists(const std::string& source, const std::string& destination,
	std::string *error)
{
	if (!PathExists(source)) {
		return true;
	}
	if (std::rename(source.c_str(), destination.c_str()) == 0) {
		return true;
	}
	if (error != nullptr) {
		*error = std::strerror(errno);
	}
	return false;
}

bool Prepare(sqlite3 *db, const char *sql, sqlite3_stmt **stmt,
	std::string *error)
{
	const int rc = sqlite3_prepare_v2(db, sql, -1, stmt, nullptr);
	if (rc != SQLITE_OK) {
		if (error != nullptr) {
			*error = sqlite3_errmsg(db);
		}
		return false;
	}
	return true;
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

std::string ColumnText(sqlite3_stmt *stmt, int column)
{
	const unsigned char *text = sqlite3_column_text(stmt, column);
	return text != nullptr ? reinterpret_cast<const char*>(text) : "";
}

std::string SortTitle(const std::string& title)
{
	std::string result = title;
	for (char& ch : result) {
		if (ch >= 'A' && ch <= 'Z') {
			ch = static_cast<char>(ch - 'A' + 'a');
		}
	}
	return result;
}

} // namespace

RaLibrary::RaLibrary() : db_(nullptr)
{
}

RaLibrary::~RaLibrary()
{
	Close();
}

bool RaLibrary::Open(const std::string& ra_root, std::string *error)
{
	Close();
	root_ = ra_root;

	if (!MakeDirectoryTree(root_, error) ||
		!MakeDirectoryTree(MediaRoot(), error) ||
		!MakeDirectoryTree(TempRoot(), error) ||
		!MakeDirectoryTree(JoinPath(root_, "images"), error) ||
		!MakeDirectoryTree(JoinPath(root_, "states"), error)) {
		return false;
	}

	for (int attempt = 0; attempt < 2; attempt++) {
		const int rc = sqlite3_open_v2(DatabasePath().c_str(), &db_,
			SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
			nullptr);
		if (rc != SQLITE_OK) {
			if (error != nullptr) {
				*error = db_ != nullptr ? sqlite3_errmsg(db_) : "cannot open DB";
			}
			CloseDatabaseOnly();
			return false;
		}
		sqlite3_extended_result_codes(db_, 1);

		std::string setup_error;
		if (SetupDatabase(&setup_error)) {
			if (error != nullptr) {
				error->clear();
			}
			return true;
		}

		const bool can_retry = attempt == 0 && IsDatabaseDamage();
		CloseDatabaseOnly();
		if (!can_retry) {
			if (error != nullptr) {
				*error = setup_error;
			}
			root_.clear();
			return false;
		}
		if (!QuarantineDatabase(error)) {
			root_.clear();
			return false;
		}
	}

	if (error != nullptr) {
		*error = "cannot initialize RA library database";
	}
	root_.clear();
	return false;
}

void RaLibrary::Close()
{
	CloseDatabaseOnly();
	root_.clear();
}

void RaLibrary::CloseDatabaseOnly()
{
	if (db_ != nullptr) {
		sqlite3_close(db_);
		db_ = nullptr;
	}
}

std::string RaLibrary::MediaRoot() const
{
	return JoinPath(root_, "media");
}

std::string RaLibrary::TempRoot() const
{
	return JoinPath(root_, "temp");
}

std::string RaLibrary::DatabasePath() const
{
	return JoinPath(root_, "library.sqlite3");
}

bool RaLibrary::Exec(const char *sql, std::string *error)
{
	char *message = nullptr;
	const int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &message);
	if (rc != SQLITE_OK) {
		if (error != nullptr) {
			*error = message != nullptr ? message : sqlite3_errmsg(db_);
		}
		sqlite3_free(message);
		return false;
	}
	return true;
}

bool RaLibrary::SetupDatabase(std::string *error)
{
	return Exec("PRAGMA foreign_keys = ON", error) &&
		Exec("PRAGMA journal_mode = WAL", error) &&
		Exec("PRAGMA synchronous = FULL", error) &&
		Exec("PRAGMA busy_timeout = 3000", error) &&
		CheckIntegrity(error) &&
		InitializeSchema(error) &&
		EnsureSettingsRow(error);
}

bool RaLibrary::CheckIntegrity(std::string *error)
{
	sqlite3_stmt *stmt = nullptr;
	if (!Prepare(db_, "PRAGMA integrity_check", &stmt, error)) {
		return false;
	}
	const int rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW) {
		if (error != nullptr) {
			*error = sqlite3_errmsg(db_);
		}
		sqlite3_finalize(stmt);
		return false;
	}
	const unsigned char *text = sqlite3_column_text(stmt, 0);
	const std::string result = text != nullptr ?
		reinterpret_cast<const char*>(text) : "";
	sqlite3_finalize(stmt);
	if (result == "ok") {
		return true;
	}
	if (error != nullptr) {
		*error = "RA library database integrity check failed: " + result;
	}
	return false;
}

bool RaLibrary::InitializeSchema(std::string *error)
{
	const char *schema =
		"BEGIN IMMEDIATE;"
		"CREATE TABLE IF NOT EXISTS schema_meta ("
		" singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
		" schema_version INTEGER NOT NULL,"
		" created_at INTEGER NOT NULL,"
		" migrated_at INTEGER NOT NULL"
		");"
		"CREATE TABLE IF NOT EXISTS ra_settings ("
		" singleton INTEGER PRIMARY KEY CHECK(singleton = 1),"
		" enabled INTEGER NOT NULL DEFAULT 0 CHECK(enabled IN (0, 1)),"
		" last_mode INTEGER NOT NULL DEFAULT 1 CHECK(last_mode IN (1, 2)),"
		" unofficial_enabled INTEGER NOT NULL DEFAULT 0 CHECK(unofficial_enabled IN (0, 1)),"
		" encore_enabled INTEGER NOT NULL DEFAULT 0 CHECK(encore_enabled IN (0, 1)),"
		" spectator_enabled INTEGER NOT NULL DEFAULT 0 CHECK(spectator_enabled IN (0, 1)),"
		" notification_seconds INTEGER NOT NULL DEFAULT 5 CHECK(notification_seconds IN (3, 5, 8)),"
		" image_cache_limit_mib INTEGER NOT NULL DEFAULT 128 CHECK(image_cache_limit_mib IN (64, 128, 256)),"
		" updated_at INTEGER NOT NULL"
		");"
		"CREATE TABLE IF NOT EXISTS games ("
		" id INTEGER PRIMARY KEY,"
		" ra_game_id INTEGER UNIQUE,"
		" title TEXT NOT NULL,"
		" sort_title TEXT NOT NULL,"
		" title_source INTEGER NOT NULL DEFAULT 0,"
		" badge_url TEXT,"
		" rich_presence TEXT,"
		" identification_state INTEGER NOT NULL DEFAULT 0,"
		" created_at INTEGER NOT NULL,"
		" updated_at INTEGER NOT NULL,"
		" last_played_at INTEGER,"
		" CHECK(ra_game_id IS NULL OR ra_game_id > 0),"
		" CHECK(title_source IN (0, 1, 2)),"
		" CHECK(identification_state BETWEEN 0 AND 4)"
		");"
		"CREATE TABLE IF NOT EXISTS media ("
		" md5 TEXT PRIMARY KEY,"
		" game_id INTEGER REFERENCES games(id) ON DELETE SET NULL,"
		" ra_game_id INTEGER,"
		" identification_state INTEGER NOT NULL DEFAULT 0,"
		" source_kind INTEGER NOT NULL,"
		" source_locator TEXT NOT NULL,"
		" source_display_name TEXT NOT NULL,"
		" source_size INTEGER NOT NULL,"
		" source_mtime INTEGER,"
		" bank_count INTEGER NOT NULL,"
		" working_relpath TEXT NOT NULL,"
		" ordinal INTEGER,"
		" health_state INTEGER NOT NULL DEFAULT 0,"
		" detached_at INTEGER,"
		" created_at INTEGER NOT NULL,"
		" verified_at INTEGER NOT NULL,"
		" CHECK(length(md5) = 32),"
		" CHECK(md5 NOT GLOB '*[^0-9a-f]*'),"
		" CHECK(ra_game_id IS NULL OR ra_game_id > 0),"
		" CHECK(identification_state BETWEEN 0 AND 4),"
		" CHECK(source_kind IN (0, 1)),"
		" CHECK(source_size > 0 AND source_size <= 1073741824),"
		" CHECK(bank_count > 0),"
		" CHECK(ordinal IS NULL OR ordinal >= 0),"
		" CHECK(health_state BETWEEN 0 AND 4),"
		" CHECK((game_id IS NULL AND ordinal IS NULL AND detached_at IS NOT NULL) OR"
		"       (game_id IS NOT NULL AND ordinal IS NOT NULL AND detached_at IS NULL)),"
		" UNIQUE(game_id, ordinal),"
		" UNIQUE(game_id, md5)"
		");"
		"CREATE TABLE IF NOT EXISTS media_banks ("
		" media_md5 TEXT NOT NULL REFERENCES media(md5) ON DELETE CASCADE,"
		" bank_index INTEGER NOT NULL,"
		" label TEXT NOT NULL,"
		" PRIMARY KEY(media_md5, bank_index),"
		" CHECK(bank_index >= 0)"
		");"
		"CREATE TABLE IF NOT EXISTS launch_profiles ("
		" game_id INTEGER NOT NULL REFERENCES games(id) ON DELETE CASCADE,"
		" drive INTEGER NOT NULL,"
		" media_md5 TEXT NOT NULL REFERENCES media(md5),"
		" bank_index INTEGER NOT NULL,"
		" is_ra_anchor INTEGER NOT NULL DEFAULT 0,"
		" PRIMARY KEY(game_id, drive),"
		" CHECK(drive IN (0, 1)),"
		" CHECK(is_ra_anchor IN (0, 1)),"
		" FOREIGN KEY(game_id, media_md5) REFERENCES media(game_id, md5),"
		" FOREIGN KEY(media_md5, bank_index) REFERENCES media_banks(media_md5, bank_index)"
		");"
		"CREATE UNIQUE INDEX IF NOT EXISTS one_ra_anchor_per_game "
		"ON launch_profiles(game_id) WHERE is_ra_anchor = 1;"
		"CREATE TABLE IF NOT EXISTS progress ("
		" username TEXT NOT NULL,"
		" ra_game_id INTEGER NOT NULL,"
		" core_total INTEGER NOT NULL,"
		" core_unlocked INTEGER NOT NULL,"
		" hardcore_unlocked INTEGER NOT NULL,"
		" points_total INTEGER,"
		" points_unlocked INTEGER,"
		" synced_at INTEGER NOT NULL,"
		" PRIMARY KEY(username, ra_game_id)"
		");"
		"CREATE TABLE IF NOT EXISTS image_cache ("
		" id INTEGER PRIMARY KEY,"
		" url TEXT NOT NULL UNIQUE,"
		" image_kind INTEGER NOT NULL,"
		" relative_path TEXT NOT NULL UNIQUE,"
		" content_type TEXT NOT NULL,"
		" byte_size INTEGER NOT NULL,"
		" last_used_at INTEGER NOT NULL,"
		" etag TEXT,"
		" modified TEXT"
		");"
		"CREATE TABLE IF NOT EXISTS sync_state ("
		" sync_key TEXT PRIMARY KEY,"
		" completed_at INTEGER NOT NULL,"
		" result_code INTEGER NOT NULL"
		");"
		"CREATE INDEX IF NOT EXISTS games_sort_title ON games(sort_title);"
		"CREATE INDEX IF NOT EXISTS games_last_played ON games(last_played_at DESC);"
		"CREATE INDEX IF NOT EXISTS media_game_order ON media(game_id, ordinal);";

	if (!Exec(schema, error)) {
		return false;
	}

	sqlite3_stmt *stmt = nullptr;
	if (!Prepare(db_, "SELECT schema_version FROM schema_meta WHERE singleton = 1",
		&stmt, error)) {
		Exec("ROLLBACK", nullptr);
		return false;
	}
	const int step = sqlite3_step(stmt);
	if (step == SQLITE_ROW) {
		const int version = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
		if (version != kSchemaVersion) {
			if (error != nullptr) {
				*error = "unsupported RA library schema version";
			}
			Exec("ROLLBACK", nullptr);
			return false;
		}
	}
	else if (step == SQLITE_DONE) {
		sqlite3_finalize(stmt);
		stmt = nullptr;
		if (!Prepare(db_,
			"INSERT INTO schema_meta(singleton, schema_version, created_at, migrated_at)"
			" VALUES(1, ?, ?, ?)",
			&stmt, error)) {
			Exec("ROLLBACK", nullptr);
			return false;
		}
		const int64_t now = NowUnixTime();
		sqlite3_bind_int(stmt, 1, kSchemaVersion);
		sqlite3_bind_int64(stmt, 2, now);
		sqlite3_bind_int64(stmt, 3, now);
		if (!StepDone(db_, stmt, error)) {
			Exec("ROLLBACK", nullptr);
			return false;
		}
	}
	else {
		if (error != nullptr) {
			*error = sqlite3_errmsg(db_);
		}
		sqlite3_finalize(stmt);
		Exec("ROLLBACK", nullptr);
		return false;
	}

	return Exec("COMMIT", error);
}

bool RaLibrary::EnsureSettingsRow(std::string *error)
{
	sqlite3_stmt *stmt = nullptr;
	if (!Prepare(db_,
		"INSERT OR IGNORE INTO ra_settings(singleton, updated_at) VALUES(1, ?)",
		&stmt, error)) {
		return false;
	}
	sqlite3_bind_int64(stmt, 1, NowUnixTime());
	return StepDone(db_, stmt, error);
}

bool RaLibrary::IsDatabaseDamage() const
{
	if (db_ == nullptr) {
		return false;
	}
	const int code = sqlite3_extended_errcode(db_);
	return code == SQLITE_CORRUPT || code == SQLITE_NOTADB ||
		(code & 0xff) == SQLITE_CORRUPT ||
		(code & 0xff) == SQLITE_NOTADB;
}

bool RaLibrary::QuarantineDatabase(std::string *error)
{
	const std::string base = DatabasePath();
	const std::string suffix = ".corrupt." + std::to_string(NowUnixTime());
	if (!RenameIfExists(base, base + suffix, error) ||
		!RenameIfExists(base + "-wal", base + "-wal" + suffix, error) ||
		!RenameIfExists(base + "-shm", base + "-shm" + suffix, error)) {
		return false;
	}
	return true;
}

int64_t RaLibrary::NowUnixTime() const
{
	return std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
}

bool RaLibrary::ValidateSettings(const RaSettings& settings,
	std::string *error) const
{
	if (settings.last_mode != kRaModeSoftcore &&
		settings.last_mode != kRaModeHardcore) {
		if (error != nullptr) {
			*error = "invalid RA mode";
		}
		return false;
	}
	if (settings.notification_seconds != 3 &&
		settings.notification_seconds != 5 &&
		settings.notification_seconds != 8) {
		if (error != nullptr) {
			*error = "invalid RA notification duration";
		}
		return false;
	}
	if (settings.image_cache_limit_mib != 64 &&
		settings.image_cache_limit_mib != 128 &&
		settings.image_cache_limit_mib != 256) {
		if (error != nullptr) {
			*error = "invalid RA image cache limit";
		}
		return false;
	}
	return true;
}

bool RaLibrary::LoadSettings(RaSettings *settings, std::string *error)
{
	if (settings == nullptr) {
		if (error != nullptr) {
			*error = "invalid argument";
		}
		return false;
	}

	sqlite3_stmt *stmt = nullptr;
	if (!Prepare(db_,
		"SELECT enabled, last_mode, unofficial_enabled, encore_enabled,"
		" spectator_enabled, notification_seconds, image_cache_limit_mib"
		" FROM ra_settings WHERE singleton = 1",
		&stmt, error)) {
		return false;
	}
	const int rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW) {
		if (error != nullptr) {
			*error = rc == SQLITE_DONE ? "RA settings row is missing" :
				sqlite3_errmsg(db_);
		}
		sqlite3_finalize(stmt);
		return false;
	}

	RaSettings loaded;
	loaded.enabled = sqlite3_column_int(stmt, 0) != 0;
	loaded.last_mode = sqlite3_column_int(stmt, 1);
	loaded.unofficial_enabled = sqlite3_column_int(stmt, 2) != 0;
	loaded.encore_enabled = sqlite3_column_int(stmt, 3) != 0;
	loaded.spectator_enabled = sqlite3_column_int(stmt, 4) != 0;
	loaded.notification_seconds = sqlite3_column_int(stmt, 5);
	loaded.image_cache_limit_mib = sqlite3_column_int(stmt, 6);
	sqlite3_finalize(stmt);

	if (!ValidateSettings(loaded, error)) {
		return false;
	}
	*settings = loaded;
	return true;
}

bool RaLibrary::SaveSettings(const RaSettings& settings, std::string *error)
{
	if (!ValidateSettings(settings, error)) {
		return false;
	}

	sqlite3_stmt *stmt = nullptr;
	if (!Prepare(db_,
		"UPDATE ra_settings SET enabled = ?, last_mode = ?,"
		" unofficial_enabled = ?, encore_enabled = ?, spectator_enabled = ?,"
		" notification_seconds = ?, image_cache_limit_mib = ?, updated_at = ?"
		" WHERE singleton = 1",
		&stmt, error)) {
		return false;
	}
	sqlite3_bind_int(stmt, 1, settings.enabled ? 1 : 0);
	sqlite3_bind_int(stmt, 2, settings.last_mode);
	sqlite3_bind_int(stmt, 3, settings.unofficial_enabled ? 1 : 0);
	sqlite3_bind_int(stmt, 4, settings.encore_enabled ? 1 : 0);
	sqlite3_bind_int(stmt, 5, settings.spectator_enabled ? 1 : 0);
	sqlite3_bind_int(stmt, 6, settings.notification_seconds);
	sqlite3_bind_int(stmt, 7, settings.image_cache_limit_mib);
	sqlite3_bind_int64(stmt, 8, NowUnixTime());
	return StepDone(db_, stmt, error);
}

bool RaLibrary::FindMedia(const std::string& md5, MediaRecord *record,
	std::string *error)
{
	sqlite3_stmt *stmt = nullptr;
	if (!Prepare(db_,
		"SELECT md5, game_id, working_relpath FROM media WHERE md5 = ?",
		&stmt, error)) {
		return false;
	}
	sqlite3_bind_text(stmt, 1, md5.c_str(), -1, SQLITE_TRANSIENT);
	const int rc = sqlite3_step(stmt);
	if (rc == SQLITE_DONE) {
		sqlite3_finalize(stmt);
		return false;
	}
	if (rc != SQLITE_ROW) {
		if (error != nullptr) {
			*error = sqlite3_errmsg(db_);
		}
		sqlite3_finalize(stmt);
		return false;
	}
	if (record != nullptr) {
		record->md5 = ColumnText(stmt, 0);
		record->game_id = sqlite3_column_int64(stmt, 1);
		record->working_relpath = ColumnText(stmt, 2);
		record->inserted = false;
	}
	sqlite3_finalize(stmt);
	return true;
}

bool RaLibrary::LoadMediaHealthRecord(const std::string& md5,
	MediaHealthRecord *record, std::string *error)
{
	if (record == nullptr) {
		if (error != nullptr) {
			*error = "invalid argument";
		}
		return false;
	}

	sqlite3_stmt *stmt = nullptr;
	if (!Prepare(db_,
		"SELECT md5, game_id, source_locator, source_size, source_mtime,"
		" working_relpath, health_state FROM media WHERE md5 = ?",
		&stmt, error)) {
		return false;
	}
	sqlite3_bind_text(stmt, 1, md5.c_str(), -1, SQLITE_TRANSIENT);
	const int rc = sqlite3_step(stmt);
	if (rc == SQLITE_DONE) {
		sqlite3_finalize(stmt);
		if (error != nullptr) {
			*error = "media is not registered";
		}
		return false;
	}
	if (rc != SQLITE_ROW) {
		if (error != nullptr) {
			*error = sqlite3_errmsg(db_);
		}
		sqlite3_finalize(stmt);
		return false;
	}

	record->md5 = ColumnText(stmt, 0);
	record->game_id = sqlite3_column_int64(stmt, 1);
	record->source_locator = ColumnText(stmt, 2);
	record->source_size = sqlite3_column_int64(stmt, 3);
	record->source_mtime = sqlite3_column_type(stmt, 4) == SQLITE_NULL ?
		-1 : sqlite3_column_int64(stmt, 4);
	record->working_relpath = ColumnText(stmt, 5);
	record->health_state = sqlite3_column_int(stmt, 6);
	sqlite3_finalize(stmt);
	return true;
}

bool RaLibrary::UpdateMediaHealth(const MediaHealthStatus& status,
	std::string *error)
{
	if (status.md5.size() != 32 ||
		status.health_state < kRaMediaHealthOk ||
		status.health_state > kRaMediaHealthWorkingCorrupt) {
		if (error != nullptr) {
			*error = "invalid media health status";
		}
		return false;
	}

	sqlite3_stmt *stmt = nullptr;
	if (!Prepare(db_,
		"UPDATE media SET health_state = ?,"
		" source_size = CASE WHEN ? THEN ? ELSE source_size END,"
		" source_mtime = CASE WHEN ? THEN ? ELSE source_mtime END,"
		" verified_at = ? WHERE md5 = ?",
		&stmt, error)) {
		return false;
	}
	sqlite3_bind_int(stmt, 1, status.health_state);
	const bool update_source_metadata =
		status.source_exists && !status.source_hash_changed &&
		status.source_size > 0;
	sqlite3_bind_int(stmt, 2, update_source_metadata ? 1 : 0);
	sqlite3_bind_int64(stmt, 3,
		update_source_metadata ?
			static_cast<sqlite3_int64>(status.source_size) : 0);
	sqlite3_bind_int(stmt, 4, update_source_metadata ? 1 : 0);
	if (update_source_metadata && status.source_mtime >= 0) {
		sqlite3_bind_int64(stmt, 5, status.source_mtime);
	}
	else {
		sqlite3_bind_null(stmt, 5);
	}
	sqlite3_bind_int64(stmt, 6, NowUnixTime());
	sqlite3_bind_text(stmt, 7, status.md5.c_str(), -1, SQLITE_TRANSIENT);
	if (!StepDone(db_, stmt, error)) {
		return false;
	}
	if (sqlite3_changes(db_) != 1) {
		if (error != nullptr) {
			*error = "media is not registered";
		}
		return false;
	}
	return true;
}

bool RaLibrary::LoadLaunchProfile(int64_t game_id, LaunchProfile *profile,
	std::string *error)
{
	if (game_id <= 0 || profile == nullptr) {
		if (error != nullptr) {
			*error = "invalid argument";
		}
		return false;
	}

	LaunchProfile loaded;
	loaded.game_id = game_id;
	sqlite3_stmt *stmt = nullptr;
	if (!Prepare(db_,
		"SELECT drive, media_md5, bank_index, is_ra_anchor"
		" FROM launch_profiles WHERE game_id = ? ORDER BY drive",
		&stmt, error)) {
		return false;
	}
	sqlite3_bind_int64(stmt, 1, game_id);
	while (true) {
		const int rc = sqlite3_step(stmt);
		if (rc == SQLITE_DONE) {
			break;
		}
		if (rc != SQLITE_ROW) {
			if (error != nullptr) {
				*error = sqlite3_errmsg(db_);
			}
			sqlite3_finalize(stmt);
			return false;
		}
		const int drive = sqlite3_column_int(stmt, 0);
		if (drive < 0 || drive > 1) {
			sqlite3_finalize(stmt);
			if (error != nullptr) {
				*error = "invalid launch profile drive";
			}
			return false;
		}
		LaunchDrive& slot = loaded.drives[drive];
		slot.assigned = true;
		slot.media_md5 = ColumnText(stmt, 1);
		slot.bank_index = sqlite3_column_int(stmt, 2);
		slot.is_ra_anchor = sqlite3_column_int(stmt, 3) != 0;
	}
	sqlite3_finalize(stmt);
	*profile = loaded;
	return true;
}

bool RaLibrary::SaveLaunchProfile(const LaunchProfile& profile,
	std::string *error)
{
	if (profile.game_id <= 0) {
		if (error != nullptr) {
			*error = "invalid launch profile game";
		}
		return false;
	}

	int assigned_count = 0;
	int anchor_count = 0;
	for (int drive = 0; drive < 2; drive++) {
		const LaunchDrive& slot = profile.drives[drive];
		if (!slot.assigned) {
			continue;
		}
		assigned_count++;
		if (slot.media_md5.size() != 32 || slot.bank_index < 0) {
			if (error != nullptr) {
				*error = "invalid launch profile media";
			}
			return false;
		}
		if (slot.is_ra_anchor) {
			anchor_count++;
		}
	}
	if (assigned_count == 0 || anchor_count != 1) {
		if (error != nullptr) {
			*error = "launch profile must contain exactly one RA anchor";
		}
		return false;
	}

	if (!Exec("BEGIN IMMEDIATE", error)) {
		return false;
	}

	sqlite3_stmt *stmt = nullptr;
	if (!Prepare(db_, "DELETE FROM launch_profiles WHERE game_id = ?",
		&stmt, error)) {
		Exec("ROLLBACK", nullptr);
		return false;
	}
	sqlite3_bind_int64(stmt, 1, profile.game_id);
	if (!StepDone(db_, stmt, error)) {
		Exec("ROLLBACK", nullptr);
		return false;
	}

	for (int drive = 0; drive < 2; drive++) {
		const LaunchDrive& slot = profile.drives[drive];
		if (!slot.assigned) {
			continue;
		}
		if (!Prepare(db_,
			"INSERT INTO launch_profiles(game_id, drive, media_md5,"
			" bank_index, is_ra_anchor) VALUES(?, ?, ?, ?, ?)",
			&stmt, error)) {
			Exec("ROLLBACK", nullptr);
			return false;
		}
		sqlite3_bind_int64(stmt, 1, profile.game_id);
		sqlite3_bind_int(stmt, 2, drive);
		sqlite3_bind_text(stmt, 3, slot.media_md5.c_str(), -1,
			SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 4, slot.bank_index);
		sqlite3_bind_int(stmt, 5, slot.is_ra_anchor ? 1 : 0);
		if (!StepDone(db_, stmt, error)) {
			Exec("ROLLBACK", nullptr);
			return false;
		}
	}

	if (!Prepare(db_,
		"SELECT COUNT(*) FROM launch_profiles"
		" WHERE game_id = ? AND is_ra_anchor = 1",
		&stmt, error)) {
		Exec("ROLLBACK", nullptr);
		return false;
	}
	sqlite3_bind_int64(stmt, 1, profile.game_id);
	const int rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW || sqlite3_column_int(stmt, 0) != 1) {
		if (error != nullptr) {
			*error = rc == SQLITE_ROW ? "launch profile anchor invariant failed" :
				sqlite3_errmsg(db_);
		}
		sqlite3_finalize(stmt);
		Exec("ROLLBACK", nullptr);
		return false;
	}
	sqlite3_finalize(stmt);

	return Exec("COMMIT", error);
}

bool RaLibrary::MergeGameMedia(int64_t target_game_id,
	int64_t source_game_id, std::string *error)
{
	if (target_game_id <= 0 || source_game_id <= 0 ||
		target_game_id == source_game_id) {
		if (error != nullptr) {
			*error = "invalid game merge target";
		}
		return false;
	}
	if (!Exec("BEGIN IMMEDIATE", error)) {
		return false;
	}

	sqlite3_stmt *stmt = nullptr;
	if (!Prepare(db_, "SELECT 1 FROM games WHERE id = ?",
		&stmt, error)) {
		Exec("ROLLBACK", nullptr);
		return false;
	}
	sqlite3_bind_int64(stmt, 1, target_game_id);
	const int target_rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if (target_rc != SQLITE_ROW) {
		if (error != nullptr) {
			*error = "target game does not exist";
		}
		Exec("ROLLBACK", nullptr);
		return false;
	}

	if (!Prepare(db_, "SELECT 1 FROM games WHERE id = ?",
		&stmt, error)) {
		Exec("ROLLBACK", nullptr);
		return false;
	}
	sqlite3_bind_int64(stmt, 1, source_game_id);
	const int source_rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if (source_rc != SQLITE_ROW) {
		if (error != nullptr) {
			*error = "source game does not exist";
		}
		Exec("ROLLBACK", nullptr);
		return false;
	}

	int max_ordinal = -1;
	if (!Prepare(db_,
		"SELECT COALESCE(MAX(ordinal), -1) FROM media WHERE game_id = ?",
		&stmt, error)) {
		Exec("ROLLBACK", nullptr);
		return false;
	}
	sqlite3_bind_int64(stmt, 1, target_game_id);
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		max_ordinal = sqlite3_column_int(stmt, 0);
	}
	else {
		if (error != nullptr) {
			*error = sqlite3_errmsg(db_);
		}
		sqlite3_finalize(stmt);
		Exec("ROLLBACK", nullptr);
		return false;
	}
	sqlite3_finalize(stmt);

	std::vector<std::string> media_md5s;
	if (!Prepare(db_,
		"SELECT md5 FROM media WHERE game_id = ? ORDER BY ordinal, md5",
		&stmt, error)) {
		Exec("ROLLBACK", nullptr);
		return false;
	}
	sqlite3_bind_int64(stmt, 1, source_game_id);
	while (true) {
		const int rc = sqlite3_step(stmt);
		if (rc == SQLITE_DONE) {
			break;
		}
		if (rc != SQLITE_ROW) {
			if (error != nullptr) {
				*error = sqlite3_errmsg(db_);
			}
			sqlite3_finalize(stmt);
			Exec("ROLLBACK", nullptr);
			return false;
		}
		media_md5s.push_back(
			reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
	}
	sqlite3_finalize(stmt);
	if (media_md5s.empty()) {
		if (error != nullptr) {
			*error = "source game has no media";
		}
		Exec("ROLLBACK", nullptr);
		return false;
	}

	if (!Prepare(db_, "DELETE FROM launch_profiles WHERE game_id = ?",
		&stmt, error)) {
		Exec("ROLLBACK", nullptr);
		return false;
	}
	sqlite3_bind_int64(stmt, 1, source_game_id);
	if (!StepDone(db_, stmt, error)) {
		Exec("ROLLBACK", nullptr);
		return false;
	}

	for (size_t i = 0; i < media_md5s.size(); i++) {
		if (!Prepare(db_,
			"UPDATE media SET game_id = ?, ordinal = ?, detached_at = NULL"
			" WHERE md5 = ? AND game_id = ?",
			&stmt, error)) {
			Exec("ROLLBACK", nullptr);
			return false;
		}
		sqlite3_bind_int64(stmt, 1, target_game_id);
		sqlite3_bind_int(stmt, 2,
			max_ordinal + 1 + static_cast<int>(i));
		sqlite3_bind_text(stmt, 3, media_md5s[i].c_str(), -1,
			SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 4, source_game_id);
		if (!StepDone(db_, stmt, error)) {
			Exec("ROLLBACK", nullptr);
			return false;
		}
	}

	if (!Prepare(db_, "DELETE FROM games WHERE id = ?",
		&stmt, error)) {
		Exec("ROLLBACK", nullptr);
		return false;
	}
	sqlite3_bind_int64(stmt, 1, source_game_id);
	if (!StepDone(db_, stmt, error)) {
		Exec("ROLLBACK", nullptr);
		return false;
	}

	return Exec("COMMIT", error);
}

bool RaLibrary::ListGames(std::vector<RaLibraryGameListItem> *games,
	std::string *error)
{
	if (games == nullptr) {
		if (error != nullptr) {
			*error = "invalid argument";
		}
		return false;
	}
	games->clear();

	sqlite3_stmt *stmt = nullptr;
	if (!Prepare(db_,
		"SELECT g.id, g.ra_game_id, g.title, g.badge_url,"
		" g.identification_state, COUNT(DISTINCT m.md5),"
		" COALESCE(MAX(m.health_state), 0), g.last_played_at,"
		" MAX(p.core_total), MAX(p.core_unlocked),"
		" MAX(p.hardcore_unlocked), MAX(p.points_total),"
		" MAX(p.points_unlocked)"
		" FROM games g"
		" LEFT JOIN media m ON m.game_id = g.id"
		" LEFT JOIN progress p ON p.ra_game_id = g.ra_game_id"
		" WHERE g.ra_game_id IS NOT NULL AND g.identification_state = 1"
		" GROUP BY g.id"
		" ORDER BY g.last_played_at IS NULL, g.last_played_at DESC,"
		" g.sort_title ASC, g.id ASC",
		&stmt, error)) {
		return false;
	}

	while (true) {
		const int rc = sqlite3_step(stmt);
		if (rc == SQLITE_DONE) {
			break;
		}
		if (rc != SQLITE_ROW) {
			if (error != nullptr) {
				*error = sqlite3_errmsg(db_);
			}
			sqlite3_finalize(stmt);
			return false;
		}

		RaLibraryGameListItem item;
		item.game_id = sqlite3_column_int64(stmt, 0);
		item.ra_game_id = sqlite3_column_type(stmt, 1) == SQLITE_NULL ?
			0 : sqlite3_column_int64(stmt, 1);
		item.title = ColumnText(stmt, 2);
		item.badge_url = ColumnText(stmt, 3);
		item.identification_state = sqlite3_column_int(stmt, 4);
		item.media_count = sqlite3_column_int(stmt, 5);
		item.health_state = sqlite3_column_int(stmt, 6);
		item.last_played_at = sqlite3_column_type(stmt, 7) == SQLITE_NULL ?
			0 : sqlite3_column_int64(stmt, 7);
		item.has_progress = sqlite3_column_type(stmt, 8) != SQLITE_NULL;
		if (item.has_progress) {
			item.core_total = sqlite3_column_int(stmt, 8);
			item.core_unlocked = sqlite3_column_int(stmt, 9);
			item.hardcore_unlocked = sqlite3_column_int(stmt, 10);
			item.points_total = sqlite3_column_type(stmt, 11) == SQLITE_NULL ?
				0 : sqlite3_column_int(stmt, 11);
			item.points_unlocked = sqlite3_column_type(stmt, 12) == SQLITE_NULL ?
				0 : sqlite3_column_int(stmt, 12);
		}
		games->push_back(item);
	}
	sqlite3_finalize(stmt);
	return true;
}

bool RaLibrary::MarkGameIdentified(int64_t game_id, int64_t ra_game_id,
	const std::string& title, const std::string& badge_url,
	std::string *error)
{
	if (game_id <= 0 || ra_game_id <= 0) {
		if (error != nullptr) {
			*error = "invalid RA game";
		}
		return false;
	}

	const std::string saved_title =
		title.empty() ? "RA Game " + std::to_string(ra_game_id) : title;
	const std::string sort_title = SortTitle(saved_title);

	if (!Exec("BEGIN IMMEDIATE", error)) {
		return false;
	}

	sqlite3_stmt *stmt = nullptr;
	if (!Prepare(db_,
		"UPDATE games SET ra_game_id = ?,"
		" title = CASE WHEN title_source = 2 THEN title ELSE ? END,"
		" sort_title = CASE WHEN title_source = 2 THEN sort_title ELSE ? END,"
		" title_source = CASE WHEN title_source = 2 THEN title_source ELSE 1 END,"
		" badge_url = ?, identification_state = 1, updated_at = ?"
		" WHERE id = ?",
		&stmt, error)) {
		Exec("ROLLBACK", nullptr);
		return false;
	}
	const int64_t now = NowUnixTime();
	sqlite3_bind_int64(stmt, 1, ra_game_id);
	sqlite3_bind_text(stmt, 2, saved_title.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, sort_title.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, badge_url.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 5, now);
	sqlite3_bind_int64(stmt, 6, game_id);
	if (!StepDone(db_, stmt, error)) {
		Exec("ROLLBACK", nullptr);
		return false;
	}
	if (sqlite3_changes(db_) != 1) {
		Exec("ROLLBACK", nullptr);
		if (error != nullptr) {
			*error = "game is not registered";
		}
		return false;
	}

	if (!Prepare(db_,
		"UPDATE media SET ra_game_id = ?, identification_state = 1,"
		" verified_at = ? WHERE game_id = ?",
		&stmt, error)) {
		Exec("ROLLBACK", nullptr);
		return false;
	}
	sqlite3_bind_int64(stmt, 1, ra_game_id);
	sqlite3_bind_int64(stmt, 2, now);
	sqlite3_bind_int64(stmt, 3, game_id);
	if (!StepDone(db_, stmt, error)) {
		Exec("ROLLBACK", nullptr);
		return false;
	}

	return Exec("COMMIT", error);
}

bool RaLibrary::MarkGamePlayed(int64_t game_id, std::string *error)
{
	if (game_id <= 0) {
		if (error != nullptr) {
			*error = "invalid game";
		}
		return false;
	}

	sqlite3_stmt *stmt = nullptr;
	if (!Prepare(db_,
		"UPDATE games SET last_played_at = ?, updated_at = ? WHERE id = ?",
		&stmt, error)) {
		return false;
	}
	const int64_t now = NowUnixTime();
	sqlite3_bind_int64(stmt, 1, now);
	sqlite3_bind_int64(stmt, 2, now);
	sqlite3_bind_int64(stmt, 3, game_id);
	if (!StepDone(db_, stmt, error)) {
		return false;
	}
	if (sqlite3_changes(db_) != 1) {
		if (error != nullptr) {
			*error = "game is not registered";
		}
		return false;
	}
	return true;
}

bool RaLibrary::RegisterDesktopMedia(const D88MediaInfo& media,
	const std::string& source_path, const std::string& display_name,
	int64_t source_mtime, MediaRecord *record, std::string *error)
{
	return RegisterDesktopMediaInternal(media, source_path, display_name,
		source_mtime, 0, 0, true, record, error);
}

bool RaLibrary::RegisterDesktopMediaInGame(const D88MediaInfo& media,
	const std::string& source_path, const std::string& display_name,
	int64_t source_mtime, int64_t game_id, int ordinal,
	MediaRecord *record, std::string *error)
{
	if (game_id <= 0 || ordinal <= 0) {
		if (error != nullptr) {
			*error = "invalid grouped media target";
		}
		return false;
	}
	return RegisterDesktopMediaInternal(media, source_path, display_name,
		source_mtime, game_id, ordinal, false, record, error);
}

bool RaLibrary::RegisterDesktopMediaInternal(const D88MediaInfo& media,
	const std::string& source_path, const std::string& display_name,
	int64_t source_mtime, int64_t game_id, int ordinal,
	bool create_anchor_profile, MediaRecord *record, std::string *error)
{
	if (media.md5.size() != 32 || media.banks <= 0) {
		if (error != nullptr) {
			*error = "invalid media metadata";
		}
		return false;
	}

	MediaRecord existing;
	if (FindMedia(media.md5, &existing, nullptr)) {
		if (game_id > 0 && existing.game_id != game_id) {
			if (error != nullptr) {
				*error = "media is already registered to another game";
			}
			return false;
		}
		if (record != nullptr) {
			*record = existing;
		}
		return true;
	}

	if (!Exec("BEGIN IMMEDIATE", error)) {
		return false;
	}

	const int64_t now = NowUnixTime();
	if (game_id == 0) {
		std::string title =
			!media.bank_names.empty() && !media.bank_names[0].empty() ?
			media.bank_names[0] : display_name;
		if (title.empty()) {
			title = media.md5;
		}

		sqlite3_stmt *stmt = nullptr;
		if (!Prepare(db_,
			"INSERT INTO games(title, sort_title, title_source, identification_state,"
			" created_at, updated_at) VALUES(?, ?, 0, 0, ?, ?)",
			&stmt, error)) {
			Exec("ROLLBACK", nullptr);
			return false;
		}
		sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
		const std::string sort_title = SortTitle(title);
		sqlite3_bind_text(stmt, 2, sort_title.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 3, now);
		sqlite3_bind_int64(stmt, 4, now);
		if (!StepDone(db_, stmt, error)) {
			Exec("ROLLBACK", nullptr);
			return false;
		}
		game_id = sqlite3_last_insert_rowid(db_);
	}

	const std::string working_relpath =
		std::string("media/") + media.md5 + "/working.d88";
	sqlite3_stmt *stmt = nullptr;
	if (!Prepare(db_,
		"INSERT INTO media(md5, game_id, identification_state, source_kind,"
		" source_locator, source_display_name, source_size, source_mtime,"
		" bank_count, working_relpath, ordinal, health_state, created_at, verified_at)"
		" VALUES(?, ?, 0, 0, ?, ?, ?, ?, ?, ?, ?, 0, ?, ?)",
		&stmt, error)) {
		Exec("ROLLBACK", nullptr);
		return false;
	}
	sqlite3_bind_text(stmt, 1, media.md5.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 2, game_id);
	sqlite3_bind_text(stmt, 3, source_path.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, display_name.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(media.size));
	if (source_mtime >= 0) {
		sqlite3_bind_int64(stmt, 6, source_mtime);
	}
	else {
		sqlite3_bind_null(stmt, 6);
	}
	sqlite3_bind_int(stmt, 7, media.banks);
	sqlite3_bind_text(stmt, 8, working_relpath.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 9, ordinal);
	sqlite3_bind_int64(stmt, 10, now);
	sqlite3_bind_int64(stmt, 11, now);
	if (!StepDone(db_, stmt, error)) {
		Exec("ROLLBACK", nullptr);
		return false;
	}

	for (int i = 0; i < media.banks; i++) {
		if (!Prepare(db_,
			"INSERT INTO media_banks(media_md5, bank_index, label)"
			" VALUES(?, ?, ?)",
			&stmt, error)) {
			Exec("ROLLBACK", nullptr);
			return false;
		}
		const std::string label =
			i < static_cast<int>(media.bank_names.size()) ? media.bank_names[i] : "";
		sqlite3_bind_text(stmt, 1, media.md5.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 2, i);
		sqlite3_bind_text(stmt, 3, label.c_str(), -1, SQLITE_TRANSIENT);
		if (!StepDone(db_, stmt, error)) {
			Exec("ROLLBACK", nullptr);
			return false;
		}
	}

	if (create_anchor_profile) {
		if (!Prepare(db_,
			"INSERT INTO launch_profiles(game_id, drive, media_md5, bank_index, is_ra_anchor)"
			" VALUES(?, 0, ?, 0, 1)",
			&stmt, error)) {
			Exec("ROLLBACK", nullptr);
			return false;
		}
		sqlite3_bind_int64(stmt, 1, game_id);
		sqlite3_bind_text(stmt, 2, media.md5.c_str(), -1, SQLITE_TRANSIENT);
		if (!StepDone(db_, stmt, error)) {
			Exec("ROLLBACK", nullptr);
			return false;
		}
	}

	if (!Exec("COMMIT", error)) {
		return false;
	}

	if (record != nullptr) {
		record->md5 = media.md5;
		record->game_id = game_id;
		record->working_relpath = working_relpath;
		record->inserted = true;
	}
	return true;
}

} // namespace Xm8Ra
