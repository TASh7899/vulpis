#include "secure_storage.h"
#include "pathUtils.h"
#include <fstream>
#include <ios>
#include <vector>
#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#else
#include <sys/stat.h>
#endif

namespace Vulpis {
  bool SecureStorage::Save(const std::string &filename, const std::string &data) {
    std::filesystem::path filepath = getCacheDirectory() / filename;

#if defined(_WIN32)
    DATA_BLOB DataIn;
    DATA_BLOB DataOut;
    DataIn.pbData = (BYTE*)data.data();
    DataIn.cbData = (DWORD)data.length();

    if (CryptProtectData(&DataIn, L"Vulpis Engine Secure Data", NULL, NULL, NULL, 0, &DataOut)) {
      std::ofstream outFile(filepath, std::ios::binary | std::ios::trunc);
      if (outFile) {
        outFile.write((char*)DataOut.pbData, DataOut.cbData);
        outFile.close();
        LocalFree(DataOut.pbData);
        return true;
      }
      LocalFree(DataOut.pbData);
    }
    return false;
#else
    // POSIX (Linux/macOS): Write file and apply strict 0600 permissions
    std::ofstream outFile(filepath, std::ios::binary | std::ios::trunc);
    if (outFile) {
      outFile.write(data.data(), data.length());
      outFile.close();

      // 0600: Read & Write only by the owner. Denies group and others.
      chmod(filepath.string().c_str(), S_IRUSR | S_IWUSR);
      return true;
    }
    return false;
#endif
  }

  bool SecureStorage::Load(const std::string &filename, std::string &outData) {
    std::filesystem::path filepath = getCacheDirectory() / filename;
    if (!std::filesystem::exists(filepath)) {
      return false;
    }

    std::ifstream inFile(filepath, std::ios::binary);
    if (!inFile) return false;

    std::vector<char> buffer((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
    if (buffer.empty()) return false;

#if defined(_WIN32)
    DATA_BLOB DataIn;
    DATA_BLOB DataOut;
    DataIn.pbData = (BYTE*)buffer.data();
    DataIn.cbData = (DWORD)buffer.size();

    if (CryptUnprotectData(&DataIn, NULL, NULL, NULL, NULL, 0, &DataOut)) {
      outData = std::string((char*)DataOut.pbData, DataOut.cbData);
      LocalFree(DataOut.pbData);
      return true;
    }
    return false;
#else
    outData = std::string(buffer.begin(), buffer.end());
    return true;
#endif
  }

  bool SecureStorage::Delete(const std::string& filename) {
    std::filesystem::path filepath = getCacheDirectory() / filename;
    std::error_code ec;
    return std::filesystem::remove(filepath, ec);
  }

}
