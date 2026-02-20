// renderer.h - Graphics API abstraction layer
#ifndef RENDERER_H
#define RENDERER_H

#include <cstdint>
#include "math_utils.h"

// Forward declarations for platform types
struct GLFWwindow;

// ==================== Instance Data ====================
struct InstanceData {
    Mat4 worldMatrix;    // Per-instance world transform
    Vec4 colorTint;      // Per-instance color tint (r, g, b, intensity)
};

// ==================== Vertex Format ====================
struct Vertex {
    float px, py, pz;    // position
    float nx, ny, nz;    // normal
    float r, g, b, a;    // color
    float u, v;          // texture coordinates
    float tx, ty, tz;    // tangent (for normal mapping)
    float bx, by, bz;    // bitangent (for normal mapping)
};

// ==================== Renderer Interface ====================
class IRenderer {
public:
    virtual ~IRenderer() = default;
    
    // Initialization
    virtual bool initialize(GLFWwindow* window) = 0;
    virtual void shutdown() = 0;
    
    // Frame management
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void setClearColor(float r, float g, float b, float a) = 0;
    
    // Viewport
    virtual void setViewport(int width, int height) = 0;
    
    // Mesh creation (with texture coordinates)
    virtual uint32_t createMesh(const Vertex* vertices, uint32_t vertexCount,
                               const uint16_t* indices, uint32_t indexCount) = 0;
    virtual void destroyMesh(uint32_t meshHandle) = 0;
    
    // Texture support
    virtual uint32_t createTexture(const char* filepath) = 0;
    virtual uint32_t createTextureFromData(const uint8_t* data, int width, int height, int channels) = 0;
    virtual void destroyTexture(uint32_t textureHandle) = 0;
    virtual void bindTextureToUnit(uint32_t textureHandle, int unit) = 0;  // Bind texture to specific unit
    
    // Shader/material
    virtual uint32_t createShader(const char* vertexSource, const char* fragmentSource) = 0;
    virtual void destroyShader(uint32_t shaderHandle) = 0;
    virtual void useShader(uint32_t shaderHandle) = 0;
    
    // Uniforms
    virtual void setUniformMat4(uint32_t shaderHandle, const char* name, const Mat4& matrix) = 0;
    virtual void setUniformVec3(uint32_t shaderHandle, const char* name, const Vec3& vec) = 0;
    virtual void setUniformInt(uint32_t shaderHandle, const char* name, int value) = 0;
    
    // Drawing
    virtual void drawMesh(uint32_t meshHandle, uint32_t textureHandle = 0) = 0;
    virtual void drawMeshInstanced(uint32_t meshHandle, uint32_t textureHandle,
                                   const InstanceData* instances, uint32_t instanceCount) = 0;
    
    // State
    virtual void setDepthTest(bool enable) = 0;
    virtual void setCulling(bool enable) = 0;
};

// ==================== Renderer Factory ====================
enum class RendererAPI {
    OpenGL,
    Direct3D11,
    Direct3D12,
    // Future: Vulkan, Metal, etc.
};

IRenderer* createRenderer(RendererAPI api);

// Default shader sources (defined in renderer_opengl.cpp)
extern const char* OPENGL_VERTEX_SHADER;
extern const char* OPENGL_FRAGMENT_SHADER;

#endif // RENDERER_H
