#pragma once
#include "commands.h"

class Renderer {
  public:
    virtual ~Renderer() = default;

    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void submit(const RenderCommandList& commandList) = 0;
};
