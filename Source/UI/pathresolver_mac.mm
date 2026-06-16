#import <Foundation/Foundation.h>

#include <cerrno>
#include <cstring>
#include <sys/stat.h>

#include "pathresolver.h"

namespace {

bool CopyPath(const char *source, char *destination, size_t capacity)
{
	const size_t length = std::strlen(source);
	if (length >= capacity) {
		return false;
	}
	std::memcpy(destination, source, length + 1);
	return true;
}

}

bool ResolveMacAlias(const char *path, char *resolved, size_t capacity)
{
	if (path == nullptr || resolved == nullptr || capacity == 0) {
		return false;
	}

	@autoreleasepool {
		NSString *string = [NSString stringWithUTF8String:path];
		if (string == nil) {
			return false;
		}

		NSURL *url = [NSURL fileURLWithPath:string];
		NSNumber *is_alias = nil;
		NSError *error = nil;
		if (![url getResourceValue:&is_alias
							forKey:NSURLIsAliasFileKey
							 error:&error]) {
			struct stat file_stat;
			if (lstat(path, &file_stat) != 0 && errno == ENOENT) {
				// Nonexistent paths must remain usable for file creation.
				return CopyPath(path, resolved, capacity);
			}
			return false;
		}
		if (![is_alias boolValue]) {
			return CopyPath(path, resolved, capacity);
		}

		NSMutableSet<NSString *> *visited = [NSMutableSet set];
		for (int depth = 0; depth < 16; depth++) {
			NSString *current_path = [url path];
			if (current_path == nil || [visited containsObject:current_path]) {
				return false;
			}
			[visited addObject:current_path];

			url = [NSURL URLByResolvingAliasFileAtURL:url
				options:(NSURLBookmarkResolutionWithoutUI |
					NSURLBookmarkResolutionWithoutMounting)
				error:&error];
			if (url == nil || ![url isFileURL]) {
				return false;
			}

			is_alias = nil;
			error = nil;
			if (![url getResourceValue:&is_alias
								forKey:NSURLIsAliasFileKey
								 error:&error]) {
				return false;
			}
			if (![is_alias boolValue]) {
				const char *file_path = [url fileSystemRepresentation];
				return file_path != nullptr &&
					CopyPath(file_path, resolved, capacity);
			}
		}
	}

	return false;
}
