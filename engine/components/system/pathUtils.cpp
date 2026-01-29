#include "pathUtils.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#endif



namespace Vulpis {
  namespace fs = std::filesystem;

  // retrives the absolute path to the directory containing the executable
  // uses os-specific API to ensure reliability regardless of current working directory
  fs::path getExecutableDir() {
#if defined (__linux__)
    char buffer[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (count != -1) {
      buffer[count] = '\0';
      return fs::path(buffer).parent_path();
    }
    return fs::current_path();

#elif defined(_WIN32)
    std::vector<char> buffer(MAX_PATH);
    DWORD size = GetModuleFileNameA(NULL, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (size == buffer.size()) { // Handle paths longer than MAX_PATH
      buffer.resize(buffer.size() * 2);
      size = GetModuleFileNameA(NULL, buffer.data(), static_cast<DWORD>(buffer.size()));
    }
    return fs::path(buffer.data()).parent_path();

#elif defined(__APPLE__)
    char buffer[1024];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
      return fs::canonical(buffer).parent_path();
    }
    return fs::current_path();

#endif
  }

  // getAssetPath combines relative path with executable directory to get absolute path
  std::string getAssetPath(const std::string& relativePath) {
    static const fs::path exeDir = getExecutableDir();

    std::vector<fs::path> candidates;

    // 1. Check ../assets/ (Development: build folder is usually one level deep)
    candidates.push_back(exeDir.parent_path() / "assets" / relativePath);

    // 2. Check ./assets/ (Release: assets next to exe)
    candidates.push_back(exeDir / "assets" / relativePath);

    // 3. Check direct path (User supplied "assets/..." manually)
    candidates.push_back(exeDir / relativePath);
    
    // 4. Check ../ (Directly in root)
    candidates.push_back(exeDir.parent_path() / relativePath);

    for (const auto& path : candidates) {
      if (fs::exists(path)) {
        return path.string();
      }
    }

    // Return the first candidate as default for error reporting
    return candidates[0].string();
  }
}



