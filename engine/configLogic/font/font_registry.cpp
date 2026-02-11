#include "font_registry.h"
#include "../../components/text/font.h"
#include <algorithm>
#include <cctype>
#include <cinttypes>
#include <filesystem>
#include <lua.h>
#include <string>
#include <unordered_map>
#include <iostream>
#include "../../components/system/pathUtils.h"
#include "../engineConf/engine_config.h"
#include "../../scripting/regsitry.h"

static std::unordered_map<std::string, FontConfig> g_fontRegistry;
namespace fs = std::filesystem;

const FontConfig* GetFontConfig(const std::string& alias) {
  auto it = g_fontRegistry.find(alias);
  if (it != g_fontRegistry.end()) {
    return &it->second;
  }

  return nullptr;
}

static bool g_canLoadTextures = false;

static void RebuildFallbackList() {

  if (!g_canLoadTextures) return;

  Font::ResetFallback();
  for (const auto& [alias, config] : g_fontRegistry) {
    if (config.fallback) {
      std::string fullpath = config.path;
      auto [id, fontptr] = UI_LoadFont(fullpath, config.size, FONT_STYLE_NORMAL);
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

void RegisterFontsFromTable(lua_State* L, int tableIndex) {
  if (tableIndex < 0) tableIndex = lua_gettop(L) + tableIndex + 1;
  luaL_checktype(L, tableIndex, LUA_TTABLE);
  
  lua_pushnil(L);
  while (lua_next(L, tableIndex) != 0) {
    // Stack: key (-2), value (-1)
    if (lua_isstring(L, -2) && lua_istable(L, -1)) {
      std::string alias = lua_tostring(L, -2);
      
      std::string path = "";
      std::string aliasOf = "";
      int size = -1;
      int fallback = -1;

      // --- PATH ---
      lua_getfield(L, -1, "path");
      if (lua_isstring(L, -1)) path = lua_tostring(L, -1);
      lua_pop(L, 1); // Always pop

      // --- ALIAS_OF ---
      lua_getfield(L, -1, "alias_of");
      if (lua_isstring(L, -1)) aliasOf = lua_tostring(L, -1);
      lua_pop(L, 1); // FIX: Always pop, even if it wasn't a string!

      // --- SIZE ---
      lua_getfield(L, -1, "size");
      if (lua_isnumber(L, -1)) size = (int)lua_tonumber(L, -1);
      lua_pop(L, 1);

      // --- FALLBACK ---
      lua_getfield(L, -1, "fallback");
      if (lua_isboolean(L, -1)) fallback = lua_toboolean(L, -1); 
      lua_pop(L, 1);

      // --- REGISTRATION LOGIC ---
      auto existingIt = g_fontRegistry.find(alias);
      bool exists = (existingIt != g_fontRegistry.end());

      if (!path.empty()) {
        int finalSize = (size > 0) ? size : 16;
        bool finalFb = (fallback == 1);
        RegisterFontInternal(alias, path, finalSize, finalFb);
        exists = true;
      }
      else if (!aliasOf.empty()) {
        auto sourceIt = g_fontRegistry.find(aliasOf);
        if (sourceIt != g_fontRegistry.end()) {
          FontConfig copy = sourceIt->second;
          if (size > 0) copy.size = size;
          if (fallback != -1) copy.fallback = (fallback == 1);
          g_fontRegistry[alias] = copy;
          exists = true;
        } else {
          std::cerr << "Error: Cannot alias '" << alias << "' to '" << aliasOf << "': Source not found.\n";
        }
      }
      else if (exists) {
        if (size > 0) g_fontRegistry[alias].size = size;
        if (fallback != -1) g_fontRegistry[alias].fallback = (fallback == 1);
      }

      if (exists) {
        lua_getfield(L, -1, "variants");
        if (lua_istable(L, -1)) {
          FontConfig& config = g_fontRegistry[alias];
          
          lua_pushnil(L);
          while(lua_next(L, -2) != 0) {
            if(lua_isstring(L, -2)) { 
              std::string vKey = lua_tostring(L, -2);
              VariantConfig vConf;

              if(lua_isstring(L, -1)) { 
                std::string val = lua_tostring(L, -1);

                auto aliasIt = g_fontRegistry.find(val);
                if (aliasIt != g_fontRegistry.end()) {
                  vConf.path = aliasIt->second.path;
                } else {
                  vConf.path = val;
                }
                vConf.size = -1;

              } else if (lua_istable(L, -1)) {

                lua_getfield(L, -1, "alias_of");
                if (lua_isstring(L, -1)) {
                  std::string target = lua_tostring(L, -1);
                  auto aliasIt = g_fontRegistry.find(target);
                  if (aliasIt != g_fontRegistry.end()) {
                    vConf.path = aliasIt->second.path;
                  }
                }
                lua_pop(L, 1);

                if (vConf.path.empty()) {
                  lua_getfield(L, -1, "path");
                  if(lua_isstring(L, -1)) vConf.path = lua_tostring(L, -1);
                  lua_pop(L, 1);
                }

                lua_getfield(L, -1, "size");
                if(lua_isnumber(L, -1)) vConf.size = (int)lua_tonumber(L, -1);
                else vConf.size = -1;
                lua_pop(L, 1);
              }

              if(!vConf.path.empty()) {
                config.variants[vKey] = vConf;
              }
            }
            lua_pop(L, 1);
          }
        }
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);
  }
}


void AutoRegisterAllFonts() {
  std::string assetRoot = Vulpis::getAssetPath("");
  fs::path rootPath(assetRoot);

  if (!fs::exists(rootPath) || !fs::is_directory(rootPath)) {
    std::cerr << "[FontRegistry] Warning: Assets directory not found at " << rootPath << std::endl;
    return;
  }

  for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
    if (entry.is_regular_file()) {
      std::string ext = entry.path().extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

      if (ext == ".ttf" || ext == ".otf") {
        std::string alias = entry.path().stem().string();
        std::string relativePath = fs::relative(entry.path(), rootPath).string();

        if (GetFontConfig(alias) == nullptr) {
          RegisterFontInternal(alias, relativePath,  16, false);
        }
      }
    }
  }

}



void LoadFontConfig(lua_State* L) {
  g_canLoadTextures = true;
  loadEngineConfig(L);

  AutoRegisterAllFonts();

  namespace fs = std::filesystem;
  fs::path configPath = Vulpis::getExecutableDir().parent_path() / "config" / "VP_FONT_CONFIG.lua";

  if (fs::exists(configPath)) {
    int top = lua_gettop(L);
    if (luaL_dofile(L, configPath.string().c_str()) != LUA_OK) {
      std::cerr << "Error loading font config" << lua_tostring(L, -1) << std::endl;
    } else {
      if (lua_gettop(L) > top && lua_istable(L, -1)) {
        RegisterFontsFromTable(L, -1);
      }
    }
    lua_settop(L, top);
  } else {
    std::cout << "Info: Font Config file not found at " << configPath << ". Skipping.\n";
  }

  const EngineConfig& engineConf = GetEngineConfig();
  if (engineConf.enableDefaultFonts) {
    if (g_fontRegistry.find("default") == g_fontRegistry.end()) {
      RegisterFontInternal("default", "builtin/NotoSans/NotoSans-Regular.ttf", 16, true);
      std::cout << "Info: Default font (Noto Sans) loaded\n";
    }
  }

  RebuildFallbackList();
}



int l_update_font_config(lua_State* L) {
  const char* alias = luaL_checkstring(L, 1);
  luaL_checktype(L, 2, LUA_TTABLE);
  auto it = g_fontRegistry.find(alias);

  lua_getfield(L, 2, "path");
  const char* newPath = lua_tostring(L, -1);
  lua_pop(L, 1);

  if (it != g_fontRegistry.end()) {
    // --- CASE 1: UPDATE EXISTING FONT ---
    FontConfig& config = it->second;
    if (newPath) config.path = newPath; // Override path if requested

    lua_getfield(L, 2, "size");
    if (lua_isnumber(L, -1)) config.size = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "fallback");
    if (lua_isboolean(L, -1)) config.fallback = lua_toboolean(L, -1);
    lua_pop(L, 1);

  } else {
    // --- CASE 2: REGISTER NEW ALIAS ---
    if (!newPath) {
      return luaL_error(L, "Font '%s' not found. To register a new font, you must provide a 'path'.", alias);
    }

    int size = 16;
    bool fallback = false;

    lua_getfield(L, 2, "size");
    if (lua_isnumber(L, -1)) size = (int)lua_tonumber(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 2, "fallback");
    if (lua_isboolean(L, -1)) fallback = lua_toboolean(L, -1);
    lua_pop(L, 1);

    RegisterFontInternal(alias, newPath, size, fallback);
  }

  // Important: Refresh the engine's fallback cache immediately
  RebuildFallbackList(); 
  return 0;
}


static AutoRegisterLua _reg_update_font("update_font_config", l_update_font_config);

