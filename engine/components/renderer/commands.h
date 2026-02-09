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
};

struct PushClipCommand {
  Rect rect;
};

struct PopClipCommand {

};

using RenderCommand = std::variant<DrawRectCommand, PushClipCommand, PopClipCommand, DrawTextCommand>;

struct RenderCommandList {
  std::vector<RenderCommand> commands;

  void push(const RenderCommand& cmd) {
    commands.push_back(cmd);
  }
  void clear() {
    commands.clear();
  }
};
