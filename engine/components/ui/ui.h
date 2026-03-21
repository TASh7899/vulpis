#pragma once
#include <cstdint>
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
  bool isSet;

  Length() : value(0), type(UnitType::PIXEL), isSet(true) {}
  Length(float v) : value(v), type(UnitType::PIXEL), isSet(true) {}
  Length(int v) : value((float)v), type(UnitType::PIXEL), isSet(true) {}

  static Length Percent(float v) {
    Length l;
    l.value = v;
    l.type = UnitType::PERCENT;
    l.isSet = true;
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

struct ScrollbarMetrics {
  bool isVisible;
  float trackX, trackY, trackW, trackH;
  float thumbY, thumbH;
  float maxScrollY;
};

struct DamageRect {
  bool active = true;
  bool fullScreen = true;
  float x = 0, y = 0, w = 0, h = 0;
  int framesLeft = 2;

  void add(float nx, float ny, float nw, float nh);
  void damageAll();
  void update();
};

extern DamageRect g_damageTracker;

struct Node {
  std::string type;
  std::string key;
  std::string id = "";
  std::vector<Node*> children;

  std::string src;
  uint32_t textureId = 0;

  std::string text;
std::vector<uint32_t> codepoints;
  std::string objectFit = "fill";
  int fontId = 0;
  Font* font = nullptr;
  Color textColor = {255, 255, 255, 255};

  std::string fontFamily;
  int fontSize = 0;
  FontStyle fontStyle = FontStyle::Normal;
  FontWeight fontWeight = FontWeight::Normal;
  TextDecoration textDecoration = TextDecoration::None;

  bool loadedVariantBold = false;
  bool loadedVariantItalic = false;
  bool loadedVariantThin = false;

  TextAlign textAlign = TextAlign::Left;

  std::vector<TextLine> computedLines;
  float computedLineHeight = 0.0f;

  float flexShrink = 0.0f;

  float x = 0, y = 0;
  float w = 0, h = 0;

  Length widthStyle;
  Length heightStyle;

  bool overflowHidden = true;
  bool overflowScroll = false;

  // cached offset for nodes position change
  float cachedOffsetX = 0.0f;
  float cachedOffsetY = 0.0f;

  bool wordWrap = true;

  float scrollX = 0.0f;
  float scrollY = 0.0f;
  float targetScrollX = 0.0f;
  float targetScrollY = 0.0f;
  float contentW = 0.0f;
  float contentH = 0.0f;

  // attributes for dragging
  bool isDraggable = false;
  bool isDragging = false;

  float dragOffsetX = 0.0f;
  float dragOffsetY = 0.0f;

  int onDragStartRef = -2;
  int onDragRef = -2;
  int onDragEndRef = -2;

  bool isFocusable = false;
  bool isFocused = false;
  int onTextInputRef = -2;
  int onKeyDownRef = -2;
  int onFocusRef = -2;
  int onBlurRef = -2;


  // Cursor state for rendering
  int cursorPosition = -1; // -1 means no cursor/unfocused
  int lastCursorPosition = -1;
  int selectionStart = -1;
  int selectionEnd = -1;


  int spacing = 0;
  float opacity = 1.0f;
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
  int onMouseEnterRef = -2;
  int onMouseLeaveRef = -2;
  int onRightClickRef = -2;
  bool isHovered = false;


  bool hasLeft = false; float leftVal = 0.0f;
  bool hasTop = false; float topVal = 0.0f;
  bool hasRight = false; float rightVal = 0.0f;
  bool hasBottom = false; float bottomVal = 0.0f;


  float scrollbarOpacity = 0.0f;
  float scrollbarTimer = 0.0f;

  ScrollbarMetrics getScrollbarMetrics();

  SDL_Color color = {0,0,0,0};
  bool hasBackground = false;

  std::string bgImageSrc = "";
  uint32_t bgTextureId = 0;
  std::string bgImageFit = "cover";

  Node* parent = nullptr;
  bool isLayoutDirty = true;
  bool isPaintDirty = true;

  void markTreePaintDirty() {
    isPaintDirty = true;
    if (parent) {
      parent->markTreePaintDirty();
    }
  }

  void markTreeLayoutDirty() {
    isLayoutDirty = true;
    isPaintDirty = true;
    if (parent) {
      parent->markTreeLayoutDirty();
    }
  }

  void makeLayoutDirty() {
    isLayoutDirty = true;
    makePaintDirty();
    if (parent) {
      parent->markTreeLayoutDirty();
    }
  }

  void makePaintDirty() {
    g_damageTracker.add(this->x + this->cachedOffsetX + this->dragOffsetX, this->y + this->cachedOffsetY + this->dragOffsetY, this->w, this->h);
    isPaintDirty = true;
    if (parent) {
      parent->markTreePaintDirty();
    }
  }

  PositionType position = PositionType::Relative;

  // Cache Storage
  RenderCommandList cachedCommands;
  bool hasCachedCommands = false;

  // subtree invalidator
  void invalidateSubtreePaint() {
    isPaintDirty = true;
    hasCachedCommands = false;
    for (Node* c : children) {
      c->invalidateSubtreePaint();
    }
  }

};


struct TextLayoutResult {
  std::vector<std::string> lines;
  float width;
  float height;
};

TextLayoutResult calculateTextLayout(const std::string& text, Font* font, float maxWidth);



Node* buildNode(lua_State* L, int idx);
void generateRenderCommands(Node* n, RenderCommandList& list, float parentOffsetX = 0.0f, float parentOffsetY = 0.0f);
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

void UI_InitTypes(lua_State *L);


FontStyle parseFontStyle(const std::string& s);
FontWeight parseFontWeight(const std::string& s);
TextDecoration parseTextDecoration(const std::string& s);
std::string getVariantKey(FontWeight w, FontStyle s);
void parseEvents(lua_State* L, Node* n, int idx);
int getFlags(Node* node);
void UI_UpdateSmoothScrolling(Node* n, float dt);

static void appendCP(std::string& s, uint32_t cp);

void computeTextLayout(Node* n);
