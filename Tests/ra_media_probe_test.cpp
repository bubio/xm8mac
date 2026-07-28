#include "Fixtures/d88_fixture.h"
#include "ra_file_util.h"
#include "ra_media_probe.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

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

bool WriteBinary(const std::string& path, const std::vector<uint8_t>& data)
{
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream.is_open()) {
		return false;
	}
	stream.write(reinterpret_cast<const char*>(data.data()),
		static_cast<std::streamsize>(data.size()));
	return stream.good();
}

void CheckProbe(const std::string& path, const char *expected_md5,
	int expected_banks, const char *message)
{
	Xm8Ra::D88MediaInfo info;
	std::string error;
	Check(Xm8Ra::ProbeD88File(path.c_str(), &info, &error), message);
	if (!error.empty()) {
		std::cerr << error << '\n';
	}
	Check(info.md5 == expected_md5, "D88 whole-file MD5");
	Check(info.banks == expected_banks, "D88 bank count");
	Check(static_cast<int>(info.bank_names.size()) == expected_banks,
		"D88 bank names");
	Check(static_cast<int>(info.bank_md5s.size()) == expected_banks,
		"D88 bank MD5 list");
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
	) + "/xm8-ra-media-probe-" + std::to_string(unique);

	std::string error;
	Check(D88Fixture::GenerateStandardSet(base, &error),
		"generate fixture set");
	if (!error.empty()) {
		std::cerr << error << '\n';
	}

	CheckProbe(JoinPath(base, "single.d88"),
		"5c50ca4f9e3a7afbe4d6666e8974949d", 1, "probe single D88");
	CheckProbe(JoinPath(base, "second.d88"),
		"ff400f51a2567419b3778691a905952e", 1, "probe second D88");
	CheckProbe(JoinPath(base, "multi.d88"),
		"9be57f249da12241c8785db0b195216b", 2, "probe multi D88");
	Xm8Ra::D88MediaInfo multi;
	Check(Xm8Ra::ProbeD88File(JoinPath(base, "multi.d88").c_str(), &multi,
		&error), "probe multi for bank hashes");
	Check(multi.bank_md5s[0] == "5c50ca4f9e3a7afbe4d6666e8974949d",
		"multi D88 first bank hash uses single disk image");
	Check(multi.bank_md5s[1] == "ff400f51a2567419b3778691a905952e",
		"multi D88 second bank hash uses second disk image");

	char hash[33] = {};
	Check(Xm8Ra::HashPc8800File(JoinPath(base, "multi.d88").c_str(), hash),
		"hash multi D88");
	Check(std::strcmp(hash, "9be57f249da12241c8785db0b195216b") == 0,
		"hash ignores bank selection outside file bytes");
	Check(std::strcmp(hash, multi.bank_md5s[0].c_str()) != 0,
		"multi whole-file hash differs from first bank RA hash");

	const std::string invalid = JoinPath(base, "invalid.d88");
	Check(WriteBinary(invalid, {'n', 'o', 't', ' ', 'd', '8', '8'}),
		"write invalid D88");
	Xm8Ra::D88MediaInfo invalid_info;
	Check(!Xm8Ra::ProbeD88File(invalid.c_str(), &invalid_info, &error),
		"reject invalid D88");

	Check(Xm8Ra::RemoveRaTree(base), "remove temporary fixtures");

	if (failures != 0) {
		std::cerr << failures << " test(s) failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "RA media probe tests passed\n";
	return EXIT_SUCCESS;
}
