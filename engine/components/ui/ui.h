#pragma once

#include <string>
#include <vector>
#include <SDL2/SDL.h>

enum UnitType {
  PIXEL,
  PERCENT,
};

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

enum class Justify {
  Start,
  End,
  Center,
  SpaceBetween,
  SpaceAround,
  SpaceEvenly
};

enum class Align {
  Start,
  Center,
  End,
  Stretch
};

struct Length {
  float value;
  UnitType type;

  Length() : value(0), type(UnitType::PIXEL) {}
  Length(float v) : value(v), type(UnitType::PIXEL) {}
  Length(int v) : value((float)v), type(UnitType::PIXEL) {}

  static Length Percent(float v) {
    Length l;
    l.value = v;
    l.type = UnitType::PERCENT;
    return l;
  }
  
  float resolve(float parentSize) const {
    if (type == UnitType::PIXEL) return value;
    return parentSize * (value/100.0f);
  }
};

inline Length pct(float v) {
  return Length::Percent(v);
}

struct Node {
  std::string type;
  std::vector<Node*> children;

  float x = 0, y = 0;
  float w = 0, h = 0;

  Length widthStyle;
  Length heightStyle;

  int spacing = 0;
  int margin = 0;
  int marginTop = 0, marginBottom = 0, marginLeft = 0, marginRight = 0;
  int padding = 0;
  int paddingTop = 0, paddingBottom = 0, paddingLeft = 0, paddingRight = 0;

  float minWidth = 0, maxWidth = 99999.0f;
  float minHeight = 0, maxHeight = 99999.0f;

  float flexGrow = 0.0f;
  Align alignItems = Align::Start;
  Justify justifyContent = Justify::Start;

  SDL_Color color = {0,0,0,0};
  bool hasBackground = false;
};


Node* buildNode(lua_State* L, int idx);
void renderNode(SDL_Renderer* r, Node* n);
void freeTree(Node* n);
void resolveStyles(Node* n, int parentW, int parentH);
