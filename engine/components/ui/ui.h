#pragma once
#include <string>
#include <vector>
#include <SDL2/SDL.h>
#include "../renderer/commands.h"
#include "../text/font.h"

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

enum class TextAlign {
  Left,
  Center,
  Right
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

enum class PositionType {
  Relative,
  Absolute
};

struct Node {
  std::string type;
  std::string key;
  std::vector<Node*> children;

  std::string text;
  int fontId = 0;
  Font* font = nullptr;
  Color textColor = {255, 255, 255, 255};

  TextAlign textAlign = TextAlign::Left;

  std::vector<std::string> computedLines;
  float computedLineHeight = 0.0f;

  float flexShrink = 0.0f;

  float x = 0, y = 0;
  float w = 0, h = 0;

  Length widthStyle;
  Length heightStyle;

  bool overflowHidden = true;

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

  int onClickRef = -2;
  Node* hitTest(Node* root, int x, int y);

  SDL_Color color = {0,0,0,0};
  bool hasBackground = false;

  Node* parent = nullptr;
  bool isLayoutDirty = true;
  bool isPaintDirty = true;

  void makeLayoutDirty() {
    isLayoutDirty = true;
    if (parent) {
      parent->makeLayoutDirty();
    }
  }

  void makePaintDirty() {
    isLayoutDirty = true;
    if (parent) {
      parent->makeLayoutDirty();
    }
  }

  PositionType position = PositionType::Relative;
};


struct TextLayoutResult {
  std::vector<std::string> lines;
  float width;
  float height;
};

TextLayoutResult calculateTextLayout(const std::string& text, Font* font, float maxWidth);



Node* buildNode(lua_State* L, int idx);
void generateRenderCommands(Node* n, RenderCommandList& list);
void freeTree(lua_State* L, Node* n);
void resolveStyles(Node* n, int parentW, int parentH);
void reconcile(lua_State* L, Node* current, int idx);

Length getLength(lua_State* L, const char* key);
Align parseAlign(std::string s);
Justify parseJustify(std::string s);
TextAlign parseTextAlign(std::string s);

void UI_RegisterLuaFunctions(lua_State* L);
void UI_SetRenderCommandList(RenderCommandList* list);
void updateTextLayout(Node* root);



extern RenderCommandList* activeCommandList;

void UI_SetRenderCommandList(RenderCommandList* list);
