#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>
#include <SDL_stdinc.h>
#include <SDL_timer.h>
#include <iostream>
#include <ostream>
#include <string>

#include "components/renderer/commands.h"
#include "components/renderer/opengl_renderer.h"
#include "components/text/font.h"
#include "components/ui/ui.h"
#include "components/layout/layout.h"
#include "components/state/state.h"
#include "components/input/input.h"
#include "components/vdom/vdom.h"
#include "configLogic/font/font_registry.h"
#include "./scripting/regsitry.h"

int protected_buildNode(lua_State* L) {
  Node* root = buildNode(L, 1);
  lua_pushlightuserdata(L, root);
  return 1;
}

int protected_reconcile(lua_State* L) {
  Node* root = (Node*)lua_touserdata(L, 1);
  VDOM::reconcile(L, root, 2);
  return 0;
}

int main(int argc, char* argv[]) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cout << "SDL Init Failed: " << SDL_GetError() << std::endl;
    return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  int winW = 800;
  int winH = 600;

  SDL_Window* window = nullptr;

  // initializing lua
  lua_State* L = luaL_newstate();
  luaL_openlibs(L);
  registerStateBindings(L);
  UI_InitTypes(L);
  RegisterGlobalFunctions(L, "vulpis");
  AutoRegisterAllFonts();

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
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }


  //          ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
  //          ╏                    [WINDOW SECTION]                     ╏
  //          ╏               WINDOW CONFIG AND CREATION                ╏
  //          ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛

  std::string title = "Vulpis window";
  std::string mode = "";
  int w = 800;
  int h = 600;
  bool resizable = false;

  lua_getglobal(L, "Window");
  if (lua_isfunction(L, -1)) {

    if (lua_pcall(L, 0, 1, 0) == LUA_OK) {

      if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "title");

        if (lua_isstring(L, -1)) title = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "mode");
        if (lua_isstring(L, -1)) mode = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "w");

        if (lua_isnumber(L, -1)) w = (int)lua_tointeger(L, -1);;
        lua_pop(L, 1);

        lua_getfield(L, -1, "h");
        if (lua_isnumber(L, -1)) h = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "resizable");
        if (lua_isboolean(L, -1)) resizable = lua_toboolean(L, -1);
        lua_pop(L, 1);

      } else {
        std::cerr << "WARNING: Window() did not return a table, using default values" << std::endl;
      }
    } else {
      std::cerr << "ERROR: Window() execution failed: "<< lua_tostring(L, -1) << std::endl;
      std::cerr << lua_tostring(L, -1) << std::endl;
      std::cerr << "Reverting to default window settings" << std::endl;
    }
  } else {
    std::cerr << "WARNING: Window() not declared, using default settings" << std::endl;
  }
  lua_pop(L, 1);

  // ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
  // ╏ WINDOW FLAGS ╏
  // ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
  int windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;

  if (resizable) {
    windowFlags |= SDL_WINDOW_RESIZABLE;
  }

  if (mode == "full") {
    windowFlags |= SDL_WINDOW_MAXIMIZED;
    windowFlags |= SDL_WINDOW_RESIZABLE;
  } else if (mode == "whole screen") {
    windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
  }

  window = SDL_CreateWindow(
      title.c_str(),
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      w,
      h,
      windowFlags
      );

  if (!window) {
    std::cout << "Window Creation Failed: " << SDL_GetError() << std::endl;
    SDL_Quit();
    return 1;
  }
  SDL_GetWindowSize(window, &winW, &winH);

  //          ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
  //          ╏                  END OF WINDOW SECTION                  ╏
  //          ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛





  // ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
  // ╏ INITIALIZING RENDERER ╏
  // ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
  OpenGLRenderer renderer(window);


  LoadFontConfig(L);
  // ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
  // ╏ HANDLING APP FUNCTION ╏
  // ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
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

  Node* root = nullptr;
  lua_pushcfunction(L, protected_buildNode);
  lua_pushvalue(L, -2);

  if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
    std::cerr << "CRITICAL VDOM ERROR: " << lua_tostring(L, -1) << std::endl;
    return 1;
  }

  if (lua_islightuserdata(L, -1)) {
    root = (Node*)lua_touserdata(L, -1);
  }
  lua_pop(L, 1); // Pop lightuserdata
  lua_pop(L, 1); // Pop App


  // ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
  // ╏ LAYOUT SOLVER CREATION ╏
  // ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
  Layout::LayoutSolver* solver = Layout::createYogaSolver();
  solver->solve(root, {winW, winH});
  updateTextLayout(root);
  root->isLayoutDirty = false;
  root->isPaintDirty = false;

  bool running = true;
  SDL_Event event;

  // ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
  // ╏ INITIALIZED TIME ╏
  // ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
  Uint32 lastTime = SDL_GetTicks();

  //          ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
  //          ╏                    MAIN PROGRAM LOOP                    ╏
  //          ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
  while (running) {

    Uint32 currentTime = SDL_GetTicks();
    float dt = (currentTime - lastTime) / 1000.0f;
    lastTime = currentTime;

    lua_getglobal(L, "on_tick");
    if (lua_isfunction(L, -1)) {
      lua_pushnumber(L, dt);
      if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        std::cerr << "on_tick Error " << lua_tostring(L, -1) << std::endl;
        lua_pop(L, 1);
      }
    } else {
      lua_pop(L, 1);
    }

    // HANDLING SDL EVENTS
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

    // ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
    // ╏ RECONCILE TREE IF DIRTY ╏
    // ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
    if (StateManager::instance().isDirty()) {
      lua_getglobal(L, "App");
      if (lua_isfunction(L, -1)) {
        if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
          std::cerr << "App Update Error: " << lua_tostring(L, -1) << std::endl;
          lua_pop(L, 1);
        } else {
          // App table is at top of stack (-1)

          if (lua_istable(L, -1)) {
            // PROTECTED RECONCILE
            lua_pushcfunction(L, protected_reconcile);
            lua_pushlightuserdata(L, root); // Arg 1: Root
            lua_pushvalue(L, -3);           // Arg 2: App Table (copy from -3)

            if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
              // Now, if an error happens here, it prints gracefully!
              std::cerr << "VDOM Reconcile Error: " << lua_tostring(L, -1) << std::endl;
              lua_pop(L, 1); // Pop error
            }
          } else {
            std::cerr << "Error: App() returned non-table during update" << std::endl;
          }

          lua_pop(L, 1); // Pop App table
        }
      } else {
        lua_pop(L, 1);
      }
      StateManager::instance().clearDirty();
    }


    if (root->isLayoutDirty) {
      solver->solve(root, {winW, winH});
      updateTextLayout(root);
      root->isLayoutDirty = false;
    }

    renderer.beginFrame();
    RenderCommandList cmdList;
    generateRenderCommands(root, cmdList);
    UI_SetRenderCommandList(&cmdList);

    // ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
    // ╏ on_render() FUNCTION ╏
    // ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
    lua_getglobal(L, "on_render");
    if (lua_isfunction(L, -1)) {
      if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        std::cout << "Error in on_render: " << lua_tostring(L, -1) << std::endl;
        lua_pop(L, 1);
      }
    } else {
      lua_pop(L, 1);
    }

    UI_SetRenderCommandList(nullptr);

    renderer.submit(cmdList);
    renderer.endFrame();
  }

  UI_ShutdownFonts();
  freeTree(L, root);
  SDL_DestroyWindow(window);
  SDL_Quit();
  lua_close(L);;
  return 0;
}

