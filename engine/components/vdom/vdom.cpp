#include "vdom.h"
#include <lauxlib.h>
#include <lua.h>
#include <string>
#include <vector>
#include "../text/font.h"
#include "../../configLogic/font/font_registry.h"

namespace VDOM {

  PositionType parsePosition(std::string s) {
    if (s == "absolute") return PositionType::Absolute;
    return PositionType::Relative;
  }


  bool operator!=(const Length& a, const Length& b) {
    return a.value != b.value || a.type != b.type;
  }

  bool operator!=(const SDL_Color& a, const SDL_Color b) {
    return a.a != b.a || a.r != b.r || a.g != b.g || a.b != b.b;
  }

  bool operator!=(const Color& a, const Color& b) {
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
      val = (int)lua_tonumber(L, -1);
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
      if (ref != -2) {
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        ref = -2;
      }
      lua_pop(L, 1);
    }
  }


// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ NODE PATCHER ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛

  void patchNode(lua_State* L, Node* n, int idx) {
    bool layoutChanged = false;
    bool paintChanged = false;

    lua_getfield(L, idx, "text");
    if (lua_isstring(L, -1)) {
      std::string newText = lua_tostring(L, -1);
      if (n->text != newText) {
        n->text = newText;
        n->computedLines.clear();
        layoutChanged = true;
      }
    }
    lua_pop(L, 1);


    lua_getfield(L, idx, "style");
    if (!lua_istable(L, -1)) {
      lua_pop(L, 1);
      lua_newtable(L);
    }

    if (n->type == "text") {

      std::string fStyleStr = getStringProp(L, "fontStyle", "normal");
      update(n->fontStyle, parseFontStyle(fStyleStr), layoutChanged);

      std::string fWeightStr = getStringProp(L, "fontWeight", "normal");
      update(n->fontWeight, parseFontWeight(fWeightStr), layoutChanged);

      std::string fDecorStr = getStringProp(L, "textDecoration", "none");
      update(n->textDecoration, parseTextDecoration(fDecorStr), paintChanged);

      auto getFlags = [](Node* n) {
        int flags = FONT_STYLE_NORMAL;
        if (n->fontWeight == FontWeight::VeryBold) flags |= FONT_STYLE_VERY_BOLD;
        else if (n->fontWeight == FontWeight::Bold) flags |= FONT_STYLE_BOLD;
        else if (n->fontWeight == FontWeight::SemiBold) flags |= FONT_STYLE_SEMI_BOLD;
        else if (n->fontWeight == FontWeight::Thin) flags |= FONT_STYLE_THIN;
        if (n->fontStyle == FontStyle::Italics) flags |= FONT_STYLE_ITALIC;

        // Don't apply fake styles if real variants were loaded
        if (n->loadedVariantBold) flags &= ~(FONT_STYLE_BOLD | FONT_STYLE_SEMI_BOLD | FONT_STYLE_VERY_BOLD);
        if (n->loadedVariantItalic) flags &= ~FONT_STYLE_ITALIC;
        if (n->loadedVariantThin) flags &= ~FONT_STYLE_THIN;
        return flags;
      };

      lua_getfield(L, -1, "font");
      if (lua_isuserdata(L, -1)) {
        FontHandle* h = (FontHandle*)luaL_checkudata(L, -1, "FontMeta");
        if (h) {
          Font* fontPtr = UI_GetFontById(h->id);
          // If new font is valid and different, update
          if (fontPtr && n->font != fontPtr) {
            n->font = fontPtr;
            n->fontId = h->id;
            n->computedLines.clear();
            layoutChanged = true;
          }
        }
      }
      lua_pop(L, 1);

      lua_getfield(L, -1, "fontFamily");
      if (lua_isstring(L, -1)) {
        std::string alias = lua_tostring(L, -1);
        const FontConfig* config = GetFontConfig(alias);

        if (config) {
          int targetSize = config->size;

          // Check if fontSize overrides the config size locally
          lua_getfield(L, -2, "fontSize");
          if (lua_isnumber(L, -1)) {
            targetSize = (int)lua_tonumber(L, -1);
          }
          lua_pop(L, 1);

          // Load/Get the font from the cache
          auto [id, fontPtr] = UI_LoadFont(config->path, targetSize, getFlags(n));

          // If the resolved font is different, update the node
          if (n->font != fontPtr) {
            n->font = fontPtr;
            n->fontId = id;
            n->computedLines.clear();
            layoutChanged = true;
          }
        } else {
          if (n->font != nullptr) {
            n->font = nullptr;
            n->fontId = 0;
            n->computedLines.clear();
            layoutChanged = true;
          }
        }
      }
      lua_pop(L, 1);

      bool explicitFont = (n->font != nullptr && n->fontId != 0);

      if (!explicitFont) {
        const FontConfig* config = GetFontConfig("default");
        if (config) {
          int targetSize = config->size;
          lua_getfield(L, -1, "fontSize");
          if (lua_isnumber(L, -1)) targetSize = (int)lua_tonumber(L, -1);
          lua_pop(L, 1);

          auto [id, fontPtr] = UI_LoadFont(config->path, targetSize, getFlags(n));
          if (n->font != fontPtr) {
            n->font = fontPtr;
            n->fontId = id;
            n->computedLines.clear();
            layoutChanged = true;
          }
        } else {
          if (n->font != nullptr) {
            n->font = nullptr;
            n->fontId = 0;
            layoutChanged = true;
          }
        }
      }

      lua_getfield(L, -1, "fontSize");
      if (lua_isnumber(L, -1)) {
        int newSize = (int)lua_tointeger(L, -1);

        // If the size changed, we need to request a new font handle from the cache
        if (n->font && newSize > 0 && (int)n->font->GetSize() != newSize) {
          auto [id, fontPtr] = UI_LoadFont(n->font->GetPath(), newSize, getFlags(n));

          if (n->font != fontPtr) {
            n->font = fontPtr;
            n->fontId = id;
            n->computedLines.clear(); // Text needs re-measuring
            layoutChanged = true;
          }
        }
      }
      lua_pop(L, 1);

      const FontConfig* conf = GetFontConfig(n->fontFamily);
      if (conf) {
        std::string vkey = getVariantKey(n->fontWeight, n->fontStyle);
        int targetSize = (n->fontSize > 0) ? n->fontSize : conf->size;
        std::string targetPath = conf->path;

        n->loadedVariantBold = false;
        n->loadedVariantItalic = false;

        if (vkey.empty() && conf->variants.count(vkey)) {
          const VariantConfig& v = conf->variants.at(vkey);
          targetPath = v.path;

          if (vkey == "bold-italics") {
            n->loadedVariantBold = true;
            n->loadedVariantItalic = true;
          } 
          else if (vkey == "bold" || vkey == "semi-bold") {
            n->loadedVariantBold = true;
          } 
          else if (vkey == "italics") {
            n->loadedVariantItalic = true;
          }
          else if (vkey == "thin") {
            n->loadedVariantThin = true;
          }

          if (n->fontSize <= 0 && v.size > 0) targetSize = v.size;
        }

        auto [id, fontPtr] = UI_LoadFont(targetPath, targetSize, getFlags(n));
        if (n->font != fontPtr) {
          n->font = fontPtr;
          n->fontId = id;
          n->computedLines.clear();
          layoutChanged = true;
        }
      }

      lua_getfield(L, -1, "color");
      if (!lua_isnil(L, -1)) {
        Color newTextColor = n->textColor;

        if (lua_isstring(L, -1)) {
          SDL_Color sc = parseHexColor(lua_tostring(L, -1));
          newTextColor = {sc.r, sc.g, sc.b, sc.a};
        } 
        else if (lua_istable(L, -1)) {
          lua_rawgeti(L, -1, 1); int r = luaL_optinteger(L, -1, 255); lua_pop(L, 1);
          lua_rawgeti(L, -1, 2); int g = luaL_optinteger(L, -1, 255); lua_pop(L, 1);
          lua_rawgeti(L, -1, 3); int b = luaL_optinteger(L, -1, 255); lua_pop(L, 1);
          lua_rawgeti(L, -1, 4); int a = luaL_optinteger(L, -1, 255); lua_pop(L, 1);
          newTextColor = {(Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a};
        }

        if (n->textColor != newTextColor) {
          n->textColor = newTextColor;
          paintChanged = true;
        }
      }
      lua_pop(L, 1);
    }

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
    update(n->flexShrink, getFloatProp(L, "flexShrink", 0.0f), layoutChanged);
    update(n->alignItems, parseAlign(getStringProp(L, "alignItems", "start")), layoutChanged);
    update(n->textAlign, parseTextAlign(getStringProp(L, "textAlign", "left")), layoutChanged);

    update(n->justifyContent, parseJustify(getStringProp(L, "justifyContent", "start")), layoutChanged);
    update(n->position, parsePosition(getStringProp(L, "position", "relative")), layoutChanged);
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

    std::string overflow = getStringProp(L, "overflow", "hidden");
    bool newOverflowHidden = (overflow != "visible");

    if (n->overflowHidden != newOverflowHidden) {
      n->overflowHidden = newOverflowHidden;
      n->makePaintDirty();
    }

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
        freeTree(L, current->children[i]);
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
