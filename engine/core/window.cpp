#include "window.h"
#include <iostream>

//global window pointer for lua callbacks
static SDL_Window* g_window = nullptr;

SDL_Window* createWindow(const char* title, int width, int height) {
  SDL_Window* window = SDL_CreateWindow(
    title,
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    width,
    height,
    SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
  );

  if (!window) {
    std::cout << "Window Creation Failed: " << SDL_GetError() << std::endl;
    return nullptr;
  }

  g_window = window;
  return window;
}

void destroyWindow(SDL_Window* window) {
  if (window) {
    SDL_DestroyWindow(window);
    g_window = nullptr;
  }
}

//lua function: setwindowsize(width, height)
int l_setWindowSize(lua_State* L) {
  if (!g_window) {
    lua_pushstring(L, "Window not initialized");
    lua_error(L);
    return 0;
  }

  int width = luaL_checkinteger(L, 1);
  int height = luaL_checkinteger(L, 2);

  if (width <= 0 || height <= 0) {
    lua_pushstring(L, "Window size must be positive");
    lua_error(L);
    return 0;
  }

  SDL_SetWindowSize(g_window, width, height);
  // Recenter the window after resizing
  SDL_SetWindowPosition(g_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  return 0;
}

void registerWindowFunctions(lua_State* L, SDL_Window* window) {
  g_window = window;
  
  // Register setWindowSize as a global function
  lua_register(L, "setWindowSize", l_setWindowSize);
}

