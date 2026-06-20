#include "Fixtures/d88_fixture.h"
#include "d88probe.h"
#include "m3u.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

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

std::string JoinPath(const std::string& dir, const char *name)
{
	return dir + "/" + name;
}

std::vector<char> ReadFile(const std::string& path)
{
	std::ifstream stream(path, std::ios::binary);
	return std::vector<char>(std::istreambuf_iterator<char>(stream),
		std::istreambuf_iterator<char>());
}

long FileSize(const std::string& path)
{
	std::ifstream stream(path, std::ios::binary | std::ios::ate);
	return stream.is_open() ? static_cast<long>(stream.tellg()) : -1;
}

} // namespace

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
	) + "/xm8-d88-fixtures-" + std::to_string(unique);
	std::string error;

	Check(D88Fixture::GenerateStandardSet(base, &error),
		"generate fixture set");
	if (!error.empty()) {
		std::cerr << error << '\n';
	}

	int banks = 0;
	Check(ProbeD88Image(JoinPath(base, "single.d88").c_str(), &banks) &&
		banks == 1, "single image has one bank");
	Check(ProbeD88Image(JoinPath(base, "second.d88").c_str(), &banks) &&
		banks == 1, "second image has one bank");
	Check(ProbeD88Image(JoinPath(base, "multi.d88").c_str(), &banks) &&
		banks == 2, "multi image has two banks");
	Check(FileSize(JoinPath(base, "single.d88")) == 0x3c0,
		"single image size");
	Check(FileSize(JoinPath(base, "multi.d88")) == 0x780,
		"multi image size");

	const M3UResult playlist = LoadM3U(JoinPath(base, "pair.m3u"));
	Check(playlist.success, "load generated playlist");
	Check(playlist.entries.size() == 2, "playlist has two entries");
	if (playlist.entries.size() == 2) {
		Check(playlist.entries[0] == JoinPath(base, "single.d88#0"),
			"playlist first medium");
		Check(playlist.entries[1] == JoinPath(base, "second.d88#0"),
			"playlist second medium");
	}

	const auto before = ReadFile(JoinPath(base, "multi.d88"));
	Check(D88Fixture::GenerateStandardSet(base, &error),
		"regenerate fixture set");
	Check(before == ReadFile(JoinPath(base, "multi.d88")),
		"fixture generation is deterministic");

	for (const char *name : {"single.d88", "second.d88", "multi.d88", "pair.m3u"}) {
		Check(std::remove(JoinPath(base, name).c_str()) == 0,
			"remove temporary fixture");
	}
#ifdef _WIN32
	Check(_rmdir(base.c_str()) == 0, "remove temporary directory");
#else
	Check(rmdir(base.c_str()) == 0, "remove temporary directory");
#endif

	if (failures != 0) {
		std::cerr << failures << " test(s) failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "D88 fixture tests passed\n";
	return EXIT_SUCCESS;
}
