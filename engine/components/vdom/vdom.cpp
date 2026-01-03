#include "vdom.h"
#include <lua.h>
#include <string>
#include <vector>

namespace VDOM {


  bool operator!=(const Length& a, const Length& b) {
    return a.value != b.value || a.type != b.type;
  }

  bool operator!=(const SDL_Color& a, const SDL_Color b) {
    return a.a != b.a || a.r != b.r || a.g != b.g || a.b != b.b;
  }

  template <typename T> 
    void update(T& field, const T& newVal, bool& dirtyFlag) {
      if (field != newVal) {
        field = newVal;
        dirtyFlag = true;
      }
    }

  int getIntProp(lua_State* L, const char* key, int defaultVal) {
    // assumes stlye in already on top of lua stack
    int val = defaultVal;
    lua_getfield(L, -1, key);
    if (lua_isnumber(L, -1)) {
      val = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);
    return val;
  }

  float getFloatProp(lua_State* L, const char* key, float defaultVal) {
    // assumes stlye in already on top of lua stack
    float val = defaultVal;
    lua_getfield(L, -1, key);
    if (lua_isnumber(L, -1)) {
      val = (float)lua_tonumber(L, -1);
    }
    lua_pop(L, 1);
    return val;
  }


  std::string getStringProp(lua_State* L, const char* key, std::string defaultVal) {
    // assumes stlye in already on top of lua stack
    std::string val = defaultVal;
    lua_getfield(L, -1, key);
    if (lua_isstring(L, -1)) {
      val = lua_tostring(L, -1);
    }
    lua_pop(L, 1);
    return val;
  }

  void updateCallback(lua_State* L, int tableIdx, const char* key, int& ref) {
    lua_getfield(L, tableIdx, key);
    if (lua_isfunction(L, -1)) {
      if (ref != -2) {
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
      }
      ref = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
      lua_pop(L, 1);
    }
  }

  void patchNode(lua_State* L, Node* n, int idx) {
    lua_getfield(L, idx, "style");
    if (!lua_istable(L, -1)) {
      lua_pop(L, 1);
      return;
    }

    bool layoutChanged = false;
    bool paintChanged = false;

    // comparing % w and h 
    update(n->widthStyle, getLength(L, "w"), layoutChanged);
    update(n->heightStyle, getLength(L, "h"), layoutChanged);

    // min max height width
    update(n->minWidth, getFloatProp(L, "minWidth", 0.0f), layoutChanged);
    update(n->maxWidth,  getFloatProp(L, "maxWidth", 99999.0f),  layoutChanged);
    update(n->minHeight, getFloatProp(L, "minHeight", 0.0f),     layoutChanged);
    update(n->maxHeight, getFloatProp(L, "maxHeight", 99999.0f), layoutChanged);

    // flexbox - alignItems - justifyContent
    update(n->flexGrow, getFloatProp(L, "flexGrow", 0.0f), layoutChanged);
    update(n->alignItems, parseAlign(getStringProp(L, "alignItems", "start")), layoutChanged);
    update(n->justifyContent, parseJustify(getStringProp(L, "justifyContent", "start")), layoutChanged);

    // we have support for both gap and spacing
    int gapVal = getIntProp(L, "gap", getIntProp(L, "spacing", 0));
    update(n->spacing, gapVal, layoutChanged);

    // Box Model (margin / padding)
    auto applyBoxModel = [&](const char* base, const char* t, const char* b, const char* l, const char* r, int& vBase, int& vT, int& vB, int& vL, int& vR) {
      int val = getIntProp(L, base, 0);
      update(vBase, val, layoutChanged);
      update(vT, getIntProp(L, t, val), layoutChanged);
      update(vB, getIntProp(L, b, val), layoutChanged);
      update(vL, getIntProp(L, l, val), layoutChanged);
      update(vR, getIntProp(L, r, val), layoutChanged);
    };

    applyBoxModel("padding", "paddingTop", "paddingBottom", "paddingLeft", "paddingRight", n->padding, n->paddingTop, n->paddingBottom, n->paddingLeft, n->paddingRight);
    applyBoxModel("margin", "marginTop", "marginBottom", "marginLeft", "marginRight", n->margin, n->marginTop, n->marginBottom, n->marginLeft, n->marginRight);

    lua_getfield(L, -1, "BGColor");
    if (lua_isstring(L, -1)) {
      const char* hex = lua_tostring(L, -1);
      SDL_Color newCol = parseHexColor(hex);
      update(n->color, newCol, paintChanged);

      if (!n->hasBackground) {
        n->hasBackground = true;
        paintChanged = true;
      }
    } else if (lua_isnil(L, -1)) {
      if (n->hasBackground) {
        n->hasBackground = false;
        paintChanged = true;
      }
    }
    lua_pop(L, 1);

    if (layoutChanged) {
      n->makeLayoutDirty();
    } else if (paintChanged) {
      n->makePaintDirty();
    }

    lua_pop(L, 1);
    updateCallback(L, idx, "onClick", n->onClickRef);
  }

  void reconcileChildren(lua_State* L, Node* current, int childrenIdx) {
    int luaCount = lua_rawlen(L, childrenIdx);

    std::vector<bool> reused(current->children.size(), false);
    std::vector<Node*> newChildren;
    newChildren.reserve(luaCount);
    
    for (int i = 0; i < luaCount; ++i) {
      lua_rawgeti(L, childrenIdx, i+1);
      int childIdx = lua_gettop(L);

      std::string key = "";
      lua_getfield(L, childIdx, "key");
      if (lua_isstring(L, -1)) key = lua_tostring(L, -1);
      lua_pop(L, 1);

      // trying to find match by key
      Node* matchedNode = nullptr;
      if (!key.empty()) {
        for (size_t j = 0; j < current->children.size(); j++) {
          if (!reused[j] && current->children[j]->key == key ) {
            matchedNode = current->children[j];
            reused[j] = true;
            break;
          }
        }
      }

      // try to find match by index
      if (!matchedNode) {
        if (i < current->children.size() && !reused[i] && current->children[i]->key.empty()) {
          matchedNode = current->children[i];
          reused[i] = true;
        }
      }

      if (!matchedNode) {
        matchedNode = buildNode(L, childIdx);
        matchedNode->parent = current;
        matchedNode->makeLayoutDirty();
      }

      patchNode(L, matchedNode, childIdx);

      lua_getfield(L, childIdx, "children");
      if (lua_istable(L, -1)) {
        reconcileChildren(L, matchedNode, lua_gettop(L));
      }
      lua_pop(L, 1);

      newChildren.push_back(matchedNode);
      lua_pop(L, 1);
    }

    for (size_t i = 0; i < current->children.size(); i++) {
      if (!reused[i]) {
        freeTree(current->children[i]);
        current->makeLayoutDirty();
      }
    }

    current->children = newChildren;

  }

  void reconcile(lua_State *L, Node *current, int idx) {
    if (!current) return;

    patchNode(L, current, idx);

    lua_getfield(L, idx, "children");
    if (lua_istable(L, -1)) {
      reconcileChildren(L, current, lua_gettop(L));
    }
    lua_pop(L, 1);
  }


}
