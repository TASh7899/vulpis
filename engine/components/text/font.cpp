#include "font.h"
#include <SDL_opengl.h>
#include <SDL_video.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <freetype/fttypes.h>
#include <iostream>
#include <ft2build.h>
#include <lauxlib.h>
#include <memory>
#include <ostream>
#include <string>
#include <sys/types.h>
#include <utility>
#include "../ui/ui.h"
#include "freetype/ftimage.h"
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include "tvg_ui.h"
#include "bundled_registry.h"
#include <freetype/otsvg.h>
#include FT_BITMAP_H
#include FT_MODULE_H


FT_Library Font::ft = nullptr;


FT_Error SVG_Init(FT_Pointer* state) { return FT_Err_Ok; }

void SVG_Free(FT_Pointer* state) {}

FT_Error SVG_Render(FT_GlyphSlot slot, FT_Pointer* _state) {
    FT_SVG_Document document = (FT_SVG_Document)slot->other;

    int width = slot->bitmap.width;
    int height = slot->bitmap.rows;

    if (width == 0 || height == 0) return FT_Err_Ok;

    std::vector<unsigned char> buffer;

    if (!TVG_UI::RasterizeSvgToBuffer((const char*)document->svg_document, 
                                      document->svg_document_length, 
                                      buffer, width, height)) {
      return FT_Err_Invalid_File_Format;
    }


    slot->bitmap.pixel_mode = FT_PIXEL_MODE_BGRA;
    slot->bitmap.num_grays = 256;
    slot->format = FT_GLYPH_FORMAT_BITMAP;

    if (FT_GlyphSlot_Own_Bitmap(slot)) return FT_Err_Out_Of_Memory;

    // Copy data
    memcpy(slot->bitmap.buffer, buffer.data(), width * height * 4);

    return FT_Err_Ok;
}

FT_Error SVG_Preset_Slot(FT_GlyphSlot slot, FT_Bool cache, FT_Pointer* _state) {
    FT_SVG_Document document = (FT_SVG_Document)slot->other;

    //    (slot->metrics is 26.6 fixed point, so we shift by 6 to get pixels)
    int width  = (slot->metrics.width  + 63) >> 6;
    int height = (slot->metrics.height + 63) >> 6;

    int bearingX = slot->metrics.horiBearingX >> 6;
    int bearingY = slot->metrics.horiBearingY >> 6;

    if (width == 0 || height == 0) {
        width  = document->metrics.x_ppem; // x_ppem is essentially "fontSize" in pixels
        height = document->metrics.y_ppem;
        bearingX = 0;
        bearingY = height; // heuristic: assume it fills the line height
    }

    slot->bitmap.width = width;
    slot->bitmap.rows  = height;
    slot->bitmap.pitch = width * 4; // BGRA = 4 bytes per pixel
    slot->bitmap.pixel_mode = FT_PIXEL_MODE_BGRA;
    slot->bitmap.num_grays  = 256;
    slot->format = FT_GLYPH_FORMAT_BITMAP;

    slot->bitmap_left = bearingX;
    slot->bitmap_top  = bearingY;

    return FT_Err_Ok;
}

SVG_RendererHooks tvg_svg_hooks = {
  (SVG_Lib_Init_Func)SVG_Init,
  (SVG_Lib_Free_Func)SVG_Free,
  (SVG_Lib_Render_Func)SVG_Render,
  (SVG_Lib_Preset_Slot_Func)SVG_Preset_Slot
};



void Font::InitFreeType() {
  if (ft == nullptr) {
    if (FT_Init_FreeType(&ft)) {
      std::cerr << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
    }
  }

  FT_Error err = FT_Property_Set(ft, "ot-svg", "svg-hooks", &tvg_svg_hooks);
  if (err != 0) {
    std::cerr << "ERROR::FREETYPE: Failed to register SVG Hooks. Error Code: " << err << std::endl;
    std::cerr << "Note: Ensure your FreeType is built with FT_CONFIG_OPTION_SVG enabled." << std::endl;
  } else {
    std::cout << "SUCCESS: SVG Hooks registered." << std::endl;
  }
}

void Font::ShutdownFreeType() {
  if (ft != nullptr) {
    FT_Done_FreeType(ft);
    ft = nullptr;
  }
}


// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ this is the main storage for all font class ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
static std::unordered_map<int, std::unique_ptr<Font>> g_fonts;
static int g_nextFontId = 1;

// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ Return Font from g_fonts using id ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
Font* UI_GetFontById(int id) {
  auto it = g_fonts.find(id);
  if (it == g_fonts.end()) return nullptr;
  return it->second.get();
}

// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ Load fonts from g_fonts using path and size ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
std::pair<int, Font*> UI_LoadFont(const std::string &path, int size, bool isItalic) {
  for (const auto& [id, font] : g_fonts ) {
    if (font->GetPath() == path && font->GetSize() == (unsigned int)size && font->IsFakeItalic() == isItalic) {
      return {id, font.get()};
    }
  }

  int id = g_nextFontId++;
  auto font = std::make_unique<Font>(path, size, isItalic);

  if (!font->IsValid()) {
    std::cerr << "CRITICAL: Failed to create font instance for " << path << ". Skipping." << std::endl;
    return {-1, nullptr};
  }

  Font* newFontPtr = font.get();

  for (const auto& [existingId, existingFont] : g_fonts) {
    if (existingFont->GetPath() == path && existingFont.get() != newFontPtr) {

      const auto& siblingFallbacks = existingFont->GetFallbacks();

      for (Font* siblingFallback : siblingFallbacks) {
        auto [fbId, fbPtr] = UI_LoadFont(siblingFallback->GetPath(), size, isItalic);
        if (fbPtr) {
          newFontPtr->AddFallback(fbPtr);
        }
      }
      break; 
    }
  }

  g_fonts[id] = std::move(font);
  return {id, newFontPtr};
}


// ┏╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ UTF Parser ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍┛
uint32_t utf8_next(const std::string& str, int& i) {
  if (i >= (int)str.size()) return 0;

  auto byte = [&](int idx) -> uint32_t {
    return (idx < (int)str.size()) ? (uint32_t)(unsigned char)str[idx] : 0;
  };

  uint32_t b0 = byte(i);

  // ascii
  if (b0 < 0x80) {
    i += 1;
    return b0;
  }

  // latin characters
  if ((b0 & 0xE0) == 0xC0 && i+1 < (int)str.size()) {
    uint32_t b1 = byte(i+1);
    i += 2;
    return ((b0 & 0x1F) << 6) | (b1 & 0x3F);
  }

  // many asian characters
  if ((b0 & 0xF0) == 0xE0 && i+2 < (int)str.size())  {
    uint32_t b1 = byte(i+1), b2 = byte(i+2);
    i += 3;
    return ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
  }

  // emojis and other rare characters
  if ((b0 & 0xF8) == 0xF0 && i+3 < (int)str.size()) {
    uint32_t b1 = byte(i+1), b2 = byte(i+2), b3 = byte(i+3);
    i += 4;
    return ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
  }
  i += 1;
  return 0xFFFD;
}


// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ Font class contructor, it sets fontPath and fontSize    ╏
// ╏ while it also calls Load                                ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
Font::Font(const std::string& fontPath, unsigned int fontSize, bool isItalic) : fontPath(fontPath), fontSize(fontSize), isFakeItalic(isItalic), face(nullptr) {

  if (ft == nullptr) Font::InitFreeType();

  if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
    std::cerr << "ERROR::FREETYPE: Failed to load font: " << fontPath << std::endl;
    return;
  }

  Load(fontPath, fontSize);
}

Font::Font(const std::string name, const unsigned char* data, unsigned int dataLen, unsigned int fontSize, bool isItalic) : fontPath(name), fontSize(fontSize), isFakeItalic(isItalic) {
  loadFromMemory(data, dataLen, fontSize);
}


void Font::loadFromMemory(const unsigned char* data, unsigned int dataLen, unsigned int fontSize) {

  if (ft == nullptr) Font::InitFreeType();

  if (FT_New_Memory_Face(ft, (const FT_Byte*)data, (FT_Long)dataLen, 0, &face)) {
    std::cerr << "ERROR::FREETYPE: Failed to load font from memory" << std::endl;
    return;
  }
  FT_Select_Charmap(face, FT_ENCODING_UNICODE);
  if (FT_IS_SCALABLE(face)) {
    FT_Set_Pixel_Sizes(face, 0, fontSize);
    this->scale = 1.0f;
  } else if (face->num_fixed_sizes > 0) {
    FT_Select_Size(face, 0); 
    this->scale = 1.0f;
  }
  this->lineHeight = (face->size->metrics.height >> 6) * this->scale;
  this->ascent = (face->size->metrics.ascender >> 6) * this->scale;
  this->isColorFont = (face->face_flags & FT_FACE_FLAG_COLOR);
  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_2D, textureID);
  GLint internalFormat = isColorFont ? GL_RGBA : GL_RED;
  glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, atlasWidth, atlasHeight, 0, isColorFont ? GL_RGBA : GL_RED, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}



std::pair<int, Font*> UI_LoadBundleFont(const std::string& name, const unsigned char* data, unsigned int dataLen, int size, bool isItalic) {

  std::string cacheName = name;

  for (const auto& [id, font]: g_fonts) {
    if (font->GetPath() == name && font->GetSize() == (unsigned int)size && font->IsFakeItalic() == isItalic) {
      return {id, font.get()};
    }
  }

  int id = g_nextFontId++;
  auto font = std::make_unique<Font>(name, data, dataLen, size, isItalic);

  Font* ptr = font.get();
  g_fonts[id] = std::move(font);
  return {id, ptr};

}

// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ Destructor to delete Textures of Font ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
Font::~Font() {
  glDeleteTextures(1, &textureID);
  FT_Done_Face(face);
}

// ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
// ╏ The main function responsible for loading our font and  ╏
// ╏ doing texture configuration                             ╏
// ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
void Font::Load(const std::string& path, unsigned int size) {

  FT_Select_Charmap(face, FT_ENCODING_UNICODE);

  if (FT_IS_SCALABLE(face)) {
    FT_Set_Pixel_Sizes(face, 0, size);
    this->scale = 1.0f;
  } else if (face->num_fixed_sizes > 0) {
    int bestMatch = 0;
    int diff = abs((int)size - face->available_sizes[0].height);
    for (int i = 1; i < face->num_fixed_sizes; i++) {
      int newDiff = abs((int)size - face->available_sizes[i].height);
      if (newDiff < diff) {
        diff = newDiff;
        bestMatch = i;
      }
    }
    FT_Select_Size(face, bestMatch);
    this->scale = (float)size / (float)face->available_sizes[bestMatch].height;
  }

  this->lineHeight = (face->size->metrics.height >> 6) * this->scale;
  this->ascent = (face->size->metrics.ascender >> 6) * this->scale;

  // check if font supports color
  this->isColorFont = (face->face_flags & FT_FACE_FLAG_COLOR);

  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_2D, textureID);

  // Only use RGBA memory if it's a color font
  GLint internalFormat = isColorFont ? GL_RGBA : GL_RED;

  glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, atlasWidth, atlasHeight, 0, isColorFont ? GL_RGBA : GL_RED, GL_UNSIGNED_BYTE, nullptr);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

static constexpr uint32_t REPLACEMENT_CHAR = 0xFFFD; // '�'

const Character& Font::GetCharacter(uint32_t c) {
  if (c < 128 && asciiValid[c]) return asciiCache[c];

  auto it = extendedCache.find(c);
  if (it != extendedCache.end()) return it->second;

  if (hasGlyph(c)) {
    return renderAndPack(c);
  }

  for (Font* fallback: fallbacks) {
    if (fallback->hasGlyph(c)) {
      return fallback->GetCharacter(c);
    }
  }

  if (c != REPLACEMENT_CHAR) {
    return GetCharacter(REPLACEMENT_CHAR);
  }

  return renderAndPack(c);

}


const Character& Font::renderAndPack(uint32_t codepoint) {
  int error = FT_Load_Char(face, codepoint, FT_LOAD_COLOR);
  bool ftSuccess = (error == 0);


  if (this->isFakeItalic) {
    FT_Matrix matrix;
    matrix.xx = 0x10000L;
    matrix.xy = 0x5800L;
    matrix.yx = 0;
    matrix.yy = 0x10000L;

    if (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
      FT_Outline_Transform(&face->glyph->outline, &matrix);
    }
  }


  bool useTvgs = false;
  if (!ftSuccess || (isColorFont && face->glyph->format == FT_GLYPH_FORMAT_OUTLINE)) {
    useTvgs = true;
  }

if (!useTvgs && ftSuccess && (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE || face->glyph->format == FT_GLYPH_FORMAT_SVG)) {
    if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL)) {
      static Character empty = {};
      return empty;
    }
  }


  int width = 0, height = 0;
  int bearX = 0, bearY = 0;
  unsigned char* pixelData = nullptr;
  std::vector<unsigned char> tvgBuffer;

  if (useTvgs) {
    if (TVG_UI::RasterizeGlyph(fontPath, (float)fontSize, codepoint, tvgBuffer, width, height, bearX, bearY)) {
      pixelData = tvgBuffer.data();
      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    } else {
      // both engines Failed
      std::cerr << "ERROR: could not render Font\n";
      static Character empty = {};
      return empty;
    }

  } else if (ftSuccess) {
    width = face->glyph->bitmap.width;
    height = face->glyph->bitmap.rows;
    bearX = face->glyph->bitmap_left;
    bearY = face->glyph->bitmap_top;
    pixelData = face->glyph->bitmap.buffer;

    int bytesPerPixel = (face->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) ? 4 : 1;
    int pitch = face->glyph->bitmap.pitch;

    if (pitch > 0 && pitch != width * bytesPerPixel) {
      // GL_UNPACK_ROW_LENGTH expects the stride in PIXELS, not bytes.
      glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / bytesPerPixel);
    } else {
      glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }

  } else {
    static Character empty = {};
    return empty;
  }

  bool ftIsColor = (ftSuccess && face->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_BGRA);

  if ((useTvgs || ftIsColor) && !isColorFont) {
    isColorFont = true;

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlasWidth, atlasHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    extendedCache.clear();
    for (int i = 0; i < 128; i++) asciiValid[i] = false;
    nextX = 2; nextY = 2; currentRowHeight = 0;
  }

  if (nextX + width >= atlasWidth) {
    nextX = 2;
    nextY += currentRowHeight + 1;
    currentRowHeight = 0;
  }

  if (nextY + height >= atlasHeight) {
    extendedCache.clear();
    for(int i=0; i<128; ++i) asciiValid[i] = false;
    nextX = 2; nextY = 2; currentRowHeight = 0;

    glBindTexture(GL_TEXTURE_2D, textureID);
    GLint internalFormat = isColorFont ? GL_RGBA : GL_RED;
    GLenum format = isColorFont ? GL_RGBA : GL_RED;
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, atlasWidth, atlasHeight, 0, format, GL_UNSIGNED_BYTE, nullptr);
  }

  glBindTexture(GL_TEXTURE_2D, textureID);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  GLenum uploadFormat = GL_RED;
  if (useTvgs) {
    uploadFormat = GL_BGRA;
  } else if (ftIsColor) {
    uploadFormat = GL_BGRA;
  }

  if (width > 0 && height > 0) {
    glTexSubImage2D(GL_TEXTURE_2D, 0, nextX, nextY, width, height, uploadFormat, GL_UNSIGNED_BYTE, pixelData);
  }

  unsigned int advance = 0;

  if (ftSuccess) {
    advance = (unsigned int)face->glyph->advance.x;
  } else {
    if (FT_Load_Char(face, codepoint, FT_LOAD_NO_BITMAP) == 0) {
      advance = (unsigned int)face->glyph->advance.x;
    } else {
      advance = (unsigned int)(width << 6);
    }
  }

  Character ch = {
    textureID,
    width, height,     // Use generic vars
    bearX, bearY,      // Use generic vars
    advance,
    (float)nextX / atlasWidth,
    (float)nextY / atlasHeight,
    (float)(nextX + width) / atlasWidth,
    (float)(nextY + height) / atlasHeight,
    (useTvgs || ftIsColor)
  };

  currentRowHeight = std::max(currentRowHeight, height);
  nextX += width + 2;

  if (codepoint < 128) {
    asciiValid[codepoint] = true;
    return (asciiCache[codepoint] = ch);
  }

  return (extendedCache[codepoint] = ch);
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

  bool isItalic = false;
  std::string weight = "regular";

  auto [id, font] = UI_LoadFont(path, size, isItalic);

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

int l_add_fallback(lua_State* L) {
  FontHandle* main = (FontHandle*)luaL_checkudata(L, 1, "FontMeta");
  FontHandle* fallback = (FontHandle*)luaL_checkudata(L, 2, "FontMeta");

  Font* mainFont = UI_GetFontById(main->id);
  Font* fallbackFont = UI_GetFontById(fallback->id);

  if (mainFont && fallbackFont) {
    mainFont->AddFallback(fallbackFont);
    std::cout << "SUCCESS: Linked Font ID " << fallback->id 
      << " as fallback for ID " << main->id << std::endl;
  } else {
    std::cout << "ERROR: Could not link fonts. Invalid IDs." << std::endl;
  }

  return 0;
}


//          ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
//          ╏             font config file parsing logic              ╏
//          ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛

// static storage
static std::unordered_map<std::string, std::string> g_fontAliasMap;
static std::vector<std::string> g_globalFallbackPaths;

// Implement registry functions
void UI_RegisterFontAlias(const std::string &alias, const std::string &path) {
  g_fontAliasMap[alias] = path;
}

void UI_AddGlobalFallbackPath(const std::string &path) {
  for (const auto& p : g_globalFallbackPaths) {
    if (p == path) return;
  }
  g_globalFallbackPaths.push_back(path);
}

int UI_GetOrLoadFont(const std::string& family, const std::string& weight, int size, bool isItalics) {
  std::string resolvedPath = family;
  if (g_fontAliasMap.find(family) != g_fontAliasMap.end()) {
    resolvedPath = g_fontAliasMap[family];
  }

  std::string uniqueName = resolvedPath + "_" + weight + "_" + std::to_string(size) + (isItalics ? "-italic" : "");

  for (const auto& [id, font] : g_fonts) {
    if (font->GetPath() == uniqueName) {
      return id;
    }
  }

  int mainFontId = -1;
  Font* mainFontPtr = nullptr;

  bool isBundledRequest = (resolvedPath == "Iosevka" || resolvedPath == "default");

  if (isBundledRequest) {
    FontBlob blob = GetBundledIosevka(weight);
    if (blob.data) {
      auto result = UI_LoadBundleFont(uniqueName, blob.data, blob.len, size, isItalics);
      mainFontId = result.first;
      mainFontPtr = result.second;
    }
  } else {
    auto result = UI_LoadFont(resolvedPath, size, isItalics);
    mainFontId = result.first;
    mainFontPtr = result.second;
  }

  if (mainFontPtr) {
    for (const std::string& fallback : g_globalFallbackPaths) {
      if (fallback == resolvedPath) continue;
      auto [fbId, fbPtr] = UI_LoadFont(fallback, size, false);

      if (fbPtr) {
        mainFontPtr->AddFallback(fbPtr);
      }
    }
    return mainFontId;
  }

  std::cerr << "ERROR: Could not load font: " << resolvedPath << std::endl;
  return g_defaultFontId;
}


