#include <iostream>
#include <filesystem>
#include <string>
#include <algorithm>
#include <cstring>
#include <zip.h>

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
                    std::memcpy(&block[(by * 4 + bx) * 4], &rgba[(py * w + px) * 4], 4);
                }
            }
            stb_compress_dxt_block(&out_dxt[(y * blocksW + x) * 16], block, 1, STB_DXT_NORMAL);
        }
    }
}

// Ensures paths inside the ZIP always use forward slashes (Unix style)
std::string sanitizeZipPath(const std::string& path) {
    std::string sanitized = path;
    std::replace(sanitized.begin(), sanitized.end(), '\\', '/');
    return sanitized;
}

void packRawFile(zip_t* archive, const fs::path& filePath, const std::string& internalPath) {
  std::cout << "  Packing raw: " << filePath.filename() << "...\n";
  zip_source_t* source = zip_source_file(archive, filePath.string().c_str(), 0, -1);
  if (source) {
    zip_file_add(archive, sanitizeZipPath(internalPath).c_str(), source, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8);
  }
}

void bakeAndPackTexture(zip_t* archive, const fs::path& imgPath, const std::string& internalPath) {
    int w, h, c;
    std::cout << "  Baking: " << imgPath.filename() << "...\n";
    unsigned char* pixels = stbi_load(imgPath.string().c_str(), &w, &h, &c, STBI_rgb_alpha);

    if (!pixels) {
        std::cerr << "  [!] Failed to load image: " << imgPath << "\n";
        return;
    }

    size_t dxtSize = ((w + 3) / 4) * ((h + 3) / 4) * 16;
    size_t headerSize = sizeof(int) * 2;
    size_t totalSize = headerSize + dxtSize;

    unsigned char* outData = (unsigned char*)malloc(totalSize);
    
    std::memcpy(outData, &w, sizeof(int));
    std::memcpy(outData + sizeof(int), &h, sizeof(int));
    
    CompressToDXT5(pixels, w, h, outData + headerSize);
    stbi_image_free(pixels);

    fs::path finalInternalPath = fs::path(internalPath).replace_extension(".vtex");

    zip_source_t* source = zip_source_buffer(archive, outData, totalSize, 1);
    if (source) {
        zip_file_add(archive, sanitizeZipPath(finalInternalPath.string()).c_str(), source, ZIP_FL_OVERWRITE | ZIP_FL_ENC_UTF_8);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: asset_baker <assets_dir> <staged_src_dir> <output.vpak>\n";
        return 1;
    }

    fs::path assetsDir = argv[1];
    fs::path stagedSrcDir = argv[2];
    fs::path outVpak = argv[3];

    int err = 0;
    zip_t* archive = zip_open(outVpak.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &err);
    if (!archive) {
        std::cerr << "Failed to create VFS archive: " << outVpak << "\n";
        return 1;
    }

    std::cout << "--- Building VFS Archive: " << outVpak.filename() << " ---\n";

    if (fs::exists(assetsDir)) {
        for (const auto& entry : fs::recursive_directory_iterator(assetsDir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                std::string relPath = "assets/" + fs::relative(entry.path(), assetsDir).string();

                if (ext == ".jpg" || ext == ".jpeg" || ext == ".png") {
                    bakeAndPackTexture(archive, entry.path(), relPath);
                } else {
                    packRawFile(archive, entry.path(), relPath);
                }
            }
        }
    }

    if (fs::exists(stagedSrcDir)) {
        for (const auto& entry : fs::recursive_directory_iterator(stagedSrcDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".luac") {
              std::string relPath = "src/" + fs::relative(entry.path(), stagedSrcDir).string();
                packRawFile(archive, entry.path(), relPath);
            }
        }
    }

    zip_close(archive);
    std::cout << "VFS Packaging Complete!\n";
    return 0;
}
