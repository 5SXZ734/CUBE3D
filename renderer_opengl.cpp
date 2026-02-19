// renderer_opengl.cpp - OpenGL implementation
#include "renderer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>
#include "stb_image.h"

// ==================== OpenGL Texture ====================
struct GLTexture {
    GLuint id;
    int width;
    int height;
};

// ==================== OpenGL Mesh ====================
struct GLMesh {
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    uint32_t indexCount;
};

// ==================== OpenGL Shader ====================
struct GLShader {
    GLuint program;
    std::unordered_map<std::string, GLint> uniformLocations;
};

// ==================== OpenGL Renderer ====================
class OpenGLRenderer : public IRenderer {
private:
    GLFWwindow* m_window;
    std::unordered_map<uint32_t, GLMesh> m_meshes;
    std::unordered_map<uint32_t, GLShader> m_shaders;
    std::unordered_map<uint32_t, GLTexture> m_textures;
    uint32_t m_nextMeshHandle;
    uint32_t m_nextShaderHandle;
    uint32_t m_nextTextureHandle;
    uint32_t m_currentShader;
    
    // Store view-projection for instanced rendering
    Mat4 m_viewProj;
    bool m_hasViewProj;
    bool m_inInstancedDraw;  // Flag to prevent overwriting viewProj during instanced draw

    GLuint compileShader(GLenum type, const char* src) {
        GLuint sh = glCreateShader(type);
        glShaderSource(sh, 1, &src, nullptr);
        glCompileShader(sh);

        GLint ok = 0;
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
            std::string log((size_t)len, '\0');
            glGetShaderInfoLog(sh, len, nullptr, log.data());
            std::fprintf(stderr, "Shader compile error:\n%s\n", log.c_str());
            glDeleteShader(sh);
            return 0;
        }
        return sh;
    }

    GLuint linkProgram(GLuint vs, GLuint fs) {
        GLuint p = glCreateProgram();
        glAttachShader(p, vs);
        glAttachShader(p, fs);
        glLinkProgram(p);

        GLint ok = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
            std::string log((size_t)len, '\0');
            glGetProgramInfoLog(p, len, nullptr, log.data());
            std::fprintf(stderr, "Program link error:\n%s\n", log.c_str());
            glDeleteProgram(p);
            return 0;
        }
        return p;
    }

public:
    OpenGLRenderer() 
        : m_window(nullptr)
        , m_nextMeshHandle(1)
        , m_nextShaderHandle(1)
        , m_nextTextureHandle(1)
        , m_currentShader(0)
        , m_hasViewProj(false)
        , m_inInstancedDraw(false)
    {}

    virtual ~OpenGLRenderer() {
        shutdown();
    }

    bool initialize(GLFWwindow* window) override {
        m_window = window;

        // Load OpenGL functions
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::fprintf(stderr, "Failed to load OpenGL via GLAD.\n");
            return false;
        }

        std::printf("OpenGL Renderer initialized\n");
        std::printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
        std::printf("GLSL Version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

        // Set default state
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glClearDepth(1.0);

        return true;
    }

    void shutdown() override {
        // Clean up meshes
        for (auto& pair : m_meshes) {
            glDeleteVertexArrays(1, &pair.second.vao);
            glDeleteBuffers(1, &pair.second.vbo);
            glDeleteBuffers(1, &pair.second.ebo);
        }
        m_meshes.clear();

        // Clean up shaders
        for (auto& pair : m_shaders) {
            glDeleteProgram(pair.second.program);
        }
        m_shaders.clear();
    }

    void beginFrame() override {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void endFrame() override {
        glfwSwapBuffers(m_window);
    }

    void setClearColor(float r, float g, float b, float a) override {
        glClearColor(r, g, b, a);
    }

    void setViewport(int width, int height) override {
        glViewport(0, 0, width, height);
    }

    uint32_t createMesh(const Vertex* vertices, uint32_t vertexCount,
                       const uint16_t* indices, uint32_t indexCount) override {
        GLMesh mesh;
        mesh.indexCount = indexCount;

        glGenVertexArrays(1, &mesh.vao);
        glGenBuffers(1, &mesh.vbo);
        glGenBuffers(1, &mesh.ebo);

        glBindVertexArray(mesh.vao);

        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(Vertex), vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(uint16_t), indices, GL_STATIC_DRAW);

        // Position (attribute 0)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

        // Normal (attribute 1)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));

        // Color (attribute 2)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float)));

        // TexCoord (attribute 3)
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(10 * sizeof(float)));

        // Tangent (attribute 4) - for normal mapping
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(12 * sizeof(float)));

        // Bitangent (attribute 5) - for normal mapping
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(15 * sizeof(float)));

        glBindVertexArray(0);

        uint32_t handle = m_nextMeshHandle++;
        m_meshes[handle] = mesh;
        return handle;
    }

    void destroyMesh(uint32_t meshHandle) override {
        auto it = m_meshes.find(meshHandle);
        if (it != m_meshes.end()) {
            glDeleteVertexArrays(1, &it->second.vao);
            glDeleteBuffers(1, &it->second.vbo);
            glDeleteBuffers(1, &it->second.ebo);
            m_meshes.erase(it);
        }
    }

    uint32_t createShader(const char* vertexSource, const char* fragmentSource) override {
        GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
        if (!vs || !fs) {
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
            return 0;
        }

        GLuint program = linkProgram(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);

        if (!program) return 0;

        GLShader shader;
        shader.program = program;

        uint32_t handle = m_nextShaderHandle++;
        m_shaders[handle] = shader;
        return handle;
    }

    void destroyShader(uint32_t shaderHandle) override {
        auto it = m_shaders.find(shaderHandle);
        if (it != m_shaders.end()) {
            glDeleteProgram(it->second.program);
            m_shaders.erase(it);
        }
    }

    void useShader(uint32_t shaderHandle) override {
        auto it = m_shaders.find(shaderHandle);
        if (it != m_shaders.end()) {
            glUseProgram(it->second.program);
            m_currentShader = shaderHandle;
        }
    }

    void setUniformMat4(uint32_t shaderHandle, const char* name, const Mat4& matrix) override {
        auto it = m_shaders.find(shaderHandle);
        if (it != m_shaders.end()) {
            GLShader& shader = it->second;
            
            // Cache uniform location
            auto locIt = shader.uniformLocations.find(name);
            GLint loc;
            if (locIt == shader.uniformLocations.end()) {
                loc = glGetUniformLocation(shader.program, name);
                shader.uniformLocations[name] = loc;
            } else {
                loc = locIt->second;
            }

            if (loc >= 0) {
                glUniformMatrix4fv(loc, 1, GL_FALSE, matrix.m);
                
                // Store view-projection for instanced rendering
                // Only store when setting the initial view-proj, not during instanced drawing
                if (std::string(name) == "uMVP" && !m_inInstancedDraw) {
                    m_viewProj = matrix;
                    m_hasViewProj = true;
                }
            }
        }
    }

    void setUniformVec3(uint32_t shaderHandle, const char* name, const Vec3& vec) override {
        auto it = m_shaders.find(shaderHandle);
        if (it != m_shaders.end()) {
            GLShader& shader = it->second;
            
            // Cache uniform location
            auto locIt = shader.uniformLocations.find(name);
            GLint loc;
            if (locIt == shader.uniformLocations.end()) {
                loc = glGetUniformLocation(shader.program, name);
                shader.uniformLocations[name] = loc;
            } else {
                loc = locIt->second;
            }

            if (loc >= 0) {
                glUniform3f(loc, vec.x, vec.y, vec.z);
            }
        }
    }

    uint32_t createTexture(const char* filepath) override {
        // Load image using stb_image
        int width, height, channels;
        stbi_set_flip_vertically_on_load(true); // OpenGL expects bottom-left origin
        unsigned char* data = stbi_load(filepath, &width, &height, &channels, 0);
        
        if (!data) {
            std::fprintf(stderr, "Failed to load texture: %s\n", filepath);
            std::fprintf(stderr, "STB Error: %s\n", stbi_failure_reason());
            return 0;
        }

        std::printf("Loaded texture: %s (%dx%d, %d channels)\n", filepath, width, height, channels);

        // Create OpenGL texture
        GLTexture texture;
        texture.width = width;
        texture.height = height;

        glGenTextures(1, &texture.id);
        glBindTexture(GL_TEXTURE_2D, texture.id);
        
        // Determine format
        GLenum format = GL_RGB;
        if (channels == 1) format = GL_RED;
        else if (channels == 3) format = GL_RGB;
        else if (channels == 4) format = GL_RGBA;
        
        // Upload texture data
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        
        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        // Generate mipmaps
        glGenerateMipmap(GL_TEXTURE_2D);
        
        glBindTexture(GL_TEXTURE_2D, 0);
        
        // Free image data
        stbi_image_free(data);
        
        uint32_t handle = m_nextTextureHandle++;
        m_textures[handle] = texture;
        return handle;
    }
    
    uint32_t createTextureFromData(const uint8_t* data, int width, int height, int channels) override {
        if (!data) {
            std::fprintf(stderr, "createTextureFromData: null data pointer\n");
            return 0;
        }
        
        std::printf("Creating texture from data (%dx%d, %d channels)\n", width, height, channels);
        
        // Create OpenGL texture
        GLTexture texture;
        texture.width = width;
        texture.height = height;

        glGenTextures(1, &texture.id);
        glBindTexture(GL_TEXTURE_2D, texture.id);
        
        // Determine format
        GLenum format = GL_RGB;
        if (channels == 1) format = GL_RED;
        else if (channels == 3) format = GL_RGB;
        else if (channels == 4) format = GL_RGBA;
        
        // Upload texture data (no flip needed for procedural data)
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        
        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        // Generate mipmaps
        glGenerateMipmap(GL_TEXTURE_2D);
        
        glBindTexture(GL_TEXTURE_2D, 0);
        
        // Store and return handle
        uint32_t handle = m_nextTextureHandle++;
        m_textures[handle] = texture;
        return handle;
    }

    void destroyTexture(uint32_t textureHandle) override {
        auto it = m_textures.find(textureHandle);
        if (it != m_textures.end()) {
            glDeleteTextures(1, &it->second.id);
            m_textures.erase(it);
        }
    }
    
    void bindTextureToUnit(uint32_t textureHandle, int unit) override {
        auto it = m_textures.find(textureHandle);
        if (it == m_textures.end()) {
            std::fprintf(stderr, "bindTextureToUnit: Invalid texture handle %u\n", textureHandle);
            return;
        }
        
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, it->second.id);
    }

    void setUniformInt(uint32_t shaderHandle, const char* name, int value) override {
        auto it = m_shaders.find(shaderHandle);
        if (it != m_shaders.end()) {
            GLShader& shader = it->second;
            auto locIt = shader.uniformLocations.find(name);
            GLint loc;
            if (locIt == shader.uniformLocations.end()) {
                loc = glGetUniformLocation(shader.program, name);
                shader.uniformLocations[name] = loc;
            } else {
                loc = locIt->second;
            }
            if (loc >= 0) {
                glUniform1i(loc, value);
            }
        }
    }

    void drawMesh(uint32_t meshHandle, uint32_t textureHandle = 0) override {
        // Bind texture if present
        if (textureHandle > 0) {
            auto texIt = m_textures.find(textureHandle);
            if (texIt != m_textures.end()) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, texIt->second.id);
            }
        } else {
            // Unbind texture
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        
        // Draw mesh
        auto it = m_meshes.find(meshHandle);
        if (it != m_meshes.end()) {
            glBindVertexArray(it->second.vao);
            glDrawElements(GL_TRIANGLES, it->second.indexCount, GL_UNSIGNED_SHORT, (void*)0);
            glBindVertexArray(0);
        }
    }
    
    // Instanced drawing
    void drawMeshInstanced(uint32_t meshHandle, uint32_t textureHandle,
                          const InstanceData* instances, uint32_t instanceCount) override {
        if (instanceCount == 0 || !instances) return;
        if (!m_hasViewProj) return;  // Need view-projection set first
        
        static int debugCount = 0;
        if (debugCount < 3) {
            printf("GL drawMeshInstanced: mesh=%u, tex=%u, count=%u\n", 
                   meshHandle, textureHandle, instanceCount);
            printf("  ViewProj[0]=%f, Instance.world[0]=%f\n", 
                   m_viewProj.m[0], instances[0].worldMatrix.m[0]);
            debugCount++;
        }
        
        // Set flag to prevent viewProj from being overwritten in setUniformMat4 calls
        m_inInstancedDraw = true;
        
        // Fallback implementation: draw one at a time
        // Using stored view-projection from setUniformMat4
        
        for (uint32_t i = 0; i < instanceCount; i++) {
            // Compute MVP = ViewProj * WorldMatrix
            Mat4 world = instances[i].worldMatrix;
            Mat4 mvp = matrixMultiply(m_viewProj, world);
            
            setUniformMat4(m_currentShader, "uMVP", mvp);
            setUniformMat4(m_currentShader, "uWorld", world);
            setUniformInt(m_currentShader, "uUseTexture", textureHandle ? 1 : 0);
            
            drawMesh(meshHandle, textureHandle);
        }
        
        m_inInstancedDraw = false;
    }
    
private:
    // Matrix multiplication helper
    Mat4 matrixMultiply(const Mat4& a, const Mat4& b) {
        Mat4 result{};
        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 4; col++) {
                result.m[col * 4 + row] = 0;
                for (int k = 0; k < 4; k++) {
                    result.m[col * 4 + row] += a.m[k * 4 + row] * b.m[col * 4 + k];
                }
            }
        }
        return result;
    }

    void setDepthTest(bool enable) override {
        if (enable) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
    }

    void setCulling(bool enable) override {
        if (enable) {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glFrontFace(GL_CCW);
        } else {
            glDisable(GL_CULL_FACE);
        }
    }
};

// ==================== Factory ====================
// Note: This factory is duplicated in renderer_d3d11.cpp for Windows builds
// On Windows, link both files and the D3D11 version will be used
#ifndef _WIN32
IRenderer* createRenderer(RendererAPI api) {
    switch (api) {
        case RendererAPI::OpenGL:
            return new OpenGLRenderer();
        case RendererAPI::Direct3D11:
            std::fprintf(stderr, "Direct3D11 is only available on Windows\n");
            return nullptr;
        default:
            return nullptr;
    }
}
#else
// On Windows, provide a helper for D3D11 renderer to call
IRenderer* createOpenGLRenderer() {
    return new OpenGLRenderer();
}
#endif
