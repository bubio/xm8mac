#include "ra_build_info.h"

#include "rc_version.h"
#include "sqlite3.h"
#include "stb_image.h"

#include <climits>

namespace Xm8RaBuildInfo {

uint32_t RcheevosVersion()
{
	return rc_version();
}

const char *RcheevosVersionString()
{
	return rc_version_string();
}

const char *SqliteVersion()
{
	return sqlite3_libversion();
}

int SqliteVersionNumber()
{
	return sqlite3_libversion_number();
}

const char *SqliteSourceId()
{
	return sqlite3_sourceid();
}

int SqliteThreadsafe()
{
	return sqlite3_threadsafe();
}

int SqliteCompileOptionUsed(const char *option)
{
	return sqlite3_compileoption_used(option);
}

bool ProbeImage(const uint8_t *data, size_t size, int *width, int *height,
	int *components)
{
	if (data == nullptr || size > static_cast<size_t>(INT_MAX)) {
		return false;
	}
	return stbi_info_from_memory(data, static_cast<int>(size), width, height,
		components) != 0;
}

} // namespace Xm8RaBuildInfo
