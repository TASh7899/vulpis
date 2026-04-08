#include "texture_registry.h"
#include <cmath>
#include <cpr/response.h>
#include <cpr/ssl_options.h>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <iterator>
#include <memory>
#include <queue>
#include <string>
#include <system_error>
#include <unordered_map>
#include <iostream>
#include <mutex>
#include <vector>
#include <thread>
#include <cpr/cpr.h>

#include "../../scripting/regsitry.h"
#include "../../lua.hpp"
#include "../../components/system/pathUtils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../../../third_party/stb_image/stb_image.h"

#define STB_DXT_IMPLEMENTATION
#include "../../../third_party/stb_image/stb_dxt.h"

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif

namespace TextureRegistry {

  struct UploadTask {
    GLuint targetID;
    GLuint pbo;
    int width;
    int height;
    size_t dataSize;
    unsigned char* rawPixels = nullptr;
    bool isCompressed = false;
  };

  struct TextureInfo {
    GLuint id;
    int refCount;
    int width;
    int height;
    bool isLoaded;
  };
  static std::unordered_map<std::string, TextureInfo> textureCache;
  static std::unordered_map<GLuint, std::string> idToPath;

  static std::vector<UploadTask> uploadQueue;
  static std::mutex queueMutex;

  void CompressToDXT5(const unsigned char* rgba, int w, int h, unsigned char* out_dxt) {
    int blocksW = (w + 3) / 4;
    int blocksH = (h + 3) / 4;
    for (int y = 0; y < blocksH; ++y) {
      for (int x = 0; x < blocksW; ++x) {
        unsigned char block[64];
        for (int by = 0; by < 4; ++by) {
          int py = std::min(y * 4 + by, h - 1);
          for (int bx = 0; bx < 4; ++bx) {
            int px = std::min(x * 4 + bx, w - 1);
            int srcIdx = (py * w + px) * 4;
            int dstIdx = (by * 4 + bx) * 4;
            std::memcpy(&block[dstIdx], &rgba[srcIdx], 4);
          }
        }
        stb_compress_dxt_block(&out_dxt[(y * blocksW + x) * 16], block, 1, STB_DXT_NORMAL);
      }
    }
  }

  GLuint GetTexture(const std::string &path) {
    if (path.empty()) return 0;
    auto it = textureCache.find(path);
    if (it != textureCache.end()) {
      it->second.refCount++;
      return it->second.id;
    }

    if (path.find("http://") == 0 || path.find("https://") == 0 ) {
      GLuint textureID;
      glGenTextures(1, &textureID);
      glBindTexture(GL_TEXTURE_2D, textureID);
      unsigned char emptyPixel[] = {0, 0, 0, 0};
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, emptyPixel);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

      textureCache[path] = {textureID, 1, 0, 0, false};
      idToPath[textureID] = path;

      // 1. Hash the URL to create a safe cache filename
      size_t urlHash = std::hash<std::string>{}(path);
      std::string safeFilename = std::to_string(urlHash) + ".vtex";
      
      namespace fs = std::filesystem;
      fs::path cachePath = Vulpis::getCacheDirectory() / "web_textures" / safeFilename;
      std::string cachePathStr = cachePath.string();

      // 2. Check if we already downloaded it in a previous session
      bool needsDownload = !fs::exists(cachePath);

      std::thread([textureID, path, cachePathStr, needsDownload]() {
        if (needsDownload) {
          cpr::Session session;
          session.SetUrl(cpr::Url{path});

#if defined (__linux__)
          const char* certPaths[] = {
            "/etc/ssl/certs/ca-certificates.crt", 
            "/etc/pki/tls/certs/ca-bundle.crt",   
            "/etc/ssl/ca-bundle.pem"              
          };
          for (const char* cert : certPaths) {
            if (std::filesystem::exists(cert)) {
              session.SetOption(cpr::Ssl(cpr::ssl::CaInfo{cert}));
              break;
            }
          }
#endif
          cpr::Response r = session.Get();

          if (r.status_code == 200) {
            std:std::filesystem::create_directories(std::filesystem::path(cachePathStr).parent_path());
            std::ofstream out(cachePathStr, std::ios::binary);

            if (out) {
              out.write(r.text.data(), r.text.size());
            }

            int tw, th, tc;
            unsigned char* pixels = stbi_load_from_memory(
              reinterpret_cast<unsigned char*>(r.text.data()),
              r.text.size(), &tw, &th, &tc, 4);

            if (pixels) {
              std::lock_guard<std::mutex> lock(queueMutex);
              uploadQueue.push_back({textureID, 0, tw, th, 0, pixels, false});
            }
          } else {
            std::cerr << "[Texture Download Failed] Status: " << r.status_code << " Error: " << r.error.message << std::endl;
          }
        } else {
          // Load instantly from the local hard drive cache!
          std::ifstream file(cachePathStr, std::ios::binary | std::ios::ate);
          if (file) {
            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<unsigned char> buffer(size);
            if (file.read(reinterpret_cast<char*>(buffer.data()), size)) {
              int tw, th, tc;
              unsigned char* pixels = stbi_load_from_memory(buffer.data(), size, &tw, &th, &tc, 4);

              if (pixels) {
                std::lock_guard<std::mutex> lock(queueMutex);
                uploadQueue.push_back({textureID, 0, tw, th, 0, pixels, false});
              }
            }
          }
        }
      }).detach();

      return textureID;
    }


    namespace fs = std::filesystem;
    fs::path rootPath = Vulpis::getProjectRoot();
    fs::path originalFileFullPath = rootPath / path;
    std::string relativePath = path;
    if (relativePath.find("assets/") == 0) relativePath = relativePath.substr(7);
    else if (relativePath.find("./assets/") == 0) relativePath = relativePath.substr(9);

    fs::path cachePath = Vulpis::getCacheDirectory() / "local_baked" / relativePath;
    cachePath.replace_extension(".vtex");

    std::string origPath = originalFileFullPath.string();
    std::string cachePathStr = cachePath.string();

    int w = 0, h = 0;
    bool needsBake = false;

    std::error_code ec;
    if (fs::exists(cachePath, ec) && fs::exists(originalFileFullPath, ec)) {
      if (fs::last_write_time(originalFileFullPath, ec) > fs::last_write_time(cachePath, ec)) {
        needsBake = true;
      }
    }

    if (!needsBake) {
      std::ifstream file(cachePath, std::ios::binary);
      if (file) {
        file.read(reinterpret_cast<char*>(&w), sizeof(int));
        file.read(reinterpret_cast<char*>(&h), sizeof(int));
        file.close();
      } else {
        int comp;
        if (!stbi_info(origPath.c_str(), &w, &h, &comp)) {
          std::cerr << "TEXTURE ERROR: Asset missing entirely: " << origPath << std::endl;
          return 0;
        }
        needsBake = true;
      }
    }

    if (needsBake) {
      int comp;
      if (!stbi_info(origPath.c_str(), &w, &h, &comp)) {
        std::cerr << "TEXTURE ERROR: Asset missing entirely: " << origPath << std::endl;
        return 0;
      }
    }

    size_t dataSize = ((w + 3) / 4) * ((h + 3) / 4) * 16;

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    unsigned char emptyPixel[] = {0, 0, 0, 0};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, emptyPixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    textureCache[path] = {textureID, 1, w, h, false};
    idToPath[textureID] = path;

    GLuint pbo;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, nullptr, GL_STREAM_DRAW);
    void* mappedPtr = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, dataSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    std::thread([textureID, pbo, mappedPtr, dataSize, w, h, cachePathStr, origPath, needsBake]() {
        if (needsBake) {
        int tw, th, tc;
        unsigned char* pixels = stbi_load(origPath.c_str(), &tw, &th, &tc, STBI_rgb_alpha);
        if (pixels && mappedPtr) {
        CompressToDXT5(pixels, tw, th, static_cast<unsigned char*>(mappedPtr));
        stbi_image_free(pixels); 

        std::filesystem::create_directories(std::filesystem::path(cachePathStr).parent_path());
        std::ofstream out(cachePathStr, std::ios::binary);
        if (out) {
        out.write(reinterpret_cast<char*>(&tw), sizeof(int));
        out.write(reinterpret_cast<char*>(&th), sizeof(int));
        out.write(reinterpret_cast<const char*>(mappedPtr), dataSize);
        }

        std::lock_guard<std::mutex> lock(queueMutex);
        uploadQueue.push_back({textureID, pbo, tw, th, dataSize}); 
        } else {
        if (pixels) stbi_image_free(pixels);
        std::lock_guard<std::mutex> lock(queueMutex);
        uploadQueue.push_back({0, pbo, 0, 0, 0});
        }
        } else {
          std::ifstream t_file(cachePathStr, std::ios::binary);
          if (t_file && mappedPtr) {
            t_file.seekg(8);
            t_file.read(reinterpret_cast<char*>(mappedPtr), dataSize);
            std::lock_guard<std::mutex> lock(queueMutex);
            uploadQueue.push_back({textureID, pbo, w, h, dataSize});
          } else {
            std::lock_guard<std::mutex> lock(queueMutex);
            uploadQueue.push_back({0, pbo, 0, 0, 0});
          }
        }
    }).detach();

    return textureID;
  }

  void ReleaseTexture(GLuint textureID) {
    if (textureID == 0) return;
    auto pathIt = idToPath.find(textureID);
    if (pathIt == idToPath.end()) return;
    std::string path = pathIt->second;
    auto cacheIt = textureCache.find(path);
    if (cacheIt != textureCache.end()) {
      cacheIt->second.refCount--;
      if (cacheIt->second.refCount <= 0) {
        glDeleteTextures(1, &cacheIt->second.id);
        textureCache.erase(cacheIt);
        idToPath.erase(pathIt);
      }
    }
  }

  bool ProcessUploads() {
    std::lock_guard<std::mutex> lock(queueMutex);
    if (uploadQueue.empty()) return false;

    for (const auto& task : uploadQueue ) {
      if (task.targetID != 0 && idToPath.find(task.targetID) != idToPath.end()) {

        std::string path = idToPath[task.targetID];
        textureCache[path].width = task.width;
        textureCache[path].height = task.height;
        textureCache[path].isLoaded = true;

        glBindTexture(GL_TEXTURE_2D, task.targetID);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);

        if (task.pbo != 0) {
          glBindBuffer(GL_PIXEL_UNPACK_BUFFER, task.pbo);
          glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
          int paddedW = ((task.width + 3) / 4) * 4;
          int paddedH = ((task.height + 3) / 4) * 4;
          glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, paddedW, paddedH, 0, task.dataSize, nullptr);
          glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
          glDeleteBuffers(1, &task.pbo);
        } else if (task.rawPixels != nullptr) {
          if (task.isCompressed) {
            int paddedW = ((task.width + 3) / 4) * 4;
            int paddedH = ((task.height + 3) / 4) * 4;
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, paddedW, paddedH, 0, task.dataSize, task.rawPixels);
            delete[] task.rawPixels;
          } else {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, task.width, task.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, task.rawPixels);
            stbi_image_free(task.rawPixels);
          }
        }
      } else {
        if (task.pbo != 0) {
          glBindBuffer(GL_PIXEL_UNPACK_BUFFER, task.pbo);
          glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
          glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
          glDeleteBuffers(1, &task.pbo);
        }
        if (task.rawPixels != nullptr) {
          if (task.isCompressed) delete [] task.rawPixels; 
          else stbi_image_free(task.rawPixels);
        }
      }
    }
    uploadQueue.clear();
    return true;
  }

  void Cleanup() {
    for (const auto& pair : textureCache) {
      glDeleteTextures(1, &pair.second.id);
    }
    textureCache.clear();
    idToPath.clear();
  }

  void GetTextureDimensions(GLuint textureID, int &w, int &h) {
    w = 0; h = 0;
    auto pathIt = idToPath.find(textureID);
    if (pathIt != idToPath.end()) {
      auto cacheIt = textureCache.find(pathIt->second);
      if (cacheIt != textureCache.end()) {
        w = cacheIt->second.width;
        h = cacheIt->second.height;
      }
    }
  }

  // ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
  // ╏ LUA BINDING: CLEAR DISK & MEMORY CACHE                  ╏
  // ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
  int l_clearCache(lua_State* L) {
    namespace fs = std::filesystem;
    fs::path cachePath = Vulpis::getCacheDirectory();
    // Safety check: Make sure path exists and is long enough to not be root ("/")
    if (fs::exists(cachePath) && cachePath.string().length() > 10) {
      try {
        fs::remove_all(cachePath); 
        fs::create_directories(cachePath); 
        // Wipe the OpenGL memory cache so we don't hold dead pointers!
        Cleanup(); 
        std::cout << "[Vulpis] Disk cache successfully cleared: " << cachePath << std::endl;
        lua_pushboolean(L, true);
      } catch (const std::exception& e) {
        std::cerr << "[Vulpis] Cache Wipe Failed: " << e.what() << std::endl;
        lua_pushboolean(L, false);
      }
    } else {
      std::cerr << "[Vulpis] Invalid or missing cache directory." << std::endl;
      lua_pushboolean(L, false);
    }
    return 1;
  }

  AutoRegisterLua regClearCache("clearCache", l_clearCache);

  bool IsTextureLoaded(GLuint textureID) {
    if (textureID == 0) return false;
    auto pathIt = idToPath.find(textureID);
    if (pathIt != idToPath.end()) {
      auto cacheIt = textureCache.find(pathIt->second);
      if (cacheIt != textureCache.end()) {
        return cacheIt->second.isLoaded;
      }
    }
    return false;
  }

  bool IsValidTexture(GLuint textureID) {
    if (textureID == 0) return false;
    return idToPath.find(textureID) != idToPath.end();
  }

}
