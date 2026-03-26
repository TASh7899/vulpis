#pragma once
#include <string>
#include <glad/glad.h>
#include <lua.hpp>

namespace TextureRegistry {
    GLuint GetTexture(const std::string& path);
    bool ProcessUploads();
    void Cleanup();
    void ReleaseTexture(GLuint textureID);
    void GetTextureDimensions(GLuint textureID, int& w, int& h);
    bool IsTextureLoaded(GLuint textureID);
    bool IsValidTexture(GLuint textureID);
}


