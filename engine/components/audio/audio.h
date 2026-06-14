#pragma once
#include <lua.hpp>

namespace Audio {
    // Engine lifecycle
    void Init();
    void ShutDown();

    // Lua Bindings
    int PlaySound(lua_State* L);
    int PlayMusic(lua_State *L);
    int PauseMusic(lua_State* L);
      int ResumeMusic(lua_State* L);
}
