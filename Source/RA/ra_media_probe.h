#ifndef XM8_RA_MEDIA_PROBE_H
#define XM8_RA_MEDIA_PROBE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Xm8Ra {

struct D88MediaInfo {
	std::string md5;
	uint64_t size = 0;
	int banks = 0;
	std::vector<std::string> bank_names;
	std::vector<std::string> bank_md5s;
};

bool HashPc8800File(const char *path, char hash[33]);
bool HashPc8800Buffer(const uint8_t *buffer, size_t buffer_size, char hash[33]);
bool ProbeD88File(const char *path, D88MediaInfo *info, std::string *error);

} // namespace Xm8Ra

#endif
