#pragma once

#include <string>
#include <vector>
#include <SDL2/SDL.h>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

struct Node {
  std::string type;
  std::vector<Node*> children;

  int x = 0, y = 0;
  int w = 0, h = 0;

  int spacing = 0;
  int margin = 0;
  int marginTop = 0, marginBottom = 0, marginLeft = 0, marginRight = 0;
  int padding = 0;
  int paddingTop = 0, paddingBottom = 0, paddingLeft = 0, paddingRight = 0;

  SDL_Color color = {0,0,0,0};
  bool hasBackground = false;
};

Node* buildNode(lua_State* L, int idx);
void measure(Node* n);
void layout(Node* n, int x, int y);
void renderNode(SDL_Renderer* r, Node* n);
void freeTree(Node* n);

