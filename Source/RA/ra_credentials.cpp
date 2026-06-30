#include "ra_credentials.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace Xm8Ra {
namespace {

constexpr uint32_t kMagic = 0x31524d58; // "XMR1", little endian.
constexpr uint32_t kVersion = 1;
constexpr size_t kMaxUsernameBytes = 256;
constexpr size_t kMaxTokenBytes = 4096;

std::string JoinPath(const std::string& dir, const char *name)
{
	if (!dir.empty() && dir.back() == '/') {
		return dir + name;
	}
	return dir + "/" + name;
}

void PutLe32(std::vector<uint8_t> *bytes, uint32_t value)
{
	bytes->push_back(static_cast<uint8_t>(value & 0xff));
	bytes->push_back(static_cast<uint8_t>((value >> 8) & 0xff));
	bytes->push_back(static_cast<uint8_t>((value >> 16) & 0xff));
	bytes->push_back(static_cast<uint8_t>((value >> 24) & 0xff));
}

bool GetLe32(const std::vector<uint8_t>& bytes, size_t offset, uint32_t *value)
{
	if (offset + 4 > bytes.size()) {
		return false;
	}
	*value = static_cast<uint32_t>(bytes[offset]) |
		(static_cast<uint32_t>(bytes[offset + 1]) << 8) |
		(static_cast<uint32_t>(bytes[offset + 2]) << 16) |
		(static_cast<uint32_t>(bytes[offset + 3]) << 24);
	return true;
}

uint32_t Crc32(const uint8_t *data, size_t size)
{
	uint32_t crc = 0xffffffffU;
	for (size_t i = 0; i < size; i++) {
		crc ^= data[i];
		for (int bit = 0; bit < 8; bit++) {
			const uint32_t mask = 0U - (crc & 1U);
			crc = (crc >> 1) ^ (0xedb88320U & mask);
		}
	}
	return ~crc;
}

bool WriteFile0600(const std::string& path, const std::vector<uint8_t>& bytes,
	std::string *error)
{
#ifdef _WIN32
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	if (!stream.is_open()) {
		if (error != nullptr) {
			*error = "failed to create credentials temporary file";
		}
		return false;
	}
	stream.write(reinterpret_cast<const char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size()));
	if (!stream.good()) {
		if (error != nullptr) {
			*error = "failed to write credentials temporary file";
		}
		return false;
	}
	return true;
#else
	const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (fd < 0) {
		if (error != nullptr) {
			*error = std::strerror(errno);
		}
		return false;
	}

	size_t offset = 0;
	while (offset < bytes.size()) {
		const ssize_t written = write(fd, bytes.data() + offset,
			bytes.size() - offset);
		if (written < 0) {
			if (errno == EINTR) {
				continue;
			}
			if (error != nullptr) {
				*error = std::strerror(errno);
			}
			close(fd);
			return false;
		}
		offset += static_cast<size_t>(written);
	}

	if (fsync(fd) != 0) {
		if (error != nullptr) {
			*error = std::strerror(errno);
		}
		close(fd);
		return false;
	}
	if (close(fd) != 0) {
		if (error != nullptr) {
			*error = std::strerror(errno);
		}
		return false;
	}
	return true;
#endif
}

} // namespace

RaCredentialsStore::RaCredentialsStore(const std::string& ra_root)
	: ra_root_(ra_root)
{
}

std::string RaCredentialsStore::Path() const
{
	return JoinPath(ra_root_, "credentials.bin");
}

bool RaCredentialsStore::Save(const RaCredentials& credentials,
	std::string *error)
{
	if (credentials.username.size() > kMaxUsernameBytes) {
		if (error != nullptr) {
			*error = "RA username is too long";
		}
		return false;
	}
	if (credentials.token.size() > kMaxTokenBytes) {
		if (error != nullptr) {
			*error = "RA token is too long";
		}
		return false;
	}

	std::vector<uint8_t> bytes;
	PutLe32(&bytes, kMagic);
	PutLe32(&bytes, kVersion);
	PutLe32(&bytes, static_cast<uint32_t>(credentials.username.size()));
	PutLe32(&bytes, static_cast<uint32_t>(credentials.token.size()));
	bytes.insert(bytes.end(), credentials.username.begin(),
		credentials.username.end());
	bytes.insert(bytes.end(), credentials.token.begin(),
		credentials.token.end());
	const uint32_t crc = Crc32(bytes.data(), bytes.size());
	PutLe32(&bytes, crc);

	const std::string temporary = Path() + ".tmp";
	std::remove(temporary.c_str());
	if (!WriteFile0600(temporary, bytes, error)) {
		std::remove(temporary.c_str());
		return false;
	}
	if (std::rename(temporary.c_str(), Path().c_str()) != 0) {
		if (error != nullptr) {
			*error = std::strerror(errno);
		}
		std::remove(temporary.c_str());
		return false;
	}
	return true;
}

bool RaCredentialsStore::Load(RaCredentials *credentials,
	std::string *error) const
{
	if (credentials == nullptr) {
		return false;
	}
	credentials->username.clear();
	credentials->token.clear();

	std::ifstream stream(Path(), std::ios::binary);
	if (!stream.is_open()) {
		if (error != nullptr) {
			*error = "RA credentials are not stored";
		}
		return false;
	}
	const std::vector<uint8_t> bytes{
		std::istreambuf_iterator<char>(stream),
		std::istreambuf_iterator<char>()};
	if (bytes.size() < 20) {
		if (error != nullptr) {
			*error = "RA credentials file is truncated";
		}
		return false;
	}

	uint32_t magic = 0;
	uint32_t version = 0;
	uint32_t username_size = 0;
	uint32_t token_size = 0;
	uint32_t stored_crc = 0;
	if (!GetLe32(bytes, 0, &magic) ||
		!GetLe32(bytes, 4, &version) ||
		!GetLe32(bytes, 8, &username_size) ||
		!GetLe32(bytes, 12, &token_size) ||
		!GetLe32(bytes, bytes.size() - 4, &stored_crc)) {
		return false;
	}
	if (magic != kMagic || version != kVersion ||
		username_size > kMaxUsernameBytes ||
		token_size > kMaxTokenBytes) {
		if (error != nullptr) {
			*error = "RA credentials file has unsupported metadata";
		}
		return false;
	}
	const size_t expected_size = 16U + username_size + token_size + 4U;
	if (expected_size != bytes.size()) {
		if (error != nullptr) {
			*error = "RA credentials file has invalid length";
		}
		return false;
	}
	if (Crc32(bytes.data(), bytes.size() - 4) != stored_crc) {
		if (error != nullptr) {
			*error = "RA credentials file CRC mismatch";
		}
		return false;
	}

	const char *text = reinterpret_cast<const char *>(bytes.data() + 16);
	credentials->username.assign(text, text + username_size);
	credentials->token.assign(text + username_size,
		text + username_size + token_size);
	return true;
}

bool RaCredentialsStore::Delete(std::string *error)
{
	if (std::remove(Path().c_str()) == 0 || errno == ENOENT) {
		return true;
	}
	if (error != nullptr) {
		*error = std::strerror(errno);
	}
	return false;
}

void RaCredentialsStore::ClearSecret(RaCredentials *credentials) const
{
	if (credentials == nullptr) {
		return;
	}
	volatile char *token = credentials->token.empty() ? nullptr :
		&credentials->token[0];
	for (size_t i = 0; i < credentials->token.size(); i++) {
		token[i] = 0;
	}
	credentials->token.clear();
}

} // namespace Xm8Ra
