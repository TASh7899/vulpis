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
    std::filesystem::path getCacheDirectory();
    void setProjectRoot(const std::string& path);

    void setRootDirectoryOverride(const std::filesystem::path& path);
    void setAssetDirectoryOverride(const std::filesystem::path& path);

    std::filesystem::path resolvePath(const std::filesystem::path& relativePath);

    void setVpakOverride(const std::filesystem::path& path);
    std::filesystem::path getVpakPath();
}


