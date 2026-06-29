#ifndef XM8_RA_MEDIA_STORE_H
#define XM8_RA_MEDIA_STORE_H

#include "ra_library.h"

#include <string>
#include <vector>

namespace Xm8Ra {

struct ImportedMedia {
	MediaRecord record;
	std::string working_path;
	bool copied = false;
};

struct ImportedPlaylist {
	std::vector<ImportedMedia> media;
	int64_t game_id = 0;
	std::string anchor_md5;
};

class RaMediaStore {
public:
	explicit RaMediaStore(RaLibrary *library);

	bool ImportDesktopD88(const std::string& source_path,
		ImportedMedia *imported, std::string *error);
	bool ImportM3U(const std::string& playlist_path,
		ImportedPlaylist *imported, std::string *error);

private:
	bool ImportD88IntoGame(const std::string& source_path, int64_t game_id,
		int ordinal, ImportedMedia *imported, std::string *error);
	bool EnsureWorkingCopy(const std::string& source_path,
		const D88MediaInfo& media, std::string *working_path,
		bool *copied, std::string *error);
	bool CopyAndVerify(const std::string& source_path,
		const std::string& temporary_path, const D88MediaInfo& expected,
		std::string *error);
	int64_t FileMtime(const std::string& path) const;

	RaLibrary *library_;
};

} // namespace Xm8Ra

#endif
