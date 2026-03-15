#pragma once
#include "commands.h"
#include "../ui/ui.h"

class Renderer {
  public:
    virtual ~Renderer() = default;

    virtual void beginFrame(const DamageRect& damage) = 0;
    virtual void endFrame() = 0;
    virtual void submit(const RenderCommandList& commandList) = 0;
};
