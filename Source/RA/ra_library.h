#ifndef XM8_RA_LIBRARY_H
#define XM8_RA_LIBRARY_H

#include "ra_media_probe.h"

#include <cstdint>
#include <string>

struct sqlite3;

namespace Xm8Ra {

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

	bool RegisterDesktopMedia(const D88MediaInfo& media,
		const std::string& source_path, const std::string& display_name,
		int64_t source_mtime, MediaRecord *record, std::string *error);
	bool FindMedia(const std::string& md5, MediaRecord *record,
		std::string *error);

private:
	bool Exec(const char *sql, std::string *error);
	bool InitializeSchema(std::string *error);
	bool EnsureSettingsRow(std::string *error);
	int64_t NowUnixTime() const;

	sqlite3 *db_;
	std::string root_;
};

} // namespace Xm8Ra

#endif
