#include "engine_config.h"
#include <iostream>
#include <filesystem>
#include <lauxlib.h>
#include <lua.h>
#include <sys/types.h>
#include "../../components/system/pathUtils.h"

static EngineConfig g_config;

const EngineConfig& GetEngineConfig() {
  return g_config;
}

void loadEngineConfig(lua_State *L) {
  namespace fs = std::filesystem;
  fs::path configPath = Vulpis::getExecutableDir().parent_path() / "config" / "VP_ENGINE_CONFIG.lua";

  g_config = EngineConfig();

  if (!fs::exists(configPath)) {
    return;
  }

  int top = lua_gettop(L);

  if (luaL_dofile(L, configPath.string().c_str()) != LUA_OK) {
    std::cerr << "Error loading engine config " << lua_tostring(L, -1) << std::endl;
    lua_settop(L, top);
    return;
  }

  lua_getglobal(L, "enable_default_fonts");
  if (lua_isboolean(L, -1)) {
    g_config.enableDefaultFonts = lua_toboolean(L, -1);
  }
  lua_pop(L, 1);
  lua_settop(L, top);
}
