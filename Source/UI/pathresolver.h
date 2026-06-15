#ifndef PATHRESOLVER_H
#define PATHRESOLVER_H

#include <cstddef>

enum PathKind {
	PATH_KIND_UNAVAILABLE = 0,
	PATH_KIND_FILE,
	PATH_KIND_DIRECTORY,
	PATH_KIND_OTHER
};

bool ResolvePathForIO(const char *path, char *resolved, size_t capacity);
PathKind InspectPath(const char *path, char *resolved = nullptr,
	size_t capacity = 0);

#endif
