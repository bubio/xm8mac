#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>

#include "pc88_ra_memory.h"

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
	std::array<uint8_t, 0x10000> ram{};
	std::array<uint8_t, 0x1000> tvram{};
	for (size_t i = 0; i < ram.size(); i++) {
		ram[i] = static_cast<uint8_t>(i & 0xff);
	}
	for (size_t i = 0; i < tvram.size(); i++) {
		tvram[i] = static_cast<uint8_t>(0x80 | (i & 0x7f));
	}

	std::array<uint8_t, 8> buffer{};

	Check(ReadPc88RaInspectionMemory(ram.data(), tvram.data(), 0, buffer.data(),
		4) == 4, "read RAM start");
	Check(buffer[0] == 0x00 && buffer[3] == 0x03, "RAM start values");

	buffer.fill(0);
	Check(ReadPc88RaInspectionMemory(ram.data(), tvram.data(), 0xfffe,
		buffer.data(), 4) == 4, "read RAM to TVRAM boundary");
	Check(buffer[0] == 0xfe && buffer[1] == 0xff &&
		buffer[2] == 0x80 && buffer[3] == 0x81, "boundary values");

	buffer.fill(0);
	Check(ReadPc88RaInspectionMemory(ram.data(), tvram.data(), 0x10ffe,
		buffer.data(), 4) == 2, "read TVRAM to unmapped boundary");
	Check(buffer[0] == 0xfe && buffer[1] == 0xff && buffer[2] == 0,
		"partial TVRAM values");

	Check(ReadPc88RaInspectionMemory(ram.data(), tvram.data(), 0x11000,
		buffer.data(), 4) == 0, "reject unmapped start");
	Check(ReadPc88RaInspectionMemory(ram.data(), tvram.data(), 0,
		nullptr, 4) == 0, "reject null output");
	Check(ReadPc88RaInspectionMemory(ram.data(), tvram.data(), 0,
		buffer.data(), 0) == 0, "read zero bytes");

	if (failures != 0) {
		std::cerr << failures << " test(s) failed\n";
		return EXIT_FAILURE;
	}
	std::cout << "RA memory map tests passed\n";
	return EXIT_SUCCESS;
}
