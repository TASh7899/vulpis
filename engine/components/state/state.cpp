#include "state.h"
#include <lauxlib.h>
#include <lua.h>
#include <variant>

void pushStateValue(lua_State* L, const StateValue& val) {
  if (std::holds_alternative<int>(val)) {
    lua_pushinteger(L, std::get<int>(val));
  } else if (std::holds_alternative<float>(val)) {
    lua_pushnumber(L, std::get<float>(val));
  } else if (std::holds_alternative<std::string>(val)) {
    lua_pushstring(L, std::get<std::string>(val).c_str());
  } else if (std::holds_alternative<bool>(val)) {
    lua_pushboolean(L, std::get<bool>(val));
  } else {
    lua_pushnil(L);
  }
}

int l_setState(lua_State* L) {
  const char* key = luaL_checkstring(L, 1);

  StateValue val;
  int type = lua_type(L, 2);

  if (type == LUA_TNUMBER) {
    double d = lua_tonumber(L, 2);
    if (d == (int)d) val = (int)d;
    else val = (float)d;
  }

  else if (type == LUA_TSTRING) {
    val = std::string(lua_tostring(L, 2));
  }

  else if (type == LUA_TBOOLEAN) {
    val = (bool)lua_toboolean(L, 2);
  }

  else {
    val = 0;
  }

  StateManager::instance().setState(key, val);
  return 0;
}


int l_useState(lua_State* L) {
  const char* key = luaL_checkstring(L, 1);
  
  StateValue defVal = 0;
  if (lua_gettop(L) >= 2) {
    int type = lua_type(L, 2);
    if (type == LUA_TNUMBER) {
      double d = lua_tonumber(L, 2);
      if (d == (int)d) defVal = (int)d;
      else defVal = (float)d;
    } else if (type == LUA_TSTRING) {
      defVal = std::string(lua_tostring(L, 2));
    } else if (type == LUA_TBOOLEAN) {
      defVal = (bool)lua_toboolean(L, 2);
    }
  }

  StateValue current = StateManager::instance().getState(key, defVal);
  pushStateValue(L, current);
  return 1;
}

void registerStateBindings(lua_State* L) {
  lua_register(L, "setState", l_setState);
  lua_register(L, "useState", l_useState);
}




