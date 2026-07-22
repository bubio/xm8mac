#include "ra_file_util.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Check(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}
}

std::string JoinPath(const std::string& directory, const std::string& name)
{
	return directory + "/" + name;
}

} // namespace

int main()
{
	const char *temporary = std::getenv(
#ifdef _WIN32
		"TEMP"
#else
		"TMPDIR"
#endif
	);
	const auto unique = std::chrono::steady_clock::now()
		.time_since_epoch().count();
	const std::string root = std::string(temporary != nullptr ? temporary :
#ifdef _WIN32
		"."
#else
		"/tmp"
#endif
	) + "/xm8-ra-file-util-" + std::to_string(unique);
	const std::string unicode_directory = JoinPath(root, u8"日本語 space");
	const std::string source = JoinPath(unicode_directory, u8"元データ.bin");
	const std::string copy = JoinPath(unicode_directory, u8"複製.bin");
	const std::string replacement = JoinPath(unicode_directory, u8"置換.bin");
	const std::vector<uint8_t> first = {0, 1, 2, 3, 0xff};
	const std::vector<uint8_t> second = {9, 8, 7};
	std::string error;

	Check(Xm8Ra::EnsureRaDirectoryTree(unicode_directory, &error),
		"create UTF-8 directory tree");
	Check(Xm8Ra::WriteRaFile(source, first.data(), first.size(), &error),
		"write UTF-8 file path");
	std::vector<uint8_t> loaded;
	Check(Xm8Ra::ReadRaFile(source, &loaded, 1024, &error) && loaded == first,
		"read UTF-8 file path");
	Check(Xm8Ra::CopyRaFile(source, copy, &error), "copy file");
	Check(Xm8Ra::WriteRaFile(replacement, second.data(), second.size(), &error),
		"write replacement file");
	Check(Xm8Ra::MoveRaFile(replacement, copy, true, &error),
		"replace existing destination");
	Check(Xm8Ra::ReadRaFile(copy, &loaded, 1024, &error) && loaded == second,
		"replacement content is visible");

	std::vector<Xm8Ra::RaDirectoryEntry> entries;
	Check(Xm8Ra::ListRaDirectoryNoFollow(unicode_directory, &entries, &error) &&
		entries.size() == 2, "list UTF-8 directory");
	Check(!Xm8Ra::ReadRaFile(source, &loaded, 2, &error),
		"enforce read size limit");

#ifdef _WIN32
	const std::string invalid_utf8("bad-\xc0\xaf", 6);
	Check(!Xm8Ra::EnsureRaDirectoryTree(JoinPath(root, invalid_utf8), &error),
		"reject invalid UTF-8 path");
#endif

	Check(Xm8Ra::RemoveRaTree(root, &error), "remove test directory tree");
	Check(!Xm8Ra::RaPathExists(root), "test directory removed");
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
