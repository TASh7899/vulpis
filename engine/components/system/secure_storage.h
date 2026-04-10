#pragma once
#include <string>

namespace Vulpis {
  class SecureStorage {
    public:
      static bool Save(const std::string& filename, const std::string& data);

      static bool Load(const std::string& filename, std::string& outData);

      static bool Delete(const std::string& filename);
  };
}
