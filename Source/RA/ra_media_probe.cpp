#include "ra_media_probe.h"

#include "rc_consoles.h"
#include "rc_hash.h"

#include <array>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>

namespace Xm8Ra {
namespace {

constexpr size_t kD88HeaderSize = 0x2b0;
constexpr size_t kD88NameSize = 16;

uint32_t ReadLe32(const std::array<uint8_t, kD88HeaderSize>& header,
	size_t offset)
{
	return static_cast<uint32_t>(header[offset]) |
		(static_cast<uint32_t>(header[offset + 1]) << 8) |
		(static_cast<uint32_t>(header[offset + 2]) << 16) |
		(static_cast<uint32_t>(header[offset + 3]) << 24);
}

std::string ReadBankName(const std::array<uint8_t, kD88HeaderSize>& header)
{
	size_t length = 0;
	while (length < kD88NameSize && header[length] != 0) {
		length++;
	}
	return std::string(reinterpret_cast<const char*>(header.data()), length);
}

} // namespace

bool HashPc8800File(const char *path, char hash[33])
{
	if (path == nullptr || hash == nullptr) {
		return false;
	}
	return rc_hash_generate_from_file(hash, RC_CONSOLE_PC8800, path) != 0;
}

bool ProbeD88File(const char *path, D88MediaInfo *info, std::string *error)
{
	if (error != nullptr) {
		error->clear();
	}
	if (path == nullptr || info == nullptr) {
		if (error != nullptr) {
			*error = "invalid argument";
		}
		return false;
	}

	std::ifstream stream(path, std::ios::binary | std::ios::ate);
	if (!stream.is_open()) {
		if (error != nullptr) {
			*error = "cannot open D88 file";
		}
		return false;
	}

	const std::streamoff end = stream.tellg();
	if (end < 0) {
		if (error != nullptr) {
			*error = "cannot determine D88 file size";
		}
		return false;
	}
	const uint64_t file_size = static_cast<uint64_t>(end);
	stream.seekg(0, std::ios::beg);

	D88MediaInfo probed;
	probed.size = file_size;

	uint64_t offset = 0;
	for (;;) {
		if (offset + kD88HeaderSize > file_size) {
			break;
		}

		std::array<uint8_t, kD88HeaderSize> header{};
		stream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
		stream.read(reinterpret_cast<char*>(header.data()),
			static_cast<std::streamsize>(header.size()));
		if (stream.gcount() != static_cast<std::streamsize>(header.size())) {
			break;
		}

		if ((header[0x23] != 0x00) ||
			(header[0x22] != 0x00) ||
			(header[0x21] != 0x02) ||
			((header[0x20] & 0x0f) != 0x00)) {
			if (header[0x21] != 0x00) {
				break;
			}
		}

		const uint32_t image_size = ReadLe32(header, 0x1c);
		if (image_size < kD88HeaderSize) {
			break;
		}
		if (offset > std::numeric_limits<uint64_t>::max() - image_size) {
			break;
		}

		probed.bank_names.push_back(ReadBankName(header));
		probed.banks++;
		offset += image_size;
	}

	if (probed.banks == 0) {
		if (error != nullptr) {
			*error = "invalid D88 structure";
		}
		return false;
	}

	char hash[33] = {};
	if (!HashPc8800File(path, hash)) {
		if (error != nullptr) {
			*error = "cannot hash D88 file";
		}
		return false;
	}
	probed.md5 = hash;

	*info = probed;
	return true;
}

} // namespace Xm8Ra
