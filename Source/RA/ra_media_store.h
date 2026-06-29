#ifndef XM8_RA_MEDIA_STORE_H
#define XM8_RA_MEDIA_STORE_H

#include "ra_library.h"

#include <string>

namespace Xm8Ra {

struct ImportedMedia {
	MediaRecord record;
	std::string working_path;
	bool copied = false;
};

class RaMediaStore {
public:
	explicit RaMediaStore(RaLibrary *library);

	bool ImportDesktopD88(const std::string& source_path,
		ImportedMedia *imported, std::string *error);

private:
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
