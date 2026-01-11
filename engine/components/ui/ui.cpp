#include "ui.h"
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <algorithm>
#include <lauxlib.h>
#include <lua.h>
#include <string>
#include <vector>
#include "../color/color.h"
#include "../vdom/vdom.h"
#include "../text/font.h"

// global pointer for immediate mode
static RenderCommandList* activeCommandList = nullptr;

void UI_SetRenderCommandList(RenderCommandList* list) {
  activeCommandList = list;
}

// garbage collection for font userdata
int l_gc_font(lua_State* L) {
  Font** font = (Font**)luaL_checkudata(L, 1, "FontMeta");
  if (*font) {
    delete *font;
    *font = nullptr;
  }
  return 0;
}

// load_font
int l_load_font(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  int size = luaL_checkinteger(L, 2);

  Font** font = (Font**)lua_newuserdata(L, sizeof(Font*));
  *font = new Font(path, size);
  
  if (luaL_newmetatable(L, "FontMeta")) {
    lua_pushcfunction(L, l_gc_font);
    lua_setfield(L, -2, "__gc");
  }
  lua_setmetatable(L, -2);
  return 1;
}

int l_draw_text(lua_State* L) {
  if (!activeCommandList) {
    return 0;
  }

  const char* str = luaL_checkstring(L, 1);
  Font** fontPtr = (Font**)luaL_checkudata(L, 2, "FontMeta");
  Font* font = *fontPtr;
  float x = luaL_checknumber(L, 3);
  float y = luaL_checknumber(L, 4);

  Color color = {255, 255, 255, 255};
  if (lua_istable(L, 5)) {
    lua_rawgeti(L, 5, 1); color.r = luaL_optinteger(L, -1, 255); lua_pop(L, 1);
    lua_rawgeti(L, 5, 2); color.g = luaL_optinteger(L, -1, 255); lua_pop(L, 1);
    lua_rawgeti(L, 5, 3); color.b = luaL_optinteger(L, -1, 255); lua_pop(L, 1);
    lua_rawgeti(L, 5, 4); color.a = luaL_optinteger(L, -1, 255); lua_pop(L, 1);
    }
  

  activeCommandList->push(DrawTextCommand{std::string(str), font, x, y, color});
  return 0;
}

void UI_RegisterLuaFunctions(lua_State *L) {
  lua_register(L, "load_font", l_load_font);
  lua_register(L, "text", l_draw_text);
}


Align parseAlign(std::string s) {
    if (s == "center") return Align::Center;
    if (s == "end") return Align::End;
    if (s == "stretch") return Align::Stretch;
    return Align::Start;
}

Justify parseJustify(std::string s) {
    if (s == "center") return Justify::Center;
    if (s == "end") return Justify::End;
    if (s == "space-around") return Justify::SpaceAround;
    if (s == "space-between") return Justify::SpaceBetween;
    if (s == "space-evenly") return Justify::SpaceEvenly;
    return Justify::Start;
}

Length getLength(lua_State* L, const char* key) {
    Length len;
    lua_getfield(L, -1, key);

    if (lua_isnumber(L, -1)) {
        len.value = (float)lua_tonumber(L, -1);
        len.type = PIXEL;
    }
    else if (lua_isstring(L, -1)) {
        std::string s = lua_tostring(L, -1);
        if (!s.empty() && s.back() == '%') {
            try {
                float val = std::stof(s.substr(0, s.size() - 1));
                len = Length::Percent(val);
            } catch (...) {
                len = Length(0);
            }
        } else {
             len = Length(0);
        }
    }
    
    lua_pop(L, 1);
    return len;
}



Node* buildNode(lua_State* L, int idx) {
    luaL_checktype(L, idx, LUA_TTABLE);

    Node* n = new Node();

    lua_getfield(L, idx, "type");
    if (lua_isstring(L, -1))
        n->type = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, idx, "text");
    if (lua_isstring(L, -1)) {
      n->text = lua_tostring(L, -1);
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "style");
    bool hasStyle = lua_istable(L, -1);

    auto getInt = [&](const char* key, int defaultVal) {
      int val = defaultVal;
      if (hasStyle) {
        lua_getfield(L, -1, key);
        if (lua_isnumber(L, -1))
          val = lua_tointeger(L, -1);
        lua_pop(L, 1);
      }
      return val;
    };

    auto getFloat = [&](const char* key, float defaultVal) {
      float val = defaultVal;
      if (hasStyle) {
        lua_getfield(L, -1, key);
        if (lua_isnumber(L, -1)) val = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
      }
      return val;
    };

    auto getString = [&](const char* key, std::string defaultVal) {
      std::string val = defaultVal;
      if (hasStyle) {
        lua_getfield(L, -1, key);
        if (lua_isstring(L, -1)) val = lua_tostring(L, -1);
        lua_pop(L, 1);
      }
      return val;
    };

    if (hasStyle) {
      if (n->type == "text") {
        lua_getfield(L, -1, "font");
        if (lua_isuserdata(L, -1)) {
          Font** fontptr = (Font**) luaL_checkudata(L, -1, "FontMeta");
          if (fontptr && *fontptr) {
            n->font = *fontptr;
          }
        }
        lua_pop(L, 1);
        lua_getfield(L, -1, "color");
        if (lua_isstring(L, -1)) {
          const char* hex = lua_tostring(L, -1);
          SDL_Color sc = parseHexColor(hex);
          n->textColor = {(uint8_t)sc.r, (uint8_t)sc.g, (uint8_t)sc.b, (uint8_t)sc.a};
        }
        else if (lua_istable(L, -1)) {
          lua_rawgeti(L, -1, 1); n->textColor.r = luaL_optinteger(L, -1, 255); lua_pop(L, 1);
          lua_rawgeti(L, -1, 2); n->textColor.g = luaL_optinteger(L, -1, 255); lua_pop(L, 1);
          lua_rawgeti(L, -1, 3); n->textColor.b = luaL_optinteger(L, -1, 255); lua_pop(L, 1);
          lua_rawgeti(L, -1, 4); n->textColor.a = luaL_optinteger(L, -1, 255); lua_pop(L, 1);
        }
        lua_pop(L, 1);
      }

      n->widthStyle = getLength(L, "w");
      n->heightStyle = getLength(L, "h");
    }

    n->spacing = getInt("gap", getInt("spacing", 0));

    int p = getInt("padding", 0);
    n->padding       = p;
    n->paddingTop    = getInt("paddingTop", p);
    n->paddingBottom = getInt("paddingBottom", p);
    n->paddingLeft   = getInt("paddingLeft", p);
    n->paddingRight  = getInt("paddingRight", p);

    int m = getInt("margin", 0);
    n->margin       = m;
    n->marginTop    = getInt("marginTop", m);
    n->marginBottom = getInt("marginBottom", m);
    n->marginLeft   = getInt("marginLeft", m);
    n->marginRight  = getInt("marginRight", m);

    n->minHeight = getInt("minHeight", 0);
    n->maxHeight = getInt("maxHeight", 99999);
    n->minWidth = getInt("minWidth", 0);
    n->maxWidth = getInt("maxWidth", 99999);

    n->flexGrow = getFloat("flexGrow", 0.0f);
    n->flexShrink  = getFloat("flexShrink", 0.0f);
    n->alignItems = parseAlign(getString("alignItems", "start"));
    n->justifyContent = parseJustify(getString("justifyContent", "start"));

    std::string overflow = getString("overflow", "hidden");
    if (overflow == "visible") {
      n->overflowHidden = false;
    } else {
      n->overflowHidden = true;
    }

    if (hasStyle) {
      lua_getfield(L, -1, "BGColor");
      if (lua_isstring(L, -1)) {
        const char* hex = lua_tostring(L, -1);
        n->color = parseHexColor(hex);
        n->hasBackground = true;
      }
      else if (lua_istable(L, -1)) {
        lua_rawgeti(L, -1, 1); n->color.r = luaL_optinteger(L, -1, 255); lua_pop(L, 1);
        lua_rawgeti(L, -1, 2); n->color.g = luaL_optinteger(L, -1, 255); lua_pop(L, 1);
        lua_rawgeti(L, -1, 3); n->color.b = luaL_optinteger(L, -1, 255); lua_pop(L, 1);
        lua_rawgeti(L, -1, 4); n->color.a = luaL_optinteger(L, -1, 255); lua_pop(L, 1);

        n->hasBackground = true;
      }
      lua_pop(L, 1);
    }

    lua_pop(L, 1);

    lua_getfield(L, idx, "onClick");
    if (lua_isfunction(L, -1)) {
      n->onClickRef = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
      lua_pop(L, 1);
    }

    VDOM::updateCallback(L, idx, "onClick", n->onClickRef);
    lua_getfield(L, idx, "children");
    if (lua_istable(L, -1)) {
      lua_pushnil(L);
      while (lua_next(L, -2) != 0) {
        Node* child = buildNode(L, lua_gettop(L));
        child->parent = n;
        n->children.push_back(child);
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);

    return n;
}

void resolveStyles(Node* n, int parentW, int parentH) {
  if (!n) return;

  if (n->widthStyle.value != 0) {
    n->w = n->widthStyle.resolve((float)parentW);
  }

  if (n->heightStyle.value != 0) {
    n->h = n->heightStyle.resolve((float)parentH);
  }

  int contentW = (int)n->w - (n->paddingLeft + n->paddingRight);
  int contentH = (int)n->h - (n->paddingTop + n->paddingBottom);

  if (contentW < 0) contentW = 0;
  if (contentH < 0) contentH = 0;

  for (Node* c : n->children) {
    resolveStyles(c, contentW, contentH);
  }
}

void measure(Node* n) {
  if (n->type == "vbox") {
    int totalH = 0;
    int maxW = 0;

    for (Node* c : n->children) {
      measure(c);

      int childH = (int)c->h + c->marginBottom + c->marginTop;
      int childW = (int)c->w + c->marginLeft + c->marginRight;

      totalH += childH + n->spacing;
      maxW = std::max(maxW, childW);
    }

    if (!n->children.empty()) {
      totalH -= n->spacing;
    }

    if (n->w == 0) n->w = maxW + n->paddingLeft + n->paddingRight;
    if (n->h == 0) n->h = totalH + n->paddingTop + n->paddingBottom;
  }
  else if (n->type == "hbox") {
    int totalW = 0;
    int maxH = 0;

    for (Node* c : n->children) {
      measure(c);

      int childH = (int)c->h + c->marginTop + c->marginBottom;
      int childW = (int)c->w + c->marginLeft + c->marginRight;

      totalW += childW + n->spacing;
      maxH = std::max(maxH, childH);
    }

    if (!n->children.empty()) {
      totalW -= n->spacing;
    }

    if (n->w == 0) n->w = totalW + n->paddingRight + n->paddingLeft;
    if (n->h == 0) n->h = maxH + n->paddingTop + n->paddingBottom;
  }
}

void layout(Node* n, int x, int y) {
  n->x = (float)x;
  n->y = (float)y;

  if (n->type == "vbox") {
    int cursor = y + n->paddingTop;

    for (Node* c : n->children) {
      int cx = x + n->paddingLeft + c->marginLeft;
      int cy = cursor + c->marginTop;

      layout(c, cx, cy);
      cursor += (int)c->h + n->spacing + c->marginTop + c->marginBottom;
    }
  }
  else if (n->type == "hbox") {
    int cursor = x + n->paddingLeft; 

    for (Node* c : n->children) {
      int cx = cursor + c->marginLeft;
      int cy = y + n->paddingTop + c->marginTop;

      layout(c, cx, cy);
      cursor += (int)c->w + n->spacing + c->marginRight + c->marginLeft;
    }
  }
}

void generateRenderCommands(Node *n, RenderCommandList &list) {
  if (n->hasBackground) {
    list.push(DrawRectCommand{
        {n->x, n->y, n->w, n->h},
        {n->color.r, n->color.g, n->color.b, n->color.a}
        });
  }

  if (n->type == "text" && !n->computedLines.empty()) {
    float cursorX = n->x + n->paddingLeft;
    float cursorY = n->y + n->paddingTop + n->font->GetAscent();

    for (const std::string& line : n->computedLines) {
      list.push(DrawTextCommand{line, n->font, cursorX, cursorY, n->textColor});
      cursorY += n->computedLineHeight;
    }
  }

  if (n->overflowHidden) {
    list.push(PushClipCommand{{n->x, n->y, n->w, n->h}});
  }

  for (Node* c :  n->children) {
    generateRenderCommands(c, list);
  }

  if (n->overflowHidden) {
    list.push(PopClipCommand{});
  }
}

void freeTree(lua_State* L, Node* n) {
  if (!n) return;

  if (n->onClickRef != -2) {
    luaL_unref(L, LUA_REGISTRYINDEX, n->onClickRef);
    n->onClickRef = -2;
  }

  for (Node* c : n->children) {
    freeTree(L, c);
  }

  delete n;
}


TextLayoutResult calculateTextLayout(const std::string& text, Font* font, float maxWidth) {
  TextLayoutResult result;
  result.width = 0;
  result.height = 0;

  if (!font || text.empty()) return result;
  if (maxWidth <= 0) maxWidth = 1;

  float lineHeight = (float)font->GetLineHeight();
  float currentX = 0.0f;
  float currentY = lineHeight;

  auto measureStr = [&](const std::string& str) -> float {
    float w = 0;
    for (char c : str) w += (font->GetCharacter(c).Advance >> 6);
    return w;
  };

  std::string currentLine;
  float currentLineWidth = 0.0f;
  std::string word;

  for (size_t i = 0; i <= text.size(); i++) {
    char c = (i < text.size()) ? text[i] : 0;

    if (c == ' ' || c == '\n' || c == 0) {
      float wordW = measureStr(word);
      float spaceW = (c == ' ') ? measureStr(" ") : 0;

      if (currentLineWidth + wordW <= maxWidth) {
        currentLine += word;
        currentLineWidth += wordW;
      } else {
        result.lines.push_back(currentLine);
        result.width = std::max(result.width, currentLineWidth); // Track max width
        currentLine = word;
        currentLineWidth = wordW;
        currentY += lineHeight;
      }

      if (c == ' ') {
        currentLine += ' ';
        currentLineWidth += spaceW;
      } else if (c == '\n') {
        result.lines.push_back(currentLine);
        result.width = std::max(result.width, currentLineWidth);
        currentLine = "";
        currentLineWidth = 0;
        currentY += lineHeight;
      }
      word.clear();
    } else {
      word += c;
    }
  }
  if (!currentLine.empty()) {
    result.lines.push_back(currentLine);
    result.width = std::max(result.width, currentLineWidth);
  }

  result.height = currentY;
  return result;
}


void computeTextLayout(Node* n) {
  if (n->type != "text" || !n->font || n->text.empty()) {
    n->computedLines.clear();
    return;
  }

  float maxWidth = n->w - (n->paddingLeft + n->paddingRight);

  TextLayoutResult res = calculateTextLayout(n->text, n->font, maxWidth);

  n->computedLines = res.lines;
  n->computedLineHeight = (float)n->font->GetLineHeight();
}

void updateTextLayout(Node* root) {
  if (!root) return;
  computeTextLayout(root);
  for (Node* c : root->children) {
    updateTextLayout(c);
  }
}


