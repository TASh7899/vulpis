#include "opengl_renderer.h"
#include "commands.h"
#include <SDL_video.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <variant>
#include <vector>

const char* vertexSource = R"(
#version 330 core
  layout (location = 0) in vec2 aPos;
  layout (location = 1) in vec2 aTextCoord;
  layout (location = 2) in vec4 aColor;
  layout (location = 3) in float aType;
  layout (location = 4) in float aWeight;

  out vec4 fColor;
  out vec2 fTextCoord;
  uniform mat4 projection;
  out float fType;
  out float fWeight;


  void main() {
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
    fColor = aColor;
    fTextCoord = aTextCoord;
    fType = aType;
    fWeight = aWeight;
  }
)";

const char* fragmentSource = R"(
#version 330 core

out vec4 FragColor;

in vec4 fColor;
in vec2 fTextCoord;
in float fType;
in float fWeight;

uniform sampler2D texSampler;
uniform float u_pxRange;

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main() {
    if (fType > 0.5) {
        vec3 msd = texture(texSampler, fTextCoord).rgb;
        float sd = median(msd.r, msd.g, msd.b);

        vec2 texSize = vec2(textureSize(texSampler, 0));
        vec2 unitRange = vec2(u_pxRange) / texSize;

        vec2 fw = max(fwidth(fTextCoord), vec2(1e-6));
        vec2 screenTexSize = 1.0 / fw;

        float screenPxRange = max(0.5 * dot(unitRange, screenTexSize), 1.0);

        float alpha = clamp(screenPxRange * (sd - fWeight) + 0.5, 0.0, 1.0);

        FragColor = vec4(fColor.rgb, fColor.a * alpha);
    } else {
        FragColor = fColor * texture(texSampler, fTextCoord);
    }
}
)";

static void checkShader(GLuint shader, const char* name) {
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        std::cerr << "Shader compile failed (" << name << "):\n" << log << std::endl;
    }
}

static void checkProgram(GLuint program) {
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(program, len, nullptr, log.data());
        std::cerr << "Program link failed:\n" << log << std::endl;
    }
}

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
  glEnable(GL_MULTISAMPLE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  initShaders();
  initBuffers();

  glGenTextures(1, &whiteTexture);
  glBindTexture(GL_TEXTURE_2D, whiteTexture);
  unsigned char whitePixel[] = { 255, 255, 255, 255 };
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

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
  // --- Compile Vertex Shader ---
  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vs, 1, &vertexSource, NULL);
  glCompileShader(vs);
  checkShader(vs, "vertex");

  // --- Compile Fragment Shader ---
  GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fs, 1, &fragmentSource, NULL);
  glCompileShader(fs);
  checkShader(fs, "fragment");

  // --- Link Program ---
  shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vs);
  glAttachShader(shaderProgram, fs);
  glLinkProgram(shaderProgram);
  checkProgram(shaderProgram);

  glDeleteShader(fs);
  glDeleteShader(vs);

  // --- Setup uniforms ---
  glUseProgram(shaderProgram);

  GLint texLoc = glGetUniformLocation(shaderProgram, "texSampler");
  if (texLoc == -1)
    std::cerr << "texSampler uniform not found!" << std::endl;
  glUniform1i(texLoc, 0); // texture unit 0

  GLint pxRangeLoc = glGetUniformLocation(shaderProgram, "u_pxRange");
  if (pxRangeLoc == -1)
    std::cerr << "u_pxRange uniform not found!" << std::endl;

  glUniform1f(pxRangeLoc, 8.0f); // MUST match MSDF generation

  glUseProgram(0);
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

  glEnableVertexAttribArray(3); // Type
  glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, type));

  glEnableVertexAttribArray(4); // Weight
  glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, weight));

  glBindVertexArray(0);
}

void OpenGLRenderer::beginFrame(const DamageRect& damage) {

  SDL_GetWindowSize(window, &winWidth, &winHeight);
  int drawableW, drawableH;
  SDL_GL_GetDrawableSize(window, &drawableW, &drawableH);
  glViewport(0, 0, drawableW, drawableH);

  float dpiScale = (float)drawableW / (float)winWidth;

  if (damage.active && !damage.fullScreen) {
    glEnable(GL_SCISSOR_TEST);
    int sx = (int)std::floor(damage.x * dpiScale);
    int sw = (int)std::floor(damage.w * dpiScale);
    int sh = (int)std::floor(damage.h * dpiScale);
    int sy = drawableH - (int)std::floor(damage.y * dpiScale) - sh;

    sx = std::max(0, sx);
    sy = std::max(0, sy);
    sw = std::min(drawableW - sx, sw);
    sh = std::min(drawableH - sy, sh);

    glScissor(sx, sy, sw, sh);
  } else {
    glDisable(GL_SCISSOR_TEST);
  }

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
  glDisable(GL_SCISSOR_TEST);
  flush();
  SDL_GL_SwapWindow(window);
}

void OpenGLRenderer::flush() {
  if (vertices.empty()) return;

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, currentTextureID);

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(Vertex), vertices.data());

  glDrawArrays(GL_TRIANGLES, 0, vertices.size());

  glBindVertexArray(0);
  vertices.clear();
}

void OpenGLRenderer::submit(const RenderCommandList& list) {
  float dpiScale = UI_GetDPIScale();

  auto snap = [dpiScale](float val) {
    return std::round(val * dpiScale) / dpiScale;
  };

  std::vector<Rect> clipStack;

  for (const auto& cmd : list.commands) {

    if (std::holds_alternative<DrawRectCommand>(cmd)) {
      if (currentTextureID != whiteTexture) {
        flush();
        currentTextureID = whiteTexture;
      }

      const auto& data = std::get<DrawRectCommand>(cmd);
      float x = snap(data.rect.x);
      float y = snap(data.rect.y);
      float w = snap(data.rect.w);
      float h = snap(data.rect.h);
      Color c = data.color;

      // triangle 1
      vertices.push_back({x, y, 0.0f, 0.0f, c, 0.0f, 0.5f});
      vertices.push_back({x + w, y, 0.0f, 0.0f ,c, 0.0f, 0.5f});
      vertices.push_back({x + w, y + h, 0.0f, 0.0f, c, 0.0f, 0.5f});

      // triangle 2
      vertices.push_back({x, y, 0.0f, 0.0f, c, 0.0f, 0.5f});
      vertices.push_back({x, y + h, 0.0f, 0.0f, c, 0.0f, 0.5f});
      vertices.push_back({x + w, y + h, 0.0f, 0.0f, c, 0.0f, 0.5f});
    }

    else if (std::holds_alternative<DrawTextCommand>(cmd)) {
      const auto& data = std::get<DrawTextCommand>(cmd);
      if (!data.font) continue;


      float fSize = (float)data.font->GetLogicalSize();

      float underlineY = data.y + (fSize*0.1f);
      float strikeThroughY = data.y - (data.font->GetLogicalAscent() * 0.32f);

      float decorationThickness;
      if (data.decoration == TextDecoration::StrikeThrough) {
        decorationThickness = std::max(1.0f, fSize / 14.0f);
      } else {
        decorationThickness = std::max(1.0f, fSize/24.0f);
      }

      GLuint fontTex = data.font->GetTextureID();

      if (currentTextureID != fontTex) {
        flush();
        currentTextureID = fontTex;
      }

      float cursorX = snap(data.x);
      float cursorY = snap(data.y);
      float textStartX = cursorX;

      std::vector<uint32_t> codepoints = Font::DecodeUTF8(data.text);

      float weight = 0.5f; 
      int style = data.font->GetStyle();
      if (style & FONT_STYLE_VERY_BOLD) weight = 0.35f;
      else if (style & FONT_STYLE_BOLD) weight = 0.40f;
      else if (style & FONT_STYLE_SEMI_BOLD) weight = 0.45f;
      else if (style & FONT_STYLE_THIN) weight = 0.60f;

      float slantRatio = (style & FONT_STYLE_ITALIC) ? 0.2f : 0.0f;

      for (uint32_t c : codepoints) {
        const Character& ch = data.font->GetCharacter(c);

        if (ch.TextureID != currentTextureID) {
          flush();
          currentTextureID = ch.TextureID;
        }

        float xpos = cursorX + ch.BearingX;
        float ypos = cursorY - ch.BearingY;

        float w = ch.SizeX;
        float h = ch.SizeY;

        float left   = xpos;
        float right  = xpos + w;
        float top    = ypos;
        float bottom = ypos + h;

        float slant = h * slantRatio;

        // Triangle 1 (Top-Right -> Bottom-Right -> Bottom-Left)
        vertices.push_back({ right + slant, top,    ch.uMax, ch.vMin, data.color, 1.0f, weight });
        vertices.push_back({ right,         bottom, ch.uMax, ch.vMax, data.color, 1.0f, weight });
        vertices.push_back({ left,          bottom, ch.uMin, ch.vMax, data.color, 1.0f, weight });

        // Triangle 2 (Bottom-Left -> Top-Left -> Top-Right)
        vertices.push_back({ left,          bottom, ch.uMin, ch.vMax, data.color, 1.0f, weight });
        vertices.push_back({ left + slant,  top,    ch.uMin, ch.vMin, data.color, 1.0f, weight });
        vertices.push_back({ right + slant, top,    ch.uMax, ch.vMin, data.color, 1.0f, weight });

        // Restore the 1/64th bitshift AND the DPI scale for the spacing!
        cursorX += ch.Advance;
      }

      // ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
      // ╏ LOGIC FOR UNDERLINE OR STRIKE LINE ╏
      // ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
      if (data.decoration != TextDecoration::None) {
        if (currentTextureID != whiteTexture) {
          flush();
          currentTextureID = whiteTexture;
        }

        float decorationThickness = std::max(1.0f, std::round(fSize * 0.05f));

        float width = cursorX - textStartX;
        float rawY = (data.decoration == TextDecoration::Underline) ? underlineY : strikeThroughY;

        float lineY = snap(rawY);
        float lineBottom = snap(rawY + (decorationThickness / dpiScale));

        // triangle 1
        vertices.push_back({ textStartX + width, lineY,      0.0f, 0.0f, data.color, 0.0f, 0.5f }); 
        vertices.push_back({ textStartX + width, lineBottom, 0.0f, 0.0f, data.color, 0.0f, 0.5f }); 
        vertices.push_back({ textStartX,         lineBottom, 0.0f, 0.0f, data.color, 0.0f, 0.5f }); 

        // Triangle 2
        vertices.push_back({ textStartX,         lineBottom, 0.0f, 0.0f, data.color, 0.0f, 0.5f }); 
        vertices.push_back({ textStartX,         lineY,      0.0f, 0.0f, data.color, 0.0f, 0.5f }); 
        vertices.push_back({ textStartX + width, lineY,      0.0f, 0.0f, data.color, 0.0f, 0.5f });

      }
    }


    else if (std::holds_alternative<PushClipCommand>(cmd)) {
      flush();
      const auto& data = std::get<PushClipCommand>(cmd);

      Rect currentClip = data.rect;
      if (!clipStack.empty()) {
        Rect parentClip = clipStack.back();

        float x1 = std::max(currentClip.x, parentClip.x);
        float y1 = std::max(currentClip.y, parentClip.y);
        float x2 = std::min(currentClip.x + currentClip.w, parentClip.x + parentClip.w);
        float y2 = std::min(currentClip.y + currentClip.h, parentClip.y + parentClip.h);

        currentClip.x = x1;
        currentClip.y = y1;
        currentClip.w = std::max(0.0f, x2 - x1);
        currentClip.h = std::max(0.0f, y2 - y1);
      }

      clipStack.push_back(currentClip);

      int drawW, drawH;
      SDL_GL_GetDrawableSize(window, &drawW, &drawH);

      float scaleX = (float)drawW / winWidth;
      float scaleY = (float)drawH / winHeight;

      int scissorX = (float)(currentClip.x * scaleX);
      int scissorY = (float)(drawH - (currentClip.y + currentClip.h) * scaleY);
      int scissorW = (int)(currentClip.w * scaleX);
      int scissorH = (int)(currentClip.h * scaleY);

      glEnable(GL_SCISSOR_TEST);
      glScissor(scissorX, scissorY, scissorW, scissorH);
    }

    else if (std::holds_alternative<PopClipCommand>(cmd)) {
      flush();

      if (!clipStack.empty()) {
        clipStack.pop_back();
      }

      if (clipStack.empty()) {
        glDisable(GL_SCISSOR_TEST);
      } else {
        Rect restoredClip = clipStack.back();

        int drawW, drawH;
        SDL_GL_GetDrawableSize(window, &drawW, &drawH);
        float scaleX = (float)drawW / winWidth;
        float scaleY = (float)drawH / winHeight;

        int scissorX = (float)(restoredClip.x * scaleX);
        int scissorY = (float)(drawH - (restoredClip.y + restoredClip.h) * scaleY);
        int scissorW = (int)(restoredClip.w * scaleX);
        int scissorH = (int)(restoredClip.h * scaleY);

        glScissor(scissorX, scissorY, scissorW, scissorH);
      }

    }

    else if (std::holds_alternative<DrawImageCommand>(cmd)) {
      const auto& data = std::get<DrawImageCommand>(cmd);

      if (currentTextureID != data.textureId) {
        flush();
        currentTextureID = data.textureId;
      }

      float left   = snap(data.rect.x);
      float top    = snap(data.rect.y);
      float right  = snap(data.rect.x + data.rect.w);
      float bottom = snap(data.rect.y + data.rect.h);

      vertices.push_back({left, top, data.uMin, data.vMin, data.tint, 0.0f, 0.5f});
      vertices.push_back({right, top, data.uMax, data.vMin, data.tint, 0.0f, 0.5f});
      vertices.push_back({right, bottom, data.uMax, data.vMax, data.tint, 0.0f, 0.5f});

      vertices.push_back({left, top, data.uMin, data.vMin, data.tint, 0.0f, 0.5f});
      vertices.push_back({left, bottom, data.uMin, data.vMax, data.tint, 0.0f, 0.5f});
      vertices.push_back({right, bottom, data.uMax, data.vMax, data.tint, 0.0f, 0.5f});

    }

  }
}
