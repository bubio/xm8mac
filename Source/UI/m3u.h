#pragma once

#include <string>
#include <vector>

struct M3UResult {
    bool success;
    std::string error;
    std::vector<std::string> entries;
};

bool IsM3UPath(const std::string& path);
M3UResult LoadM3U(const std::string& path);
