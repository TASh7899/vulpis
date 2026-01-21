#pragma once
#include <cstdint>
#include <string>
#include <glad/glad.h>
#include <unordered_map>
#include <utility>
#include "../../lua.hpp"
#include <ft2build.h>
#include <vector>
#include FT_FREETYPE_H

struct Character {
  unsigned int TextureID;
  int SizeX, SizeY;
  int BearingX, BearingY;
  unsigned int Advance;
  float uMin, vMin;
  float uMax, vMax;
  bool isColor;
};

class Font {
  public:
    Font(const std::string& fontPath, unsigned int fontSize, bool isItalic = false);
    Font(const std::string name, const unsigned char* data, unsigned int dataLen, unsigned int fontSize, bool isItalic = false);
    ~Font();

    const Character& GetCharacter(uint32_t c);

    bool IsValid() const { return face != nullptr; }

    unsigned int GetTextureID() const { return textureID; }
    bool IsColorFont() const { return isColorFont; }
    unsigned int GetLineHeight() const { return lineHeight; }
    unsigned int GetAscent() const { return ascent; }
    float GetScale() const { return scale; }

    const std::string& GetPath() const { return fontPath; }
    unsigned int GetSize() const { return fontSize; }
    void AddFallback(Font* font) { fallbacks.push_back(font); }
    const std::vector<Font*>& GetFallbacks() const { return fallbacks; }
    bool hasGlyph(uint32_t c) const { return FT_Get_Char_Index(face, c) != 0; }
    bool IsFakeItalic() const { return isFakeItalic; }

    static void InitFreeType();
    static void ShutdownFreeType();


  private:
    unsigned int textureID;
    unsigned int lineHeight;
    unsigned int ascent;
  
    std::string fontPath;
    unsigned int fontSize;

    std::vector<Font*> fallbacks;

    bool isColorFont;

    int atlasWidth = 2048;
    int atlasHeight = 2048;
    int nextX = 2;
    int nextY = 2;
    int currentRowHeight = 0;
    float scale = 1.0f;

    static FT_Library ft;
    FT_Face face;

    Character asciiCache[128];
    bool asciiValid[128];
    std::unordered_map<uint32_t, Character> extendedCache;

    void Load(const std::string& path, unsigned int size);
    void loadFromMemory(const unsigned char* data, unsigned int dataLen, unsigned int fontSize);
    const Character& renderAndPack(uint32_t codepoint);
    void clearCache();

    bool isFakeItalic = false;

};

struct FontHandle {
  int id;
};

Font* UI_GetFontById(int id);

std::pair<int, Font*> UI_LoadFont(const std::string& path, int size, bool isItalic);

void UI_ShutdownFonts();

int l_load_font(lua_State* L);
int l_draw_text(lua_State* L);
int l_add_fallback(lua_State* L);

uint32_t utf8_next(const std::string& str, int& i);
std::pair<int, Font*> UI_LoadBundleFont(const std::string& name, const unsigned char* data, unsigned int dataLen, int size, bool isItalic);

int UI_GetOrLoadFont(const std::string& family, const std::string& weight, int size, bool isItalics);

void UI_RegisterFontAlias(const std::string& alias, const std::string& path);
void UI_AddGlobalFallbackPath(const std::string& path);


