#pragma once
#include <string>
#include "../../lua.hpp"

struct EngineConfig {
  bool enableDefaultFonts = true;
};

const EngineConfig& GetEngineConfig();
void loadEngineConfig(lua_State* L);
