#import <Foundation/Foundation.h>

#include <cstring>
#include <iostream>
#include <stdlib.h>

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

bool CreateAlias(NSURL *target, NSURL *alias)
{
	NSError *error = nil;
	NSData *bookmark = [target bookmarkDataWithOptions:
		NSURLBookmarkCreationSuitableForBookmarkFile
		includingResourceValuesForKeys:nil
		relativeToURL:nil
		error:&error];
	return bookmark != nil &&
		[NSURL writeBookmarkData:bookmark toURL:alias options:0 error:&error];
}

bool MatchesCanonicalPath(const char *actual, NSURL *expected)
{
	char canonical[1024];
	return realpath([[expected path] fileSystemRepresentation], canonical) !=
		nullptr && std::strcmp(actual, canonical) == 0;
}

}

int main()
{
	@autoreleasepool {
		NSFileManager *manager = [NSFileManager defaultManager];
		NSURL *root = [NSURL fileURLWithPath:
			@"/tmp/xm8-pathresolver-mac-test" isDirectory:YES];
		[manager removeItemAtURL:root error:nil];
		Check([manager createDirectoryAtURL:root
			withIntermediateDirectories:YES attributes:nil error:nil],
			"create temporary directory");

		NSURL *target = [root URLByAppendingPathComponent:@"target.d88"];
		NSURL *moved = [root URLByAppendingPathComponent:@"moved.d88"];
		NSURL *alias = [root URLByAppendingPathComponent:@"disk alias"];
		NSURL *directory = [root URLByAppendingPathComponent:@"directory"
			isDirectory:YES];
		NSURL *directory_alias = [root URLByAppendingPathComponent:
			@"directory alias"];

		Check([manager createFileAtPath:[target path] contents:[NSData data]
			attributes:nil], "create alias target");
		Check([manager createDirectoryAtURL:directory
			withIntermediateDirectories:YES attributes:nil error:nil],
			"create directory alias target");
		Check(CreateAlias(target, alias), "create file alias");
		Check(CreateAlias(directory, directory_alias), "create directory alias");

		char resolved[1024];
		Check(InspectPath([[alias path] fileSystemRepresentation], resolved,
			sizeof(resolved)) == PATH_KIND_FILE, "classify file alias");
		Check(MatchesCanonicalPath(resolved, target), "resolve file alias");
		Check(InspectPath([[directory_alias path] fileSystemRepresentation],
			resolved, sizeof(resolved)) == PATH_KIND_DIRECTORY,
			"classify directory alias");

		Check([manager moveItemAtURL:target toURL:moved error:nil],
			"move alias target");
		Check(ResolvePathForIO([[alias path] fileSystemRepresentation],
			resolved, sizeof(resolved)), "resolve moved alias");
		Check(MatchesCanonicalPath(resolved, moved),
			"follow moved alias target");
		Check([manager removeItemAtURL:moved error:nil],
			"remove alias target");
		Check(InspectPath([[alias path] fileSystemRepresentation], resolved,
			sizeof(resolved)) == PATH_KIND_UNAVAILABLE,
			"reject broken alias");

		[manager removeItemAtURL:root error:nil];
	}

	return failures == 0 ? 0 : 1;
}
