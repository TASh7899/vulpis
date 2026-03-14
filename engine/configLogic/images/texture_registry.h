#pragma once
#include <string>
#include <glad/glad.h>

namespace TextureRegistry {
    GLuint GetTexture(const std::string& path);
    bool ProcessUploads();
    void Cleanup();
    void ReleaseTexture(GLuint textureID);
}


