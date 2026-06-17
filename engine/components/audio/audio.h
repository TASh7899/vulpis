#pragma once
#include <lua.hpp>

namespace Audio {
    // Engine lifecycle
    void Init();
    void ShutDown();

    // sound and music function
    int PlaySound(lua_State* L);
    int PlayMusic(lua_State *L);
    int PauseMusic(lua_State* L);
    int ResumeMusic(lua_State* L);
    int StopMusic(lua_State *L);

    // Volume control
    int SetMasterVolume(lua_State* L);
    int SetMusicVolume(lua_State *L);
}
