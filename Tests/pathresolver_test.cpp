#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "pathresolver.h"

namespace {

int failures = 0;

void Check(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		failures++;
	}
}

}

int main()
{
#if defined(__linux__) || defined(__APPLE__)
	char temporary[] = "/tmp/xm8-pathresolver-XXXXXX";
	char *directory = mkdtemp(temporary);
	Check(directory != nullptr, "create temporary directory");
	if (directory == nullptr) {
		return 1;
	}

	char file_path[1024];
	char file_link[1024];
	char dir_link[1024];
	char broken_link[1024];
	std::snprintf(file_path, sizeof(file_path), "%s/disk.d88", directory);
	std::snprintf(file_link, sizeof(file_link), "%s/file-link", directory);
	std::snprintf(dir_link, sizeof(dir_link), "%s/dir-link", directory);
	std::snprintf(broken_link, sizeof(broken_link), "%s/broken-link", directory);

	FILE *file = std::fopen(file_path, "wb");
	Check(file != nullptr, "create regular file");
	if (file != nullptr) {
		std::fclose(file);
	}
	Check(symlink(file_path, file_link) == 0, "create file symlink");
	Check(symlink(directory, dir_link) == 0, "create directory symlink");
	Check(symlink("/path/that/does/not/exist", broken_link) == 0,
		"create broken symlink");

	char resolved[1024];
	Check(InspectPath(file_path, resolved, sizeof(resolved)) == PATH_KIND_FILE,
		"classify regular file");
	Check(InspectPath(file_link, resolved, sizeof(resolved)) == PATH_KIND_FILE,
		"classify file symlink");
	char canonical[1024];
	Check(realpath(file_path, canonical) != nullptr &&
		std::strcmp(resolved, canonical) == 0, "resolve file symlink");
	Check(InspectPath(dir_link, resolved, sizeof(resolved)) ==
		PATH_KIND_DIRECTORY, "classify directory symlink");
	Check(realpath(directory, canonical) != nullptr &&
		std::strcmp(resolved, canonical) == 0, "resolve directory symlink");
	Check(InspectPath(broken_link, resolved, sizeof(resolved)) ==
		PATH_KIND_UNAVAILABLE, "reject broken symlink");

	unlink(broken_link);
	unlink(dir_link);
	unlink(file_link);
	unlink(file_path);
	rmdir(directory);
#else
	char resolved[32];
	Check(ResolvePathForIO("disk.d88", resolved, sizeof(resolved)),
		"copy normal path");
	Check(std::strcmp(resolved, "disk.d88") == 0, "preserve normal path");
#endif

	return failures == 0 ? 0 : 1;
}
