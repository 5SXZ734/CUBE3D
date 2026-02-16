// app.h - Application logic (graphics API independent)
#ifndef APP_H
#define APP_H

#include "renderer.h"

// Forward declarations
struct GLFWwindow;

// ==================== Application Class ====================
class CubeApp {
public:
    CubeApp();
    ~CubeApp();

    // Initialize with a specific renderer
    bool initialize(RendererAPI api);
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

    // Window
    GLFWwindow* m_window;
    int m_width;
    int m_height;

    // Renderer
    IRenderer* m_renderer;

    // Scene data
    uint32_t m_cubeMesh;
    uint32_t m_shader;

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
