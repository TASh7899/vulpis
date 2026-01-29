#include "font_registry.h"
#include "../../text/font.h"
#include <filesystem>
#include <lauxlib.h>
#include <lua.h>
#include <string>
#include <unordered_map>
#include <iostream>
#include "../../system/pathUtils.h"

static std::unordered_map<std::string, FontConfig> g_fontRegistry;

const FontConfig* GetFontConfig(const std::string& alias) {
  auto it = g_fontRegistry.find(alias);
  if (it != g_fontRegistry.end()) {
    return &it->second;
  }

  return nullptr;
}

static void RebuildFallbackList() {
  Font::ResetFallback();
  for (const auto& [alias, config] : g_fontRegistry) {
    if (config.fallback) {
      std::string fullpath = config.path;
      auto [id, fontptr] = UI_LoadFont(fullpath, config.size);
      if (fontptr) {
        Font::AddFallback(fontptr);
      }
    }
  }
}

void RegisterFontInternal(const std::string& alias, const std::string& path, int size, bool fallback) {
  FontConfig conf = {path, size, fallback};
  g_fontRegistry[alias] = conf;
}

int l_register_font_family(lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);

  lua_pushnil(L);
  while (lua_next(L, 1) != 0) {
    if (lua_isstring(L, -2) && lua_istable(L, -1)) {
      std::string alias = lua_tostring(L, -2);

      std::string path = "";
      int size = 12;
      bool Fallback = false;

      lua_getfield(L, -1, "path");
      if (lua_isstring(L, -1)) path = lua_tostring(L, -1);
      else {
        std::cerr << "Error: Font alias '" << alias << "' missing path.\n";
        lua_pop(L, 1);
        lua_pop(L, 1);
        continue;
      }
      lua_pop(L, 1);
      
      lua_getfield(L, -1, "size");
        if (lua_isnumber(L, -1)) {
          size = lua_tonumber(L, -1);
        }
        lua_pop(L, 1);

      lua_getfield(L, -1, "fallback");
      if (lua_isboolean(L, -1)) {
        Fallback = lua_toboolean(L, -1);
      }
      lua_pop(L, 1);

      RegisterFontInternal(alias, path, size, Fallback);
    }
    lua_pop(L, 1);
  }

  RebuildFallbackList();
  return 0;
}

int l_unregister_font_family(lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);

  int len = lua_rawlen(L, 1);
  for (int i = 1; i <= len; ++i) {
    lua_rawgeti(L, 1, i);
    if (lua_isstring(L, -1)) {
      std::string alias = lua_tostring(L, -1);
      g_fontRegistry.erase(alias);
    }
    lua_pop(L, 1);
  }

  RebuildFallbackList();
  return 0;

}


void LoadFontConfig(lua_State* L) {
  namespace fs = std::filesystem;
  fs::path configPath = Vulpis::getExecutableDir().parent_path() / "config" / "VP_FONT_CONFIG.lua";

  if (!fs::exists(configPath)) {
    std::cout << "Info: Config file not found at " << configPath << ". Skipping.\n";
    return;
  }

  if (luaL_dofile(L, configPath.string().c_str()) != LUA_OK) {
    std::cerr << "Error loading font config: " << lua_tostring(L, -1) << std::endl;
    lua_pop(L, 1);
    return;
  }

  if (lua_istable(L, -1)) {
    l_register_font_family(L);
  }

  lua_pop(L, 1);
}

