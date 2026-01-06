#pragma once
#include <variant>
#include <vector>
#include <cstdint>

struct Color {
  uint8_t r, g, b, a;
};

struct Rect {
  float x, y, w, h;
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

using RenderCommand = std::variant<DrawRectCommand, PushClipCommand, PopClipCommand>;

struct RenderCommandList {
  std::vector<RenderCommand> commands;

  void push(const RenderCommand& cmd) {
    commands.push_back(cmd);
  }
  void clear() {
    commands.clear();
  }
};
