#include "ra_media_probe.h"

#include "ra_file_util.h"

#include "rc_consoles.h"
#include "rc_hash.h"

#include <array>
#include <cstring>
#include <limits>
#include <vector>

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
	std::vector<uint8_t> data;
	return ReadRaFile(path, &data, std::numeric_limits<size_t>::max(), nullptr) &&
		!data.empty() && HashPc8800Buffer(data.data(), data.size(), hash);
}

bool HashPc8800Buffer(const uint8_t *buffer, size_t buffer_size, char hash[33])
{
	if (buffer == nullptr || buffer_size == 0 || hash == nullptr) {
		return false;
	}
	return rc_hash_generate_from_buffer(hash, RC_CONSOLE_PC8800, buffer,
		buffer_size) != 0;
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

	std::vector<uint8_t> file_data;
	if (!ReadRaFile(path, &file_data, std::numeric_limits<size_t>::max(), error)) {
		return false;
	}
	const uint64_t file_size = static_cast<uint64_t>(file_data.size());

	D88MediaInfo probed;
	probed.size = file_size;

	uint64_t offset = 0;
	for (;;) {
		if (offset + kD88HeaderSize > file_size) {
			break;
		}

		std::array<uint8_t, kD88HeaderSize> header{};
		std::memcpy(header.data(), file_data.data() + static_cast<size_t>(offset),
			header.size());

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
		if (offset + image_size > file_size) {
			if (error != nullptr) {
				*error = "cannot read D88 bank";
			}
			return false;
		}
		char bank_hash[33] = {};
		if (!HashPc8800Buffer(file_data.data() + static_cast<size_t>(offset),
			image_size, bank_hash)) {
			if (error != nullptr) {
				*error = "cannot hash D88 bank";
			}
			return false;
		}
		probed.bank_md5s.push_back(bank_hash);
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
	if (!HashPc8800Buffer(file_data.data(), file_data.size(), hash)) {
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
