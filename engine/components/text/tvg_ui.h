#pragma once
#include <cstdint>
#include <thorvg.h>
#include <vector>
#include <string>

namespace TVG_UI {
  void Init();
  void ShutDown();

  bool RasterizeGlyph(const std::string& fontPath, float fontSize, uint32_t codepoint, std::vector<unsigned char>& outBuffer, 
      int& outW, int&outH, int& outBearingX, int& outBearingY);

  bool RasterizeSvgToBuffer(const char* svgData, uint32_t dataLen, std::vector<unsigned char>& outBuffer, int width, int height);

}
