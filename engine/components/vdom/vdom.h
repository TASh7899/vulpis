#pragma once
#include "../ui/ui.h"
#include "../../lua.hpp"
#include "../color/color.h"


namespace VDOM {
  void reconcile(lua_State *L, Node *current, int idx);
  void updateCallback(lua_State* L, int tableIdx, const char* key, int& ref);
}
