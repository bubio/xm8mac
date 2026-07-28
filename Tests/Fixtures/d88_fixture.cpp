#include "d88_fixture.h"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace D88Fixture {
namespace {

constexpr size_t kHeaderSize = 0x2b0;
constexpr size_t kSectorHeaderSize = 0x10;
constexpr size_t kSectorSize = 0x100;
constexpr size_t kBankSize = kHeaderSize + kSectorHeaderSize + kSectorSize;

void PutLe16(std::vector<uint8_t>& data, size_t offset, uint16_t value)
{
	data[offset] = static_cast<uint8_t>(value);
	data[offset + 1] = static_cast<uint8_t>(value >> 8);
}

void PutLe32(std::vector<uint8_t>& data, size_t offset, uint32_t value)
{
	data[offset] = static_cast<uint8_t>(value);
	data[offset + 1] = static_cast<uint8_t>(value >> 8);
	data[offset + 2] = static_cast<uint8_t>(value >> 16);
	data[offset + 3] = static_cast<uint8_t>(value >> 24);
}

std::vector<uint8_t> MakeBank(const char *name, uint8_t seed)
{
	std::vector<uint8_t> bank(kBankSize, 0);
	for (size_t i = 0; name[i] != '\0' && i < 16; i++) {
		bank[i] = static_cast<uint8_t>(name[i]);
	}

	bank[0x1a] = 0;
	bank[0x1b] = 0;
	PutLe32(bank, 0x1c, static_cast<uint32_t>(bank.size()));
	PutLe32(bank, 0x20, static_cast<uint32_t>(kHeaderSize));

	const size_t sector = kHeaderSize;
	bank[sector + 0] = 0;
	bank[sector + 1] = 0;
	bank[sector + 2] = 1;
	bank[sector + 3] = 1;
	PutLe16(bank, sector + 4, 1);
	PutLe16(bank, sector + 14, static_cast<uint16_t>(kSectorSize));
	for (size_t i = 0; i < kSectorSize; i++) {
		bank[sector + kSectorHeaderSize + i] =
			static_cast<uint8_t>(seed + static_cast<uint8_t>(i));
	}
	return bank;
}

std::string JoinPath(const std::string& dir, const char *name)
{
	if (!dir.empty() && (dir.back() == '/' || dir.back() == '\\')) {
		return dir + name;
	}
	return dir + "/" + name;
}

bool MakeDirectory(const std::string& path, std::string *error)
{
#ifdef _WIN32
	const int result = _mkdir(path.c_str());
#else
	const int result = mkdir(path.c_str(), 0755);
#endif
	if (result == 0 || errno == EEXIST) {
		return true;
	}
	*error = "cannot create output directory: " +
		std::string(std::strerror(errno));
	return false;
}

bool WriteBinary(const std::string& path,
	const std::vector<std::vector<uint8_t>>& banks, std::string *error)
{
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream.is_open()) {
		*error = "cannot create " + path;
		return false;
	}
	for (const auto& bank : banks) {
		stream.write(reinterpret_cast<const char*>(bank.data()),
			static_cast<std::streamsize>(bank.size()));
	}
	if (!stream.good()) {
		*error = "cannot write " + path;
		return false;
	}
	return true;
}

bool WriteText(const std::string& path, const std::string& text,
	std::string *error)
{
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream.is_open()) {
		*error = "cannot create " + path;
		return false;
	}
	stream.write(text.data(), static_cast<std::streamsize>(text.size()));
	if (!stream.good()) {
		*error = "cannot write " + path;
		return false;
	}
	return true;
}

} // namespace

bool GenerateStandardSet(const std::string& output_dir, std::string *error)
{
	if (error == nullptr) {
		return false;
	}
	error->clear();

	if (!MakeDirectory(output_dir, error)) {
		return false;
	}

	const auto first = MakeBank("XM8 FIXTURE A", 0x11);
	const auto second = MakeBank("XM8 FIXTURE B", 0x42);
	if (!WriteBinary(JoinPath(output_dir, "single.d88"), {first}, error) ||
		!WriteBinary(JoinPath(output_dir, "second.d88"), {second}, error) ||
		!WriteBinary(JoinPath(output_dir, "multi.d88"), {first, second}, error) ||
		!WriteText(JoinPath(output_dir, "pair.m3u"),
			"# XM8 generated fixture\nsingle.d88#0\nsecond.d88#0\n", error)) {
		return false;
	}
	return true;
}

} // namespace D88Fixture
