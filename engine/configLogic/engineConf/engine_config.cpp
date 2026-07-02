#include "engine_config.h"
#include <iostream>
#include <filesystem>
#include <lauxlib.h>
#include <lua.h>
#include <zip.h>
#include <sys/types.h>
#include "../../components/system/pathUtils.h"

static EngineConfig g_config;

const EngineConfig& GetEngineConfig() {
  return g_config;
}

bool loadConfigSafely(lua_State* L, const std::string& configName) {
    namespace fs = std::filesystem;
    fs::path configDiskPath = Vulpis::resolvePath(fs::path("config") / configName);

    if (fs::exists(configDiskPath)) {
      return luaL_dofile(L, configDiskPath.string().c_str()) == LUA_OK;
    }

    std::string vpakPath = Vulpis::getVpakPath().string();
    int zipErr = 0;
    zip_t* vpak = zip_open(vpakPath.c_str(), ZIP_RDONLY, &zipErr);

    if (vpak) {
      std::string vfsInternalPath = "config/" + configName;
      zip_stat_t stat;

      if (zip_stat(vpak, vfsInternalPath.c_str(), 0, &stat) == 0) {
        zip_file_t* f = zip_fopen(vpak, vfsInternalPath.c_str(), 0);
        if (f) {
          std::vector<char> buffer(stat.size);
          zip_fread(f, buffer.data(), stat.size);
          zip_fclose(f);
          zip_close(vpak);

          // Load from the in-memory buffer
          return (luaL_loadbuffer(L, buffer.data(), buffer.size(), configName.c_str()) == LUA_OK 
              && lua_pcall(L, 0, LUA_MULTRET, 0) == LUA_OK);
        }
      }
      zip_close(vpak);
    }

    return false;
}
void loadEngineConfig(lua_State *L) {
  g_config = EngineConfig();
  int top = lua_gettop(L);

  if (!loadConfigSafely(L, "VP_ENGINE_CONFIG.lua")) {
    std::cout << "Info: Engine Config file not found at disk or vpak. Using defaults.\n";
    lua_settop(L, top);
    return;
  }

  lua_getglobal(L, "enable_default_fonts");
  if (lua_isboolean(L, -1)) {
    g_config.enableDefaultFonts = lua_toboolean(L, -1);
  }
  lua_pop(L, 1);

  lua_getglobal(L, "enable_stats_logging");
  if (lua_isboolean(L, -1)) {
    g_config.enableStatsLogging = lua_toboolean(L, -1);
  }
  lua_pop(L, 1);

  lua_settop(L, top);
}
