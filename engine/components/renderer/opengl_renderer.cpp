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
layout (location = 1) in vec3 aTextCoord;
layout (location = 2) in vec4 aColor;
layout (location = 3) in vec2 aLocalPos;
layout (location = 4) in vec4 aBoxData;
layout (location = 5) in vec4 aBorderColor;
layout (location = 6) in float aType;

out vec4 fColor;
out vec3 fTextCoord;
out vec2 fLocalPos;
out vec4 fBoxData;
out vec4 fBorderColor;
out float fType;

uniform mat4 projection;

void main() {
  gl_Position = projection * vec4(aPos, 0.0, 1.0);
  fColor = aColor;
  fTextCoord = aTextCoord;
  fLocalPos = aLocalPos;
  fBoxData = aBoxData;
  fBorderColor = aBorderColor;
  fType = aType;
}
)";

const char* fragmentSource = R"(
#version 330 core
out vec4 FragColor;

in vec4 fColor;
in vec3 fTextCoord;
in vec2 fLocalPos;
in vec4 fBoxData;
in vec4 fBorderColor;
in float fType;

uniform sampler2D texSampler;
uniform sampler2DArray fontSampler;
uniform int useArray;

// Signed Distance Field calculation for a rounded box
float roundedBoxSDF(vec2 p, vec2 b, float r) {
  vec2 q = abs(p) - b + vec2(r);
  return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
  if (fType > 0.5) {
     vec2 halfSize = vec2(fBoxData.x, fBoxData.y) / 2.0;
     vec2 center = halfSize;
     
     // cap radius so it doesn't exceed the box dimensions
     float radius = min(fBoxData.z, min(halfSize.x, halfSize.y));
     float borderW = fBoxData.w;

     float dist = roundedBoxSDF(fLocalPos - center, halfSize, radius);
     float edgeSoftness = max(fwidth(dist), 0.001); // fwidth guarantees smooth anti-aliasing

     float alpha = smoothstep(edgeSoftness, -edgeSoftness, dist);
     if (alpha < 0.001) discard;

     vec4 finalColor = fColor;

     // mix border and background inwards from the edge
     if (borderW > 0.0) {
         float borderDist = dist + borderW;
         float borderAlpha = smoothstep(edgeSoftness, -edgeSoftness, borderDist);
         finalColor = mix(fBorderColor, fColor, borderAlpha);
     }
     
     FragColor = finalColor * vec4(1.0, 1.0, 1.0, alpha);
  } else {
     vec4 texColor;
     if (useArray == 1) {
       float mask = texture(fontSampler, fTextCoord).r;
       mask = pow(max(mask, 0.0), 1.0/2.2);
       texColor = vec4(1.0, 1.0, 1.0, mask);
     } else {
       texColor = texture(texSampler, fTextCoord.xy);
     }
     FragColor = fColor * texColor;
  }
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

  glEnableVertexAttribArray(1); // UVW
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));

  glEnableVertexAttribArray(2); // Color
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, color));

  glEnableVertexAttribArray(3); // LocalPos
  glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, localX));

  glEnableVertexAttribArray(4); // BoxData
  glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, boxW));

  glEnableVertexAttribArray(5); // BorderColor
  glVertexAttribPointer(5, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, borderColor));

  glEnableVertexAttribArray(6); // Type
  glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, type));


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

  useArrayLoc = glGetUniformLocation(shaderProgram, "useArray");
  glUniform1i(useArrayLoc, 0);
  currentIsArray = false;

  glUniform1i(glGetUniformLocation(shaderProgram, "texSampler"), 0);
  glUniform1i(glGetUniformLocation(shaderProgram, "fontSampler"), 1);

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
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, currentTextureID);
}

void OpenGLRenderer::endFrame() {
  glDisable(GL_SCISSOR_TEST);
  flush();
  SDL_GL_SwapWindow(window);
}

void OpenGLRenderer::flush() {
  if (vertices.empty()) return;


  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_DYNAMIC_DRAW);
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
      if ( currentIsArray || currentTextureID != whiteTexture) {
        flush();
        currentIsArray = false;
        glUniform1i(useArrayLoc, 0);
        currentTextureID = whiteTexture;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, whiteTexture);
      }

      const auto& data = std::get<DrawRectCommand>(cmd);
      float x = snap(data.rect.x);
      float y = snap(data.rect.y);
      float w = snap(data.rect.w);
      float h = snap(data.rect.h);
      Color c = data.color;

      float radius = data.borderRadius;
      float borderW = data.borderWidth;
      Color bc = data.borderColor;
      float type = (radius > 0.0f || borderW > 0.0f) ? 1.0f : 0.0f;

      // triangle 1
      vertices.push_back({x, y, 0.0f, 0.0f, 0.0f, c,     0.0f, 0.0f, w, h, radius, borderW, bc, type});
      vertices.push_back({x + w, y, 0.0f, 0.0f, 0.0f, c, w,    0.0f, w, h, radius, borderW, bc, type});
      vertices.push_back({x + w, y + h, 0.0f, 0.0f, 0.0f, c, w, h, w, h, radius, borderW, bc, type});

      // triangle 2
      vertices.push_back({x, y, 0.0f, 0.0f, 0.0f, c,     0.0f, 0.0f, w, h, radius, borderW, bc, type});
      vertices.push_back({x, y + h, 0.0f, 0.0f, 0.0f, c, 0.0f, h,    w, h, radius, borderW, bc, type});
      vertices.push_back({x + w, y + h, 0.0f, 0.0f, 0.0f, c, w,    h,    w, h, radius, borderW, bc, type});
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

      if (!currentIsArray || currentTextureID != fontTex) {
        flush();
        currentIsArray = true;
        glUniform1i(useArrayLoc, 1);
        currentTextureID = fontTex;
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D_ARRAY, fontTex);
        glActiveTexture(GL_TEXTURE0);
      }

      float cursorX = snap(data.x);
      float cursorY = snap(data.y);
      float textStartX = cursorX;

      std::vector<uint32_t> codepoints = Font::DecodeUTF8(data.text);

      for (uint32_t c : codepoints) {
        const Character& ch = data.font->GetCharacter(c);


        float xpos = snap(cursorX + (ch.BearingX / dpiScale));
        float ypos = snap(cursorY + ((ch.SizeY - ch.BearingY) / dpiScale));
        float w = (float)ch.SizeX / dpiScale;
        float h = (float)ch.SizeY / dpiScale;


        float left   = xpos;
        float right  = xpos + w;
        float top    = ypos - h;
        float bottom = ypos;

        vertices.push_back({ right, top,    ch.uMax, ch.vMin, ch.pageIndex, data.color });
        vertices.push_back({ right, bottom, ch.uMax, ch.vMax, ch.pageIndex, data.color });
        vertices.push_back({ left,  bottom, ch.uMin, ch.vMax, ch.pageIndex, data.color });

        vertices.push_back({ left,  bottom, ch.uMin, ch.vMax, ch.pageIndex, data.color });
        vertices.push_back({ left,  top,    ch.uMin, ch.vMin, ch.pageIndex, data.color });
        vertices.push_back({ right, top,    ch.uMax, ch.vMin, ch.pageIndex, data.color });

        cursorX += ((ch.Advance >> 6) / dpiScale);
      }

      // ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
      // ╏ LOGIC FOR UNDERLINE OR STRIKE LINE ╏
      // ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛
      if (data.decoration != TextDecoration::None) {
        if (currentIsArray || currentTextureID != whiteTexture) {
          flush();
          currentIsArray = false;
          glUniform1i(useArrayLoc, 0);
          currentTextureID = whiteTexture;
          glActiveTexture(GL_TEXTURE0);
          glBindTexture(GL_TEXTURE_2D, whiteTexture);
        }

        float decorationThickness = std::max(1.0f, std::round(fSize * 0.05f));

        float width = cursorX - textStartX;
        float rawY = (data.decoration == TextDecoration::Underline) ? underlineY : strikeThroughY;

        float lineY = snap(rawY);
        float lineBottom = snap(rawY + (decorationThickness / dpiScale));

        vertices.push_back({ textStartX + width, lineY,       0.0f, 0.0f, 0.0f, data.color });
        vertices.push_back({ textStartX + width, lineBottom,  0.0f, 0.0f, 0.0f, data.color });
        vertices.push_back({ textStartX,         lineBottom,  0.0f, 0.0f, 0.0f, data.color });

        vertices.push_back({ textStartX,         lineBottom,  0.0f, 0.0f, 0.0f, data.color });
        vertices.push_back({ textStartX,         lineY,       0.0f, 0.0f, 0.0f, data.color });
        vertices.push_back({ textStartX + width, lineY,       0.0f, 0.0f, 0.0f, data.color });

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

      if ( currentIsArray || currentTextureID != data.textureId) {
        flush();
        currentIsArray = false;
        glUniform1i(useArrayLoc, 0);
        currentTextureID = data.textureId;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, data.textureId);
      }

      float left   = snap(data.rect.x);
      float top    = snap(data.rect.y);
      float right  = snap(data.rect.x + data.rect.w);
      float bottom = snap(data.rect.y + data.rect.h);

      vertices.push_back({left, top, data.uMin, data.vMin, 0.0f, data.tint});
      vertices.push_back({right, top, data.uMax, data.vMin, 0.0f, data.tint});
      vertices.push_back({right, bottom, data.uMax, data.vMax, 0.0f, data.tint});

      vertices.push_back({left, top, data.uMin, data.vMin, 0.0f, data.tint});
      vertices.push_back({left, bottom, data.uMin, data.vMax, 0.0f, data.tint});
      vertices.push_back({right, bottom, data.uMax, data.vMax, 0.0f, data.tint});
    }

  }
}
