#pragma once
#include <string>
#include <map>
#include <glad/glad.h>

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

  private:
    unsigned int textureID;
    unsigned int lineHeight;
    unsigned int ascent;
    std::map<char, Character> characters;
    void Load(const std::string& path, unsigned int size);
};

