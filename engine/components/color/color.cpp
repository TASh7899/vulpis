#include "color.h"

SDL_Color parseHexColor(const char* hexStr) {
  SDL_Color color = {0, 0, 0, 255};

  if (hexStr == nullptr) {
    return color;
  }

  if (hexStr[0] == '#') hexStr++;

  size_t len = std::strlen(hexStr);
  if (len != 6 && len != 8) {
    return color;
  }

  for (size_t i = 0; i < len; i++) {
    if (!std::isxdigit(static_cast<unsigned char>(hexStr[i])))
      return color;
  }

  char* end = nullptr;
  unsigned long value = std::strtoul(hexStr, &end, 16);

  if (end == hexStr || *end != '\0' ) {
    return color;
  }

  if (len == 6) {
    color.r = (value >> 16) & 0xFF;
    color.g = (value >> 8) & 0xFF;
    color.b = (value) & 0xFF;
    color.a = 0xFF;
  } else {
    color.r = (value >> 24) & 0xFF;
    color.g = (value >> 16) & 0xFF;
    color.b = (value >> 8) & 0xFF;
    color.a = (value) & 0xFF;

  }

  return color;
}
