#include "ui.h"
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL_timer.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <lauxlib.h>
#include <lua.h>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>
#include "../color/color.h"
#include "../vdom/vdom.h"
#include "../text/font.h"
#include "../../configLogic/font/font_registry.h"
#include "../../configLogic/engineConf/engine_config.h"
#include "../../configLogic/font/font_registry.h"
#include "../../configLogic/images/texture_registry.h"
#include "../input/input.h"

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
    len.isSet = true;
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

  getCallback("onTextInput", n->onTextInputRef);
  getCallback("onKeyDown", n->onKeyDownRef);
  getCallback("onFocus", n->onFocusRef);
  getCallback("onBlur", n->onBlurRef);
  
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

void NodeParseImage(Node* n, lua_State* L, int idx) {
  lua_getfield(L, idx, "src");
  if (lua_isstring(L, -1)) {
    n->src = lua_tostring(L, -1);
    n->textureId = TextureRegistry::GetTexture(n->src);
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
    n->codepoints = Font::DecodeUTF8(n->text);
  }
  lua_pop(L, 1);

  if (n->type == "image") {
    NodeParseImage(n, L, idx);
  }

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
    
    n->opacity = getFloat("opacity", 1.0f);

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
  n->objectFit = getString("fit", "fill");
  std::string pos = getString("position", "relative");
  if (pos == "absolute") {
    n->position = PositionType::Absolute;
  } else {
    n->position = PositionType::Relative;
  }


  std::string overflow = getString("overflow", "visible");
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

  n->wordWrap = true;
  if (hasStyle) {
    lua_getfield(L, -1, "wordWrap");
    if (lua_isboolean(L, -1)) n->wordWrap = lua_toboolean(L, -1);
    lua_pop(L, 1);
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

    lua_getfield(L, -1, "BGImage");
    if (lua_isstring(L, -1)) {
      n->bgImageSrc = lua_tostring(L, -1);
      n->bgTextureId = TextureRegistry::GetTexture(n->bgImageSrc);
    }
    lua_pop(L, 1);

    n->bgImageFit = getString("BGFit", "cover");


  }

  lua_pop(L, 1);

  lua_getfield(L, idx, "draggable");
  if (lua_isboolean(L, -1)) {
    n->isDraggable = lua_toboolean(L, -1);
  }
  lua_pop(L, 1);

  parseEvents(L, n, idx);

  lua_getfield(L, idx, "focusable");
  if (lua_isboolean(L, -1)) n->isFocusable = lua_toboolean(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, idx, "isFocused");
  if (lua_isboolean(L, -1)) n->isFocused = lua_toboolean(L, -1);
  lua_pop(L, 1);


  lua_getfield(L, idx, "cursorPosition");
  if (lua_isnumber(L, -1)) n->cursorPosition = lua_tointeger(L, -1);
  lua_pop(L, 1);

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

  if (n->widthStyle.isSet) {
    n->w = n->widthStyle.resolve((float)parentW);
  }

  if (n->heightStyle.isSet) {
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

void computeTextLayout(Node* n);

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

    n->contentH = totalH + n->paddingTop + n->paddingBottom;
    n->contentW = maxW + n->paddingLeft + n->paddingRight;

    if (n->w == 0) n->w = maxW + n->paddingLeft + n->paddingRight;
    if (n->h == 0 && !n->overflowScroll) n->h = totalH + n->paddingTop + n->paddingBottom;
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

    n->contentW = totalW + n->paddingLeft + n->paddingRight;
    n->contentH = maxH + n->paddingTop + n->paddingBottom;

    if (n->w == 0 && !n->overflowScroll) n->w = totalW + n->paddingRight + n->paddingLeft;
    if (n->h == 0) n->h = maxH + n->paddingTop + n->paddingBottom;
  }

  if (n->type == "text") {
    computeTextLayout(n);

    // If height isn't explicitly fixed in Lua, calculate it from the lines
    if (n->heightStyle.value == 0) {
      n->h = (n->computedLines.size() * n->computedLineHeight) + n->paddingTop + n->paddingBottom;
    }
    // If width isn't fixed, you can also calculate max line width here
    if (n->widthStyle.value == 0) {
      // (Optional logic to set n->w based on longest line)
    }
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


void TranslateRenderCommand(RenderCommand& cmd, float dx, float dy) {
  std::visit([dx, dy](auto& c){
    using T = std::decay_t<decltype(c)>;
    if constexpr (std::is_same_v<T, DrawRectCommand>) {
      c.rect.x += dx;
      c.rect.y += dy;
    } else if constexpr (std::is_same_v<T, DrawTextCommand>) {
      c.x += dx;
      c.y += dy;
    } else if constexpr (std::is_same_v<T, DrawImageCommand>) {
      c.rect.x += dx;
      c.rect.y += dy;
    } else if constexpr (std::is_same_v<T, PushClipCommand>) {
      c.rect.x += dx;
      c.rect.y += dy;
    }
  }, cmd);
}


static void renderNodePass(Node* n, RenderCommandList& list, float parentOffsetX,
    float parentOffsetY, bool isDragPass, bool isInsideDraggedNode, float parentAlpha) {

  float totalOffsetX = parentOffsetX;
  float totalOffsetY = parentOffsetY;

  if (isDragPass) {
    totalOffsetX += n->dragOffsetX;
    totalOffsetY += n->dragOffsetY;
  }


  // ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
  // ╏ IF CLEAN, DUMP CACHE AND RETURN INSTANTLY ╏
  // ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
  if (!isDragPass && !n->isPaintDirty && n->hasCachedCommands) {
    float dx = totalOffsetX - n->cachedOffsetX;
    float dy = totalOffsetY - n->cachedOffsetY;

    if (dx != 0.0f || dy != 0.0f) {
      for (auto& cmd : n->cachedCommands.commands) {
        TranslateRenderCommand(cmd, dx, dy);
      }

      n->cachedOffsetX = totalOffsetX;
      n->cachedOffsetY = totalOffsetY;
    }

    for (const auto& cmd : n->cachedCommands.commands) {
      list.push(cmd);
    }
    return;
  }


  float currentAlpha = parentAlpha * n->opacity;
  bool treatAsDragged = isInsideDraggedNode || (n->isDragging && n->isDraggable);
  float alphaMultiplier = (isDragPass && treatAsDragged) ? (currentAlpha * 0.7f) : currentAlpha;


  if (isDragPass && !treatAsDragged) {
    for (Node* c : n->children) {
      renderNodePass(c, list, totalOffsetX - n->scrollX, totalOffsetY - n->scrollY, isDragPass, false, parentAlpha);
    }
    return;
  }

  size_t startIndex = list.commands.size();

  // Accumulate offsets

  float renderX = n->x + totalOffsetX;
  float renderY = n->y + totalOffsetY;



  if (n->hasBackground) {
    list.push(DrawRectCommand{
        {renderX, renderY, n->w, n->h},
        {n->color.r, n->color.g, n->color.b, (uint8_t)(n->color.a * alphaMultiplier)}
        });
  }

  if (n->bgTextureId != 0) {
    float uMin = 0.0f, vMin = 0.0f, uMax = 1.0f, vMax = 1.0f;
    float drawX = renderX, drawY = renderY, drawW = n->w, drawH = n->h;

    int texW = 0, texH = 0;
    TextureRegistry::GetTextureDimensions(n->bgTextureId, texW, texH);

    if (texW > 0 && texH > 0) {
      float imgAspect = (float)texW / (float)texH;
      float boxAspect = n->w / n->h;

      if (n->bgImageFit == "cover") {
        if (imgAspect > boxAspect) {
          float scaledW = n->h * imgAspect;
          float crop = (scaledW - n->w) / 2.0f;
          uMin = crop / scaledW;
          uMax = 1.0f - uMin;
        } else {
          float scaledH = n->w / imgAspect;
          float crop = (scaledH - n->h) / 2.0f;
          vMin = crop / scaledH;
          vMax = 1.0f - vMin;
        }
      } else if (n->bgImageFit == "contain") {
        if (imgAspect > boxAspect) {
          drawW = n->w;
          drawH = n->w / imgAspect;
          drawY += (n->h - drawH) / 2.0f;
        } else {
          drawH = n->h;
          drawW = n->h * imgAspect;
          drawX += (n->w - drawW) / 2.0f; 
        }
      }
    }

    list.push(DrawImageCommand{
        {drawX, drawY, drawW, drawH},
        n->bgTextureId,
        {255, 255, 255, (uint8_t)(255 * alphaMultiplier)},
        uMin, vMin, uMax, vMax
        });
  }


  bool applyClip = false;
  if (n->overflowHidden) {
    if (!isDragPass || treatAsDragged) {
      applyClip = true;
    }
  }

  if (applyClip) {
    list.push(PushClipCommand{{renderX, renderY, n->w, n->h}});
  }

  if (n->type == "image" && n->textureId != 0) {
    Color imageTint = {255, 255, 255, (uint8_t)(255 * alphaMultiplier)};

    float uMin = 0.0f, vMin = 0.0f, uMax = 1.0f, vMax = 1.0f;
    float drawX = renderX, drawY = renderY, drawW = n->w, drawH = n->h;

    int texW = 0, texH = 0;
    TextureRegistry::GetTextureDimensions(n->textureId, texW, texH);

    if (texW > 0 && texH > 0) {
      float imgAspect = (float)texW / (float)texH;
      float boxAspect = n->w / n->h;

      if (n->objectFit == "cover") {
        if (imgAspect > boxAspect) {
          float scaledW = n->h * imgAspect;
          float crop = (scaledW - n->w) / 2.0f;
          uMin = crop / scaledW;
          uMax = 1.0f - uMin;
        } else {
          float scaledH = n->w / imgAspect;
          float crop = (scaledH - n->h) / 2.0f;
          vMin = crop / scaledH;
          vMax = 1.0f - vMin;
        }
      } else if (n->objectFit == "contain") {
        if (imgAspect > boxAspect) {
          drawW = n->w;
          drawH = n->w / imgAspect;
          drawY += (n->h - drawH) / 2.0f;
        } else {
          drawH = n->h;
          drawW = n->h * imgAspect;
          drawX += (n->w - drawW) / 2.0f; 
        }
      }
    }


    list.push(DrawImageCommand{
        {drawX, drawY, drawW, drawH},
        n->textureId,
        imageTint,
        uMin, vMin, uMax, vMax
        });
  }


  if (n->type == "text") {
    Font* font = n->font ? n->font : UI_GetFontById(n->fontId);
    if (font) {
      n->font = font;
      float contentWidth = n->w - (n->paddingLeft + n->paddingRight);
      float contentHeight = n->h - (n->paddingTop + n->paddingBottom);
      const std::vector<uint32_t>& codepoints = n->codepoints;

      float startX = renderX + n->paddingLeft - n->scrollX;
      float cursorY = renderY + n->paddingTop + n->font->GetLogicalAscent() - n->scrollY;

      int selMin = -1, selMax = -1;
      if (n->selectionStart >= 0 && n->selectionEnd >= 0 && n->selectionStart != n->selectionEnd) {
          selMin = std::min(n->selectionStart, n->selectionEnd);
          selMax = std::max(n->selectionStart, n->selectionEnd);
      }

      for (size_t lineIdx = 0; lineIdx < n->computedLines.size(); ++lineIdx) {
        const TextLine& line = n->computedLines[lineIdx];
        float lineXOffset = 0;
        
        if (n->wordWrap) {
          if (n->textAlign == TextAlign::Center)  lineXOffset = (contentWidth - line.width) / 2.0f;
          else if (n->textAlign == TextAlign::Right) lineXOffset = contentWidth - line.width;
        }

        uint32_t lineEndIdx = line.startIndex + line.count;

        // 1. DRAW MULTILINE SELECTION HIGHLIGHTS
        if (selMin >= 0) {
            if (selMin < (int)lineEndIdx && selMax > (int)line.startIndex) {
                uint32_t localStart = std::max((uint32_t)0, selMin > (int)line.startIndex ? selMin - line.startIndex : 0);
                uint32_t localEnd = std::min(line.count, (uint32_t)(selMax - line.startIndex));

                float selOffsetX = 0;
                float selWidth = 0;
                for (uint32_t i = 0; i < line.count; i++) {
                    float adv = font->GetLogicalAdvance(codepoints[line.startIndex + i]);
                    if (i < localStart) selOffsetX += adv;
                    else if (i < localEnd) selWidth += adv;
                }

                list.push(DrawRectCommand{
                    {startX + lineXOffset + selOffsetX, cursorY - font->GetLogicalAscent(), selWidth, n->computedLineHeight},
                    {59, 130, 246, 128} // Blue highlight
                });
            }
        }

        // 2. DRAW BLINKING CURSOR & HANDLE 2D AUTO-SCROLLING
        if (n->cursorPosition >= 0 && n->cursorPosition >= (int)line.startIndex && n->cursorPosition <= (int)lineEndIdx) {
            // Prevent drawing the cursor twice if it's at the boundary between two lines
            bool isLastLine = (lineIdx == n->computedLines.size() - 1);
            if (n->cursorPosition < (int)lineEndIdx || isLastLine || codepoints[n->cursorPosition-1] == '\n') {
                float cursorOffsetX = 0;
                uint32_t localCursor = n->cursorPosition - line.startIndex;
                for (uint32_t i = 0; i < localCursor; i++) {
                    cursorOffsetX += font->GetLogicalAdvance(codepoints[line.startIndex + i]);
                }
                
                if (n->cursorPosition != n->lastCursorPosition) {

                Node* scroller = n;
                while (scroller && !scroller->overflowScroll) {
                  scroller = scroller->parent;
                }

                if (scroller) {
                  float sContentW = scroller->w - scroller->paddingLeft - scroller->paddingRight;
                  float sContentH = scroller->h - scroller->paddingTop - scroller->paddingBottom;


                  // Horizontal Auto-Scroll mapped to the parent
                  float absoluteCursorX = (n->x - scroller->x - scroller->paddingLeft) + n->paddingLeft + lineXOffset + cursorOffsetX;
                  if (absoluteCursorX < scroller->targetScrollX) {
                    scroller->targetScrollX = absoluteCursorX;
                  } else if (absoluteCursorX > scroller->targetScrollX + sContentW) {
                    scroller->targetScrollX = absoluteCursorX - sContentW + 1.0f;
                  }
                  if (scroller->targetScrollX < 0) scroller->targetScrollX = 0;

                  // Vertical Auto-Scroll mapped to absolute layout coordinates
                  float absoluteCursorY = (n->y - scroller->y - scroller->paddingTop) + n->paddingTop + (lineIdx * n->computedLineHeight);
                  if (absoluteCursorY < scroller->targetScrollY) {
                    scroller->targetScrollY = absoluteCursorY; // Push view up
                  } else if (absoluteCursorY + n->computedLineHeight > scroller->targetScrollY + sContentH) {
                    scroller->targetScrollY = absoluteCursorY + n->computedLineHeight - sContentH; // Push view down
                  }
                }
                n->lastCursorPosition = n->cursorPosition;
              }


                // Draw the actual cursor rect
                bool showCursor = ((SDL_GetTicks() - Input::lastInputTime) % 1000) < 500;
                if (showCursor) {
                    list.push(DrawRectCommand{
                        {startX + lineXOffset + cursorOffsetX, cursorY - font->GetLogicalAscent(), 1.0f, n->computedLineHeight},
                        {n->textColor.r, n->textColor.g, n->textColor.b, 255}
                    });
                }
            }
        }

        // 3. DRAW TEXT LINE
        std::string lineStr = "";
        for (uint32_t i = 0; i < line.count; i++) {
            appendCP(lineStr, codepoints[line.startIndex + i]);
        }

        Color renderTextColor = {
          n->textColor.r, n->textColor.g, n->textColor.b,
          (uint8_t)(n->textColor.a * alphaMultiplier)
        };

        list.push(DrawTextCommand{lineStr, n->font, startX + lineXOffset, cursorY, renderTextColor, n->textDecoration});

        cursorY += n->computedLineHeight;
      }
    }
  }

  for (Node* c : n->children) {
    renderNodePass(c, list, totalOffsetX - n->scrollX, totalOffsetY - n->scrollY, isDragPass, treatAsDragged, parentAlpha);
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

  // ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
  // ╏ CAPTURE GENERATED COMMANDS TO RAM ╏
  // ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
  if (!isDragPass) {
    n->cachedCommands.commands.clear();
    for (size_t i = startIndex; i < list.commands.size(); i++) {
      n->cachedCommands.commands.push_back(list.commands[i]);
    }
    n->hasCachedCommands = true;
    n->isPaintDirty = false;

    n->cachedOffsetX = totalOffsetX;
    n->cachedOffsetY = totalOffsetY;
  }

}

void generateRenderCommands(Node *n, RenderCommandList &list, float parentOffsetX, float parentOffsetY) {
  // PASS 1: Draw the base UI layer normally
  renderNodePass(n, list, parentOffsetX, parentOffsetY, false, false, 1.0f);

  // PASS 2: Draw the dragged elements on top of everything!
  renderNodePass(n, list, parentOffsetX, parentOffsetY, true, false, 1.0f);
}

void freeTree(lua_State* L, Node* n) {
  if (!n) return;

  Input::clearNodeState(n);

  if (n->type == "image" && n->textureId != 0) {
    TextureRegistry::ReleaseTexture(n->textureId);
    n->textureId = 0;
  }

  if (n->bgTextureId != 0) {
    TextureRegistry::ReleaseTexture(n->bgTextureId);
    n->bgTextureId = 0;
  }

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
  if (n->onTextInputRef != -2) {
    luaL_unref(L, LUA_REGISTRYINDEX, n->onTextInputRef);
    n->onTextInputRef = -2;
  }
  if (n->onKeyDownRef != -2) {
    luaL_unref(L, LUA_REGISTRYINDEX, n->onKeyDownRef);
    n->onKeyDownRef = -2;
  }
  if (n->onFocusRef != -2) {
    luaL_unref(L, LUA_REGISTRYINDEX, n->onFocusRef);
    n->onFocusRef = -2;
  }
  if (n->onBlurRef != -2) {
    luaL_unref(L, LUA_REGISTRYINDEX, n->onBlurRef);
    n->onBlurRef = -2;
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
  if (maxWidth <= 0) maxWidth = 1.0f;

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
      float spaceW = (c == ' ') ? font->GetLogicalAdvance(' ') : 0.0f;

      if (currentLineWidth + wordWidth <= maxWidth) {
        currentLine += word;
        currentLineWidth += wordWidth;
      } else {
        if (!currentLine.empty()) {
          result.lines.push_back(currentLine);
          result.width = std::max(result.width, currentLineWidth);
          currentLine = "";
          currentLineWidth = 0.0f;
          currentY += lineHeight;
        }
        currentLine += word;
        currentLineWidth += wordWidth;
      }

      if (c == ' ') {
        if (currentLineWidth + spaceW <= maxWidth) {
          currentLine += ' ';
          currentLineWidth += spaceW;
        }
      } else if (c == '\n') {
        result.lines.push_back(currentLine);
        result.width = std::max(result.width, currentLineWidth);
        currentLine = "";
        currentLineWidth = 0.0f;
        currentY += lineHeight;
      }
      word.clear();
      wordWidth = 0.0f;
    } else {
      float charAdvance = font->GetLogicalAdvance(c);

      if (wordWidth > 0 && wordWidth + charAdvance > maxWidth) {
        if (!currentLine.empty()) {
          result.lines.push_back(currentLine);
          result.width = std::max(result.width, currentLineWidth);
          currentLine = "";
          currentLineWidth = 0.0f;
          currentY += lineHeight;
        }

        result.lines.push_back(word);
        result.width = std::max(result.width, wordWidth);
        word.clear();
        wordWidth = 0.0f;
        currentY += lineHeight;
      }

      appendCP(word, c);
      wordWidth += charAdvance;

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

  if (n->type != "text" || !n->font) {
    n->computedLines.clear();
    return;
  }

  n->computedLineHeight = (float)n->font->GetLogicalLineHeight();

  float maxWidth = n->wordWrap ? (n->w - (n->paddingLeft + n->paddingRight)) : 999999.0f;

  const auto& codepoints = n->codepoints;
  n->computedLines = n->font->CalculateWordWrap(codepoints, maxWidth);

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
      n->makePaintDirty();
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


DamageRect g_damageTracker;

void DamageRect::add(float nx, float ny, float nw, float nh) {
  if (fullScreen) return;

  // expand the region a little bit for better result
  nx -= 2.0f; ny -= 2.0f; nw += 4.0f; nh += 4.0f;

  if (!active) {
    x = nx; y = ny; w = nw; h = nh;
    active = true;
  } else {
    float right = std::max(x + w, nx + nw);
    float bottom = std::max(y + h, ny + nh);
    x = std::min(x, nx);
    y = std::min(y, ny);
    w = right - x;
    h = bottom - y;
  }
  framesLeft = 2;
}

void DamageRect::damageAll() {
  fullScreen = true;
  active = true;
  framesLeft = 2;
}

void DamageRect::update() {
  if (framesLeft > 0) framesLeft--;
  if (framesLeft == 0) {
    active = false;
    fullScreen = false;
  }

}



