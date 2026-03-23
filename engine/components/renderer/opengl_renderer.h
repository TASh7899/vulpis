#pragma once
#include <glad/glad.h>
#include "commands.h"
#include "renderer.h"
#include "../ui/ui.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <vector>

struct Vertex {
  float x, y;
  float u, v, page;
  Color color;

  float localX = 0.0f;
  float localY = 0.0f; 
  float boxW = 0.0f;
  float boxH = 0.0f;
  float radius = 0.0f;
  float borderW = 0.0f;
  Color borderColor = {0, 0, 0, 0};
  float type = 0.0f; // 0.0 defaults to Text/Image (Standard)
};

class OpenGLRenderer : public Renderer {
  public:
    OpenGLRenderer(SDL_Window* window);
    ~OpenGLRenderer();

    void beginFrame(const DamageRect& damage) override;
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

    GLuint useArrayLoc;
    bool currentIsArray = false;

    std::vector<Vertex> vertices;
    void initShaders();
    void initBuffers();
    void flush();
};
