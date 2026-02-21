// app.h - Application logic (graphics API independent)
#ifndef APP_H
#define APP_H

#include "renderer.h"
#include "model.h"
#include "debug.h"
#include "texture_cache.h"
#include "entity_registry.h"
#include "model_registry.h"
#include "scene_loader_v2.h"
#include "osd.h"
#include "text_renderer.h"
#include <vector>
#include <unordered_map>

// Forward declarations
struct GLFWwindow;

// ==================== Application Class ====================
class CubeApp {
public:
    CubeApp();
    ~CubeApp();

    // Initialize with a specific renderer and scene file
    bool initialize(RendererAPI api, const char* sceneFile = nullptr);
    void shutdown();

    // Main loop
    void run();

    // Input callbacks (to be called by GLFW)
    void onFramebufferSize(int width, int height);
    void onMouseButton(int button, int action, int mods);
    void onCursorPos(double x, double y);
    void onScroll(double xoffset, double yoffset);
    void onKey(int key, int scancode, int action, int mods);
    
    // Debug/Stats control
    void setDebugMode(bool enabled) { m_debugMode = enabled; }
    void setStrictValidation(bool enabled) { m_strictValidation = enabled; }
    void setShowStats(bool enabled) { m_showStats = enabled; }
    void printStats() const;

private:
    void update(float deltaTime);
    void render();
    void createGroundPlane(const SceneConfigV2::GroundConfig& groundConfig);
    void createModelRenderData(const Model* model);
    void renderEntity(const Entity* entity, const Mat4& viewProj);
    Entity* getPlayerEntity();  // Get user-controlled entity

    // Window
    GLFWwindow* m_window;
    int m_width;
    int m_height;

    // Renderer
    IRenderer* m_renderer;
    TextureCache m_textureCache;
    uint32_t m_shader;
    
    // ECS - Entity Component System
    ModelRegistry m_modelRegistry;
    EntityRegistry m_entityRegistry;
    
    // Mesh handles per model (for rendering)
    std::unordered_map<const Model*, std::vector<uint32_t>> m_modelMeshHandles;
    std::unordered_map<const Model*, std::vector<uint32_t>> m_modelTextureHandles;
    
    // Scene environment
    struct SceneEnvironment {
        // Ground
        uint32_t groundMesh;
        uint32_t runwayMesh;
        uint32_t groundTexture;
        uint32_t runwayTexture;
        bool showGround;
        
        // Background
        bool useLightBackground;
        
        // Lighting
        Vec3 lightDirection;
        Vec3 lightColor;
    } m_environment;
    
    // Procedural normal map for testing
    uint32_t m_proceduralNormalMap;
    bool m_useNormalMapping;

    // Camera state (driven by behaviors or manual)
    Vec3 m_cameraPos;
    Vec3 m_cameraTarget;
    
    // Orbit camera parameters (for orbit mode)
    float m_orbitDistance;
    float m_orbitYaw;
    float m_orbitPitch;
    bool m_orbitMode;
    bool m_dragging;
    double m_lastX;
    double m_lastY;
    
    // Control inputs (arrow keys, etc.)
    bool m_arrowUpPressed;
    bool m_arrowDownPressed;
    bool m_arrowLeftPressed;
    bool m_arrowRightPressed;
    bool m_deletePressed;
    bool m_pageDownPressed;

    // Timing
    double m_lastFrameTime;
    double m_startTime;
    float m_deltaTime;
    
    // Debug and stats
    bool m_debugMode;
    bool m_strictValidation;
    bool m_showStats;
    PerformanceStats m_stats;
    
    // On-Screen Display
    FlightOSD m_osd;
    ITextRenderer* m_textRenderer;
};

#endif // APP_H
