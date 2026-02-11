#include <lua.h>
#include <vector>
#include <string>

struct GlobalFuncEntry {
  std::string name;
  lua_CFunction func;
};

inline std::vector<GlobalFuncEntry>& GetGlobalFunctionRegistry() {
  static std::vector<GlobalFuncEntry> registry;
  return registry;
}

struct AutoRegisterLua {
  AutoRegisterLua(const std::string& name, lua_CFunction func) {
    GetGlobalFunctionRegistry().push_back({name, func});
  }
};

inline void RegisterGlobalFunctions(lua_State* L, const char* tableName = "vulpis") {
  lua_getglobal(L, tableName);
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setglobal(L, tableName);
  }

  for (const auto& entry : GetGlobalFunctionRegistry()) {
    lua_pushcfunction(L, entry.func);
    lua_setfield(L, -2, entry.name.c_str());
  }
  lua_pop(L, 1);
}


