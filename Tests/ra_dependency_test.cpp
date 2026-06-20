#include "ra_build_info.h"

#include "sqlite3.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>

#ifndef RC_CLIENT_SUPPORTS_HASH
#error RC_CLIENT_SUPPORTS_HASH must be defined for RA targets
#endif

#ifndef RC_DISABLE_LUA
#error RC_DISABLE_LUA must be defined for RA targets
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

} // namespace

int main()
{
	Check(Xm8RaBuildInfo::RcheevosVersion() == 12003000,
		"rcheevos numeric version");
	Check(std::strcmp(Xm8RaBuildInfo::RcheevosVersionString(), "12.3") == 0,
		"rcheevos string version");

	Check(std::strcmp(Xm8RaBuildInfo::SqliteVersion(), "3.53.0") == 0,
		"SQLite version");
	Check(Xm8RaBuildInfo::SqliteVersionNumber() == 3053000,
		"SQLite version number");
	Check(std::strcmp(Xm8RaBuildInfo::SqliteSourceId(),
		"2026-04-09 11:41:38 "
		"4525003a53a7fc63ca75c59b22c79608659ca12f0131f52c18637f829977f20b") == 0,
		"SQLite source ID");
	Check(Xm8RaBuildInfo::SqliteThreadsafe() == 1, "SQLite threadsafe");
	Check(Xm8RaBuildInfo::SqliteCompileOptionUsed("THREADSAFE=1") == 1,
		"SQLite THREADSAFE option");
	Check(Xm8RaBuildInfo::SqliteCompileOptionUsed("DEFAULT_FOREIGN_KEYS") == 1,
		"SQLite foreign keys option");
	Check(Xm8RaBuildInfo::SqliteCompileOptionUsed("OMIT_LOAD_EXTENSION") == 1,
		"SQLite load extension omitted");
	Check(Xm8RaBuildInfo::SqliteCompileOptionUsed("DQS=0") == 1,
		"SQLite DQS option");

	static const uint8_t png[] = {
		0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
		0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
		0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
		0x08, 0x04, 0x00, 0x00, 0x00, 0xb5, 0x1c, 0x0c,
		0x02, 0x00, 0x00, 0x00, 0x0b, 0x49, 0x44, 0x41,
		0x54, 0x78, 0xda, 0x63, 0x64, 0xf8, 0x0f, 0x00,
		0x01, 0x05, 0x01, 0x01, 0x27, 0x18, 0xe3, 0x66,
		0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44,
		0xae, 0x42, 0x60, 0x82
	};
	int width = 0;
	int height = 0;
	int components = 0;
	Check(Xm8RaBuildInfo::ProbeImage(png, sizeof(png), &width, &height,
		&components), "stb PNG probe");
	Check(width == 1 && height == 1 && components == 2,
		"stb PNG dimensions");

	if (failures != 0) {
		std::cerr << failures << " test(s) failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "RA dependency tests passed\n";
	return EXIT_SUCCESS;
}
