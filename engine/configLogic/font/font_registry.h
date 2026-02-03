#pragma once
#include <string>
#include "../lua.hpp"


struct FontConfig {
  std::string path;
  int size;
  bool fallback;
};

void LoadFontConfig(lua_State* L);
const FontConfig* GetFontConfig(const std::string& alias);

int l_register_font_family(lua_State* L);
int l_unregister_font_family(lua_State* L);


