#include <cerrno>
#include <cstring>

#if defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
#include <cstdlib>
#include <sys/stat.h>
#endif

#include "pathresolver.h"

#ifdef __APPLE__
bool ResolveMacAlias(const char *path, char *resolved, size_t capacity);
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
	const size_t length = std::strlen(path);
	if (length >= sizeof(candidate)) {
		return false;
	}
	std::memcpy(candidate, path, length + 1);
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

	const size_t length = std::strlen(candidate);
	if (length >= capacity) {
		return false;
	}
	std::memcpy(resolved, candidate, length + 1);
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
	return PATH_KIND_OTHER;
#endif
}
