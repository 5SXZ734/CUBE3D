// model.h - 3D Model loading interface
#ifndef MODEL_H
#define MODEL_H

#include "renderer.h"
#include <string>
#include <vector>

// ==================== Model Data Structures ====================

struct ModelVertex {
    float px, py, pz;    // position
    float nx, ny, nz;    // normal
    float u, v;          // texture coordinates
    float tx, ty, tz;    // tangent (for normal mapping)
    float bx, by, bz;    // bitangent (for normal mapping)
};

struct ModelMesh {
    std::vector<ModelVertex> vertices;
    std::vector<uint32_t> indices;
    std::string texturePath;         // Diffuse/color texture
    std::string normalMapPath;       // Normal map texture (optional)
    uint32_t rendererMeshHandle;
    uint32_t rendererTextureHandle;
    uint32_t rendererNormalMapHandle;  // Handle to normal map texture
};

struct Model {
    std::vector<ModelMesh> meshes;
    std::string directory;
    
    void clear() {
        meshes.clear();
        directory.clear();
    }
};

// ==================== Model Loader ====================

class ModelLoader {
public:
    // Load .X file (DirectX mesh format)
    static bool LoadXFile(const char* filepath, Model& outModel);
    
    // Load simple OBJ file (alternative format)
    static bool LoadOBJ(const char* filepath, Model& outModel);
    
private:
    static bool LoadTexture(const std::string& path, Model& model);
    static std::string GetDirectory(const std::string& filepath);
};

// ==================== Renderer Extensions ====================

// Extended renderer interface with texture support
class IRendererExt : public IRenderer {
public:
    virtual uint32_t createTexture(const char* filepath) = 0;
    virtual void destroyTexture(uint32_t textureHandle) = 0;
    virtual void bindTexture(uint32_t textureHandle) = 0;
};

#endif // MODEL_H
