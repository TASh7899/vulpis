#include "../../lua.hpp"
#include "../../scripting/regsitry.h"
#include "pathUtils.h"
#include "secure_storage.h"
#include <lua.h>
#include <string>

// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ LUA BINDINGS FOR SECURE STORAGE                         ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛

int l_saveSecure(lua_State* L) {
  std::string filename = luaL_checkstring(L, 1);
  std::string data = luaL_checkstring(L, 2);
  bool success = Vulpis::SecureStorage::Save(filename, data);
  lua_pushboolean(L, success);
  return 1;
}

int l_loadSecure(lua_State* L) {
  std::string filename = luaL_checkstring(L, 1);
  std::string data;
  if (Vulpis::SecureStorage::Load(filename, data)) {
    lua_pushstring(L, data.c_str());
    return 1;
  }
  lua_pushnil(L); // Return nil if file doesn't exist or decryption failed
  return 1;
}

int l_deleteSecure(lua_State* L) {
  std::string filename = luaL_checkstring(L, 1);
  bool success = Vulpis::SecureStorage::Delete(filename);
  lua_pushboolean(L, success);
  return 1;
}


// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ LUA BINDINGS FOR PATH UTILITIES ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛

int l_getProjectRoot(lua_State* L) {
  std::string root = Vulpis::getProjectRoot();
  lua_pushstring(L, root.c_str());
  return 1;
}

int l_getAssetPath(lua_State* L) {
  std::string relPath = luaL_optstring(L, 1, ""); 
  std::string fullPath = Vulpis::getAssetPath(relPath);
  lua_pushstring(L, fullPath.c_str());
  return 1;
}

int l_getCacheDir(lua_State* L) {
  std::string cache = Vulpis::getCacheDirectory().string();
  lua_pushstring(L, cache.c_str());
  return 1;
}

AutoRegisterLua regGetProjectRoot("getProjectRoot", l_getProjectRoot);
AutoRegisterLua regGetAssetPath("getAssetPath", l_getAssetPath);
AutoRegisterLua regGetCacheDir("getCacheDir", l_getCacheDir);

AutoRegisterLua regSaveSecure("saveSecure", l_saveSecure);
AutoRegisterLua regLoadSecure("loadSecure", l_loadSecure);
AutoRegisterLua regDeleteSecure("deleteSecure", l_deleteSecure);
