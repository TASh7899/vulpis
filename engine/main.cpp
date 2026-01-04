#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>
#include <iostream>
#include <string>
#include <sstream>

#include "components/ui/ui.h"
#include "components/layout/layout.h"
#include "components/core/window.h"
#include "components/state/state.h"
#include "components/input/input.h"
#include "components/vdom/vdom.h"

int main(int argc, char* argv[]) {
  //prompt for window size before sdl initialization 
  int windowWidth = 0;
  int windowHeight = 0;
  
  std::cout << "Width (press Enter for fullscreen): ";
  std::string widthInput;
  std::getline(std::cin, widthInput);
  
  std::cout << "Height (press Enter for fullscreen): ";
  std::string heightInput;
  std::getline(std::cin, heightInput);
  
  
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cout << "SDL Init Failed: " << SDL_GetError() << std::endl;
    return 1;
  }
  
  
  bool shouldMaximize = false;
  
  
  if (widthInput.empty() && heightInput.empty()) {
    SDL_Rect usableBounds;
    if (SDL_GetDisplayUsableBounds(0, &usableBounds) == 0) {
      windowWidth = usableBounds.w;
      windowHeight = usableBounds.h;
      shouldMaximize = true;
      std::cout << "Using fullscreen (usable bounds): " << windowWidth << "x" << windowHeight << std::endl;
    } else {
      std::cout << "Failed to get display bounds, using default 1920x1080" << std::endl;
      windowWidth = 1920;
      windowHeight = 1080;
    }
  } else {
    
    try {
      if (!widthInput.empty()) {
        windowWidth = std::stoi(widthInput);
      }
      if (!heightInput.empty()) {
        windowHeight = std::stoi(heightInput);
      }
      
      
      if (widthInput.empty()) {
        windowWidth = 1920;
      }
      if (heightInput.empty()) {
        windowHeight = 1080;
      }
      
      if (windowWidth <= 0 || windowHeight <= 0) {
        std::cout << "Invalid dimensions, using default 1920x1080" << std::endl;
        windowWidth = 1920;
        windowHeight = 1080;
      }
    } catch (const std::exception& e) {
      std::cout << "Invalid input, using default 1920x1080" << std::endl;
      windowWidth = 1920;
      windowHeight = 1080;
    }
  }

  SDL_Window* window = createWindow("Vulpis window", windowWidth, windowHeight, shouldMaximize);

  if (!window) {
    std::cout << "Window Creation Failed: " << SDL_GetError() << std::endl;
    SDL_Quit();
    return 1;
  }
  
  
  int actualWidth, actualHeight;
  SDL_GetWindowSize(window, &actualWidth, &actualHeight);
  windowWidth = actualWidth;
  windowHeight = actualHeight;

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

  
  lua_State* L = luaL_newstate();
  luaL_openlibs(L);
  
  registerWindowFunctions(L, window);
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

  int winW = windowWidth;
  int winH = windowHeight;

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

  freeTree(root);
  SDL_DestroyRenderer(renderer);
  destroyWindow(window);
  SDL_Quit();
  lua_close(L);;
  return 0;
}

