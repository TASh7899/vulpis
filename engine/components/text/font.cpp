#include "font.h"
#include <algorithm>
#include <iostream>
#include <ft2build.h>
#include <ostream>
#include FT_FREETYPE_H

Font::Font(const std::string& fontPath, unsigned int fontSize) {
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
