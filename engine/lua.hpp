#pragma once

// Ensure we do not build against a Lua DLL import layout when linking statically
#ifdef LUA_BUILD_AS_DLL
#undef LUA_BUILD_AS_DLL
#endif

// Add this line to tell the compiler we are using Lua as a static library
#ifndef LUA_CORE
#define LUA_LIB
#endif

extern "C" {
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}
