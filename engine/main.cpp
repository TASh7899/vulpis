#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>
#include <iostream>
#include <string>

#include "components/ui/ui.h"
#include "components/layout/layout.h"
#include "components/state/state.h"
#include "components/input/input.h"
#include "components/vdom/vdom.h"

int main(int argc, char* argv[]) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cout << "SDL Init Failed: " << SDL_GetError() << std::endl;
    return 1;
  }

  int winW = 800;
  int winH = 600;

  SDL_Window* window = SDL_CreateWindow(
    "Vulpis window",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    winW,
    winH,
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
  registerStateBindings(L);

  lua_getglobal(L, "package");
  lua_getfield(L, -1, "path");
  std::string paths = lua_tostring(L, -1);
  lua_pop(L, 1);
  

paths =
    "../?.lua;"
    "../?/init.lua;"

    "../utils/?.lua;"
    "../utils/?/init.lua;"

    "../src/?.lua;"
    "../src/?/init.lua;"

    "../lua/?.lua;"
    "../lua/?/init.lua;"
    + paths;
  lua_pushstring(L, paths.c_str());
  lua_setfield(L, -2, "path");
  lua_pop(L, 1);

  if (luaL_dofile(L, "../src/app.lua") != LUA_OK) {
    std::cout << "Lua Error: " << lua_tostring(L, -1) << std::endl;
    lua_close(L);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  lua_getglobal(L, "App");
  if (!lua_isfunction(L, -1)) {
      std::cerr << "Error: Global 'App' function not found in app.lua" << std::endl;
      return 1;
  }

  // 2. Call App()
  if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
      std::cerr << "Error calling App(): " << lua_tostring(L, -1) << std::endl;
      lua_pop(L, 1);
      return 1;
  }

  // 3. Now the return value is on stack
  if (!lua_istable(L, -1)) {
      std::cerr << "Error: App() did not return a table" << std::endl;
      lua_pop(L, 1);
      return 1;
  }

  Node* root = buildNode(L, -1);
  lua_pop(L, 1);

  Layout::LayoutSolver* solver = Layout::createYogaSolver();
  solver->solve(root, {winW, winH});
  root->isLayoutDirty = false;
  root->isPaintDirty = false;

  bool running = true;
  SDL_Event event;

  while (running) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = false;
      }

      Input::handleEvent(L, event, root);

      if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
        winW = event.window.data1;
        winH = event.window.data2;
        root->makeLayoutDirty();
      }
    }

    if (StateManager::instance().isDirty()) {
      lua_getglobal(L, "App");
      if (!lua_isfunction(L, -1)) {
        std::cerr << "Error: App is not a function during reconcile" << std::endl;
        lua_pop(L, 1);
      } else {

        if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
          std::cerr << "Error calling App(): "
            << lua_tostring(L, -1) << std::endl;
          lua_pop(L, 1);
        } else {

          VDOM::reconcile(L, root, -1);
          lua_pop(L, 1);
        }
      }

      StateManager::instance().clearDirty();
    }

    if (root->isLayoutDirty) {
      solver->solve(root, {winW, winH});
      root->isLayoutDirty = false;
    }

    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);

    renderNode(renderer, root);
    SDL_RenderPresent(renderer);
  }

  freeTree(L, root);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  lua_close(L);;
  return 0;
}

