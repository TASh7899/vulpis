#pragma once
#include <stb_rect_pack.h>
#include <cstdint>
#include <string>
#include <map>
#include <glad/glad.h>
#include <unordered_map>
#include <memory>
#include <utility>
#include "../../lua.hpp"
#include <vector>


const int FONT_STYLE_NORMAL = 0;
const int FONT_STYLE_BOLD   = 1 << 0;
const int FONT_STYLE_ITALIC = 1 << 1;
const int FONT_STYLE_THIN   = 1 << 2;
const int FONT_STYLE_SEMI_BOLD = 1 << 3;
const int FONT_STYLE_VERY_BOLD = 1 << 4;

struct Character {
  float pageIndex;
  int SizeX, SizeY;
  int BearingX, BearingY;
  unsigned int Advance;
  float uMin, vMin;
  float uMax, vMax;
};

struct TextLine {
  uint32_t startIndex;
  uint32_t count;
  float width;
};

class Font {
  public:
    Font(const std::string& fontPath, unsigned int fontSize, int styleFlags = FONT_STYLE_NORMAL);
    ~Font();

    const Character& GetCharacter(uint32_t c);

    unsigned int GetTextureID() const { return atlasArrayID; }
    unsigned int GetLineHeight() const { return lineHeight; }
    unsigned int GetAscent() const { return ascent; }

    int GetStyle() const { return styleFlags; }

    const std::string& GetPath() const { return fontPath; }
    unsigned int GetSize() const { return fontSize; }

    static std::vector<uint32_t> DecodeUTF8(const std::string& text);

    static void AddFallback(Font* font);
    static void ResetFallback();
    static const std::vector<Font*>&  GetFallbacks();

    // pre calculated logical values for font
    unsigned int GetLogicalSize() const { return logicalSize; }
    float GetLogicalLineHeight() const { return (float)lineHeight / dpiScale; }
    float GetLogicalAscent() const { return (float)ascent / dpiScale; }
    float GetLogicalAdvance(uint32_t c);

    void AllocateAtlasPage();
  
    std::vector<TextLine> CalculateWordWrap(const std::vector<uint32_t>& codepoints, float maxWidth);

  private:
    unsigned int lineHeight;
    unsigned int ascent;

    unsigned int atlasWidth = 1024;
    unsigned int atlasHeight = 1024;

    unsigned int atlasArrayID = 0;
    int currentLayer = -1;
    int maxLayers = 8;

    stbrp_context packContext;
    std::vector<stbrp_node> packNodes;

    std::string fontPath;
    unsigned int fontSize;
    int styleFlags;

    // Cache of loaded characters (Key is Unicode Codepoint)
    std::unordered_map<uint32_t, Character> characters;

    void* ftLib = nullptr;
    void* ftFace = nullptr;

    void Load(const std::string& path, unsigned int size);
    const Character* LoadGlyph(uint32_t c);
    const Character* GetCharInternal(uint32_t c);

    unsigned int logicalSize;
    float dpiScale;
};



struct FontHandle {
  int id;
};

Font* UI_GetFontById(int id);
std::pair<int, Font*> UI_LoadFont(const std::string& path, int size, int styleFlags);
void UI_ShutdownFonts();

int l_load_font(lua_State* L);
int l_draw_text(lua_State* L);

// DPI Scale helper
void UI_SetDPIScale(float scale);
float UI_GetDPIScale();


