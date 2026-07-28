#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

// fileio.h inherits the legacy common.h min/max macros. Keep the standard
// library headers above it so those macros cannot corrupt their declarations.
#include "fileio.h"

namespace {

int failures = 0;

void Check(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}
}

} // namespace

int main()
{
	const auto unique = std::chrono::steady_clock::now()
		.time_since_epoch().count();
	const char *temporary = std::getenv(
#ifdef _WIN32
		"TEMP"
#else
		"TMPDIR"
#endif
	);
	const std::string path = std::string(temporary != nullptr ? temporary :
#ifdef _WIN32
		"."
#else
		"/tmp"
#endif
	) + "/xm8-fileio-error-" + std::to_string(unique);
	FILEIO file;
	Check(!file.HasError(), "new FILEIO has no error");
	Check(file.Fopen(const_cast<char *>(path.c_str()), FILEIO_WRITE_BINARY),
		"open test file for write");
	file.FputUint32(0x12345678U);
	file.Fclose();
	Check(!file.HasError(), "successful write and close have no error");

	Check(file.Fopen(const_cast<char *>(path.c_str()), FILEIO_READ_BINARY),
		"open test file for read");
	Check(!file.HasError(), "open resets prior I/O status");
	Check(file.FgetUint32() == 0x12345678U, "read written value");
	uint8_t extra = 0;
	Check(file.Fread(&extra, 1, 1) == 0, "read reports end of file");
	Check(file.HasError(), "short read is retained as an I/O error");
	file.Fclose();

	Check(file.Fopen(const_cast<char *>(path.c_str()), FILEIO_READ_BINARY),
		"reopen test file");
	Check(!file.HasError(), "successful reopen clears I/O error");
	file.Fclose();
	std::remove(path.c_str());

	if (failures != 0) {
		std::cerr << failures << " FILEIO error test(s) failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "FILEIO error tests passed\n";
	return EXIT_SUCCESS;
}
