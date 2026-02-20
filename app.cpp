// app.cpp - Application logic implementation
#include "app.h"
#include "debug.h"
#include "normal_map_gen.h"  // Procedural normal map generation
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

// SceneObject constructor implementation
SceneObject::SceneObject()
    : model(nullptr)
    , transform(mat4_identity())
    , colorTint({1.0f, 1.0f, 1.0f, 1.0f})
    , visible(true)
{}

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
    , m_shader(0)
    , m_hasModel(false)
    , m_useSceneMode(false)
    , m_useLightBackground(false)
    , m_dragging(false)
    , m_lastX(0.0)
    , m_lastY(0.0)
    , m_yaw(0.6f)
    , m_pitch(-0.4f)
    , m_distance(3.5f)
    , m_cameraPos({0, 20.0f, 80.0f})  // Start position for FPS camera
    , m_cameraForward({0, 0, -1})
    , m_cameraRight({1, 0, 0})
    , m_cameraUp({0, 1, 0})
    , m_cameraYaw(0.0f)  // Facing -Z (yaw=0 means looking along -Z)
    , m_cameraPitch(0.0f)  // Level (no pitch)
    , m_moveSpeed(30.0f)  // Units per second
    , m_firstMouse(true)
    , m_lastMouseX(0.0)
    , m_lastMouseY(0.0)
    , m_wPressed(false)
    , m_aPressed(false)
    , m_sPressed(false)
    , m_dPressed(false)
    , m_spacePressed(false)
    , m_shiftPressed(false)
    , m_lastFrameTime(0.0)
    , m_startTime(0.0)
    , m_deltaTime(0.0f)
    , m_debugMode(false)
    , m_strictValidation(false)
    , m_showStats(false)
    , m_groundMesh(0)
    , m_showGround(true)
    , m_proceduralNormalMap(0)
    , m_useNormalMapping(false)
{}

CubeApp::~CubeApp() {
    shutdown();
}

void CubeApp::printStats() const {
    m_stats.print();
    m_textureCache.printStats();
    
    if (m_useSceneMode) {
        m_scene.printRenderStats();
    }
}

bool CubeApp::initialize(RendererAPI api, const char* modelPath) {
    LOG_DEBUG("Initializing GLFW...");
    // Initialize GLFW
    if (!glfwInit()) {
        LOG_ERROR("Failed to initialize GLFW");
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
    LOG_DEBUG("Creating window (%dx%d)...", m_width, m_height);
    m_window = glfwCreateWindow(m_width, m_height, "Model Viewer", nullptr, nullptr);
    if (!m_window) {
        LOG_ERROR("Failed to create window");
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
    LOG_DEBUG("Creating renderer...");
    m_renderer = createRenderer(api);
    if (!m_renderer) {
        LOG_ERROR("Failed to create renderer");
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }

    if (!m_renderer->initialize(m_window)) {
        LOG_ERROR("Failed to initialize renderer");
        delete m_renderer;
        m_renderer = nullptr;
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }
    
    // Initialize texture cache with renderer
    m_textureCache.setRenderer(m_renderer);
    LOG_DEBUG("Texture cache initialized");

    // Set initial viewport
    int fbW, fbH;
    glfwGetFramebufferSize(m_window, &fbW, &fbH);
    onFramebufferSize(fbW, fbH);

    // Load model or create default cube
    if (modelPath != nullptr) {
        if (!loadModel(modelPath)) {
            LOG_WARNING("Failed to load model, using default cube");
            if (!createDefaultCube()) {
                return false;
            }
        }
    } else {
        if (!createDefaultCube()) {
            return false;
        }
    }

    // Create shader with texture and normal mapping support
    LOG_DEBUG("Creating shader...");
    m_shader = m_renderer->createShader(OPENGL_VERTEX_SHADER, OPENGL_FRAGMENT_SHADER);
    if (!m_shader) {
        LOG_ERROR("Failed to create shader");
        return false;
    }
    
    // Set texture unit bindings for samplers (OpenGL)
    m_renderer->useShader(m_shader);
    m_renderer->setUniformInt(m_shader, "uTexture", 0);    // Texture unit 0
    m_renderer->setUniformInt(m_shader, "uNormalMap", 1);  // Texture unit 1

    // Set render state
    m_renderer->setDepthTest(true);
    m_renderer->setCulling(false);

    m_startTime = glfwGetTime();
    m_lastFrameTime = m_startTime;
    
    // Create ground plane for scene mode
    createGroundPlane();
    
    // Create procedural normal map for testing
    LOG_INFO("Creating procedural normal map...");
    // Using flat normal map by default - proves system works without visual artifacts
    // Scene stays bright, normal mapping system is functional and ready for real textures
    std::vector<uint8_t> normalMapData = generateFlatNormalMap(256, 256);
    
    // To see surface detail effects, replace above with:
    // generateRivetNormalMap(256, 256) - metal rivets (may darken slightly)
    // generateProceduralNormalMap(256, 256, 0.15f) - wavy bumps (may darken slightly)
    
    m_proceduralNormalMap = m_renderer->createTextureFromData(
        normalMapData.data(), 256, 256, 4);
    
    if (m_proceduralNormalMap) {
        LOG_INFO("Procedural normal map created successfully");
        LOG_INFO("Press 'N' to toggle normal mapping on/off");
    }

    LOG_DEBUG("Application initialization complete");
    if (m_hasModel) {
        LOG_INFO("Loaded model with %zu meshes", m_meshes.size());
    }
    return true;
}

void CubeApp::shutdown() {
    if (m_renderer) {
        // Destroy all meshes and textures
        for (auto& mesh : m_meshes) {
            if (mesh.meshHandle) m_renderer->destroyMesh(mesh.meshHandle);
            if (mesh.textureHandle) m_renderer->destroyTexture(mesh.textureHandle);
        }
        m_meshes.clear();
        
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
        double frameStart = glfwGetTime();
        
        glfwPollEvents();

        double currentTime = glfwGetTime();
        float deltaTime = (float)(currentTime - m_lastFrameTime);
        m_lastFrameTime = currentTime;
        m_deltaTime = deltaTime;  // Store for use in render()

        update(deltaTime);
        render();
        
        // Update performance stats
        if (m_showStats || m_debugMode) {
            double frameEnd = glfwGetTime();
            double frameTime = frameEnd - frameStart;
            m_stats.updateFrameTime(frameTime);
        }
    }
}

void CubeApp::update(float deltaTime) {
    (void)deltaTime;  // Unused currently
    // Application logic updates go here
}

bool CubeApp::loadModel(const char* path) {
    LOG_INFO("Loading model: %s", path);
    
    // Validate file if in strict mode
    if (m_strictValidation && !FileValidator::validateModelPath(path)) {
        LOG_ERROR("Model file validation failed");
        return false;
    }
    
    if (!ModelLoader::LoadXFile(path, m_model)) {
        LOG_ERROR("Failed to load model from: %s", path);
        return false;
    }

    LOG_DEBUG("Model loaded: %zu meshes", m_model.meshes.size());

    // Upload meshes to renderer
    for (size_t i = 0; i < m_model.meshes.size(); i++) {
        auto& modelMesh = m_model.meshes[i];
        
        LOG_DEBUG("  Mesh %zu: %zu vertices, %zu indices", 
                 i, modelMesh.vertices.size(), modelMesh.indices.size());
        
        // Convert ModelVertex to Vertex
        std::vector<Vertex> vertices;
        vertices.reserve(modelMesh.vertices.size());
        
        for (const auto& v : modelMesh.vertices) {
            Vertex vert;
            vert.px = v.px; vert.py = v.py; vert.pz = v.pz;
            vert.nx = v.nx; vert.ny = v.ny; vert.nz = v.nz;
            vert.r = 1.0f; vert.g = 1.0f; vert.b = 1.0f; vert.a = 1.0f; // white for textured
            vert.u = v.u; vert.v = v.v;
            vertices.push_back(vert);
        }

        // Convert indices to uint16_t
        std::vector<uint16_t> indices;
        indices.reserve(modelMesh.indices.size());
        for (uint32_t idx : modelMesh.indices) {
            indices.push_back((uint16_t)idx);
        }
        
        // Validate mesh if strict mode
        if (m_strictValidation) {
            if (!MeshValidator::validate(vertices.data(), (uint32_t)vertices.size(),
                                        indices.data(), (uint32_t)indices.size())) {
                LOG_ERROR("Mesh %zu validation failed", i);
                return false;
            }
        }

        MeshData mesh;
        mesh.meshHandle = m_renderer->createMesh(
            vertices.data(), (uint32_t)vertices.size(),
            indices.data(), (uint32_t)indices.size()
        );
        
        // Update stats
        if (m_showStats || m_debugMode) {
            m_stats.meshesLoaded++;
            m_stats.meshMemoryKB += ((uint32_t)vertices.size() * sizeof(Vertex) + (uint32_t)indices.size() * sizeof(uint16_t)) / 1024;
        }

        // Load texture if present
        mesh.textureHandle = 0;
        if (!modelMesh.texturePath.empty()) {
            if (m_strictValidation && !FileValidator::validateTexturePath(modelMesh.texturePath.c_str())) {
                LOG_WARNING("Texture validation failed for mesh %zu, skipping", i);
            } else {
                LOG_DEBUG("    Loading texture: %s", modelMesh.texturePath.c_str());
                
                // Use texture cache to avoid duplicate loads
                mesh.textureHandle = m_textureCache.getOrLoad(modelMesh.texturePath.c_str());
                
                if (mesh.textureHandle) {
                    if (m_showStats || m_debugMode) {
                        // Only count unique textures in stats
                        if (m_textureCache.getStats().cacheMisses == m_stats.texturesLoaded + 1) {
                            m_stats.texturesLoaded++;
                        }
                    }
                } else {
                    LOG_WARNING("Failed to load texture: %s", modelMesh.texturePath.c_str());
                }
            }
        }

        m_meshes.push_back(mesh);
    }

    m_hasModel = true;
    LOG_INFO("Model loaded successfully: %zu meshes", m_meshes.size());
    return true;
}

bool CubeApp::createDefaultCube() {
    LOG_DEBUG("Creating default cube");
    
    // Create cube mesh (24 verts, 36 indices) with texture coordinates
    const Vertex verts[] = {
        // +Z (red) - front
        {-1,-1,+1, 0,0,+1, 1,0,0,1, 0,0}, {-1,+1,+1, 0,0,+1, 1,0.5f,0,1, 0,1},
        {+1,+1,+1, 0,0,+1, 1,1,0,1, 1,1}, {+1,-1,+1, 0,0,+1, 0.9f,0.1f,0.1f,1, 1,0},
        // -Z (cyan) - back
        {+1,-1,-1, 0,0,-1, 0,1,1,1, 0,0}, {+1,+1,-1, 0,0,-1, 0,0.7f,1,1, 0,1},
        {-1,+1,-1, 0,0,-1, 0,0.4f,1,1, 1,1}, {-1,-1,-1, 0,0,-1, 0,0.9f,0.9f,1, 1,0},
        // +X (green) - right
        {+1,-1,+1, +1,0,0, 0.2f,1,0.2f,1, 0,0}, {+1,+1,+1, +1,0,0, 0.2f,1,0.6f,1, 0,1},
        {+1,+1,-1, +1,0,0, 0.2f,1,1,1, 1,1}, {+1,-1,-1, +1,0,0, 0.2f,0.8f,0.2f,1, 1,0},
        // -X (magenta) - left
        {-1,-1,-1, -1,0,0, 1,0.2f,1,1, 0,0}, {-1,+1,-1, -1,0,0, 0.8f,0.2f,1,1, 0,1},
        {-1,+1,+1, -1,0,0, 0.6f,0.2f,1,1, 1,1}, {-1,-1,+1, -1,0,0, 1,0.2f,0.8f,1, 1,0},
        // +Y (white) - top
        {-1,+1,+1, 0,+1,0, 1,1,1,1, 0,0}, {-1,+1,-1, 0,+1,0, 0.8f,0.8f,0.8f,1, 0,1},
        {+1,+1,-1, 0,+1,0, 0.6f,0.6f,0.6f,1, 1,1}, {+1,+1,+1, 0,+1,0, 0.9f,0.9f,0.9f,1, 1,0},
        // -Y (brown) - bottom
        {-1,-1,-1, 0,-1,0, 0.6f,0.3f,0.1f,1, 0,0}, {-1,-1,+1, 0,-1,0, 0.7f,0.35f,0.12f,1, 0,1},
        {+1,-1,+1, 0,-1,0, 0.8f,0.4f,0.15f,1, 1,1}, {+1,-1,-1, 0,-1,0, 0.5f,0.25f,0.08f,1, 1,0},
    };

    const uint16_t idx[] = {
        0,1,2, 0,2,3,       // front
        4,5,6, 4,6,7,       // back
        8,9,10, 8,10,11,    // right
        12,13,14, 12,14,15, // left
        16,17,18, 16,18,19, // top
        20,21,22, 20,22,23  // bottom
    };

    MeshData mesh;
    mesh.meshHandle = m_renderer->createMesh(verts, 24, idx, 36);
    mesh.textureHandle = 0; // No texture for cube
    m_meshes.push_back(mesh);

    m_hasModel = false;
    return true;
}

void CubeApp::render() {
    // Set background color based on toggle
    if (m_useLightBackground) {
        m_renderer->setClearColor(0.9f, 0.9f, 0.95f, 1.0f);  // Light blue-gray
    } else {
        m_renderer->setClearColor(0.03f, 0.03f, 0.06f, 1.0f);  // Dark blue
    }
    
    m_renderer->beginFrame();

    float t = (float)(glfwGetTime() - m_startTime);
    float aspect = (float)m_width / (float)m_height;

    // Set uniforms once
    m_renderer->useShader(m_shader);
    m_renderer->setUniformVec3(m_shader, "uLightDir", {-0.6f, -1.0f, -0.4f});
    
    // Set normal mapping state for the entire frame
    int useNormalMap = (m_useNormalMapping && m_proceduralNormalMap) ? 1 : 0;
    m_renderer->setUniformInt(m_shader, "uUseNormalMap", useNormalMap);
    
    // Bind normal map to texture unit 1 (stays bound for the frame)
    if (m_proceduralNormalMap && m_useNormalMapping) {
        m_renderer->bindTextureToUnit(m_proceduralNormalMap, 1);
    }

    // Render in scene mode or single object mode
    if (m_useSceneMode && m_hasModel) {
        // Scene mode: FPS camera controls
        // Update FPS camera using deltaTime from main loop
        updateFPSCamera(m_deltaTime);
        
        // Build view matrix from FPS camera
        Vec3 target = {
            m_cameraPos.x + m_cameraForward.x,
            m_cameraPos.y + m_cameraForward.y,
            m_cameraPos.z + m_cameraForward.z
        };
        
        // Use world up (0,1,0) directly - simpler and more reliable
        Vec3 worldUp = {0, 1, 0};
        Mat4 view = mat4_lookAtRH(m_cameraPos, target, worldUp);
        Mat4 proj = mat4_perspectiveRH_NO(60.0f * 3.14159265359f / 180.0f, aspect, 0.1f, 1000.0f);
        Mat4 viewProj = mat4_mul(proj, view);
        
        m_renderer->setUniformMat4(m_shader, "uMVP", viewProj);
        
        // Draw ground plane first
        if (m_showGround && m_groundMesh) {
            Mat4 groundWorld = mat4_identity();
            Mat4 groundMVP = viewProj;  // Ground at origin, identity world transform
            m_renderer->setUniformMat4(m_shader, "uMVP", groundMVP);
            m_renderer->setUniformMat4(m_shader, "uWorld", groundWorld);
            m_renderer->setUniformInt(m_shader, "uUseTexture", 0);
            
            // Draw as lines
            m_renderer->drawMesh(m_groundMesh, 0);
            
            // Reset for scene rendering
            m_renderer->setUniformMat4(m_shader, "uMVP", viewProj);
        }
        
        m_scene.render(m_renderer, m_modelMeshHandles, m_modelTextureHandles);
        
        // Track scene stats
        if (m_showStats || m_debugMode) {
            auto sceneStats = m_scene.getRenderStats();
            m_stats.drawCalls = sceneStats.drawCalls;
            m_stats.meshesDrawn = sceneStats.instancesDrawn;
        }
    } else {
        // Single object mode: original behavior
        Mat4 view = mat4_lookAtRH({0, 0, m_distance}, {0, 0, 0}, {0, 1, 0});
        Mat4 proj = mat4_perspectiveRH_NO(60.0f * 3.14159265359f / 180.0f, aspect, 0.1f, 100.0f);
        
        Mat4 RyAuto = mat4_rotate_y(t * 0.3f);
        Mat4 Ry     = mat4_rotate_y(m_yaw);
        Mat4 Rx     = mat4_rotate_x(m_pitch);
        Mat4 world  = mat4_mul(RyAuto, mat4_mul(Ry, Rx));
        
        Mat4 mvp = mat4_mul(proj, mat4_mul(view, world));
        
        m_renderer->setUniformMat4(m_shader, "uMVP", mvp);
        m_renderer->setUniformMat4(m_shader, "uWorld", world);
        
        // Draw all meshes
        for (const auto& mesh : m_meshes) {
            m_renderer->setUniformInt(m_shader, "uUseTexture", mesh.textureHandle ? 1 : 0);
            
            m_renderer->drawMesh(mesh.meshHandle, mesh.textureHandle);
            
            if (m_showStats || m_debugMode) {
                m_stats.drawCalls++;
                m_stats.meshesDrawn++;
            }
        }
    }

    m_renderer->endFrame();
}

// ==================== Input Callbacks ====================
void CubeApp::onFramebufferSize(int width, int height) {
    m_width = std::max(1, width);
    m_height = std::max(1, height);
    m_renderer->setViewport(m_width, m_height);
}

void CubeApp::onMouseButton(int button, int action, int mods) {
    (void)mods;  // Unused
    
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            m_dragging = true;
            glfwGetCursorPos(m_window, &m_lastX, &m_lastY);
            if (m_useSceneMode) {
                m_lastMouseX = m_lastX;
                m_lastMouseY = m_lastY;
                m_firstMouse = false;  // Reset first mouse flag
            }
        } else if (action == GLFW_RELEASE) {
            m_dragging = false;
            if (m_useSceneMode) {
                m_firstMouse = true;  // Reset for next drag
            }
        }
    }
}

void CubeApp::onCursorPos(double x, double y) {
    if (m_useSceneMode) {
        // FPS camera look in scene mode - only when left button pressed
        if (!m_dragging) return;  // Only look when dragging
        
        if (m_firstMouse) {
            m_lastMouseX = x;
            m_lastMouseY = y;
            m_firstMouse = false;
            return;
        }
        
        double dx = x - m_lastMouseX;
        double dy = y - m_lastMouseY;
        m_lastMouseX = x;
        m_lastMouseY = y;
        
        const float sensitivity = 0.002f;  // Mouse sensitivity
        m_cameraYaw += (float)dx * sensitivity;  // Changed to + for correct direction
        m_cameraPitch += (float)dy * sensitivity;  // Positive: mouse down = look down
        
        // Clamp pitch to prevent flipping
        const float pitchLimit = 1.5708f;  // ~90 degrees
        m_cameraPitch = std::clamp(m_cameraPitch, -pitchLimit, pitchLimit);
        return;
    }
    
    // Single object mode: drag to rotate
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
    (void)xoffset;  // Unused
    
    if (m_useSceneMode) {
        // In scene mode, scroll changes movement speed
        m_moveSpeed += (float)yoffset * 5.0f;
        m_moveSpeed = std::clamp(m_moveSpeed, 5.0f, 100.0f);
        LOG_INFO("Move speed: %.1f", m_moveSpeed);
        return;
    }
    
    // Single object mode: scroll to zoom
    m_distance -= (float)yoffset * 0.25f;
    m_distance = std::clamp(m_distance, 1.5f, 12.0f);
}

void CubeApp::onKey(int key, int scancode, int action, int mods) {
    (void)scancode;  // Unused
    (void)mods;      // Unused
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(m_window, 1);
    }
    
    // FPS camera controls (WASD + Space + Shift)
    if (key == GLFW_KEY_W) m_wPressed = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_A) m_aPressed = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_S) m_sPressed = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_D) m_dPressed = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_SPACE) m_spacePressed = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT) 
        m_shiftPressed = (action != GLFW_RELEASE);
    
    // Toggle scene mode with 'T' key (changed from S to avoid conflict)
    if (key == GLFW_KEY_T && action == GLFW_PRESS) {
        m_useSceneMode = !m_useSceneMode;
        
        if (m_useSceneMode) {
            LOG_INFO("Scene mode ENABLED - FPS camera controls");
            LOG_INFO("WASD to move, Left-click + drag to look, Space/Shift for up/down");
            LOG_INFO("Press 'B' to toggle background, 'G' to toggle ground");
            
            if (m_hasModel) {
                createExampleScene();
            }
        } else {
            LOG_INFO("Scene mode DISABLED - showing single object");
        }
    }
    
    // Toggle background with 'B' key
    if (key == GLFW_KEY_B && action == GLFW_PRESS) {
        m_useLightBackground = !m_useLightBackground;
        LOG_INFO("Background: %s", m_useLightBackground ? "LIGHT" : "DARK");
    }
    
    // Toggle ground with 'G' key
    if (key == GLFW_KEY_G && action == GLFW_PRESS) {
        m_showGround = !m_showGround;
        LOG_INFO("Ground plane: %s", m_showGround ? "VISIBLE" : "HIDDEN");
    }
    
    // Toggle normal mapping with 'N' key
    if (key == GLFW_KEY_N && action == GLFW_PRESS) {
        m_useNormalMapping = !m_useNormalMapping;
        LOG_INFO("Normal mapping: %s", m_useNormalMapping ? "ENABLED" : "DISABLED");
    }
}

// Helper to create transform matrix
Mat4 CubeApp::createTransformMatrix(float x, float y, float z, float rotY, float scale) {
    Mat4 mat;
    
    // Initialize to identity
    for (int i = 0; i < 16; i++) mat.m[i] = 0.0f;
    mat.m[0] = mat.m[5] = mat.m[10] = mat.m[15] = 1.0f;
    
    // Apply scale
    mat.m[0] = scale;
    mat.m[5] = scale;
    mat.m[10] = scale;
    
    // Apply rotation around Y axis
    float cosY = std::cos(rotY);
    float sinY = std::sin(rotY);
    mat.m[0] *= cosY;
    mat.m[2] = sinY * scale;
    mat.m[8] = -sinY * scale;
    mat.m[10] *= cosY;
    
    // Apply translation
    mat.m[12] = x;
    mat.m[13] = y;
    mat.m[14] = z;
    
    return mat;
}

// Create example scene with 100 airplanes
void CubeApp::createExampleScene() {
    if (!m_hasModel) {
        LOG_WARNING("Cannot create scene: no model loaded");
        return;
    }
    
    LOG_INFO("Creating example scene with 100 airplanes...");
    
    m_scene.clear();
    
    // Store model mesh/texture handles for scene rendering
    m_modelMeshHandles[&m_model].clear();
    m_modelTextureHandles[&m_model].clear();
    
    for (const auto& mesh : m_meshes) {
        m_modelMeshHandles[&m_model].push_back(mesh.meshHandle);
        m_modelTextureHandles[&m_model].push_back(mesh.textureHandle);
    }
    
    // Create 10x10 grid of airplanes
    const float spacing = 15.0f;
    const float gridSize = 10;
    const float offset = -(gridSize - 1) * spacing * 0.5f;
    
    for (int x = 0; x < gridSize; x++) {
        for (int z = 0; z < gridSize; z++) {
            SceneObject obj;
            obj.model = &m_model;
            
            float posX = offset + x * spacing;
            float posZ = offset + z * spacing;
            float rotY = (x + z) * 0.2f;  // Vary rotation
            
            obj.transform = createTransformMatrix(posX, 0, posZ, rotY, 1.0f);
            
            // Color gradient across the grid
            obj.colorTint = {
                (float)x / (gridSize - 1),  // Red varies with X
                (float)z / (gridSize - 1),  // Green varies with Z
                0.8f,                        // Blue constant
                1.0f                         // Full intensity
            };
            
            obj.visible = true;
            m_scene.addObject(obj);
        }
    }
    
    LOG_INFO("Scene created: %zu objects", m_scene.getObjectCount());
}

// Create ground plane - a grid to show spatial reference
void CubeApp::createGroundPlane() {
    LOG_INFO("Creating ground plane...");
    
    const float gridSize = 200.0f;    // Total size
    const int gridDivisions = 20;     // Number of cells
    const float step = gridSize / gridDivisions;
    const float halfSize = gridSize * 0.5f;
    const float groundY = -10.0f;
    
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    
    // Create a flat grid of quads
    for (int z = 0; z < gridDivisions; z++) {
        for (int x = 0; x < gridDivisions; x++) {
            float x0 = -halfSize + x * step;
            float x1 = x0 + step;
            float z0 = -halfSize + z * step;
            float z1 = z0 + step;
            
            // Checkerboard pattern - alternating colors
            bool isLight = ((x + z) % 2) == 0;
            float brightness = isLight ? 0.25f : 0.15f;
            
            // Highlight center lines
            bool isCenterX = (x == gridDivisions / 2);
            bool isCenterZ = (z == gridDivisions / 2);
            
            float r = brightness, g = brightness, b = brightness;
            if (isCenterX) r = 0.4f;  // Red X axis
            if (isCenterZ) b = 0.4f;  // Blue Z axis
            
            // Create quad (2 triangles)
            uint16_t baseIdx = (uint16_t)vertices.size();
            
            Vertex v0, v1, v2, v3;
            v0.px = x0; v0.py = groundY; v0.pz = z0;
            v0.nx = 0; v0.ny = 1; v0.nz = 0;
            v0.r = r; v0.g = g; v0.b = b; v0.a = 1.0f;
            v0.u = 0; v0.v = 0;
            
            v1.px = x1; v1.py = groundY; v1.pz = z0;
            v1.nx = 0; v1.ny = 1; v1.nz = 0;
            v1.r = r; v1.g = g; v1.b = b; v1.a = 1.0f;
            v1.u = 0; v1.v = 0;
            
            v2.px = x1; v2.py = groundY; v2.pz = z1;
            v2.nx = 0; v2.ny = 1; v2.nz = 0;
            v2.r = r; v2.g = g; v2.b = b; v2.a = 1.0f;
            v2.u = 0; v2.v = 0;
            
            v3.px = x0; v3.py = groundY; v3.pz = z1;
            v3.nx = 0; v3.ny = 1; v3.nz = 0;
            v3.r = r; v3.g = g; v3.b = b; v3.a = 1.0f;
            v3.u = 0; v3.v = 0;
            
            vertices.push_back(v0);
            vertices.push_back(v1);
            vertices.push_back(v2);
            vertices.push_back(v3);
            
            // Triangle 1: v0, v1, v2
            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 1);
            indices.push_back(baseIdx + 2);
            
            // Triangle 2: v0, v2, v3
            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 2);
            indices.push_back(baseIdx + 3);
        }
    }
    
    m_groundMesh = m_renderer->createMesh(
        vertices.data(), (uint32_t)vertices.size(),
        indices.data(), (uint32_t)indices.size()
    );
    
    LOG_INFO("Ground plane created: %zu vertices, %zu triangles", 
             vertices.size(), indices.size() / 3);
}

// Update FPS camera for scene mode
void CubeApp::updateFPSCamera(float deltaTime) {
    // Standard FPS camera: yaw=0 looks along -Z, pitch=0 is level
    // Forward vector calculation using spherical coordinates
    float cosPitch = std::cos(m_cameraPitch);
    m_cameraForward.x = cosPitch * std::sin(m_cameraYaw);
    m_cameraForward.y = -std::sin(m_cameraPitch);  // Negative for correct up/down
    m_cameraForward.z = -cosPitch * std::cos(m_cameraYaw);  // Negative for -Z at yaw=0
    
    // Normalize forward vector
    float len = std::sqrt(m_cameraForward.x * m_cameraForward.x + 
                         m_cameraForward.y * m_cameraForward.y + 
                         m_cameraForward.z * m_cameraForward.z);
    if (len > 0.0001f) {
        m_cameraForward.x /= len;
        m_cameraForward.y /= len;
        m_cameraForward.z /= len;
    }
    
    // Calculate right vector (cross product: worldUp × forward)
    Vec3 worldUp = {0, 1, 0};
    m_cameraRight.x = worldUp.y * m_cameraForward.z - worldUp.z * m_cameraForward.y;
    m_cameraRight.y = worldUp.z * m_cameraForward.x - worldUp.x * m_cameraForward.z;
    m_cameraRight.z = worldUp.x * m_cameraForward.y - worldUp.y * m_cameraForward.x;
    
    // Normalize right vector
    len = std::sqrt(m_cameraRight.x * m_cameraRight.x + 
                   m_cameraRight.y * m_cameraRight.y + 
                   m_cameraRight.z * m_cameraRight.z);
    if (len > 0.0001f) {
        m_cameraRight.x /= len;
        m_cameraRight.y /= len;
        m_cameraRight.z /= len;
    }
    
    // Calculate up vector (cross product of right and forward)
    m_cameraUp.x = m_cameraRight.y * m_cameraForward.z - m_cameraRight.z * m_cameraForward.y;
    m_cameraUp.y = m_cameraRight.z * m_cameraForward.x - m_cameraRight.x * m_cameraForward.z;
    m_cameraUp.z = m_cameraRight.x * m_cameraForward.y - m_cameraRight.y * m_cameraForward.x;
    
    // Handle movement input
    float speed = m_moveSpeed * deltaTime;
    if (m_shiftPressed) speed *= 2.0f;  // Sprint
    
    static int debugFrames = 0;
    if (debugFrames < 10 && (m_wPressed || m_aPressed || m_sPressed || m_dPressed)) {
        printf("FPS Movement: W=%d A=%d S=%d D=%d, speed=%.2f, deltaTime=%.4f\n", 
               m_wPressed, m_aPressed, m_sPressed, m_dPressed, speed, deltaTime);
        debugFrames++;
    }
    
    if (m_wPressed) {  // Forward
        m_cameraPos.x += m_cameraForward.x * speed;
        m_cameraPos.y += m_cameraForward.y * speed;
        m_cameraPos.z += m_cameraForward.z * speed;
    }
    if (m_sPressed) {  // Backward
        m_cameraPos.x -= m_cameraForward.x * speed;
        m_cameraPos.y -= m_cameraForward.y * speed;
        m_cameraPos.z -= m_cameraForward.z * speed;
    }
    if (m_aPressed) {  // Left
        m_cameraPos.x += m_cameraRight.x * speed;  // Changed to + (was -)
        m_cameraPos.y += m_cameraRight.y * speed;
        m_cameraPos.z += m_cameraRight.z * speed;
    }
    if (m_dPressed) {  // Right
        m_cameraPos.x -= m_cameraRight.x * speed;  // Changed to - (was +)
        m_cameraPos.y -= m_cameraRight.y * speed;
        m_cameraPos.z -= m_cameraRight.z * speed;
    }
    if (m_spacePressed) {  // Up
        m_cameraPos.y += speed;
    }
    if (m_shiftPressed && !m_wPressed && !m_sPressed && !m_aPressed && !m_dPressed) {  // Down (when not used for sprint)
        m_cameraPos.y -= speed;
    }
}

