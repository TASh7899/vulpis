#pragma once
#include <string>
#include <map>
#include <glad/glad.h>
#include <unordered_map>
#include <memory>
#include <utility>
#include "../../lua.hpp"

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

    const Character& GetCharacter(char c) const;
    unsigned int GetTextureID() const { return textureID; }
    unsigned int GetLineHeight() const { return lineHeight; }
    unsigned int GetAscent() const { return ascent; }

    const std::string& GetPath() const { return fontPath; }
    unsigned int GetSize() const { return fontSize; }

  private:
    unsigned int textureID;
    unsigned int lineHeight;
    unsigned int ascent;
  
    std::string fontPath;
    unsigned int fontSize;

    std::map<char, Character> characters;
    void Load(const std::string& path, unsigned int size);
};



struct FontHandle {
  int id;
};

Font* UI_GetFontById(int id);

std::pair<int, Font*> UI_LoadFont(const std::string& path, int size);

void UI_ShutdownFonts();

int l_load_font(lua_State* L);
int l_draw_text(lua_State* L);

