#include "d88probe.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const char *message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		failures++;
	}
}

std::array<unsigned char, 0x2b0> MakeHeader(const char *name)
{
	std::array<unsigned char, 0x2b0> header = {};
	size_t index = 0;
	while (name[index] != '\0' && index < 16) {
		header[index] = static_cast<unsigned char>(name[index]);
		index++;
	}
	header[0x1c] = 0xb0;
	header[0x1d] = 0x02;
	header[0x20] = 0xb0;
	header[0x21] = 0x02;
	return header;
}

void WriteImage(const std::string& path, int banks)
{
	std::ofstream stream(path, std::ios::binary);
	for (int bank=0; bank<banks; bank++) {
		const auto header = MakeHeader(bank == 0 ? "BANK0" : "BANK1");
		stream.write(reinterpret_cast<const char*>(header.data()), header.size());
	}
}

} // namespace

int main(int argc, char *argv[])
{
	if (argc == 2) {
		int banks;
		if (!ProbeD88Image(argv[1], &banks)) {
			std::cerr << "invalid D88: " << argv[1] << '\n';
			return EXIT_FAILURE;
		}
		std::cout << banks << '\n';
		return EXIT_SUCCESS;
	}

	const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
	const std::string base = "/tmp/xm8-d88probe-" + std::to_string(unique);
	const std::string one = base + "-one.d88";
	const std::string two = base + "-two.d88";
	const std::string invalid = base + "-invalid.d88";
	int banks = 0;
	size_t names = 0;

	WriteImage(one, 1);
	WriteImage(two, 2);
	{
		std::ofstream stream(invalid, std::ios::binary);
		stream << "not a d88";
	}

	Check(ProbeD88Image(one.c_str(), &banks, &names),
		"probe one-bank image");
	Check(banks == 1 && names == 6, "one-bank metadata");
	Check(ProbeD88Image(two.c_str(), &banks, &names),
		"probe two-bank image");
	Check(banks == 2 && names == 12, "two-bank metadata");
	Check(!ProbeD88Image(invalid.c_str(), &banks),
		"reject invalid image");
	Check(!ProbeD88Image("/tmp/xm8-no-such-image.d88", &banks),
		"reject missing image");
	Check(!ProbeD88Image(one.c_str(), nullptr),
		"reject null output");

	std::remove(one.c_str());
	std::remove(two.c_str());
	std::remove(invalid.c_str());

	if (failures != 0) {
		std::cerr << failures << " test(s) failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "d88 probe tests passed\n";
	return EXIT_SUCCESS;
}
