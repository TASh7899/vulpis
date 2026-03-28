#pragma once
#include <variant>
#include <vector>
#include <cstdint>
#include "../color/color.h"
#include "../text/font.h"

struct Color {
  uint8_t r, g, b, a;
};

struct Rect {
  float x, y, w, h;
};

enum class FontStyle { Normal, Italics };
enum class FontWeight { VeryThin, Thin, Normal, SemiBold, Bold, VeryBold };
enum class TextDecoration { None, Underline, StrikeThrough };

struct DrawTextCommand {
  std::string text;
  Font* font;
  float x, y;
  Color color;
  TextDecoration decoration = TextDecoration::None;
};

struct DrawRectCommand {
  Rect rect;
  Color color;
  float borderRadius = 0.0f;
  float borderWidth = 0.0f;
  Color borderColor = {0, 0, 0, 0};
};

struct PushClipCommand {
  Rect rect;
  float borderRadius = 0.0f;
  float borderWidth = 0.0f;
};

struct PopClipCommand {

};

struct DrawImageCommand {
  Rect rect;
  uint32_t textureId;
  Color tint;
  float uMin = 0.0f;
  float vMin = 0.0f;
  float uMax = 1.0f;
  float vMax = 1.0f;
  
  float borderRadius = 0.0f; 
  Rect nodeRect = {0, 0, 0, 0};
  float borderWidth = 0.0f;

};

using RenderCommand = std::variant<DrawRectCommand, PushClipCommand, PopClipCommand, DrawTextCommand, DrawImageCommand>;

struct RenderCommandList {
  std::vector<RenderCommand> commands;

  void push(const RenderCommand& cmd) {
    commands.push_back(cmd);
  }
  void clear() {
    commands.clear();
  }
};
