#include "ra_library.h"

#include "sqlite3.h"

#include <chrono>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <sys/stat.h>

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

	const int rc = sqlite3_open_v2(DatabasePath().c_str(), &db_,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
		nullptr);
	if (rc != SQLITE_OK) {
		if (error != nullptr) {
			*error = db_ != nullptr ? sqlite3_errmsg(db_) : "cannot open DB";
		}
		Close();
		return false;
	}

	if (!Exec("PRAGMA foreign_keys = ON", error) ||
		!Exec("PRAGMA journal_mode = WAL", error) ||
		!Exec("PRAGMA synchronous = FULL", error) ||
		!Exec("PRAGMA busy_timeout = 3000", error) ||
		!InitializeSchema(error) ||
		!EnsureSettingsRow(error)) {
		Close();
		return false;
	}
	return true;
}

void RaLibrary::Close()
{
	if (db_ != nullptr) {
		sqlite3_close(db_);
		db_ = nullptr;
	}
	root_.clear();
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

int64_t RaLibrary::NowUnixTime() const
{
	return std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
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
		record->md5 = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
		record->game_id = sqlite3_column_int64(stmt, 1);
		record->working_relpath =
			reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
		record->inserted = false;
	}
	sqlite3_finalize(stmt);
	return true;
}

bool RaLibrary::RegisterDesktopMedia(const D88MediaInfo& media,
	const std::string& source_path, const std::string& display_name,
	int64_t source_mtime, MediaRecord *record, std::string *error)
{
	if (media.md5.size() != 32 || media.banks <= 0) {
		if (error != nullptr) {
			*error = "invalid media metadata";
		}
		return false;
	}

	MediaRecord existing;
	if (FindMedia(media.md5, &existing, nullptr)) {
		if (record != nullptr) {
			*record = existing;
		}
		return true;
	}

	if (!Exec("BEGIN IMMEDIATE", error)) {
		return false;
	}

	const int64_t now = NowUnixTime();
	std::string title = !media.bank_names.empty() && !media.bank_names[0].empty() ?
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
	const int64_t game_id = sqlite3_last_insert_rowid(db_);

	const std::string working_relpath =
		std::string("media/") + media.md5 + "/working.d88";
	if (!Prepare(db_,
		"INSERT INTO media(md5, game_id, identification_state, source_kind,"
		" source_locator, source_display_name, source_size, source_mtime,"
		" bank_count, working_relpath, ordinal, health_state, created_at, verified_at)"
		" VALUES(?, ?, 0, 0, ?, ?, ?, ?, ?, ?, 0, 0, ?, ?)",
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
	sqlite3_bind_int64(stmt, 9, now);
	sqlite3_bind_int64(stmt, 10, now);
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
