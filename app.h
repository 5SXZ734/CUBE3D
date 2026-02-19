// app.h - Application logic (graphics API independent)
#ifndef APP_H
#define APP_H

#include "renderer.h"
#include "model.h"
#include "debug.h"
#include "texture_cache.h"
#include "scene.h"
#include <vector>

// Forward declarations
struct GLFWwindow;

// ==================== Application Class ====================
class CubeApp {
public:
    CubeApp();
    ~CubeApp();

    // Initialize with a specific renderer and optional model path
    bool initialize(RendererAPI api, const char* modelPath = nullptr);
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
    bool createDefaultCube();
    bool loadModel(const char* path);
    void createExampleScene();  // Create 100 airplanes
    void createGroundPlane();   // Create ground grid
    Mat4 createTransformMatrix(float x, float y, float z, float rotY, float scale);

    // Window
    GLFWwindow* m_window;
    int m_width;
    int m_height;

    // Renderer
    IRenderer* m_renderer;
    TextureCache m_textureCache;

    // Scene data - can be either cube or loaded model
    struct MeshData {
        uint32_t meshHandle;
        uint32_t textureHandle;
    };
    std::vector<MeshData> m_meshes;
    uint32_t m_shader;
    
    // Loaded model (if any)
    Model m_model;
    bool m_hasModel;
    
    // Scene system for multiple objects
    Scene m_scene;
    bool m_useSceneMode;  // Toggle between single object and scene mode
    bool m_useLightBackground;  // Toggle between dark and light background
    std::unordered_map<const Model*, std::vector<uint32_t>> m_modelMeshHandles;
    std::unordered_map<const Model*, std::vector<uint32_t>> m_modelTextureHandles;
    
    // Ground plane
    uint32_t m_groundMesh;
    bool m_showGround;

    // Camera/input state
    bool m_dragging;
    double m_lastX;
    double m_lastY;
    float m_yaw;
    float m_pitch;
    float m_distance;

    // Timing
    double m_lastFrameTime;
    double m_startTime;
    
    // Debug and stats
    bool m_debugMode;
    bool m_strictValidation;
    bool m_showStats;
    PerformanceStats m_stats;
};

#endif // APP_H
