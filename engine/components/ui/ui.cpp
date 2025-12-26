#include "ui.h"
#include <algorithm>
#include <lua.h>
#include "../color/color.h"

Node* buildNode(lua_State* L, int idx) {
    luaL_checktype(L, idx, LUA_TTABLE);

    Node* n = new Node();

    // type (moti ladkiya)
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

    n->w = getInt("w", 0);
    n->h = getInt("h", 0);

    n->spacing = getInt("spacing", 0);

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

    // Layout properties
    n->layout.minW = getInt("minW", 0);
    n->layout.maxW = getInt("maxW", 0);
    n->layout.minH = getInt("minH", 0);
    n->layout.maxH = getInt("maxH", 0);

    auto getBool = [&](const char* key, bool defaultVal) {
        bool val = defaultVal;
        if (hasStyle) {
            lua_getfield(L, -1, key);
            if (lua_isboolean(L, -1))
                val = lua_toboolean(L, -1) != 0;
            lua_pop(L, 1);
        }
        return val;
    };

    n->layout.fillWidth = getBool("fillWidth", false);
    n->layout.fillHeight = getBool("fillHeight", false);

    auto getString = [&](const char* key, const char* defaultVal) {
        std::string val = defaultVal;
        if (hasStyle) {
            lua_getfield(L, -1, key);
            if (lua_isstring(L, -1))
                val = lua_tostring(L, -1);
            lua_pop(L, 1);
        }
        return val;
    };

    n->layout.justifyContent = getString("justifyContent", "start");
    n->layout.alignItems = getString("alignItems", "start");

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

    lua_getfield(L, idx, "children");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            Node* child = buildNode(L, lua_gettop(L));
            n->children.push_back(child);
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    return n;
}


void measure(Node* n) {

  if (n->type == "vstack") {
    int totalH = 0;
    int maxW = 0;

    for (Node* c : n->children) {
      measure(c);

      int childH = c->h + c->marginBottom + c->marginTop;
      int childW = c->w + c->marginLeft + c->marginRight;

      totalH += childH + n->spacing;
      maxW = std::max(maxW, childW);
    }

    if (!n->children.empty()) {
      totalH -= n->spacing;
    }

    // auto-sizing: calculate karo size koif w or h is 0
    if (n->w == 0) n->w = maxW + n->paddingLeft + n->paddingRight;
    if (n->h == 0) n->h = totalH + n->paddingTop + n->paddingBottom;

    // applying  constraints
    if (n->layout.minW > 0) n->w = std::max(n->w, n->layout.minW);
    if (n->layout.maxW > 0) n->w = std::min(n->w, n->layout.maxW);
    if (n->layout.minH > 0) n->h = std::max(n->h, n->layout.minH);
    if (n->layout.maxH > 0) n->h = std::min(n->h, n->layout.maxH);
  }
  else if (n->type == "hstack") {
    int totalW = 0;
    int maxH = 0;

    for (Node* c : n->children) {
      measure(c);

      int childH = c->h + c->marginTop + c->marginBottom;
      int childW = c->w + c->marginLeft + c->marginRight;

      totalW += childW + n->spacing;
      maxH = std::max(maxH, childH);
    }

    if (!n->children.empty()) {
      totalW -= n->spacing;
    }

    // auto-sizing: calculate size if w or h is 0
    if (n->w == 0) n->w = totalW + n->paddingRight + n->paddingLeft;
    if (n->h == 0) n->h = maxH + n->paddingTop + n->paddingBottom;

    // apply constraints
    if (n->layout.minW > 0) n->w = std::max(n->w, n->layout.minW);
    if (n->layout.maxW > 0) n->w = std::min(n->w, n->layout.maxW);
    if (n->layout.minH > 0) n->h = std::max(n->h, n->layout.minH);
    if (n->layout.maxH > 0) n->h = std::min(n->h, n->layout.maxH);
  }
  else if (n->type == "rect") {
    // for rect nodes,apply karo(tell crow 🐦‍⬛)constraints if size is set
    if (n->layout.minW > 0) n->w = std::max(n->w, n->layout.minW);
    if (n->layout.maxW > 0 && n->w > 0) n->w = std::min(n->w, n->layout.maxW);
    if (n->layout.minH > 0) n->h = std::max(n->h, n->layout.minH);
    if (n->layout.maxH > 0 && n->h > 0) n->h = std::min(n->h, n->layout.maxH);
  }
}

void layout(Node* n, int x, int y) {
  n->x = x;
  n->y = y;

  if (n->type == "vstack") {
    // calculate total children(tasty) height for justifyContent
    int totalChildrenH = 0;
    for (Node* c : n->children) {
      totalChildrenH += c->h + c->marginTop + c->marginBottom;
    }
    if (!n->children.empty() && n->children.size() > 1) {
      totalChildrenH += n->spacing * (n->children.size() - 1);
    }

    int availableH = n->h - n->paddingTop - n->paddingBottom;
    int startY = y + n->paddingTop;

    // apply justifycontent (vertical alignment/distribution)
    if (n->layout.justifyContent == "center") {
      startY += (availableH - totalChildrenH) / 2;
    } else if (n->layout.justifyContent == "end") {
      startY += availableH - totalChildrenH;
    } else if (n->layout.justifyContent == "space-between" && n->children.size() > 1) {
     
    } else if (n->layout.justifyContent == "space-around" && n->children.size() > 1) {
      
    }

    int cursor = startY;
    int spaceBetween = 0;
    int spaceAround = 0;

    if (n->layout.justifyContent == "space-between" && n->children.size() > 1) {
      int extraSpace = availableH - totalChildrenH;
      spaceBetween = extraSpace / (n->children.size() - 1);
    } else if (n->layout.justifyContent == "space-around" && n->children.size() > 1) {
      int extraSpace = availableH - totalChildrenH;
      spaceAround = extraSpace / (n->children.size() * 2);
      cursor += spaceAround;
    }

    for (size_t i = 0; i < n->children.size(); ++i) {
      Node* c = n->children[i];
      
      
      int cx = x + n->paddingLeft + c->marginLeft;
      int availableW = n->w - n->paddingLeft - n->paddingRight - c->marginLeft - c->marginRight;
      
      if (n->layout.alignItems == "center") {
        cx = x + n->paddingLeft + c->marginLeft + (availableW - c->w) / 2;
      } else if (n->layout.alignItems == "end") {
        cx = x + n->w - n->paddingRight - c->marginRight - c->w;
      } else if (n->layout.alignItems == "stretch" && c->w == 0) {
        
        c->w = availableW;
      }

      int cy = cursor + c->marginTop;
      layout(c, cx, cy);
      
      cursor += c->h + c->marginTop + c->marginBottom;
      if (i < n->children.size() - 1) {
        cursor += n->spacing;
        if (n->layout.justifyContent == "space-between") {
          cursor += spaceBetween;
        } else if (n->layout.justifyContent == "space-around") {
          cursor += spaceAround * 2;
        }
      }
    }
  }
  else if (n->type == "hstack") {
    
    int totalChildrenW = 0;
    for (Node* c : n->children) {
      totalChildrenW += c->w + c->marginLeft + c->marginRight;
    }
    if (!n->children.empty() && n->children.size() > 1) {
      totalChildrenW += n->spacing * (n->children.size() - 1);
    }

    int availableW = n->w - n->paddingLeft - n->paddingRight;
    int startX = x + n->paddingLeft;

    
    if (n->layout.justifyContent == "center") {
      startX += (availableW - totalChildrenW) / 2;
    } else if (n->layout.justifyContent == "end") {
      startX += availableW - totalChildrenW;
    } else if (n->layout.justifyContent == "space-between" && n->children.size() > 1) {
      
    } else if (n->layout.justifyContent == "space-around" && n->children.size() > 1) {
      
    }

    int cursor = startX;
    int spaceBetween = 0;
    int spaceAround = 0;

    if (n->layout.justifyContent == "space-between" && n->children.size() > 1) {
      int extraSpace = availableW - totalChildrenW;
      spaceBetween = extraSpace / (n->children.size() - 1);
    } else if (n->layout.justifyContent == "space-around" && n->children.size() > 1) {
      int extraSpace = availableW - totalChildrenW;
      spaceAround = extraSpace / (n->children.size() * 2);
      cursor += spaceAround;
    }

    for (size_t i = 0; i < n->children.size(); ++i) {
      Node* c = n->children[i];
      
      
      int cy = y + n->paddingTop + c->marginTop;
      int availableH = n->h - n->paddingTop - n->paddingBottom - c->marginTop - c->marginBottom;
      
      if (n->layout.alignItems == "center") {
        cy = y + n->paddingTop + c->marginTop + (availableH - c->h) / 2;
      } else if (n->layout.alignItems == "end") {
        cy = y + n->h - n->paddingBottom - c->marginBottom - c->h;
      } else if (n->layout.alignItems == "stretch" && c->h == 0) {
        
        c->h = availableH;
      }

      int cx = cursor + c->marginLeft;
      layout(c, cx, cy);
      
      cursor += c->w + c->marginLeft + c->marginRight;
      if (i < n->children.size() - 1) {
        cursor += n->spacing;
        if (n->layout.justifyContent == "space-between") {
          cursor += spaceBetween;
        } else if (n->layout.justifyContent == "space-around") {
          cursor += spaceAround * 2;
        }
      }
    }
  }
}

void renderNode(SDL_Renderer* r, Node* n) {
  if ((n->type == "hstack" || n->type == "vstack") && n->hasBackground) {
    SDL_SetRenderDrawColor(r, n->color.r, n->color.g, n->color.b, n->color.a);
    SDL_Rect bg = { n->x, n->y, n->w, n->h };
    SDL_RenderFillRect(r, &bg);
  }

  if (n->type == "rect") {
    SDL_SetRenderDrawColor(r, n->color.r, n->color.g, n->color.b, n->color.a);
    SDL_Rect rect = { n->x, n->y, n->w, n->h };
    SDL_RenderFillRect(r, &rect);
  }

  for (Node* c : n->children) {
    renderNode(r, c);
  }
}

void freeTree(Node* n) {
  for (Node* c : n->children)
    freeTree(c);
  delete n;
}

