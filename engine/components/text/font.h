#pragma once
#include <cstdint>
#include <string>
#include <map>
#include <glad/glad.h>
#include <unordered_map>
#include <memory>
#include <utility>
#include "../../lua.hpp"
#include <vector>

struct Character {
  unsigned int TextureID;
  int SizeX, SizeY;
  int BearingX, BearingY;
  unsigned int Advance;
  float uMin, vMin;
  float uMax, vMax;
};

class Font {
  public:
    Font(const std::string& fontPath, unsigned int fontSize);
    ~Font();

    const Character& GetCharacter(uint32_t c);

    unsigned int GetTextureID() const { return textureID; }
    unsigned int GetLineHeight() const { return lineHeight; }
    unsigned int GetAscent() const { return ascent; }

    const std::string& GetPath() const { return fontPath; }
    unsigned int GetSize() const { return fontSize; }

    static std::vector<uint32_t> DecodeUTF8(const std::string& text);

    static void AddFallback(Font* font);
    static void ResetFallback();
    static const std::vector<Font*>&  GetFallbacks();


  private:
    unsigned int textureID;
    unsigned int lineHeight;
    unsigned int ascent;
  
    std::string fontPath;
    unsigned int fontSize;


    // Cache of loaded characters (Key is Unicode Codepoint)
    std::unordered_map<uint32_t, Character> characters;

    void* ftLib = nullptr;
    void* ftFace = nullptr;

    void Load(const std::string& path, unsigned int size);
    const Character* LoadGlyph(uint32_t c);
    const Character* GetCharInternal(uint32_t c);
};



struct FontHandle {
  int id;
};

Font* UI_GetFontById(int id);
std::pair<int, Font*> UI_LoadFont(const std::string& path, int size);
void UI_ShutdownFonts();

int l_load_font(lua_State* L);
int l_draw_text(lua_State* L);

