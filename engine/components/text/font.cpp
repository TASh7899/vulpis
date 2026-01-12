#include "font.h"
#include <algorithm>
#include <iostream>
#include <ft2build.h>
#include <lauxlib.h>
#include <memory>
#include <ostream>
#include <utility>
#include "../ui/ui.h"
#include FT_FREETYPE_H


static std::unordered_map<int, std::unique_ptr<Font>> g_fonts;
static int g_nextFontId = 1;

Font* UI_GetFontById(int id) {
  auto it = g_fonts.find(id);
  if (it == g_fonts.end()) return nullptr;
  return it->second.get();
}


std::pair<int, Font*> UI_LoadFont(const std::string &path, int size) {
  for (const auto& [id, font] : g_fonts ) {
    if (font->GetPath() == path && font->GetSize() == (unsigned int)size) {
      return {id, font.get()};
    }
  }

  int id = g_nextFontId++;
  auto font = std::make_unique<Font>(path, size);
  Font* fontptr = font.get();
  g_fonts[id] = std::move(font);

  return {id, fontptr};
}



Font::Font(const std::string& fontPath, unsigned int fontSize) : fontPath(fontPath), fontSize(fontSize) {
  Load(fontPath, fontSize);
}

Font::~Font() {
  glDeleteTextures(1, &textureID);
}

void Font::Load(const std::string& path, unsigned int size) {
  FT_Library ft;
  if (FT_Init_FreeType(&ft)) {
    std::cerr << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
    return;
  }

  FT_Face face;
  if (FT_New_Face(ft, path.c_str(), 0, &face)) {
    std::cerr << "ERROR::FREETYPE: Failed to load font: " << path << std::endl;
    FT_Done_FreeType(ft);
    return;
  }

  FT_Set_Pixel_Sizes(face, 0, size);
  this->lineHeight = face->size->metrics.height >> 6;
  this->ascent = face->size->metrics.ascender >> 6;

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  int atlasWidth = 1024;
  int atlasHeight = 1024;

  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_2D, textureID);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasWidth, atlasHeight, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

  GLint swizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
  glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  int x = 0;
  int y = 0;
  int rowHeight = 0;


  for (unsigned char c = 32; c < 128; c++) {
    if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
      std::cerr << "ERROR::FREETYTPE: Failed to load Glyph: " << c << std::endl;
      continue;
    }

    if (x + face->glyph->bitmap.width >= atlasWidth) {
      x = 0;
      y += rowHeight;
      rowHeight = 0;
    }

    glTexSubImage2D(
      GL_TEXTURE_2D,
      0,
      x,
      y,
      face->glyph->bitmap.width,
      face->glyph->bitmap.rows,
      GL_RED,
      GL_UNSIGNED_BYTE,
      face->glyph->bitmap.buffer
    );

    Character character = {
      textureID,
      (int)face->glyph->bitmap.width, (int)face->glyph->bitmap.rows,
      (int)face->glyph->bitmap_left, (int)face->glyph->bitmap_top,
      (unsigned int)face->glyph->advance.x,
      (float)x / atlasWidth,
      (float)y / atlasHeight,
      (float)(x + face->glyph->bitmap.width) / atlasWidth,
      (float)(y + face->glyph->bitmap.rows) / atlasHeight
    };
    characters.insert(std::pair<char, Character>(c, character));
    x += face->glyph->bitmap.width + 1;
    rowHeight = std::max(rowHeight, (int)face->glyph->bitmap.rows);
  }
  
  FT_Done_Face(face);
  FT_Done_FreeType(ft);
}

const Character& Font::GetCharacter(char c) const {
  if (characters.find(c) == characters.end()) {
    return characters.at('?');
  }
  return characters.at(c);
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

  auto [id, font] = UI_LoadFont(path, size);

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

