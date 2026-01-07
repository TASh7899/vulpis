#include "ui.h"
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <algorithm>
#include <lauxlib.h>
#include <lua.h>
#include <string>
#include "../color/color.h"
#include "../vdom/vdom.h"


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
