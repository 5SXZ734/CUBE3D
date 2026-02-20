// app.h - Application logic (graphics API independent)
#ifndef APP_H
#define APP_H

#include "renderer.h"
#include "model.h"
#include "debug.h"
#include "texture_cache.h"
#include "scene.h"
#include "scene_loader.h"
#include "flight_dynamics.h"
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
    
    // Scene loading
    bool loadScene(const SceneFile& scene);
    
    // Model/cube creation (public for fallback in main.cpp)
    bool hasModel() const { return m_hasModel; }
    bool createDefaultCube();

private:
    void update(float deltaTime);
    void render();
    bool loadModel(const char* path);
    void createGroundPlane();   // Create ground grid
    void updateFPSCamera(float deltaTime);  // Update FPS camera for scene mode
    void updateChaseCamera(float deltaTime); // Update chase camera for flight mode
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
    
    // Single object transform (from scene file)
    Vec3 m_objectPosition;
    Vec3 m_objectRotation;  // Euler angles in degrees
    Vec3 m_objectScale;
    
    // Scene system for multiple objects
    Scene m_scene;
    bool m_useSceneMode;  // Toggle between single object and scene mode
    bool m_useLightBackground;  // Toggle between dark and light background
    std::unordered_map<const Model*, std::vector<uint32_t>> m_modelMeshHandles;
    std::unordered_map<const Model*, std::vector<uint32_t>> m_modelTextureHandles;
    
    // Scene file for toggling with 'T' key
    SceneFile m_sceneFile;
    bool m_hasSceneFile;
    
    // Ground plane
    uint32_t m_groundMesh;
    bool m_showGround;
    
    // Procedural normal map for testing
    uint32_t m_proceduralNormalMap;
    bool m_useNormalMapping;

    // Camera/input state (orbit camera mode)
    enum CameraType { CAMERA_FPS, CAMERA_ORBIT, CAMERA_CHASE };
    CameraType m_cameraType;
    bool m_autoRotate;
    
    // Flight simulation
    bool m_flightMode;              // Is flight simulation active?
    FlightDynamics m_flightDynamics;
    
    // Chase camera (for flight mode)
    Vec3 m_chaseCameraPos;          // Current camera position
    Vec3 m_chaseCameraTarget;       // Look-at target
    float m_chaseDistance;          // Distance behind aircraft (meters)
    float m_chaseHeight;            // Height above aircraft (meters)
    float m_chaseSmoothness;        // Camera lag smoothing (0-1)
    
    // Control inputs
    bool m_arrowUpPressed;
    bool m_arrowDownPressed;
    bool m_arrowLeftPressed;
    bool m_arrowRightPressed;
    bool m_deletePressed;           // Rudder left (Delete key)
    bool m_pageDownPressed;         // Rudder right (Page Down key)
    
    bool m_dragging;
    double m_lastX;
    double m_lastY;
    float m_yaw;
    float m_pitch;
    float m_distance;
    
    // FPS camera state (scene mode)
    Vec3 m_cameraPos;
    Vec3 m_cameraForward;
    Vec3 m_cameraRight;
    Vec3 m_cameraUp;
    float m_cameraYaw;
    float m_cameraPitch;
    float m_moveSpeed;
    bool m_firstMouse;
    double m_lastMouseX, m_lastMouseY;
    bool m_wPressed, m_aPressed, m_sPressed, m_dPressed;
    bool m_spacePressed, m_shiftPressed;

    // Timing
    double m_lastFrameTime;
    double m_startTime;
    float m_deltaTime;  // Delta time for current frame
    
    // Debug and stats
    bool m_debugMode;
    bool m_strictValidation;
    bool m_showStats;
    PerformanceStats m_stats;
};

#endif // APP_H
