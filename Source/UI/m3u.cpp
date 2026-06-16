#include "m3u.h"

#include <fstream>
#include <string>
#include <cctype>

namespace {

std::string Trim(const std::string& value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string ParentDirectory(const std::string& path)
{
    const std::string::size_type slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return "";
    }
    return path.substr(0, slash + 1);
}

std::string ResolveM3UEntry(const std::string& baseDir,
                            const std::string& entry)
{
    if (entry.empty()) {
        return entry;
    }

    if (entry[0] == '/') {
        return entry;
    }

    if (entry.size() >= 2 &&
        std::isalpha(static_cast<unsigned char>(entry[0])) &&
        entry[1] == ':') {
        return entry;
    }

    return baseDir + entry;
}

} // namespace


M3UResult LoadM3U(const std::string& path)
{
    M3UResult result;
    result.success = false;

    std::ifstream file(path);
    if (!file.is_open()) {
        result.error = "unable to open m3u: " + path;
        return result;
    }

    // 👇 get base directory ONCE
    const std::string baseDir = ParentDirectory(path);

    std::string line;
    while (std::getline(file, line)) {
        line = Trim(line);

        if (line.empty()) {
            continue;
        }

        if (line[0] == '#') {
            continue;
        }

        // 👇 resolve relative paths properly
        std::string resolved = ResolveM3UEntry(baseDir, line);

        result.entries.push_back(resolved);
    }

    result.success = true;
    return result;
}
