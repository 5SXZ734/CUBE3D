// app.h - Application logic (graphics API independent)
#ifndef APP_H
#define APP_H

#include "renderer.h"
#include "model.h"
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

private:
    void update(float deltaTime);
    void render();
    bool createDefaultCube();
    bool loadModel(const char* path);

    // Window
    GLFWwindow* m_window;
    int m_width;
    int m_height;

    // Renderer
    IRenderer* m_renderer;

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
};

#endif // APP_H
