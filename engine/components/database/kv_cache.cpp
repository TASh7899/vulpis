#include "kv_cache.h"
#include <functional>
#include <leveldb/db.h>
#include <iostream>
#include <leveldb/options.h>
#include <leveldb/status.h>
#include "../../scripting/regsitry.h"
#include "../../components/system/pathUtils.h"

std::unique_ptr<leveldb::DB> KVCache::db = nullptr;

bool KVCache::Init(const std::string &directoryName) {
  std::string fullpath = (Vulpis::getCacheDirectory() / directoryName).string();
  leveldb::Options options;
  options.create_if_missing = true;
  options.write_buffer_size = 4 * 1024 * 1034; // 4mb
  
  leveldb::DB* rawDb = nullptr;
  leveldb::Status status = leveldb::DB::Open(options, fullpath, &rawDb);
  
  if (!status.ok()) {
    std::cerr << "[KVCache Error] Unable to open LevelDB: " << status.ToString() << std::endl;
    return false;
  }

  db.reset(rawDb);
  return true;
}

void KVCache::ShutDown() {
  db.reset();
}

bool KVCache::Set(const std::string &key, const std::string &value) {
  if (!db) return false;
  leveldb::WriteOptions writeOptions;
  writeOptions.sync = false;
  return db->Put(writeOptions, key, value).ok();
}

std::string KVCache::Get(const std::string &key, bool &success) {
  if (!db) { success = false; return " "; }
  std::string value;
  leveldb::Status s = db->Get(leveldb::ReadOptions(), key, &value);
  success = s.ok();
  return value;
}

bool KVCache::Delete(const std::string& key) {
    if (!db) return false;
    return db->Delete(leveldb::WriteOptions(), key).ok();
}


// LUA BINDING
int l_kvSet(lua_State* L) {
    std::string key = luaL_checkstring(L, 1);
    std::string val = luaL_checkstring(L, 2);
    lua_pushboolean(L, KVCache::Set(key, val));
    return 1;
}

int l_kvGet(lua_State* L) {
    std::string key = luaL_checkstring(L, 1);
    bool success;
    std::string val = KVCache::Get(key, success);
    if (success) {
        lua_pushstring(L, val.c_str());
        return 1;
    }
    lua_pushnil(L);
    return 1;
}

int l_kvDelete(lua_State* L) {
    std::string key = luaL_checkstring(L, 1);
    lua_pushboolean(L, KVCache::Delete(key));
    return 1;
}

AutoRegisterLua regKvSet("kvSet", l_kvSet);
AutoRegisterLua regKvGet("kvGet", l_kvGet);
AutoRegisterLua regKvDelete("kvDelete", l_kvDelete);


