#pragma once
#include <string>
#include <filesystem>
#include <climits>
#include <limits>
#include <string>
#include <iostream>
#include <sys/types.h>
#include <vector>

namespace Vulpis {
  namespace fs = std::filesystem;
    std::string getAssetPath(const std::string& relativePath);
    fs::path getExecutableDir();
    std::string getProjectRoot();
}

