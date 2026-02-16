// app.cpp - Application logic implementation
#include "app.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

// ==================== Math Utilities ====================
static Mat4 mat4_identity() {
    Mat4 r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

static Mat4 mat4_mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c = 0; c < 4; ++c)
    for (int row = 0; row < 4; ++row) {
        r.m[c*4 + row] =
            a.m[0*4 + row] * b.m[c*4 + 0] +
            a.m[1*4 + row] * b.m[c*4 + 1] +
            a.m[2*4 + row] * b.m[c*4 + 2] +
            a.m[3*4 + row] * b.m[c*4 + 3];
    }
    return r;
}

static Mat4 mat4_rotate_x(float a) {
    Mat4 r = mat4_identity();
    float c = std::cos(a), s = std::sin(a);
    r.m[5] = c;  r.m[9]  = -s;
    r.m[6] = s;  r.m[10] =  c;
    return r;
}

static Mat4 mat4_rotate_y(float a) {
    Mat4 r = mat4_identity();
    float c = std::cos(a), s = std::sin(a);
    r.m[0] =  c; r.m[8] = s;
    r.m[2] = -s; r.m[10] = c;
    return r;
}

static Vec3 v3_sub(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
static float v3_dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static Vec3 v3_cross(Vec3 a, Vec3 b) {
    return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
}
static Vec3 v3_norm(Vec3 v) {
    float len = std::sqrt(std::max(1e-20f, v3_dot(v,v)));
    return { v.x/len, v.y/len, v.z/len };
}

static Mat4 mat4_lookAtRH(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = v3_norm(v3_sub(center, eye));
    Vec3 s = v3_norm(v3_cross(f, up));
    Vec3 u = v3_cross(s, f);

    Mat4 r = mat4_identity();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8]  = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9]  = u.z;
    r.m[2] =-f.x; r.m[6] =-f.y; r.m[10] =-f.z;

    r.m[12] = -v3_dot(s, eye);
    r.m[13] = -v3_dot(u, eye);
    r.m[14] =  v3_dot(f, eye);
    return r;
}

static Mat4 mat4_perspectiveRH_NO(float fovyRadians, float aspect, float zNear, float zFar) {
    float f = 1.0f / std::tan(fovyRadians * 0.5f);

    Mat4 r{};
    r.m[0]  = f / aspect;
    r.m[5]  = f;
    r.m[10] = (zFar + zNear) / (zNear - zFar);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
    return r;
}

// ==================== GLFW Callbacks (static wrappers) ====================
static void glfw_framebuffer_size_callback(GLFWwindow* win, int w, int h) {
    CubeApp* app = (CubeApp*)glfwGetWindowUserPointer(win);
    if (app) app->onFramebufferSize(w, h);
}

static void glfw_mouse_button_callback(GLFWwindow* win, int button, int action, int mods) {
    CubeApp* app = (CubeApp*)glfwGetWindowUserPointer(win);
    if (app) app->onMouseButton(button, action, mods);
}

static void glfw_cursor_pos_callback(GLFWwindow* win, double x, double y) {
    CubeApp* app = (CubeApp*)glfwGetWindowUserPointer(win);
    if (app) app->onCursorPos(x, y);
}

static void glfw_scroll_callback(GLFWwindow* win, double xoff, double yoff) {
    CubeApp* app = (CubeApp*)glfwGetWindowUserPointer(win);
    if (app) app->onScroll(xoff, yoff);
}

static void glfw_key_callback(GLFWwindow* win, int key, int scancode, int action, int mods) {
    CubeApp* app = (CubeApp*)glfwGetWindowUserPointer(win);
    if (app) app->onKey(key, scancode, action, mods);
}

// ==================== CubeApp Implementation ====================
CubeApp::CubeApp()
    : m_window(nullptr)
    , m_width(1280)
    , m_height(720)
    , m_renderer(nullptr)
    , m_cubeMesh(0)
    , m_shader(0)
    , m_dragging(false)
    , m_lastX(0.0)
    , m_lastY(0.0)
    , m_yaw(0.6f)
    , m_pitch(-0.4f)
    , m_distance(3.5f)
    , m_lastFrameTime(0.0)
    , m_startTime(0.0)
{}

CubeApp::~CubeApp() {
    shutdown();
}

bool CubeApp::initialize(RendererAPI api) {
    // Initialize GLFW
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return false;
    }

    // Configure GLFW for OpenGL 3.3 core
    if (api == RendererAPI::OpenGL) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_DEPTH_BITS, 24);
    }

    // Create window
    m_window = glfwCreateWindow(m_width, m_height, "Cube Viewer", nullptr, nullptr);
    if (!m_window) {
        std::fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // vsync

    // Set callbacks
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, glfw_framebuffer_size_callback);
    glfwSetMouseButtonCallback(m_window, glfw_mouse_button_callback);
    glfwSetCursorPosCallback(m_window, glfw_cursor_pos_callback);
    glfwSetScrollCallback(m_window, glfw_scroll_callback);
    glfwSetKeyCallback(m_window, glfw_key_callback);

    // Create renderer
    m_renderer = createRenderer(api);
    if (!m_renderer) {
        std::fprintf(stderr, "Failed to create renderer\n");
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }

    if (!m_renderer->initialize(m_window)) {
        std::fprintf(stderr, "Failed to initialize renderer\n");
        delete m_renderer;
        m_renderer = nullptr;
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }

    // Set initial viewport
    int fbW, fbH;
    glfwGetFramebufferSize(m_window, &fbW, &fbH);
    onFramebufferSize(fbW, fbH);

    // Create cube mesh
    const Vertex verts[] = {
        // +Z (red tones)
        {-1,-1,+1, 0,0,+1, 1,0,0,1}, {-1,+1,+1, 0,0,+1, 1,0.5f,0,1},
        {+1,+1,+1, 0,0,+1, 1,1,0,1}, {+1,-1,+1, 0,0,+1, 0.9f,0.1f,0.1f,1},
        // -Z (cyan tones)
        {+1,-1,-1, 0,0,-1, 0,1,1,1}, {+1,+1,-1, 0,0,-1, 0,0.7f,1,1},
        {-1,+1,-1, 0,0,-1, 0,0.4f,1,1}, {-1,-1,-1, 0,0,-1, 0,0.9f,0.9f,1},
        // +X (green tones)
        {+1,-1,+1, +1,0,0, 0.2f,1,0.2f,1}, {+1,+1,+1, +1,0,0, 0.2f,1,0.6f,1},
        {+1,+1,-1, +1,0,0, 0.2f,1,1,1}, {+1,-1,-1, +1,0,0, 0.2f,0.8f,0.2f,1},
        // -X (magenta tones)
        {-1,-1,-1, -1,0,0, 1,0.2f,1,1}, {-1,+1,-1, -1,0,0, 0.8f,0.2f,1,1},
        {-1,+1,+1, -1,0,0, 0.6f,0.2f,1,1}, {-1,-1,+1, -1,0,0, 1,0.2f,0.8f,1},
        // +Y (white/gray tones)
        {-1,+1,+1, 0,+1,0, 1,1,1,1}, {-1,+1,-1, 0,+1,0, 0.8f,0.8f,0.8f,1},
        {+1,+1,-1, 0,+1,0, 0.6f,0.6f,0.6f,1}, {+1,+1,+1, 0,+1,0, 0.9f,0.9f,0.9f,1},
        // -Y (brown tones)
        {-1,-1,-1, 0,-1,0, 0.6f,0.3f,0.1f,1}, {-1,-1,+1, 0,-1,0, 0.7f,0.35f,0.12f,1},
        {+1,-1,+1, 0,-1,0, 0.8f,0.4f,0.15f,1}, {+1,-1,-1, 0,-1,0, 0.5f,0.25f,0.08f,1},
    };

    const uint16_t idx[] = {
        0,1,2, 0,2,3,
        4,5,6, 4,6,7,
        8,9,10, 8,10,11,
        12,13,14, 12,14,15,
        16,17,18, 16,18,19,
        20,21,22, 20,22,23
    };

    m_cubeMesh = m_renderer->createMesh(verts, 24, idx, 36);

    // Create shader
    const char* vsSrc = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec4 aCol;

uniform mat4 uMVP;
uniform mat4 uWorld;

out vec3 vNrmW;
out vec4 vCol;

void main()
{
    gl_Position = uMVP * vec4(aPos, 1.0);
    vNrmW = normalize((uWorld * vec4(aNrm, 0.0)).xyz);
    vCol = aCol;
}
)";

    const char* fsSrc = R"(
#version 330 core
in vec3 vNrmW;
in vec4 vCol;
uniform vec3 uLightDir;
out vec4 FragColor;

void main()
{
    vec3 L = normalize(-uLightDir);
    float ndl = max(dot(normalize(vNrmW), L), 0.0);
    float ambient = 0.18;
    float diff = ambient + ndl * 0.82;
    FragColor = vec4(vCol.rgb * diff, 1.0);
}
)";

    m_shader = m_renderer->createShader(vsSrc, fsSrc);

    // Set render state
    m_renderer->setDepthTest(true);
    m_renderer->setCulling(false); // Disable culling to see all faces

    m_startTime = glfwGetTime();
    m_lastFrameTime = m_startTime;

    std::printf("Application initialized successfully\n");
    return true;
}

void CubeApp::shutdown() {
    if (m_renderer) {
        if (m_cubeMesh) m_renderer->destroyMesh(m_cubeMesh);
        if (m_shader) m_renderer->destroyShader(m_shader);
        m_renderer->shutdown();
        delete m_renderer;
        m_renderer = nullptr;
    }

    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    glfwTerminate();
}

void CubeApp::run() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        double currentTime = glfwGetTime();
        float deltaTime = (float)(currentTime - m_lastFrameTime);
        m_lastFrameTime = currentTime;

        update(deltaTime);
        render();
    }
}

void CubeApp::update(float deltaTime) {
    // Application logic updates go here
}

void CubeApp::render() {
    m_renderer->setClearColor(0.03f, 0.03f, 0.06f, 1.0f);
    m_renderer->beginFrame();

    float t = (float)(glfwGetTime() - m_startTime);
    float aspect = (float)m_width / (float)m_height;

    // World matrix: auto-spin + mouse rotation
    Mat4 RyAuto = mat4_rotate_y(t * 0.3f);
    Mat4 Ry     = mat4_rotate_y(m_yaw);
    Mat4 Rx     = mat4_rotate_x(m_pitch);
    Mat4 world  = mat4_mul(RyAuto, mat4_mul(Ry, Rx));

    // View matrix: camera at +Z looking at origin
    Mat4 view = mat4_lookAtRH({0, 0, m_distance}, {0, 0, 0}, {0, 1, 0});

    // Projection matrix
    Mat4 proj = mat4_perspectiveRH_NO(60.0f * 3.14159265359f / 180.0f, aspect, 0.1f, 100.0f);

    // Combined MVP
    Mat4 mvp = mat4_mul(proj, mat4_mul(view, world));

    // Set uniforms and draw
    m_renderer->useShader(m_shader);
    m_renderer->setUniformMat4(m_shader, "uMVP", mvp);
    m_renderer->setUniformMat4(m_shader, "uWorld", world);
    m_renderer->setUniformVec3(m_shader, "uLightDir", {-0.6f, -1.0f, -0.4f});

    m_renderer->drawMesh(m_cubeMesh);

    m_renderer->endFrame();
}

// ==================== Input Callbacks ====================
void CubeApp::onFramebufferSize(int width, int height) {
    m_width = std::max(1, width);
    m_height = std::max(1, height);
    m_renderer->setViewport(m_width, m_height);
}

void CubeApp::onMouseButton(int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            m_dragging = true;
            glfwGetCursorPos(m_window, &m_lastX, &m_lastY);
        } else if (action == GLFW_RELEASE) {
            m_dragging = false;
        }
    }
}

void CubeApp::onCursorPos(double x, double y) {
    if (!m_dragging) return;

    double dx = x - m_lastX;
    double dy = y - m_lastY;
    m_lastX = x;
    m_lastY = y;

    const float sensitivity = 0.005f;
    m_yaw   += (float)dx * sensitivity;
    m_pitch += (float)dy * sensitivity;

    const float limit = 1.57079632679f - 0.01f;
    m_pitch = std::clamp(m_pitch, -limit, +limit);
}

void CubeApp::onScroll(double xoffset, double yoffset) {
    m_distance -= (float)yoffset * 0.25f;
    m_distance = std::clamp(m_distance, 1.5f, 12.0f);
}

void CubeApp::onKey(int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(m_window, 1);
    }
}
