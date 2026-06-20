#ifndef XM8_RA_BUILD_INFO_H
#define XM8_RA_BUILD_INFO_H

#include <cstddef>
#include <cstdint>

namespace Xm8RaBuildInfo {

uint32_t RcheevosVersion();
const char *RcheevosVersionString();
const char *SqliteVersion();
int SqliteVersionNumber();
const char *SqliteSourceId();
int SqliteThreadsafe();
int SqliteCompileOptionUsed(const char *option);
bool ProbeImage(const uint8_t *data, size_t size, int *width, int *height,
	int *components);

} // namespace Xm8RaBuildInfo

#endif
