#include "opengl_renderer.h"
#include "commands.h"
#include <SDL_opengl.h>
#include <iostream>
#include <SDL_error.h>
#include <SDL_video.h>
#include <variant>

OpenGLRenderer::OpenGLRenderer(SDL_Window* win) : window(win) {
  context = SDL_GL_CreateContext(window);
  if (!context) {
    std::cerr << "Failed to create OpenGl context" << SDL_GetError() << std::endl;
  }

  SDL_GL_SetSwapInterval(1);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

OpenGLRenderer::~OpenGLRenderer() {
  SDL_GL_DeleteContext(context);
}

void OpenGLRenderer::beginFrame() {
  SDL_GetWindowSize(window, &winWidth, &winHeight);
  glViewport(0, 0, winWidth, winHeight);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, winWidth, winHeight, 0, -1, 1);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLRenderer::endFrame() {
  SDL_GL_SwapWindow(window);
}

void OpenGLRenderer::submit(const RenderCommandList& list) {
  for (const auto& cmd : list.commands) {
    if (std::holds_alternative<DrawRectCommand>(cmd)) {
      const auto& data = std::get<DrawRectCommand>(cmd);
      glColor4ub(data.color.r, data.color.g, data.color.b, data.color.a);

      glBegin(GL_QUADS);
        glVertex2f(data.rect.x, data.rect.y);
        glVertex2f(data.rect.x + data.rect.w, data.rect.y);
        glVertex2f(data.rect.x + data.rect.w, data.rect.y + data.rect.h);
        glVertex2f(data.rect.x, data.rect.y + data.rect.h);
      glEnd();
    }

    else if (std::holds_alternative<PushClipCommand>(cmd)) {
      const auto& data = std::get<PushClipCommand>(cmd);
      glEnable(GL_SCISSOR_TEST);
      int glY = winHeight - (int)(data.rect.y + data.rect.h);
      glScissor(
        (int)data.rect.x,
        glY,
        (int)std::ceil(data.rect.w),
        (int)std::ceil(data.rect.h)
      );
    }

    else if (std::holds_alternative<PopClipCommand>(cmd)) {
      glDisable(GL_SCISSOR_TEST);
    }
  }
}
