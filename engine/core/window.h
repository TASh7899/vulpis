#pragma once

#include <SDL2/SDL.h>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

// Window management functions
SDL_Window* createWindow(const char* title, int width, int height);
void destroyWindow(SDL_Window* window);

// Lua bindings
int l_setWindowSize(lua_State* L);
void registerWindowFunctions(lua_State* L, SDL_Window* window);

