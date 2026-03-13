#include <complex>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <algorithm>
#include <system_error>

#define STB_IMAGE_IMPLEMENTATION
#include "../../third_party/stb_image/stb_image.h"

namespace fs = std::filesystem;

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

  std::ofstream file(cachePath, std::ios::binary);
  if (file) {
    file.write(reinterpret_cast<const char*>(&w), sizeof(int));
    file.write(reinterpret_cast<const char*>(&h), sizeof(int));
    file.write(reinterpret_cast<const char*>(pixels), w * h * 4);
  }
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

