#include "font.h"
#include <SDL_gamecontroller.h>
#include <algorithm>
#include <cstdint>
#include <freetype/fttypes.h>
#include <iostream>
#include <ft2build.h>
#include <lauxlib.h>
#include <memory>
#include <ostream>
#include <sys/types.h>
#include <utility>
#include <vector>
#include "../ui/ui.h"
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include "../system/pathUtils.h"

// file local global font storage
static std::unordered_map<int, std::unique_ptr<Font>> g_fonts;
static int g_nextFontId = 1;

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
Font::Font(const std::string& fontPath, unsigned int fontSize, int styleFlags) : 
  textureID(0), lineHeight(0), ascent(0), fontPath(fontPath), fontSize(fontSize), styleFlags(styleFlags) {
  Load(fontPath, fontSize);
}

Font::~Font() {
  glDeleteTextures(1, &textureID);
  if (ftFace) {
    FT_Done_Face((FT_Face)ftFace);
  }
}





// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ MAIN FUNCTION TO GET FONT POINTER AND ITS ID FROM GLOBAL FONT STORAGE ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
std::pair<int, Font*> UI_LoadFont(const std::string &path, int size, int styleFlags) {
  for (const auto& [id, font] : g_fonts ) {
    if (font->GetPath() == path && font->GetSize() == (unsigned int)size &&  font->GetStyle() == styleFlags) {
      return {id, font.get()};
    }
  }

  int id = g_nextFontId++;
  auto font = std::make_unique<Font>(path, size, styleFlags);
  Font* fontptr = font.get();
  g_fonts[id] = std::move(font);

  return {id, fontptr};
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

  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_2D, textureID);
  unsigned char white[] = {255, 255, 255, 255};
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, white);
  GLint swizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
  glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

}


const Character* Font::LoadGlyph(uint32_t c) {
  if (!ftFace) {
    return nullptr;
  }
  FT_Face face = (FT_Face)ftFace;

  if (styleFlags & FONT_STYLE_ITALIC) {
    FT_Matrix matrix;
    matrix.xx = 0x10000L;
    matrix.xy = (FT_Fixed)(0.2f * 0x10000L);
    matrix.yx = 0;
    matrix.yy = 0x10000L;
    FT_Set_Transform(face, &matrix, nullptr);
  } else {
    FT_Set_Transform(face, nullptr, nullptr);
  }

  if (FT_Load_Char(face, c, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP)) {
    return nullptr;
  }

  if (styleFlags & (FONT_STYLE_BOLD | FONT_STYLE_SEMI_BOLD | FONT_STYLE_VERY_BOLD | FONT_STYLE_THIN)) {
    FT_Pos strength = 0;

    if (styleFlags & FONT_STYLE_VERY_BOLD) {
      strength = 144;
    }

    else if (styleFlags & FONT_STYLE_BOLD) {
      strength = 96;
    }

    else if (styleFlags & FONT_STYLE_SEMI_BOLD) {
      strength = 48;
    }

    else if (styleFlags & FONT_STYLE_THIN) {
      strength = -48;
    }

    if (strength != 0) {
      FT_Outline_Embolden(&face->glyph->outline, strength);
    }
  }

  if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL)) {
    return nullptr;
  }

  if (face->glyph->bitmap.width == 0 && face->glyph->bitmap.rows == 0) {
    Character character = {
      0, 0, 0, 0, 0, (unsigned int)face->glyph->advance.x, 0, 0, 0, 0
    };
    return &characters.emplace(c, character).first->second;
  }

  GLuint tex;
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);

  glTexImage2D(
      GL_TEXTURE_2D,
      0,
      GL_RED,
      face->glyph->bitmap.width,
      face->glyph->bitmap.rows,
      0,
      GL_RED,
      GL_UNSIGNED_BYTE,
      face->glyph->bitmap.buffer
      );

  GLint swizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
  glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  Character character = {
    tex,
    (int)face->glyph->bitmap.width, (int)face->glyph->bitmap.rows,
    (int)face->glyph->bitmap_left, (int)face->glyph->bitmap_top,
    (unsigned int)face->glyph->advance.x,
    0.0f, 0.0f, 1.0f, 1.0f // UVs are simple 0-1 because it's a single texture
  };
  // Insert into cache and return pointer
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
  int size = luaL_checkinteger(L, 2);

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

  auto [id, font] = UI_LoadFont(path, size, styleFlags);

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


void UI_ShutdownFonts() {
  g_fonts.clear();
  g_nextFontId = 1;
}

