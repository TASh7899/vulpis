#include <SDL2/SDL.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>
#include <iostream>
#include <ostream>
#include <string>
#include "lua.hpp"

#include "components/renderer/commands.h"
#include "components/renderer/opengl_renderer.h"
#include "components/text/font.h"
#include "components/ui/ui.h"
#include "components/layout/layout.h"
#include "components/state/state.h"
#include "components/input/input.h"
#include "components/vdom/vdom.h"
#include "components/text/tvg_ui.h"
#include "components/text/bundled_registry.h"

extern int g_defaultFontId; // From ui.cpp

int main(int argc, char* argv[]) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cout << "SDL Init Failed: " << SDL_GetError() << std::endl;
    return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);



//          ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
//          ╏                        [SECTION]                        ╏
//          ╏                      CONFIGURATION                      ╏
//          ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛

bool useBundleFonts = true;

// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ READ CONFIG ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍┛
lua_State* cfgl = luaL_newstate();
if (cfgl) {
  if (luaL_dofile(cfgl, "config.lua") == LUA_OK) {
    if (lua_istable(cfgl, -1)) {
      lua_getfield(cfgl, -1, "enable_bundle_fonts");
      if (lua_isboolean(cfgl, -1)) {
        useBundleFonts = lua_toboolean(cfgl, -1);
      }
      lua_pop(cfgl, 1);
    }
  }
  lua_close(cfgl);
}

//          ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
//          ╏                  END OF CONFIGURATION                   ╏
//          ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛




  int winW = 800;
  int winH = 600;

  SDL_Window* window = nullptr;

  // initializing lua
  lua_State* L = luaL_newstate();
  luaL_openlibs(L);
  registerStateBindings(L);
  UI_RegisterLuaFunctions(L);

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
//          ╏                 LOAD FONT CONFIGURATION                 ╏
//          ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛

if (luaL_dofile(L, "VULPIS_FONT_CONF.lua") == LUA_OK) {
  if (lua_istable(L, -1)) {
    int len = lua_rawlen(L, -1);
    for (int i = 1; i <= len; ++i) {
      lua_rawgeti(L, -1, i); // Push font config item

      if (lua_istable(L, -1)) {
        std::string path;
        lua_getfield(L, -1, "path");
        if (lua_isstring(L, -1)) path = lua_tostring(L, -1);
        lua_pop(L, 1);

        std::string alias;
        lua_getfield(L, -1, "alias");
        if (lua_isstring(L, -1)) alias = lua_tostring(L, -1);
        lua_pop(L, 1);

        bool isFallback = false;
        lua_getfield(L, -1, "fallback");
        if (lua_isboolean(L, -1)) isFallback = lua_toboolean(L, -1);
        lua_pop(L, 1);

        if (!path.empty()) {
          // Register the alias (if provided)
          if (!alias.empty()) {
            UI_RegisterFontAlias(alias, path);
            std::cout << "[Font] Registered Alias: " << alias << " -> " << path << std::endl;
          }
          // Register as fallback (if flag is true)
          if (isFallback) {
            UI_AddGlobalFallbackPath(path);
            std::cout << "[Font] Added Global Fallback: " << path << std::endl;
          }
        }
      }
      lua_pop(L, 1); // Pop item
    }
  }
  lua_pop(L, 1); // Pop the table
} else {
  // File not found or error, pop the error message to keep stack clean
  lua_pop(L, 1); 
}



// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ INITIALIZING THOR ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
TVG_UI::Init();
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


// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ LOAD DEFAULT FONT ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛

#ifndef DISABLE_BUNDLE_FONT

if (useBundleFonts) {
  FontBlob blob = GetBundledIosevka("regular");

  if (blob.data) {
    std::cout << "Loading bundle Iosevka Fonts..." << std::endl;
    auto result = UI_LoadBundleFont("Iosevka-Default", blob.data, blob.len, 16, false );
    g_defaultFontId = result.first;
  }
} else {
  std::cout << "Bundled fonts disabled via config.lua" << std::endl;
}

#endif // !DISABLE_BUNBLE_FONTS




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

Node* root = buildNode(L, -1);
lua_pop(L, 1);


// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ LAYOUT SOLVER CREATION ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
Layout::LayoutSolver* solver = Layout::createYogaSolver();
solver->solve(root, {winW, winH});
root->isLayoutDirty = false;
root->isPaintDirty = false;

bool running = true;
SDL_Event event;



//          ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
//          ╏                    MAIN PROGRAM LOOP                    ╏
//          ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
while (running) {
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
Font::ShutdownFreeType();
freeTree(L, root);
SDL_DestroyWindow(window);
TVG_UI::ShutDown();
SDL_Quit();
lua_close(L);;
delete solver;
return 0;
}

