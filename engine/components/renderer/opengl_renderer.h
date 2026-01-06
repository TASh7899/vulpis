#pragma once
#include "commands.h"
#include "renderer.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL_video.h>

class OpenGLRenderer : public Renderer {
  public:
    OpenGLRenderer(SDL_Window* window);
    ~OpenGLRenderer();

    void beginFrame() override;
    void endFrame() override;
    void submit(const RenderCommandList& commandList) override;

  private:
    SDL_Window* window;
    SDL_GLContext context;
    int winWidth = 0;
    int winHeight = 0;
};
