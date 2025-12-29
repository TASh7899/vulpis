#include "ui.h"
#include <lua.h>
#include "../color/color.h"

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


    n->w = getInt("w", 0);
    n->h = getInt("h", 0);

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

void renderNode(SDL_Renderer* r, Node* n) {
  if ((n->type == "hbox" || n->type == "vbox") && n->hasBackground) {
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

