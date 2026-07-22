#include <cerrno>
#include <climits>
#include <cstring>

#if defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
#include <cstdlib>
#include <sys/stat.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#endif

#include "pathresolver.h"

#ifdef __APPLE__
bool ResolveMacAlias(const char *path, char *resolved, size_t capacity);
#endif

#ifdef _WIN32
namespace {

bool Utf8PathToWide(const char *path, std::wstring *wide)
{
	if (path == nullptr || wide == nullptr || path[0] == '\0') {
		return false;
	}
	const size_t byte_length = std::strlen(path);
	if (byte_length > static_cast<size_t>(INT_MAX)) {
		return false;
	}
	const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
		path, static_cast<int>(byte_length), nullptr, 0);
	if (length <= 0) {
		return false;
	}
	wide->assign(static_cast<size_t>(length), L'\0');
	return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path,
		static_cast<int>(byte_length), &(*wide)[0], length) == length;
}

} // namespace
#endif

bool ResolvePathForIO(const char *path, char *resolved, size_t capacity)
{
	char candidate[4096];

	if (path == nullptr || resolved == nullptr || capacity == 0) {
		return false;
	}

#ifdef __APPLE__
	if (!ResolveMacAlias(path, candidate, sizeof(candidate))) {
		return false;
	}
#else
	const size_t input_length = std::strlen(path);
	if (input_length >= sizeof(candidate)) {
		return false;
	}
	std::memcpy(candidate, path, input_length + 1);
#endif

#ifdef _WIN32
	std::wstring wide;
	if (!Utf8PathToWide(candidate, &wide)) {
		return false;
	}
#endif

#if defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
	char canonical[4096];
	if (realpath(candidate, canonical) != nullptr) {
		const size_t length = std::strlen(canonical);
		if (length >= capacity) {
			return false;
		}
		std::memcpy(resolved, canonical, length + 1);
		return true;
	}

	struct stat file_stat;
	if (lstat(candidate, &file_stat) == 0 || errno != ENOENT) {
		return false;
	}
#endif

	const size_t output_length = std::strlen(candidate);
	if (output_length >= capacity) {
		return false;
	}
	std::memcpy(resolved, candidate, output_length + 1);
	return true;
}

PathKind InspectPath(const char *path, char *resolved, size_t capacity)
{
	char local_path[4096];
	char *target = resolved;
	size_t target_capacity = capacity;

	if (target == nullptr) {
		target = local_path;
		target_capacity = sizeof(local_path);
	}
	if (!ResolvePathForIO(path, target, target_capacity)) {
		return PATH_KIND_UNAVAILABLE;
	}

#if defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
	struct stat file_stat;
	if (stat(target, &file_stat) != 0) {
		return PATH_KIND_UNAVAILABLE;
	}
	if (S_ISREG(file_stat.st_mode)) {
		return PATH_KIND_FILE;
	}
	if (S_ISDIR(file_stat.st_mode)) {
		return PATH_KIND_DIRECTORY;
	}
	return PATH_KIND_OTHER;
#else
	#ifdef _WIN32
	std::wstring wide;
	if (!Utf8PathToWide(target, &wide)) {
		return PATH_KIND_UNAVAILABLE;
	}
	const DWORD attributes = GetFileAttributesW(wide.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		return PATH_KIND_UNAVAILABLE;
	}
	return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ?
		PATH_KIND_DIRECTORY : PATH_KIND_FILE;
	#else
	return PATH_KIND_OTHER;
	#endif
#endif
}
