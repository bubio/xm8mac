#pragma once

#include <string>
#include <vector>

struct M3UResult {
    bool success;
    std::string error;
    std::vector<std::string> entries;
};

M3UResult LoadM3U(const std::string& path);
