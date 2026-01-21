#pragma once
#include <glad/glad.h>
#include "commands.h"
#include "renderer.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <vector>

struct Vertex {
  float x, y;
  float u, v;
  Color color;
  float isColor;
};

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

    GLuint shaderProgram;
    GLuint vao;
    GLuint vbo;

    GLuint whiteTexture;
    GLuint currentTextureID;

    std::vector<Vertex> vertices;
    void initShaders();
    void initBuffers();
    void flush();
};
