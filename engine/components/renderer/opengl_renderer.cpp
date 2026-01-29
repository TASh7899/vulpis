#include "opengl_renderer.h"
#include "commands.h"
#include <SDL_video.h>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <variant>
#include <vector>

const char* vertexSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTextCoord;
layout (location = 2) in vec4 aColor;

out vec4 fColor;
out vec2 fTextCoord;
uniform mat4 projection;

void main() {
  gl_Position = projection * vec4(aPos, 0.0, 1.0);
  fColor = aColor;
  fTextCoord = aTextCoord;
}
)";

const char* fragmentSource = R"(
  #version 330 core
  out vec4 FragColor;
  in vec4 fColor;
  in vec2 fTextCoord;

  uniform sampler2D texSampler;

  void main() {
      FragColor = fColor * texture(texSampler, fTextCoord);
  }
)";

OpenGLRenderer::OpenGLRenderer(SDL_Window* win) : window(win) {
  context = SDL_GL_CreateContext(window);
  if (!context) {
    std::cerr << "Failed to create OpenGl context" << SDL_GetError() << std::endl;
  }

  if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
    std::cerr << "Failed to initialize GLAD" << std::endl;
  }

  std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;

  SDL_GL_SetSwapInterval(1);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  initShaders();
  initBuffers();

  glGenTextures(1, &whiteTexture);
  glBindTexture(GL_TEXTURE_2D, whiteTexture);
  unsigned char whitePixel[] = { 255, 255, 255, 255 };
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);

  currentTextureID = whiteTexture;
}

OpenGLRenderer::~OpenGLRenderer() {
  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(1, &vbo);
  glDeleteProgram(shaderProgram);
  glDeleteTextures(1, &whiteTexture);
  SDL_GL_DeleteContext(context);

}

void OpenGLRenderer::initShaders() {
  // Compile Vertex Shader
  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vs, 1, &vertexSource, NULL);
  glCompileShader(vs);

  // Compile Fragment Shader
  GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fs, 1, &fragmentSource, NULL);
  glCompileShader(fs);

  shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vs);
  glAttachShader(shaderProgram, fs);
  glLinkProgram(shaderProgram);

  glDeleteShader(fs);
  glDeleteShader(vs);
}

void OpenGLRenderer::initBuffers() {

  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  glEnableVertexAttribArray(0); // pos
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE ,sizeof(Vertex), (void*)offsetof(Vertex, x));

  glEnableVertexAttribArray(1); // UV
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));

  glEnableVertexAttribArray(2); // Color
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, color));

  glBindVertexArray(0);
}

void OpenGLRenderer::beginFrame() {
  SDL_GetWindowSize(window, &winWidth, &winHeight);
  int drawableW, drawableH;
  SDL_GL_GetDrawableSize(window, &drawableW, &drawableH);
  glViewport(0, 0, drawableW, drawableH);

  glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(shaderProgram);

  float L = 0.0f, R = (float)winWidth;
  float B = (float)winHeight, T = 0.0f;

  float ortho[16] = {
    2.0f/(R-L),   0.0f,         0.0f,  0.0f,
    0.0f,         2.0f/(T-B),   0.0f,  0.0f,
    0.0f,         0.0f,        -1.0f,  0.0f,
    -(R+L)/(R-L), -(T+B)/(T-B), 0.0f,  1.0f
  };

  GLint projLoc = glGetUniformLocation(shaderProgram, "projection");
  glUniformMatrix4fv(projLoc, 1, GL_FALSE, ortho);

  vertices.clear();
  currentTextureID = whiteTexture;
  glBindTexture(GL_TEXTURE_2D, currentTextureID);
}

void OpenGLRenderer::endFrame() {
  flush();
  SDL_GL_SwapWindow(window);
}

void OpenGLRenderer::flush() {
  if (vertices.empty()) return;

  glBindTexture(GL_TEXTURE_2D, currentTextureID);
  
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);
  glDrawArrays(GL_TRIANGLES, 0, vertices.size());

  glBindVertexArray(0);
  vertices.clear();
}

void OpenGLRenderer::submit(const RenderCommandList& list) {
  for (const auto& cmd : list.commands) {
    if (std::holds_alternative<DrawRectCommand>(cmd)) {

      if (currentTextureID != whiteTexture) {
        flush();
        currentTextureID = whiteTexture;
      }

      const auto& data = std::get<DrawRectCommand>(cmd);
      float x = data.rect.x;
      float y = data.rect.y;
      float w = data.rect.w;
      float h = data.rect.h;
      Color c = data.color;

      // triangle 1
      vertices.push_back({x, y, 0.0f, 0.0f, c});
      vertices.push_back({x + w, y, 0.0f, 0.0f ,c});
      vertices.push_back({x + w, y + h, 0.0f, 0.0f, c});

      // triangle 2
      vertices.push_back({x, y, 0.0f, 0.0f, c});
      vertices.push_back({x, y + h, 0.0f, 0.0f, c});
      vertices.push_back({x + w, y + h, 0.0f, 0.0f, c});
    }

    else if (std::holds_alternative<DrawTextCommand>(cmd)) {
      const auto& data = std::get<DrawTextCommand>(cmd);
      GLuint fontTex = data.font->GetTextureID();

      if (currentTextureID != fontTex) {
        flush();
        currentTextureID = fontTex;
      }

      float cursorX = data.x;
      float cursorY = data.y;

      std::vector<uint32_t> codepoints = Font::DecodeUTF8(data.text);

      for (uint32_t c : codepoints) {
        const Character& ch = data.font->GetCharacter(c);

        if (ch.TextureID != currentTextureID) {
          flush();
          currentTextureID = ch.TextureID;
        }

        float xpos = cursorX + ch.BearingX;
        float ypos = cursorY + (ch.SizeY - ch.BearingY);
        float w = (float)ch.SizeX;
        float h = (float)ch.SizeY;

        // triangle 1
        vertices.push_back({ xpos + w, ypos, ch.uMax, ch.vMax, data.color });
        vertices.push_back({ xpos + w, ypos - h, ch.uMax, ch.vMin, data.color });
        vertices.push_back({ xpos, ypos - h, ch.uMin, ch.vMin, data.color});

        // triangle 2
        vertices.push_back({ xpos, ypos - h, ch.uMin, ch.vMin, data.color });
        vertices.push_back({ xpos, ypos, ch.uMin, ch.vMax, data.color });
        vertices.push_back({ xpos + w, ypos, ch.uMax, ch.vMax, data.color });

        // freetype stores advance values as 26.6 fixed point, meaning the value is scaled by 64, we divide by 64 to get pixels.
        // this is just a faster way to divide by 64
        cursorX += (ch.Advance >> 6);
      }
    }


    else if (std::holds_alternative<PushClipCommand>(cmd)) {
      flush();
      const auto& data = std::get<PushClipCommand>(cmd);

      int drawW, drawH;
      SDL_GL_GetDrawableSize(window, &drawW, &drawH);

      float scaleX = (float)drawW / winWidth;
      float scaleY = (float)drawH / winHeight;

      int scissorX = (float)(data.rect.x * scaleX);
      int scissorY = (float)(drawH - (data.rect.y + data.rect.h) * scaleY);
      int scissorW = (int)(data.rect.w * scaleX);
      int scissorH = (int)(data.rect.h * scaleY);

      glEnable(GL_SCISSOR_TEST);
      glScissor(scissorX, scissorY, scissorW, scissorH);
    }

    else if (std::holds_alternative<PopClipCommand>(cmd)) {
      flush();
      glDisable(GL_SCISSOR_TEST);
    }
  }
}
