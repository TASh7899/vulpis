#include "pathUtils.h"
#include <SDL2/SDL_filesystem.h>
#include <SDL2/SDL_stdinc.h>
#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#include <limits.h>
#endif


namespace {
    std::filesystem::path s_rootDirOverride;
    std::filesystem::path s_assetDirOverride;
    std::filesystem::path s_vpakOverride;
}


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


  std::string getProjectRoot() {
    char* basePathRaw = SDL_GetBasePath();
    std::string basePath = basePathRaw ? basePathRaw : "./";
    if (basePathRaw) SDL_free(basePathRaw);

    if (!basePath.empty() && (basePath.back() == '/' || basePath.back() == '\\')) {
      basePath.pop_back();
    }

    // Strip the 'debug' or 'release' folder
    size_t slashPos = basePath.find_last_of("\\/");
    if (slashPos != std::string::npos) {
      basePath = basePath.substr(0, slashPos);
    }

    // Strip the 'build' folder 
    slashPos = basePath.find_last_of("\\/");
    if (slashPos != std::string::npos) {
      basePath = basePath.substr(0, slashPos + 1); 
    }

    return basePath;
  }

// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ GET CACHE DIR FOR THE CURRENT OS ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛

  std::filesystem::path getCacheDirectory() {
    std::filesystem::path cacheDir;
    std::string appName = "Vulpis";
#if defined(_WIN32)
        // Windows: C:\Users\<User>\AppData\Local\Vulpis\Cache
        const char* localAppData = std::getenv("LOCALAPPDATA");
        if (localAppData) {
            cacheDir = std::filesystem::path(localAppData) / appName / "Cache";
        } else {
            cacheDir = std::filesystem::current_path() / ".vulpis_cache"; 
        }

#elif defined(__APPLE__)
        // macOS: /Users/<User>/Library/Caches/Vulpis
        const char* home = std::getenv("HOME");
        if (home) {
            cacheDir = std::filesystem::path(home) / "Library" / "Caches" / appName;
        } else {
            cacheDir = std::filesystem::current_path() / ".vulpis_cache";
        }

#elif defined(__linux__)
        // Linux: Respect XDG_CACHE_HOME first, fallback to ~/.cache/Vulpis
        const char* xdgCache = std::getenv("XDG_CACHE_HOME");
        if (xdgCache && std::string(xdgCache).length() > 0) {
            cacheDir = std::filesystem::path(xdgCache) / appName;
        } else {
            const char* home = std::getenv("HOME");
            if (home) {
                cacheDir = std::filesystem::path(home) / ".cache" / appName;
            } else {
                cacheDir = std::filesystem::current_path() / ".vulpis_cache"; 
            }
        }
#else
        // Ultimate Fallback
        cacheDir = std::filesystem::current_path() / ".vulpis_cache";
#endif
        std::filesystem::create_directories(cacheDir);
        return cacheDir;
  }


void setRootDirectoryOverride(const std::filesystem::path& path) {
      s_rootDirOverride = fs::weakly_canonical(path);
  }

  void setAssetDirectoryOverride(const std::filesystem::path& path) {
      s_assetDirOverride = fs::weakly_canonical(path);
  }


  std::filesystem::path resolvePath(const std::filesystem::path& relativePath) {
    // 1. Check for specific --assets override
    if (!s_assetDirOverride.empty() && !relativePath.empty() && *relativePath.begin() == "assets") {
      std::filesystem::path strippedPath;
      auto it = relativePath.begin();
      ++it;
      for (; it != relativePath.end(); ++it) {
        strippedPath /= *it;
      }

      std::filesystem::path assetPath = s_assetDirOverride / strippedPath;
      if (fs::exists(assetPath)) {
        return fs::weakly_canonical(assetPath);
      }
    }

    // 2. Check for global --root-dir override
    if (!s_rootDirOverride.empty()) {
      std::filesystem::path rootPath = s_rootDirOverride / relativePath;
      if (fs::exists(rootPath)) {
        return fs::weakly_canonical(rootPath);
      }
    }

    // 3. Fallback to dynamic executable path discovery
    fs::path exeDir = getExecutableDir();

    // Look exactly next to the executable
    fs::path primaryPath = exeDir / relativePath;
    if (fs::exists(primaryPath)) {
      return fs::weakly_canonical(primaryPath);
    }

    // Look one directory up (handles standard /bin/ or /release/ deployment)
    fs::path parentPath = exeDir.parent_path() / relativePath;
    if (fs::exists(parentPath)) {
      return fs::weakly_canonical(parentPath);
    }

    // Look two directories up (handles nested dev builds like /build/debug/)
    fs::path grandParentPath = exeDir.parent_path().parent_path() / relativePath;
    if (fs::exists(grandParentPath)) {
      return fs::weakly_canonical(grandParentPath);
    }

    // 4. Final Fallback to current working directory
    return fs::weakly_canonical(fs::current_path() / relativePath);
  }

  // Safely update getAssetPath to utilize the new robust resolution
  std::string getAssetPath(const std::string& relativePath) {
    fs::path requestedPath = fs::path("assets") / relativePath;
    return resolvePath(requestedPath).string();
  }

  void setVpakOverride(const std::filesystem::path& path) {
    s_vpakOverride = fs::weakly_canonical(path);
  }

  std::filesystem::path getVpakPath() {
    // If the user provided a --vpak flag, use it
    if (!s_vpakOverride.empty() && fs::exists(s_vpakOverride)) {
      return s_vpakOverride;
    }
    // Otherwise, fallback to the default path resolution
    return resolvePath("build/release/app.vpak");
  }

}



