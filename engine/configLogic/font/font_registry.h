#pragma once
#include <string>
#include <unordered_map>
#include "../lua.hpp"

struct VariantConfig {
  std::string path;
  int size = -1;
};

struct FontConfig {
  std::string path;
  int size;
  bool fallback;
  std::unordered_map<std::string, VariantConfig> variants;
};

void LoadFontConfig(lua_State* L);
const FontConfig* GetFontConfig(const std::string& alias);

int l_register_font_family(lua_State* L);
int l_unregister_font_family(lua_State* L);


