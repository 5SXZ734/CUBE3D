// scene.h - Scene management for multiple objects with instancing
#ifndef SCENE_H
#define SCENE_H

#include "renderer.h"
#include "model.h"
#include <vector>
#include <unordered_map>
#include <string>

// ==================== Scene Object ====================
struct SceneObject {
    const Model* model;        // Reference to shared model data
    Mat4 transform;           // World transform (position, rotation, scale)
    Vec4 colorTint;          // Color tint (r, g, b, intensity)
    bool visible;            // Is object visible?
    
    SceneObject();  // Constructor defined in app.cpp where mat4_identity is available
};

// ==================== Render Batch ====================
// Groups instances that share mesh + texture for efficient rendering
struct RenderBatch {
    uint32_t meshHandle;
    uint32_t textureHandle;
    std::vector<InstanceData> instances;
    
    void clear() {
        instances.clear();
    }
    
    void addInstance(const Mat4& world, const Vec4& color) {
        instances.push_back({world, color});
    }
};

// Get rendering statistics
struct RenderStats {
	uint32_t objectCount;
	uint32_t visibleObjects;
	uint32_t drawCalls;
	uint32_t instancesDrawn;
	uint32_t batchCount;
	float averageInstancesPerBatch;
};

// ==================== Scene ====================
class Scene {
private:
    std::vector<SceneObject> m_objects;
    
    // Batching cache
    struct BatchKey {
        uint32_t meshHandle;
        uint32_t textureHandle;
        
        bool operator==(const BatchKey& other) const {
            return meshHandle == other.meshHandle && 
                   textureHandle == other.textureHandle;
        }
    };
    
    struct BatchKeyHash {
        size_t operator()(const BatchKey& key) const {
            return ((size_t)key.meshHandle << 32) | key.textureHandle;
        }
    };
    
    std::unordered_map<BatchKey, RenderBatch, BatchKeyHash> m_batches;
    
    // Statistics
    uint32_t m_lastDrawCalls;
    uint32_t m_lastInstancesDrawn;
    uint32_t m_lastBatchCount;
    
public:
    Scene() 
        : m_lastDrawCalls(0)
        , m_lastInstancesDrawn(0)
        , m_lastBatchCount(0)
    {}
    
    // Add object to scene
    uint32_t addObject(const SceneObject& obj) {
        m_objects.push_back(obj);
        return (uint32_t)(m_objects.size() - 1);
    }
    
    // Get object by index
    SceneObject* getObject(uint32_t index) {
        if (index >= m_objects.size()) return nullptr;
        return &m_objects[index];
    }
    
    const SceneObject* getObject(uint32_t index) const {
        if (index >= m_objects.size()) return nullptr;
        return &m_objects[index];
    }
    
    // Remove object
    void removeObject(uint32_t index) {
        if (index < m_objects.size()) {
            m_objects.erase(m_objects.begin() + index);
        }
    }
    
    // Clear all objects
    void clear() {
        m_objects.clear();
    }
    
    // Get object count
    size_t getObjectCount() const {
        return m_objects.size();
    }
    
    // Render scene with automatic batching
    void render(IRenderer* renderer, 
                const std::unordered_map<const Model*, std::vector<uint32_t>>& modelMeshHandles,
                const std::unordered_map<const Model*, std::vector<uint32_t>>& modelTextureHandles) {
        
        m_lastDrawCalls = 0;
        m_lastInstancesDrawn = 0;
        m_lastBatchCount = 0;
        
        // For fallback rendering: draw each object completely
        // (True instancing would batch differently, but fallback draws full models)
        
        static bool once = true;
        if (once) {
            printf("Scene::render - %zu objects to render\n", m_objects.size());
            once = false;
        }
        
        for (const auto& obj : m_objects) {
            if (!obj.visible || !obj.model) continue;
            
            const Model* model = obj.model;
            auto meshIt = modelMeshHandles.find(model);
            auto texIt = modelTextureHandles.find(model);
            
            if (meshIt == modelMeshHandles.end()) continue;
            
            const auto& meshes = meshIt->second;
            const auto& textures = (texIt != modelTextureHandles.end()) 
                                  ? texIt->second 
                                  : std::vector<uint32_t>();
            
            // Create a single-instance array for this object
            InstanceData instance;
            instance.worldMatrix = obj.transform;
            instance.colorTint = obj.colorTint;
            
            // Draw all meshes of this model with this instance transform
            for (size_t i = 0; i < meshes.size(); i++) {
                uint32_t texHandle = (i < textures.size()) ? textures[i] : 0;
                
                // Use instanced drawing with count=1 (fallback will draw once)
                renderer->drawMeshInstanced(meshes[i], texHandle, &instance, 1);
                
                m_lastDrawCalls++;
            }
            
            m_lastInstancesDrawn++;
        }
        
        m_lastBatchCount = m_lastDrawCalls;  // In fallback mode, one batch per draw call
    }
    
   
    RenderStats getRenderStats() const {
        RenderStats stats;
        stats.objectCount = (uint32_t)m_objects.size();
        stats.visibleObjects = 0;
        for (const auto& obj : m_objects) {
            if (obj.visible && obj.model) stats.visibleObjects++;
        }
        stats.drawCalls = m_lastDrawCalls;
        stats.instancesDrawn = m_lastInstancesDrawn;
        stats.batchCount = m_lastBatchCount;
        stats.averageInstancesPerBatch = (m_lastBatchCount > 0) 
            ? (float)m_lastInstancesDrawn / (float)m_lastBatchCount 
            : 0.0f;
        return stats;
    }
    
    void printRenderStats() const {
        RenderStats stats = getRenderStats();
        printf("\n=== Scene Render Statistics ===\n");
        printf("Total Objects:    %u\n", stats.objectCount);
        printf("Visible Objects:  %u\n", stats.visibleObjects);
        printf("Draw Calls:       %u\n", stats.drawCalls);
        printf("Instances Drawn:  %u\n", stats.instancesDrawn);
        printf("Batches:          %u\n", stats.batchCount);
        printf("Avg Inst/Batch:   %.1f\n", stats.averageInstancesPerBatch);
        printf("===============================\n\n");
    }
};

// Note: mat4_identity() is defined in app.cpp
// Declare it here if needed, or include the math utilities header

#endif // SCENE_H
