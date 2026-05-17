#pragma once
#include <string>
#include <memory>
#include <lua.hpp>

namespace leveldb { class DB; };

class KVCache {
  public:
    static bool Init(const std::string& directoryName);
    static void ShutDown();

    static bool Set(const std::string& key, const std::string& value);
    static std::string Get(const std::string& key, bool& success);
    static bool Delete(const std::string& key);

  private:
    static std::unique_ptr<leveldb::DB> db;
};
