#include "tvg_ui.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <thorvg.h>
#include <cstring>

namespace TVG_UI {
  void Init() {
    if (tvg::Initializer::init(tvg::CanvasEngine::Sw, 0) != tvg::Result::Success) {
      std::cerr << "TVG_UI ERROR: Failed to initialize ThorVG" << std::endl;
    }
  }

  void ShutDown() {
    tvg::Initializer::term(tvg::CanvasEngine::Sw);
  }

  std::string codepointToUTF8(uint32_t cp) {
    std::string out;
    if (cp < 0x7F) out += (char)cp;
    else if (cp <= 0x7FF) {
      out += (char)(0xC0 | (cp >> 6));
      out += (char)(0xC0 | (cp & 0x3F));
    }

    else if (cp <= 0xFFFF) {
      out += (char)(0xE0 | (cp >> 12));
      out += (char)(0x80 | ((cp >> 6) & 0x3F));
      out += (char)(0x80 | (cp & 0x3F));
    }

    else if (cp <= 0x10FFFF) {
      out += (char)(0xF0 | (cp >> 18));
      out += (char)(0x80 | ((cp >> 12) & 0x3F));
      out += (char)(0x80 | ((cp >> 6) & 0x3F));
      out += (char)(0x80 | (cp & 0x3F));
    }
    return out;
  }

  bool RasterizeGlyph(const std::string& fontPath, float fontSize, uint32_t codepoint, std::vector<unsigned char>& outBuffer, 
      int& outW, int&outH, int& outBearingX, int& outBearingY) {

    auto canvas = tvg::SwCanvas::gen();
    if (!canvas) return false;

    auto text = tvg::Text::gen();
    if (text->font(fontPath.c_str(), fontSize) != tvg::Result::Success) {
      std::cerr << "TVG Error: Failed to load font: " << fontPath << std::endl;
      return false;
    }
    text->fill(255, 255, 255);

    std::string utf8Str = codepointToUTF8(codepoint);
    text->text(utf8Str.c_str());

    float x, y, w, h;
    text->bounds(&x, &y, &w, &h, true);

    if (w <= 0 || h <= 0) return false;

    int iW = (int)ceil(w) + 2;
    int iH = (int)ceil(h) + 2;

    outBuffer.resize(iW * iH * 4);
    canvas->target(reinterpret_cast<uint32_t*>(outBuffer.data()), iW, iW, iH, tvg::SwCanvas::ARGB8888);

    text->translate(-x + 1, -y + 1);

    canvas->push(std::move(text));
    canvas->draw();
    canvas->sync();

    outW = iW;
    outH = iH;

    outBearingX = (int)std::floor(x);
    outBearingY = (int)std::floor(-y);

    return true;
  }


  bool RasterizeSvgToBuffer(const char* svgData, uint32_t dataLen, std::vector<unsigned char>& outBuffer, int width, int height) {
    auto canvas = tvg::SwCanvas::gen();
    if (!canvas) return false;

    std::cout << "tvg svg" << std::endl;

    auto picture = tvg::Picture::gen();
    if (picture->load(svgData, dataLen, "", true) != tvg::Result::Success) {
      std::cerr << "TVG ERROR: Failed to load SVG data from font. Length: " << dataLen << std::endl;
      return false;
    }
    picture->size(width, height);

    outBuffer.resize(width * height * 4);
    std::fill(outBuffer.begin(), outBuffer.end(), 0);
    canvas->target(reinterpret_cast<uint32_t*>(outBuffer.data()), width, width, height, tvg::SwCanvas::ARGB8888);

    canvas->push(std::move(picture));
    canvas->draw();
    canvas->sync();

    return true;

  }


}

