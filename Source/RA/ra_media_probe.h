#ifndef XM8_RA_MEDIA_PROBE_H
#define XM8_RA_MEDIA_PROBE_H

#include <cstdint>
#include <string>
#include <vector>

namespace Xm8Ra {

struct D88MediaInfo {
	std::string md5;
	uint64_t size = 0;
	int banks = 0;
	std::vector<std::string> bank_names;
};

bool HashPc8800File(const char *path, char hash[33]);
bool ProbeD88File(const char *path, D88MediaInfo *info, std::string *error);

} // namespace Xm8Ra

#endif
