#include "texture_registry.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
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
#include "../../scripting/regsitry.h"
#include "../../lua.hpp"
#include "../../components/system/pathUtils.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../../../third_party/stb_image/stb_image.h"


namespace TextureRegistry {

  struct UploadTask {
    GLuint targetID;
    GLuint pbo;
    int width;
    int height;
  };

  struct TextureInfo {
    GLuint id;
    int refCount;
  };
  static std::unordered_map<std::string, TextureInfo> textureCache;
  static std::unordered_map<GLuint, std::string> idToPath;

  static std::vector<UploadTask> uploadQueue;
  static std::mutex queueMutex;


  GLuint GetTexture(const std::string &path) {
    if (path.empty()) return 0;

    auto it = textureCache.find(path);
    if (it != textureCache.end()) {
      it->second.refCount++;
      return it->second.id;
    }

    namespace fs = std::filesystem;
    std::string relativePath = path;
    if (relativePath.find("assets/") == 0) {
      relativePath = relativePath.substr(7);
    }

    fs::path cachePath = std::filesystem::path(Vulpis::getProjectRoot()) / "baked" / relativePath;
    cachePath.replace_extension(".vtex");

    std::string origPath = Vulpis::getProjectRoot() + "/" + path;
    std::string cachePathStr = cachePath.string();

    int w = 0, h = 0;
    bool needsBake = false;

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

    size_t dataSize = w * h * 4;

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    unsigned char emptyPixel[] = {0, 0, 0, 0};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, emptyPixel);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    textureCache[path] = {textureID, 1};
    idToPath[textureID] = path;

    GLuint pbo;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);

    glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, nullptr, GL_STREAM_DRAW);
    void* mappedPtr = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, dataSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    std::thread([textureID, pbo, mappedPtr, dataSize, w, h, cachePathStr, origPath, needsBake]() { // <-- Added missing variables here!
      if (needsBake) {
        int tw, th, tc;
        unsigned char* pixels = stbi_load(origPath.c_str(), &tw, &th, &tc, STBI_rgb_alpha);

        if (pixels && mappedPtr) {
          std::memcpy(mappedPtr, pixels, dataSize);

          std::filesystem::create_directories(std::filesystem::path(cachePathStr).parent_path());
          std::ofstream out(cachePathStr, std::ios::binary);
          if (out) {
            out.write(reinterpret_cast<char*>(&tw), sizeof(int));
            out.write(reinterpret_cast<char*>(&th), sizeof(int));
            out.write(reinterpret_cast<char*>(pixels), dataSize);
          }
          stbi_image_free(pixels);

          std::lock_guard<std::mutex> lock(queueMutex);
          uploadQueue.push_back({textureID, pbo, w, h});
        } else {
          if (pixels) stbi_image_free(pixels);
          std::lock_guard<std::mutex> lock(queueMutex);
          uploadQueue.push_back({0, pbo, 0, 0});
        }
      } else {
        std::ifstream t_file(cachePathStr, std::ios::binary);
        if (t_file && mappedPtr) {
          t_file.seekg(8);
          t_file.read(reinterpret_cast<char*>(mappedPtr), dataSize);
          std::lock_guard<std::mutex> lock(queueMutex);
          uploadQueue.push_back({textureID, pbo, w, h});
        } else {
          std::lock_guard<std::mutex> lock(queueMutex);
          uploadQueue.push_back({0, pbo, 0, 0});
        }
      }
    }).detach();

      return textureID;
    }

    void ReleaseTexture(GLuint textureID) {
      if (textureID == 0) return;
      auto pathIt = idToPath.find(textureID);
      if (pathIt == idToPath.end()) {
        return;
      }
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
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, task.pbo);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

        if (task.targetID != 0 && idToPath.find(task.targetID) != idToPath.end()) {
          glBindTexture(GL_TEXTURE_2D, task.targetID);
          glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, task.width, task.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        }

        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glDeleteBuffers(1, &task.pbo);

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

  }
