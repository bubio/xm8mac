#include "Fixtures/d88_fixture.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "disk.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

namespace {

int failures = 0;

void Check(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		failures++;
	}
}

long FileSize(const std::string& path)
{
	std::ifstream stream(path, std::ios::binary | std::ios::ate);
	return stream.is_open() ? static_cast<long>(stream.tellg()) : -1;
}

} // namespace

// DISK only uses this fallback when rewriting the original path fails.
_TCHAR* EMU::bios_path(_TCHAR* file_name)
{
	return file_name;
}

int main()
{
	const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
	const char *temporary = std::getenv(
#ifdef _WIN32
		"TEMP"
#else
		"TMPDIR"
#endif
	);
	const std::string base = std::string(temporary != nullptr ? temporary :
#ifdef _WIN32
		"."
#else
		"/tmp"
#endif
	) + "/xm8-d88-write-" + std::to_string(unique);
	const std::string path = base + "/single.d88";
	std::string error;

	Check(D88Fixture::GenerateStandardSet(base, &error),
		"generate writable D88 fixture");
	if (!error.empty()) {
		std::cerr << error << '\n';
	}

	uint8 original = 0;
	uint8 replacement = 0;
	{
		std::unique_ptr<DISK> disk(new DISK(nullptr));
		char mutable_path[_MAX_PATH];
		std::strncpy(mutable_path, path.c_str(), sizeof(mutable_path));
		mutable_path[sizeof(mutable_path) - 1] = '\0';
		disk->open(mutable_path, 0);
		Check(disk->inserted, "open generated D88");
		Check(!disk->write_protected, "generated D88 is writable");
		Check(disk->get_sector(0, 0, 0), "read generated sector");
		Check(disk->sector_size.sd == 0x100, "generated sector size");
		if (disk->sector != nullptr) {
			original = disk->sector[0];
			replacement = static_cast<uint8>(original ^ 0xff);
			disk->sector[0] = replacement;
			disk->changed = true;
		}
		disk->close();
	}

	Check(FileSize(path) == 0x3c0, "D88 size unchanged after write");
	{
		std::unique_ptr<DISK> disk(new DISK(nullptr));
		char mutable_path[_MAX_PATH];
		std::strncpy(mutable_path, path.c_str(), sizeof(mutable_path));
		mutable_path[sizeof(mutable_path) - 1] = '\0';
		disk->open(mutable_path, 0);
		Check(disk->get_sector(0, 0, 0), "reopen written sector");
		Check(disk->sector != nullptr && disk->sector[0] == replacement &&
			disk->sector[0] != original, "sector write persisted");
		disk->close();
	}

	for (const char *name : {"single.d88", "second.d88", "multi.d88", "pair.m3u"}) {
		Check(std::remove((base + "/" + name).c_str()) == 0,
			"remove writable fixture");
	}
#ifdef _WIN32
	Check(_rmdir(base.c_str()) == 0, "remove writable fixture directory");
#else
	Check(rmdir(base.c_str()) == 0, "remove writable fixture directory");
#endif

	if (failures != 0) {
		std::cerr << failures << " test(s) failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "D88 write test passed\n";
	return EXIT_SUCCESS;
}
