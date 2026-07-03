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

bool DecodeImageRgba(const uint8_t *data, size_t size, int *width,
	int *height, std::vector<uint8_t> *rgba)
{
	if (data == nullptr || size > static_cast<size_t>(INT_MAX) ||
		width == nullptr || height == nullptr || rgba == nullptr) {
		return false;
	}

	int components = 0;
	unsigned char *decoded = stbi_load_from_memory(data,
		static_cast<int>(size), width, height, &components, 4);
	if (decoded == nullptr || *width <= 0 || *height <= 0) {
		if (decoded != nullptr) {
			stbi_image_free(decoded);
		}
		return false;
	}

	const size_t decoded_size = static_cast<size_t>(*width) *
		static_cast<size_t>(*height) * 4U;
	rgba->assign(decoded, decoded + decoded_size);
	stbi_image_free(decoded);
	return true;
}

} // namespace Xm8RaBuildInfo
