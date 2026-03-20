#include "font.h"
#include <SDL_gamecontroller.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <freetype/fttypes.h>
#include <iostream>
#include <ft2build.h>
#include <lauxlib.h>
#include <limits>
#include <memory>
#include <ostream>
#include <sys/types.h>
#include <utility>
#include <vector>
#include "../ui/ui.h"
#include "core/BitmapRef.hpp"
#include "core/Shape.h"
#include "core/Vector2.hpp"
#include "core/edge-coloring.h"
#include "ext/import-font.h"
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include "../system/pathUtils.h"
#include "../../scripting/regsitry.h"

#include <msdfgen.h>
#include <msdfgen-ext.h>

// file local global font storage
static std::unordered_map<int, std::unique_ptr<Font>> g_fonts;
static int g_nextFontId = 1;

static msdfgen::FreetypeHandle* g_msdfFt = nullptr;
static std::unordered_map<Font*, msdfgen::FontHandle*> g_msdfFonts;

// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ DPI SCALING HELPERS ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
static float g_dpiscale = 1.0f;

void UI_SetDPIScale(float scale) {
  g_dpiscale = scale;
}

float UI_GetDPIScale() {
  return g_dpiscale;
}



// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ FALLBACK FONT MANAGEMENT ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛

// fallback font storage
static std::vector<Font*> s_fallbackfonts;


void Font::AddFallback(Font* font) {
  for (auto* f : s_fallbackfonts) {
    if (f == font) return;
  }
  s_fallbackfonts.push_back(font);
}

void Font::ResetFallback() {
  s_fallbackfonts.clear();
}

const std::vector<Font*>& Font::GetFallbacks() {
  return s_fallbackfonts;
}



//          ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
//          ╏                         HELPERS                         ╏
//          ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛

// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ UTF-8 PARSER ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
std::vector<uint32_t> Font::DecodeUTF8(const std::string& text) {
    std::vector<uint32_t> codepoints;
    for (size_t i = 0; i < text.length();) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        uint32_t codepoint = 0;
        int bytes = 0;

        if      (c <= 0x7F) { codepoint = c; bytes = 1; }
        else if (c <= 0xDF) { codepoint = c & 0x1F; bytes = 2; }
        else if (c <= 0xEF) { codepoint = c & 0x0F; bytes = 3; }
        else if (c <= 0xF7) { codepoint = c & 0x07; bytes = 4; }
        else                { bytes = 1; } // Invalid, skip

        for (int j = 1; j < bytes; ++j) {
            if (i + j < text.length()) {
                codepoint = (codepoint << 6) | (text[i + j] & 0x3F);
            }
        }
        codepoints.push_back(codepoint);
        i += bytes;
    }
    return codepoints;
}

// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ HELPER TO GET FONT POINTER ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
Font* UI_GetFontById(int id) {
  auto it = g_fonts.find(id);
  if (it == g_fonts.end()) return nullptr;
  return it->second.get();
}



//          ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
//          ╏                 FONT CLASS CONSTRUCTORS                 ╏
//          ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
Font::Font(const std::string& fontPath, unsigned int requestedFontSize, int styleFlags) : 
  lineHeight(0), ascent(0), fontPath(fontPath), fontSize(requestedFontSize), styleFlags(styleFlags), logicalSize(requestedFontSize) {

  this->dpiScale = UI_GetDPIScale();

  unsigned int physicalSize = std::round(requestedFontSize * dpiScale);
  Load(fontPath, physicalSize);
}

float Font::GetLogicalAdvance(uint32_t c) {
  return GetCharacter(c).Advance;
}

Font::~Font() {

  for (unsigned int pageID : atlasPages) {
    glDeleteTextures(1, &pageID);
  }

  if (ftFace) {
    FT_Done_Face((FT_Face)ftFace);
  }

  if (g_msdfFonts.count(this)) {
    msdfgen::destroyFont(g_msdfFonts[this]);
    g_msdfFonts.erase(this);
  }
}





// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ MAIN FUNCTION TO GET FONT POINTER AND ITS ID FROM GLOBAL FONT STORAGE ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
std::pair<int, Font*> UI_LoadFont(const std::string &path, int logicalSize, int styleFlags) {
  for (const auto& [id, font] : g_fonts ) {
    if (font->GetPath() == path && font->GetLogicalSize() == logicalSize &&  font->GetStyle() == styleFlags) {
      return {id, font.get()};
    }
  }

  int id = g_nextFontId++;
  auto font = std::make_unique<Font>(path, logicalSize, styleFlags);
  Font* fontptr = font.get();
  g_fonts[id] = std::move(font);

  return {id, fontptr};
}


void Font::AllocateAtlasPage() {
  GLuint newTex;
  glGenTextures(1, &newTex);
  glBindTexture(GL_TEXTURE_2D, newTex);
  
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, atlasWidth, atlasHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

  // Put a white pixel at (0,0)
  unsigned char whitePixel[3] = {255, 255, 255};
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, whitePixel);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  atlasPages.push_back(newTex);
  currentAtlasPage = newTex;

  // Reset packing cursors for the new page
  atlasOffsetX = 1;
  atlasOffsetY = 1;
  atlasRowHeight = 0;
}



// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ MAIN FUNCTION RESPONSIBLE FOR LOADING OUR FONT ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛

static FT_Library g_ftLib = nullptr;

void Font::Load(const std::string& path, unsigned int size) {
  if (g_ftLib == nullptr) {
    if (FT_Init_FreeType(&g_ftLib)) {
      std::cerr << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
      return;
    }
  }

  if (g_msdfFt == nullptr) {
    g_msdfFt = msdfgen::initializeFreetype();
  }

  std::string fullpath = Vulpis::getAssetPath(path);
  if (!std::filesystem::exists(fullpath)) {
    std::cerr << "!!! FATAL ERROR !!!" << std::endl;
    std::cerr << "Asset missing at: " << fullpath << std::endl;
    std::cerr << "Current Working Dir: " << std::filesystem::current_path() << std::endl;
    return;
  }

  FT_Face face;
  if (FT_New_Face(g_ftLib, fullpath.c_str(), 0, &face)) {
    std::cerr << "ERROR::FREETYPE: Failed to load font: " << fullpath << std::endl;
    return;
  }

  ftFace = face;

  FT_Set_Pixel_Sizes(face, 0, size);
  this->lineHeight = face->size->metrics.height >> 6;
  this->ascent = face->size->metrics.ascender >> 6;

  g_msdfFonts[this] = msdfgen::loadFont(g_msdfFt, fullpath.c_str());

  AllocateAtlasPage();
}


const Character* Font::LoadGlyph(uint32_t c) {
  if (!ftFace) return nullptr;
  FT_Face face = (FT_Face)ftFace;

  if (c != '?' && FT_Get_Char_Index(face, c) == 0) return nullptr;

  int loadFlags = FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING;
  if (FT_Load_Char(face, c, loadFlags)) return nullptr;

  float logicalAdvance = (float)(face->glyph->advance.x >> 6) / this->dpiScale;

  int logicalW = face->glyph->metrics.width >> 6;
  int logicalH = face->glyph->metrics.height >> 6;

  // Early exit & Cache for whitespace
  if (logicalW == 0 && logicalH == 0) {
    Character character = { currentAtlasPage, 0, 0, 0, 0, logicalAdvance, 0.0f, 0.0f, 0.0f, 0.0f };
    return &characters.emplace(c, character).first->second;
  }

  // Fallback if MSDFgen fails
  auto cacheEmptyChar = [&]() {
    Character character = { currentAtlasPage, 0, 0, 0, 0, logicalAdvance, 0.0f, 0.0f, 0.0f, 0.0f };
    return &characters.emplace(c, character).first->second;
  };

  msdfgen::FontHandle* msdfFont = g_msdfFonts[this];
  if (!msdfFont) return cacheEmptyChar();

  msdfgen::Shape shape;
  if (!msdfgen::loadGlyph(shape, msdfFont, c)) return cacheEmptyChar();

  shape.normalize();
  if (shape.contours.empty()) return cacheEmptyChar();

  msdfgen::edgeColoringByDistance(shape, 3.0);
  msdfgen::Shape::Bounds bounds = shape.getBounds();

  msdfgen::FontMetrics metrics;
  msdfgen::getFontMetrics(metrics, msdfFont);
  double emSize = metrics.emSize > 0 ? metrics.emSize : (face->units_per_EM > 0 ? face->units_per_EM : 2048.0);

  double genSize = 72.0f;
  double scale = genSize / emSize;
  
  double pxRange = 8.0f;
  double shapeRange = pxRange / scale;
  int padding = 8;

  msdfgen::Vector2 translate(-bounds.l + padding / scale, -bounds.b + padding / scale);

  int glyphW = (int)std::ceil((bounds.r - bounds.l) * scale) + padding * 2;
  int glyphH = (int)std::ceil((bounds.t - bounds.b) * scale) + padding * 2;


  if (glyphW <= 0 || glyphH <= 0 || glyphW > 512 || glyphH > 512) {
    return cacheEmptyChar();
  }

  msdfgen::Bitmap<float, 3> msdf(glyphW, glyphH);
  msdfgen::generateMSDF(msdf, shape, shapeRange, scale, translate);

  std::vector<unsigned char> pixels(glyphW * glyphH * 3);
  for (int y = 0; y < glyphH; ++y) {
    for (int x = 0; x < glyphW; ++x) {
      int index = 3 * (y * glyphW + x);
      const float* px = msdf(x, glyphH - 1 - y); // Flip Y for OpenGL
      pixels[index + 0] = (unsigned char)std::clamp(px[0] * 255.0f, 0.0f, 255.0f);
      pixels[index + 1] = (unsigned char)std::clamp(px[1] * 255.0f, 0.0f, 255.0f);
      pixels[index + 2] = (unsigned char)std::clamp(px[2] * 255.0f, 0.0f, 255.0f);
    }
  }

  if (atlasOffsetX + glyphW + 1 >= atlasWidth) {
    atlasOffsetY += atlasRowHeight + 1;
    atlasOffsetX = 1;
    atlasRowHeight = 0;
  }

  if (atlasOffsetY + glyphH + 1 >= atlasHeight) {
    if (glyphW + 1 >= atlasWidth || glyphH + 1 >= atlasHeight) {
      std::cerr << "ERROR: Glyph " << c << " is too large for the atlas!" << std::endl;
      return cacheEmptyChar();
    }
    AllocateAtlasPage();
  }

  glBindTexture(GL_TEXTURE_2D, currentAtlasPage);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(GL_TEXTURE_2D, 0, atlasOffsetX, atlasOffsetY, glyphW, glyphH, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

  float uMin = (float)atlasOffsetX / (float) atlasWidth;
  float vMin = (float)atlasOffsetY / (float) atlasHeight;
  float uMax = (float)(atlasOffsetX + glyphW) / (float) atlasWidth;
  float vMax = (float)(atlasOffsetY + glyphH) / (float) atlasHeight;

  double physicalFontSize = (double)this->fontSize * (double)this->dpiScale;
  double layoutScale = (double)this->fontSize / emSize;
  double ratio = layoutScale / scale;

float quadW = (float)(glyphW * ratio);
  float quadH = (float)(glyphH * ratio);

  float bearingX = (float)(bounds.l * layoutScale) - (float)(padding * ratio);
  float bearingY = (float)(bounds.t * layoutScale) + (float)(padding * ratio);

  Character character = {
    currentAtlasPage, quadW, quadH, bearingX, bearingY,
    logicalAdvance,
    uMin, vMin, uMax, vMax
  };

  atlasOffsetX += glyphW + 1;
  atlasRowHeight = std::max(atlasRowHeight, (unsigned int)glyphH);

  return &characters.emplace(c, character).first->second;
}


// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ LOGIC FOR LOADING CACHED CHARACTERS ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
const Character* Font::GetCharInternal(uint32_t c)  {
  auto it = characters.find(c);
  if (it != characters.end()) {
    return &it->second;
  }
  return LoadGlyph(c);
}


// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ MAIN CHARACTER FUNCTION THAT ALSO CHECKS IN FALLBACK ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
const Character& Font::GetCharacter(uint32_t c) {

  const Character* ch = GetCharInternal(c);
  if (ch) return *ch;

  for (Font* fallback : s_fallbackfonts) {
    if (fallback == this) continue;

    const Character* fbChar = fallback->GetCharInternal(c);
    if (fbChar) return *fbChar;
  }

  auto fb = characters.find('?');
  if (fb != characters.end()) {
    return fb->second;
  } else {
    const Character* q = LoadGlyph('?');
    if (q) return *q;

  }
  static Character emergency = {};
  return emergency;
}




// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ load_text() function to load font using path and size ╏
// ╏ this will be called from lua                          ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛

int l_load_font(lua_State* L) {
  const GLubyte* ver = glGetString(GL_VERSION);
  if (!ver) {
    return luaL_error(L, "ERROR: load_font() called before OpenGL context is ready");
  }

  const char* path = luaL_checkstring(L, 1);
  int baseSize = luaL_optinteger(L, 2, 16);


  int styleFlags = 0;
  if (lua_istable(L, 3)) {
    lua_getfield(L, 3, "bold");
    if (lua_toboolean(L, -1)) styleFlags |= FONT_STYLE_BOLD;
    lua_pop(L, 1);

    lua_getfield(L, 3, "italic");
    if (lua_toboolean(L, -1)) styleFlags |= FONT_STYLE_ITALIC;
    lua_pop(L, 1);

    lua_getfield(L, 3, "thin");
    if (lua_toboolean(L, -1)) styleFlags |= FONT_STYLE_THIN;
    lua_pop(L, 1);
  }

  auto [id, font] = UI_LoadFont(path, baseSize, styleFlags);
  if (!font || id < 0) {
    // Return nil + error message so Lua knows it failed
    lua_pushnil(L);
    lua_pushstring(L, "Failed to load font file");
    return 2; 
  }

  FontHandle* h = (FontHandle*)lua_newuserdata(L, sizeof(FontHandle));
  h->id = id;

  luaL_getmetatable(L, "FontMeta");
  lua_setmetatable(L, -2);
  return 1;
}


// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ text() function implementation              ╏
// ╏ this will be called from lua                ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛

int l_draw_text(lua_State* L) {
  if (!activeCommandList) {
    return 0;
  }
  const char* str = luaL_checkstring(L, 1);
  FontHandle* h = (FontHandle*)luaL_checkudata(L, 2, "FontMeta");
  if (!h) {
    std::cerr << "ERROR: font not loaded\n";
    return 0;
  }

  Font* font = UI_GetFontById(h->id);
  if (!font) {
    std::cerr << "ERROR: font id" << h->id << " no found\n";
    return 0;
  }

  float x = luaL_checknumber(L, 3);
  float y = luaL_checknumber(L, 4);


  Color color = {255, 255, 255, 255};
  if (lua_isstring(L, 5)) {
    const char* hex = lua_tostring(L, 5);
    SDL_Color sc = parseHexColor(hex);
    color = {(uint8_t)sc.r, (uint8_t)sc.g, (uint8_t)sc.b, (uint8_t)sc.a};
  }
  else if (lua_istable(L, 5)) {
    lua_rawgeti(L, 5, 1); color.r = luaL_optinteger(L, -1, 255); lua_pop(L, 1);
    lua_rawgeti(L, 5, 2); color.g = luaL_optinteger(L, -1, 255); lua_pop(L, 1);
    lua_rawgeti(L, 5, 3); color.b = luaL_optinteger(L, -1, 255); lua_pop(L, 1);
    lua_rawgeti(L, 5, 4); color.a = luaL_optinteger(L, -1, 255); lua_pop(L, 1);
  }

  activeCommandList->push(DrawTextCommand{std::string(str), font, x, y, color});
  return 0;
}

static AutoRegisterLua _reg_load_font("load_font", l_load_font);
static AutoRegisterLua _reg_draw_text("draw_text", l_draw_text);

void UI_ShutdownFonts() {
  g_fonts.clear();
  g_nextFontId = 1;
}

