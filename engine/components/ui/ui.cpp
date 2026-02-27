#include "ui.h"
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <lauxlib.h>
#include <lua.h>
#include <string>
#include <vector>
#include "../color/color.h"
#include "../vdom/vdom.h"
#include "../text/font.h"
#include "../../configLogic/font/font_registry.h"
#include "../../configLogic/engineConf/engine_config.h"
#include "../../configLogic/font/font_registry.h"

// global pointer for immediate mode
RenderCommandList* activeCommandList = nullptr;

void UI_SetRenderCommandList(RenderCommandList* list) {
  activeCommandList = list;
}


int GetFontFlagsFromNode(Node* n) {
  int flags = FONT_STYLE_NORMAL;

  if (n->fontWeight == FontWeight::VeryBold) {
    flags |= FONT_STYLE_VERY_BOLD;
  }
  else if (n->fontWeight == FontWeight::Bold) {
    flags |= FONT_STYLE_BOLD;
  }
  else if (n->fontWeight == FontWeight::SemiBold) {
    flags |= FONT_STYLE_SEMI_BOLD;
  }
  else if (n->fontWeight == FontWeight::Thin) {
    flags |= FONT_STYLE_THIN;
  }

  if (n->fontStyle == FontStyle::Italics) {
    flags |= FONT_STYLE_ITALIC;
  }

  return flags;
}



void UI_InitTypes(lua_State *L) {
  luaL_newmetatable(L, "FontMeta");
  lua_pop(L, 1);
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

TextAlign parseTextAlign(std::string s) {
  if (s == "center") return TextAlign::Center;
  if (s == "right") return TextAlign::Right;
  if (s == "left") return TextAlign::Left;
  return TextAlign::Left;
}

FontStyle parseFontStyle(const std::string& s) {
  if (s == "italics") return FontStyle::Italics;
  return FontStyle::Normal;
}

FontWeight parseFontWeight(const std::string& s) {
  if (s == "thin")      return FontWeight::Thin;
  if (s == "semi-bold") return FontWeight::SemiBold;
  if (s == "bold")      return FontWeight::Bold;
  if (s == "very-bold") return FontWeight::VeryBold;
  return FontWeight::Normal;
}

TextDecoration parseTextDecoration(const std::string& s) {
  if (s == "underline")      return TextDecoration::Underline;
  if (s == "strike-through") return TextDecoration::StrikeThrough;
  return TextDecoration::None;
}

std::string getVariantKey(FontWeight w, FontStyle s) {
  if (w >= FontWeight::Bold && s == FontStyle::Italics) return "bold-italics";

  if (w == FontWeight::Bold) return "bold";
  if (w == FontWeight::SemiBold) return "semi-bold";
  if (w == FontWeight::Thin) return "thin";
  if (s == FontStyle::Italics) return "italics";
  return "";
}

int getFlags(Node* node) {
  int flags = GetFontFlagsFromNode(node);
  if (node->loadedVariantBold) flags &= ~(FONT_STYLE_BOLD | FONT_STYLE_SEMI_BOLD | FONT_STYLE_VERY_BOLD);
  if (node->loadedVariantItalic) flags &= ~FONT_STYLE_ITALIC;
  if (node->loadedVariantThin) flags &= ~FONT_STYLE_THIN;
  return flags;
};

void parseEvents(lua_State *L, Node *n, int idx) {
  auto getCallback = [&](const char* key, int& ref) {
    lua_getfield(L, idx, key);
    if (lua_isfunction(L, -1)) {
      ref = luaL_ref(L, LUA_REGISTRYINDEX);
    } else {
      lua_pop(L, 1);
    }
  };

  getCallback("onClick", n->onClickRef);
  getCallback("onMouseEnter", n->onMouseEnterRef);
  getCallback("onMouseLeave", n->onMouseLeaveRef);
  getCallback("onRightClick", n->onRightClickRef);
  getCallback("onDragStart", n->onDragStartRef);
  getCallback("onDragEnd", n->onDragEndRef);
  getCallback("onDrag", n->onDragRef);

}


void NodeParseText(Node* n, lua_State* L) {
  lua_getfield(L, -1, "fontStyle");
  if (lua_isstring(L, -1)) n->fontStyle  = parseFontStyle(lua_tostring(L, -1));
  lua_pop(L, 1);

  lua_getfield(L, -1, "fontWeight");
  if (lua_isstring(L, -1)) n->fontWeight = parseFontWeight(lua_tostring(L, -1));
  lua_pop(L, 1);

  lua_getfield(L, -1, "textDecoration");
  if (lua_isstring(L, -1)) n->textDecoration = parseTextDecoration(lua_tostring(L, -1));
  lua_pop(L, 1);


  lua_getfield(L, -1, "font");
  if (lua_isuserdata(L, -1)) {
    FontHandle* h = (FontHandle*)luaL_checkudata(L, -1, "FontMeta");
    if (h) {
      n->fontId = h->id;
      n->font = UI_GetFontById(h->id);
    }
  }
  lua_pop(L, 1);



  lua_getfield(L, -1, "fontFamily");
  if (lua_isstring(L, -1)) {
    std::string alias = lua_tostring(L, -1);
    const FontConfig* config = GetFontConfig(alias);
    if (config) {
      int size = config->size;
      lua_getfield(L, -2, "fontSize");
      if (lua_isnumber(L, -1)) {
        size = (int)lua_tonumber(L, -1);
        n->fontSize = size;
      }
      lua_pop(L, 1);

      std::string targetPath = config->path;
      std::string vKey = getVariantKey(n->fontWeight, n->fontStyle);

      n->loadedVariantBold = false;
      n->loadedVariantItalic = false;
      n->loadedVariantThin = false;

      if (!vKey.empty() && config->variants.count(vKey)) {
        const VariantConfig& v = config->variants.at(vKey);
        targetPath = v.path;
        // Only use variant size if user didn't explicitly override it locally

        if (vKey == "bold-italics") {
          n->loadedVariantBold = true;
          n->loadedVariantItalic = true;
        }
        else if (vKey == "bold" || vKey == "semi-bold") {
          n->loadedVariantBold = true;
        }
        else if (vKey == "italics") {
          n->loadedVariantItalic = true;
        }

        else if (vKey == "thin") {
          n->loadedVariantThin = true;
        }

        if (n->fontSize <= 0 && v.size > 0) {
          size = v.size;
        }
      }

      auto [id, fontptr] = UI_LoadFont(targetPath, size, getFlags(n));
      n->font = fontptr;
      n->fontId = id;
    } else {
      std::cerr << "Warning: fontFamily '" << alias << "' not found in registry.\n";
    }
  }
  lua_pop(L, 1);

  if (n->font == nullptr) {
    const FontConfig* config = GetFontConfig("default");
    if (config) {
      int size = config->size;
      lua_getfield(L, -1, "fontSize");
      if (lua_isnumber(L, -1)) {
        size = (int)lua_tonumber(L, -1);
      }
      lua_pop(L, 1);

      std::string targetPath = config->path;
      std::string vKey = getVariantKey(n->fontWeight, n->fontStyle);

      if (!vKey.empty() && config->variants.count(vKey)) {
        const VariantConfig& v = config->variants.at(vKey);
        targetPath = v.path;

        if (vKey.find("bold") != std::string::npos) n->loadedVariantBold = true;
        if (vKey.find("italics") != std::string::npos) n->loadedVariantItalic = true;
        if (vKey.find("thin") != std::string::npos) n->loadedVariantThin = true;

        if (n->fontSize <= 0 && v.size > 0) {
          size = v.size;
        }
      }

      auto [id, fontptr] = UI_LoadFont(targetPath, size, getFlags(n));
      n->font = fontptr;
      n->fontId = id;
    } else {
      const EngineConfig& ec = GetEngineConfig();
      static bool warned = false;
      if (!ec.enableDefaultFonts && !warned) {
        std::cerr << "WARNING: Text node created without font and default fonts are disabled in VP_ENGINE_CONFIG.lua\n";
        warned = true;
      }
    }
  }

  lua_getfield(L, -1, "fontSize");
  if (lua_isnumber(L, -1)) {
    int newSize = (int)lua_tointeger(L, -1);
    if (n->font && newSize > 0 && n->font->GetSize() != newSize) {
      auto [id, fontptr] = UI_LoadFont(n->font->GetPath(), newSize, getFlags(n));
      n->font = fontptr;
      n->fontId = id;
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

  lua_getfield(L, idx, "id");
  if (lua_isstring(L, -1)) n->id = lua_tostring(L, -1);
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
      NodeParseText(n, L);
    }

    n->widthStyle = getLength(L, "w");
    n->heightStyle = getLength(L, "h");

    auto checkFloat = [&](const char* k, bool& has, float& v) {
      lua_getfield(L, -1, k);
      if (!lua_isnil(L, -1)) {
        has = true;
        v = (float)lua_tonumber(L, -1);
      }
      lua_pop(L, 1);
    };
    checkFloat("left", n->hasLeft, n->leftVal);
    checkFloat("top", n->hasTop, n->topVal);
    checkFloat("right", n->hasRight, n->rightVal);
    checkFloat("bottom", n->hasBottom, n->bottomVal);
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
  n->textAlign = parseTextAlign(getString("textAlign", "left"));

  std::string pos = getString("position", "relative");
  if (pos == "absolute") {
    n->position = PositionType::Absolute;
  } else {
    n->position = PositionType::Relative;
  }


  std::string overflow = getString("overflow", "hidden");
  if (overflow == "visible") {
    n->overflowHidden = false;
    n->overflowScroll = false;
  } else if (overflow == "scroll" || overflow == "auto") {
    n->overflowHidden = true;
    n->overflowScroll = true;
  } else {
    n->overflowScroll = false;
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

  lua_getfield(L, idx, "draggable");
  if (lua_isboolean(L, -1)) {
    n->isDraggable = lua_toboolean(L, -1);
  }
  lua_pop(L, 1);

  parseEvents(L, n, idx);

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

static void renderNodePass(Node* n, RenderCommandList& list, float parentOffsetX,
    float parentOffsetY, bool isDragPass, bool isInsideDraggedNode) {

  bool treatAsDragged = isInsideDraggedNode || n->isDragging;

  // In the drag pass, skip nodes that aren't dragged (but search their children to find the dragged one)
  if (isDragPass && !treatAsDragged) {
    for (Node* c : n->children) {
      renderNodePass(c, list, parentOffsetX, parentOffsetY, isDragPass, false);
    }
    return; // Skip rendering the normal UI in the drag pass
  }

  // Accumulate offsets
  float totalOffsetX = parentOffsetX;
  float totalOffsetY = parentOffsetY;

  if (isDragPass) {
    totalOffsetX += n->dragOffsetX;
    totalOffsetY += n->dragOffsetY;
  }

  float renderX = n->x + totalOffsetX;
  float renderY = n->y + totalOffsetY;

  // Make the dragged ghost slightly transparent (70% opacity)
  float alphaMultiplier = (isDragPass && treatAsDragged) ? 0.7f : 1.0f;

  if (n->hasBackground) {
    list.push(DrawRectCommand{
        {renderX, renderY, n->w, n->h},
        {n->color.r, n->color.g, n->color.b, (uint8_t)(n->color.a * alphaMultiplier)}
        });
  }

  if (n->type == "text" && !n->computedLines.empty()) {
    Font* font = n->font ? n->font : UI_GetFontById(n->fontId);
    if (font) {
      n->font = font;
      float startX = renderX + n->paddingLeft;
      float cursorY = renderY + n->paddingTop + n->font->GetLogicalAscent();
      float contentWidth = n->w - (n->paddingLeft + n->paddingRight);

      for (const std::string& line : n->computedLines) {
        float lineWidth = 0;
        for (char c : line) {
          lineWidth += n->font->GetLogicalAdvance(c);
        }
        float xOffset = 0;
        if (n->textAlign == TextAlign::Center)  {
          xOffset = (contentWidth - lineWidth) / 2.0f;
        } else if (n->textAlign == TextAlign::Right) {
          xOffset = contentWidth - lineWidth;
        }

        // FIXED: Using Color instead of SDL_Color
        Color renderTextColor = {
          n->textColor.r, 
          n->textColor.g, 
          n->textColor.b, 
          (uint8_t)(n->textColor.a * alphaMultiplier)
        };

        list.push(DrawTextCommand{line, n->font, startX + xOffset, cursorY, renderTextColor, n->textDecoration});
        cursorY += n->computedLineHeight;
      }
    }
  }

  // Handle Clipping properly!
  bool applyClip = false;
  if (n->overflowHidden) {
    if (!isDragPass) {
      applyClip = true; // Clip normally for base UI
    } else if (treatAsDragged && !n->isDragging) {
      applyClip = true; // Clip inner children of the dragged node
    }
  }

  if (applyClip) {
    list.push(PushClipCommand{{renderX, renderY, n->w, n->h}});
  }

  // Render children
  for (Node* c : n->children) {
    renderNodePass(c, list, totalOffsetX, totalOffsetY, isDragPass, treatAsDragged);
  }

  // Render Scrollbars
  ScrollbarMetrics sb = n->getScrollbarMetrics();
  if (sb.isVisible && n->scrollbarOpacity > 0.0f) {
    uint8_t trackAlpha = (uint8_t)(180 * n->scrollbarOpacity * alphaMultiplier);
    uint8_t thumbAlpha = (uint8_t)(255 * n->scrollbarOpacity * alphaMultiplier);

    list.push(DrawRectCommand{
        {sb.trackX + totalOffsetX, sb.trackY + totalOffsetY, sb.trackW, sb.trackH},
        {0, 0, 0, trackAlpha},
        });

    float pad = 2.0f;
    list.push(DrawRectCommand{
        {sb.trackX + pad + totalOffsetX, sb.thumbY + pad + totalOffsetY, sb.trackW - (pad * 2.0f), sb.thumbH - (pad * 2.0f)},
        {180, 180, 180, thumbAlpha}
        });
  }

  if (applyClip) {
    list.push(PopClipCommand{});
  }
}

void generateRenderCommands(Node *n, RenderCommandList &list, float parentOffsetX, float parentOffsetY) {
  // PASS 1: Draw the base UI layer normally
  renderNodePass(n, list, parentOffsetX, parentOffsetY, false, false);

  // PASS 2: Draw the dragged elements on top of everything!
  renderNodePass(n, list, parentOffsetX, parentOffsetY, true, false);
}

void freeTree(lua_State* L, Node* n) {
  if (!n) return;

  if (n->onClickRef != -2) {
    luaL_unref(L, LUA_REGISTRYINDEX, n->onClickRef);
    n->onClickRef = -2;
  }
  if (n->onMouseEnterRef != -2) {
    luaL_unref(L, LUA_REGISTRYINDEX, n->onMouseEnterRef);
    n->onMouseEnterRef = -2;
  }
  if (n->onMouseLeaveRef != -2) {
    luaL_unref(L, LUA_REGISTRYINDEX, n->onMouseLeaveRef);
    n->onMouseLeaveRef = -2;
  }
  if (n->onRightClickRef != -2) {
    luaL_unref(L, LUA_REGISTRYINDEX, n->onRightClickRef);
    n->onRightClickRef = -2;
  }

  if (n->onDragStartRef != -2) {
    luaL_unref(L, LUA_REGISTRYINDEX, n->onDragStartRef);
    n->onDragStartRef = -2;
  }
  if (n->onDragRef != -2) {
    luaL_unref(L, LUA_REGISTRYINDEX, n->onDragRef);
    n->onDragRef = -2;
  }
  if (n->onDragEndRef != -2) {
    luaL_unref(L, LUA_REGISTRYINDEX, n->onDragEndRef);
    n->onDragEndRef = -2;
  }
  for (Node* c : n->children) {
    freeTree(L, c);
  }

  delete n;
}

//          ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
//          ╏                     TEXT PROCESSING                     ╏
//          ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛

static void appendCP(std::string& s, uint32_t cp) {
  if (cp <= 0x7F) {
    s += (char)cp;
  } else if (cp <= 0x7FF) {
    s += (char)(0xC0 | ((cp >> 6) & 0x1F));
    s += (char)(0x80 | (cp & 0x3F));
  } else if (cp <= 0xFFFF) {
    s += (char)(0xE0 | ((cp >> 12) & 0x0F));
    s += (char)(0x80 | ((cp >> 6) & 0x3F));
    s += (char)(0x80 | (cp & 0x3F));
  } else {
    s += (char)(0xF0 | ((cp >> 18) & 0x07));
    s += (char)(0x80 | ((cp >> 12) & 0x3F));
    s += (char)(0x80 | ((cp >> 6) & 0x3F));
    s += (char)(0x80 | (cp & 0x3F));
  }
}


TextLayoutResult calculateTextLayout(const std::string& text, Font* font, float maxWidth) {
  TextLayoutResult result;
  result.width = 0;
  result.height = 0;

  if (!font || text.empty()) return result;
  if (maxWidth <= 0) maxWidth = 1;

  float lineHeight = (float)font->GetLogicalLineHeight();
  float currentY = lineHeight;

  std::vector<uint32_t> codepoints = Font::DecodeUTF8(text);

  std::string currentLine;
  float currentLineWidth = 0.0f;
  std::string word;
  float wordWidth = 0.0f;

  for (size_t i = 0; i <= codepoints.size(); i++) {
    uint32_t c = (i < codepoints.size()) ? codepoints[i] : 0;

    if (c == ' ' || c == '\n' || c == 0) {
      float spaceW = (c == ' ') ? (font->GetCharacter(' ').Advance / 64.0f) : 0;

      if (currentLineWidth + wordWidth <= maxWidth) {
        currentLine += word;
        currentLineWidth += wordWidth;
      } else {
        result.lines.push_back(currentLine);
        result.width = std::max(result.width, currentLineWidth); // Track max width
        currentLine = word;
        currentLineWidth = wordWidth;
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
      wordWidth = 0;
    } else {
      appendCP(word, c);
      wordWidth += (font->GetLogicalAdvance(c));
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
  Font* font = n->font ? n->font : UI_GetFontById(n->fontId);
  n->font = font;

  if (n->type != "text" || !n->font || n->text.empty()) {
    n->computedLines.clear();
    return;
  }

  float maxWidth = n->w - (n->paddingLeft + n->paddingRight);
  TextLayoutResult res = calculateTextLayout(n->text, n->font, maxWidth);

  n->computedLines = res.lines;
  n->computedLineHeight = (float)n->font->GetLogicalLineHeight();
}

void updateTextLayout(Node* root) {
  if (!root) return;
  computeTextLayout(root);
  for (Node* c : root->children) {
    updateTextLayout(c);
  }
}


void UI_UpdateSmoothScrolling(Node *n, float dt) {
  if (!n) return;

  if (n->overflowScroll) {

    float maxScrollY = std::max(0.0f, n->contentH - n->h);
    float maxScrollX = std::max(0.0f, n->contentW - n->w);

    n->targetScrollY = std::clamp(n->targetScrollY, 0.0f, maxScrollY);
    n->targetScrollX = std::clamp(n->targetScrollX, 0.0f, maxScrollX);

    if (n->scrollbarTimer > 0.0f) {
      n->scrollbarTimer -= dt;
      n->scrollbarOpacity += 8.0f * dt;
      if (n->scrollbarOpacity > 1.0f) n->scrollbarOpacity = 1.0f;
    } else {
      n->scrollbarOpacity -= 2.0f * dt; // Fade out slower
      if (n->scrollbarOpacity < 0.0f) n->scrollbarOpacity = 0.0f;
    }

    float diffY = n->targetScrollY - n->scrollY;
    float diffX = n->targetScrollX - n->scrollX;

    bool needsLayout = false;

    if (std::abs(diffY) > 0.5f) {
      n->scrollY += diffY * 15.0f * dt;
      needsLayout = true;
    } else if (n->scrollY != n->targetScrollY) {
      n->scrollY = n->targetScrollY;
      needsLayout = true;
    }

    if (std::abs(diffX) > 0.5f) {
      n->scrollX += diffX * 15.0f * dt;
      needsLayout = true;
    } else if (n->scrollX != n->targetScrollX) {
      n->scrollX = n->targetScrollX;
      needsLayout = true;
    }

    if (needsLayout) {
      n->makeLayoutDirty();
    }
  }
  for (Node* c : n->children) {
    UI_UpdateSmoothScrolling(c, dt);
  }

}


ScrollbarMetrics Node::getScrollbarMetrics() {
  ScrollbarMetrics m;
  m.isVisible = false;

  if (!this->overflowScroll || this->contentH <= this->h) {
    return m;
  }
  m.isVisible = true;
  m.trackW = 10.0f;
  m.trackX = this->x + this->w - m.trackW;
  m.trackY = this->y;
  m.trackH = this->h;

  float thumbMinH = 20.0f;
  float visibleRatio = this->h / this->contentH;
  m.thumbH = std::max(thumbMinH, visibleRatio * this->h);

  m.maxScrollY = this->contentH - this->h;
  float scrollPct = this->scrollY / m.maxScrollY;

  m.thumbY = this->y + (scrollPct * (this->h - m.thumbH));

  return m;
}





