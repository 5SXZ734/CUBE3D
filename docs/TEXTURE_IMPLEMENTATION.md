# Implementing Texture Support in Renderers

The application code is now ready to load and display .X models. Each renderer needs to implement texture loading.

## Required Renderer Updates

Each renderer (OpenGL, D3D11, D3D12) needs to implement these new methods:

### 1. createTexture(const char* filepath)
- Load BMP/TGA/PNG image from disk
- Create GPU texture resource
- Return texture handle

### 2. destroyTexture(uint32_t textureHandle)
- Release texture resource

### 3. setUniformInt(uint32_t shaderHandle, const char* name, int value)
- Set integer uniform (for texture sampler binding)

### 4. drawMesh(uint32_t meshHandle, uint32_t textureHandle)
- Bind texture if textureHandle > 0
- Draw the mesh

### 5. Update Vertex Layout
- Add texture coordinates (u, v) to vertex structure
- Update vertex attribute pointers

## OpenGL Implementation Example

```cpp
// In renderer_opengl.cpp

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // Download from: https://github.com/nothings/stb

struct GLTexture {
    GLuint id;
};

std::unordered_map<uint32_t, GLTexture> m_textures;
uint32_t m_nextTextureHandle = 1;

uint32_t OpenGLRenderer::createTexture(const char* filepath) {
    int width, height, channels;
    unsigned char* data = stbi_load(filepath, &width, &height, &channels, 0);
    
    if (!data) {
        std::fprintf(stderr, "Failed to load texture: %s\n", filepath);
        return 0;
    }

    GLTexture texture;
    glGenTextures(1, &texture.id);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    
    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glGenerateMipmap(GL_TEXTURE_2D);
    
    stbi_image_free(data);
    
    uint32_t handle = m_nextTextureHandle++;
    m_textures[handle] = texture;
    return handle;
}

void OpenGLRenderer::setUniformInt(uint32_t shaderHandle, const char* name, int value) {
    auto it = m_shaders.find(shaderHandle);
    if (it != m_shaders.end()) {
        GLShader& shader = it->second;
        GLint loc = glGetUniformLocation(shader.program, name);
        if (loc >= 0) {
            glUniform1i(loc, value);
        }
    }
}

void OpenGLRenderer::drawMesh(uint32_t meshHandle, uint32_t textureHandle) {
    // Bind texture if present
    if (textureHandle > 0) {
        auto texIt = m_textures.find(textureHandle);
        if (texIt != m_textures.end()) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texIt->second.id);
        }
    }
    
    // Draw mesh
    auto meshIt = m_meshes.find(meshHandle);
    if (meshIt != m_meshes.end()) {
        glBindVertexArray(meshIt->second.vao);
        glDrawElements(GL_TRIANGLES, meshIt->second.indexCount, GL_UNSIGNED_SHORT, (void*)0);
        glBindVertexArray(0);
    }
}

// Update vertex layout to include tex coords
glEnableVertexAttribArray(3);
glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(10*sizeof(float)));
```

## D3D11 Implementation Outline

```cpp
// Use WIC (Windows Imaging Component) or stb_image
uint32_t D3D11Renderer::createTexture(const char* filepath) {
    // Load image using stb_image
    // Create ID3D11Texture2D
    // Create ID3D11ShaderResourceView
    // Store in m_textures map
}

void D3D11Renderer::drawMesh(uint32_t meshHandle, uint32_t textureHandle) {
    // Bind texture SRV to pixel shader
    if (textureHandle) {
        m_context->PSSetShaderResources(0, 1, &srv);
    }
    // Draw mesh
}
```

## D3D12 Implementation Outline

```cpp
// Requires descriptor heap for SRVs
uint32_t D3D12Renderer::createTexture(const char* filepath) {
    // Load image
    // Create upload buffer
    // Create texture resource
    // Copy data via command list
    // Create SRV in descriptor heap
}
```

## STB Image Library

For easy cross-platform image loading, use STB:

1. Download: https://github.com/nothings/stb/blob/master/stb_image.h
2. Place in project directory
3. In ONE .cpp file:
   ```cpp
   #define STB_IMAGE_IMPLEMENTATION
   #include "stb_image.h"
   ```

## Usage

```bash
# Load default cube
./cube_viewer opengl

# Load .X model
./cube_viewer opengl model.x

# Load with D3D11
./cube_viewer d3d11 aircraft.x

# Specify renderer and model
./cube_viewer d3d12 yak18.x
```

## Testing

1. Start with hardcoded cube (no model path)
2. Add simple texture to cube
3. Load simple .X model without texture
4. Load .X model with BMP texture
5. Test all three renderers

The architecture is complete - just need to implement texture support in each renderer!
