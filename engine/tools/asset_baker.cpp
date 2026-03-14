#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <algorithm>
#include <system_error>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include "../../third_party/stb_image/stb_image.h"

#define STB_DXT_IMPLEMENTATION
#include "../../third_party/stb_image/stb_dxt.h"

namespace fs = std::filesystem;

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
      stb_compress_dxt_block(&out_dxt[(y * blocksW + x) * 16], block, 1 /* has_alpha */, STB_DXT_NORMAL);
    }
  }
}

void bakeTextures(const fs::path& imgPath, const fs::path& outPath) {
  fs::path cachePath = outPath;
  cachePath.replace_extension(".vtex");

  std::error_code ec;
  if (fs::exists(cachePath, ec) && fs::exists(imgPath, ec) ) {
    if (fs::last_write_time(cachePath, ec) >= fs::last_write_time(imgPath, ec)) {
      return;
    }
  }

  fs::create_directories(cachePath.parent_path(), ec);

  int w, h, c;
  std::cout << "Baking: " << imgPath.filename() << "...\n";
  unsigned char* pixels = stbi_load(imgPath.string().c_str(), &w, &h, &c, STBI_rgb_alpha);

  if (!pixels) {
    std::cerr << "Failed to bake: " << imgPath << "\n";
    return;
  }

  size_t dataSize = ((w + 3) / 4) * ((h + 3) / 4) * 16;
  unsigned char* outData = new unsigned char[dataSize];
  CompressToDXT5(pixels, w, h, outData);

  std::ofstream file(cachePath, std::ios::binary);
  if (file) {
    file.write(reinterpret_cast<const char*>(&w), sizeof(int));
    file.write(reinterpret_cast<const char*>(&h), sizeof(int));
    file.write(reinterpret_cast<const char*>(outData), dataSize);
  }
  
  delete[] outData;
  stbi_image_free(pixels);
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: asset_baker <assets_dir> <baked_dir>\n";
    return 1;
  }

  fs::path assetsDir = argv[1];
  fs::path bakedDir = argv[2];

  if (!fs::exists(assetsDir) || !fs::is_directory(assetsDir)) {
    std::cerr << "Invalid assets directory: " << assetsDir << "\n";
    return 1;
  }

  for (const auto& entry : fs::recursive_directory_iterator(assetsDir)) {
    if (entry.is_regular_file()) {
      std::string ext = entry.path().extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

      if (ext == ".jpg" || ext == ".jpeg" || ext == ".png") {
        fs::path relativePath = fs::relative(entry.path(), assetsDir);
        fs::path outPath = bakedDir/relativePath;
        bakeTextures(entry.path(), outPath);
      }
    }
  }
  std::cout << "Asset baking complete.\n";
  return 0;
}
