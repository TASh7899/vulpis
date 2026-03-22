#pragma once
#include <string>
#include "../../lua.hpp"

struct EngineConfig {
  bool enableDefaultFonts = true;
  bool enableStatsLogging = false;
};

const EngineConfig& GetEngineConfig();
void loadEngineConfig(lua_State* L);
