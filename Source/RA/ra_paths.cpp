#include "ra_paths.h"

namespace Xm8Ra {

std::string RootFromSettingDir(const char *setting_dir)
{
	if (setting_dir == nullptr || setting_dir[0] == '\0') {
		return std::string();
	}

	std::string root = setting_dir;
	if (!root.empty() && root.back() != '/' && root.back() != '\\') {
		root += '/';
	}
	root += "ra";
	return root;
}

} // namespace Xm8Ra
