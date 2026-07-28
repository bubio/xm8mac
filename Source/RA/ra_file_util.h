#ifndef XM8_RA_FILE_UTIL_H
#define XM8_RA_FILE_UTIL_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Xm8Ra {

enum class RaFileKind {
	Missing,
	Regular,
	Directory,
	Other
};

struct RaFileInfo {
	RaFileKind kind = RaFileKind::Missing;
	uint64_t size = 0;
	int64_t modified_time = -1;
};

struct RaDirectoryEntry {
	std::string path;
	RaFileKind kind = RaFileKind::Other;
};

bool GetRaFileInfoNoFollow(const std::string& path, RaFileInfo *info,
	std::string *error = nullptr);
bool RaPathExists(const std::string& path);
bool RaIsRegularFileNoFollow(const std::string& path);
bool EnsureRaDirectoryTree(const std::string& path, std::string *error = nullptr);
bool ListRaDirectoryNoFollow(const std::string& path,
	std::vector<RaDirectoryEntry> *entries, std::string *error = nullptr);
bool RemoveRaFile(const std::string& path, std::string *error = nullptr);
bool RemoveRaTree(const std::string& path, std::string *error = nullptr);
bool MoveRaFile(const std::string& source, const std::string& destination,
	bool replace_destination, std::string *error = nullptr);
bool CopyRaFile(const std::string& source, const std::string& destination,
	std::string *error = nullptr);
bool ReadRaFile(const std::string& path, std::vector<uint8_t> *data,
	size_t maximum_size, std::string *error = nullptr);
bool WriteRaFile(const std::string& path, const uint8_t *data, size_t size,
	std::string *error = nullptr);

} // namespace Xm8Ra

#endif
