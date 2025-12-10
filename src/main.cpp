#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>
#include <iostream>
#include <string>

#include "ui.h"

int main(int argc, char* argv[]) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cout << "SDL Init Failed: " << SDL_GetError() << std::endl;
    return 1;
  }

  SDL_Window* window = SDL_CreateWindow(
    "Vulpis window",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    800,
    600,
    SDL_WINDOW_SHOWN
  );

  if (!window) {
    std::cout << "Window Creation Failed: " << SDL_GetError() << std::endl;
    SDL_Quit();
    return 1;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(
    window,
    -1,
    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
  );

  if (!renderer) {
    std::cout << "Renderer Creation Failed: " << SDL_GetError() << std::endl;
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  // initializing lua
  lua_State* L = luaL_newstate();
  luaL_openlibs(L);

  lua_getglobal(L, "package");
  lua_getfield(L, -1, "path");
  std::string cur_path = lua_tostring(L, -1);
  cur_path += ";../lua/?.lua;../lua/?/init.lua";
  lua_pop(L, 1);
  lua_pushstring(L, cur_path.c_str());
  lua_setfield(L, -2, "path");
  lua_pop(L, 1);

  if (luaL_dofile(L, "../lua/app.lua") != LUA_OK) {
    std::cout << "Lua Error: " << lua_tostring(L, -1) << std::endl;
    lua_close(L);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  bool running = true;
  SDL_Event event;

  while (running) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = false;
      }
    }

    // Build UI tree from Lua
    lua_getglobal(L, "UI");
    if (!lua_istable(L, -1)) {
      lua_pop(L, 1);
      SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
      SDL_RenderClear(renderer);
      SDL_RenderPresent(renderer);
      continue;
    }

    Node* root = buildNode(L, -1);
    lua_pop(L, 1);

    // Layout
    measure(root);
    layout(root, 0, 0);

    // Render
    SDL_SetRenderDrawColor(renderer ,30,30,30,255);
    SDL_RenderClear(renderer);
    renderNode(renderer, root);
    SDL_RenderPresent(renderer);

    // Cleanup
    freeTree(root);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  lua_close(L);
  return 0;
}

