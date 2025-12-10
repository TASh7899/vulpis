#include "ui.h"
#include <algorithm>

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

    n->padding      = p;
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

    if (hasStyle) {
        lua_getfield(L, -1, "color");
        if (lua_istable(L, -1)) {
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

    n->w = maxW + n->paddingLeft + n->paddingRight;
    n->h = totalH + n->paddingTop + n->paddingBottom;
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

    n->w = totalW + n->paddingRight + n->paddingLeft;
    n->h = maxH + n->paddingTop + n->paddingBottom;
  }
}

void layout(Node* n, int x, int y) {
  n->x = x;
  n->y = y;

  if (n->type == "vstack") {
    int cursor = y + n->paddingTop;

    for (Node* c : n->children) {
      int cx = x + n->paddingLeft + c->marginLeft;
      int cy = cursor + c->marginTop;

      layout(c, cx, cy);
      cursor += c->h + n->spacing + c->marginTop + c->marginBottom;
    }
  }
  else if (n->type == "hstack") {
    int cursor = x + n->paddingLeft; 

    for (Node* c : n->children) {
      int cx = cursor + c->marginLeft;
      int cy = y + n->paddingTop + c->marginTop;

      layout(c, cx, cy);
      cursor += c->w + n->spacing + c->marginRight + c->marginLeft;
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

