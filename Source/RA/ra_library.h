#ifndef XM8_RA_LIBRARY_H
#define XM8_RA_LIBRARY_H

#include "ra_media_probe.h"

#include <cstdint>
#include <string>

struct sqlite3;

namespace Xm8Ra {

constexpr int kRaModeSoftcore = 1;
constexpr int kRaModeHardcore = 2;

struct RaSettings {
	bool enabled = false;
	int last_mode = kRaModeSoftcore;
	bool unofficial_enabled = false;
	bool encore_enabled = false;
	bool spectator_enabled = false;
	int notification_seconds = 5;
	int image_cache_limit_mib = 128;
};

struct MediaRecord {
	std::string md5;
	int64_t game_id = 0;
	std::string working_relpath;
	bool inserted = false;
};

class RaLibrary {
public:
	RaLibrary();
	~RaLibrary();

	RaLibrary(const RaLibrary&) = delete;
	RaLibrary& operator=(const RaLibrary&) = delete;

	bool Open(const std::string& ra_root, std::string *error);
	void Close();

	const std::string& Root() const { return root_; }
	std::string MediaRoot() const;
	std::string TempRoot() const;
	std::string DatabasePath() const;

	bool LoadSettings(RaSettings *settings, std::string *error);
	bool SaveSettings(const RaSettings& settings, std::string *error);
	bool RegisterDesktopMedia(const D88MediaInfo& media,
		const std::string& source_path, const std::string& display_name,
		int64_t source_mtime, MediaRecord *record, std::string *error);
	bool RegisterDesktopMediaInGame(const D88MediaInfo& media,
		const std::string& source_path, const std::string& display_name,
		int64_t source_mtime, int64_t game_id, int ordinal,
		MediaRecord *record, std::string *error);
	bool FindMedia(const std::string& md5, MediaRecord *record,
		std::string *error);

private:
	bool Exec(const char *sql, std::string *error);
	bool SetupDatabase(std::string *error);
	bool CheckIntegrity(std::string *error);
	bool InitializeSchema(std::string *error);
	bool EnsureSettingsRow(std::string *error);
	bool IsDatabaseDamage() const;
	bool QuarantineDatabase(std::string *error);
	bool ValidateSettings(const RaSettings& settings, std::string *error) const;
	bool RegisterDesktopMediaInternal(const D88MediaInfo& media,
		const std::string& source_path, const std::string& display_name,
		int64_t source_mtime, int64_t game_id, int ordinal,
		bool create_anchor_profile, MediaRecord *record,
		std::string *error);
	int64_t NowUnixTime() const;
	void CloseDatabaseOnly();

	sqlite3 *db_;
	std::string root_;
};

} // namespace Xm8Ra

#endif
