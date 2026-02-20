// app.cpp - Application logic implementation
#include "app.h"
#include "debug.h"
#include "normal_map_gen.h"  // Procedural normal map generation
#include "math_utils.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

// ==================== Math Utilities ====================
// Math functions are now in math_utils.h

// SceneObject constructor implementation
SceneObject::SceneObject()
    : model(nullptr)
    , transform(mat4_identity())
    , colorTint({1.0f, 1.0f, 1.0f, 1.0f})
    , visible(true)
{}

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
    , m_objectPosition({0, 0, 0})
    , m_objectRotation({0, 0, 0})
    , m_objectScale({1, 1, 1})
    , m_useSceneMode(false)
    , m_useLightBackground(false)
    , m_cameraType(CAMERA_ORBIT)  // Default to orbit camera
    , m_autoRotate(true)  // Default to auto-rotate
    , m_flightMode(false)
    , m_chaseCameraPos({0, 0, 0})
    , m_chaseCameraTarget({0, 0, 0})
    , m_chaseDistance(25.0f)     // 25 meters behind
    , m_chaseHeight(8.0f)        // 8 meters above
    , m_chaseSmoothness(0.92f)   // Smooth following
    , m_arrowUpPressed(false)
    , m_arrowDownPressed(false)
    , m_arrowLeftPressed(false)
    , m_arrowRightPressed(false)
    , m_deletePressed(false)
    , m_pageDownPressed(false)
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
    , m_groundTexture(0)
    , m_runwayTexture(0)
    , m_showGround(true)
    , m_proceduralNormalMap(0)
    , m_useNormalMapping(false)
    , m_hasSceneFile(false)
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

    // Load model or create default cube (unless scene will provide models)
    if (modelPath != nullptr) {
        if (!loadModel(modelPath)) {
            LOG_WARNING("Failed to load model, using default cube");
            if (!createDefaultCube()) {
                return false;
            }
        }
    } else {
        // No model path provided
        // If a scene file will be loaded, it will provide the model
        // Otherwise we'll create a default cube later
        LOG_DEBUG("No model path provided - scene will load model or default cube will be created");
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
    
    // Create ground plane for scene mode with defaults
    SceneFileGround defaultGround;
    defaultGround.enabled = true;
    defaultGround.size = 10000.0f;  // 20km × 20km
    defaultGround.color[0] = 0.3f; defaultGround.color[1] = 0.4f;
    defaultGround.color[2] = 0.3f; defaultGround.color[3] = 1.0f;
    defaultGround.hasRunway = true;
    defaultGround.runwayWidth = 60.0f;
    defaultGround.runwayLength = 2000.0f;
    createGroundPlane(defaultGround);
    
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
    // Update flight dynamics if in flight mode
    if (m_flightMode) {
        // Gather control inputs from key states
        ControlInputs& controls = m_flightDynamics.getControlInputs();
        
        // Pitch: Arrow Up = pitch up (nose up), Arrow Down = pitch down (nose down)
        controls.elevator = 0;
        if (m_arrowUpPressed) controls.elevator += 1.0f;      // Reversed
        if (m_arrowDownPressed) controls.elevator -= 1.0f;    // Reversed
        
        // Roll: Arrow Left = roll left, Arrow Right = roll right
        controls.aileron = 0;
        if (m_arrowLeftPressed) controls.aileron += 1.0f;     // Reversed
        if (m_arrowRightPressed) controls.aileron -= 1.0f;    // Reversed
        
        // Rudder: Delete = yaw left, Page Down = yaw right
        controls.rudder = 0;
        if (m_deletePressed) controls.rudder += 1.0f;         // Reversed
        if (m_pageDownPressed) controls.rudder -= 1.0f;       // Reversed
        
        // Debug: Log controls if any are active
        if (controls.elevator != 0 || controls.aileron != 0 || controls.rudder != 0) {
            LOG_DEBUG("Controls: elev=%.2f ail=%.2f rud=%.2f thr=%.2f",
                     controls.elevator, controls.aileron, controls.rudder, controls.throttle);
        }
        
        // Throttle is adjusted with +/- keys (handled in onKey)
        
        // Update physics
        m_flightDynamics.update(deltaTime);
        
        // Update chase camera to follow aircraft
        updateChaseCamera(deltaTime);
    }
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

    // Render based on camera type
    if (m_cameraType == CAMERA_FPS && m_hasModel) {
        // Choose camera based on flight mode
        Vec3 cameraPos, cameraTarget;
        
        if (m_flightMode) {
            // Chase camera follows aircraft
            cameraPos = m_chaseCameraPos;
            cameraTarget = m_chaseCameraTarget;
            LOG_DEBUG("Rendering with chase camera: pos(%.1f, %.1f, %.1f)", 
                     cameraPos.x, cameraPos.y, cameraPos.z);
        } else {
            // Normal FPS camera
            updateFPSCamera(m_deltaTime);
            cameraPos = m_cameraPos;
            cameraTarget = {
                m_cameraPos.x + m_cameraForward.x,
                m_cameraPos.y + m_cameraForward.y,
                m_cameraPos.z + m_cameraForward.z
            };
            LOG_DEBUG("Rendering with FPS camera: pos(%.1f, %.1f, %.1f)", 
                     cameraPos.x, cameraPos.y, cameraPos.z);
        }
        
        // Build view matrix
        Vec3 worldUp = {0, 1, 0};
        Mat4 view = mat4_lookAtRH(cameraPos, cameraTarget, worldUp);
        Mat4 proj = mat4_perspectiveRH_NO(60.0f * 3.14159265359f / 180.0f, aspect, 0.1f, 1000.0f);
        Mat4 viewProj = mat4_mul(proj, view);
        
        // Draw ground plane first
        if (m_showGround && m_groundMesh) {
            Mat4 groundWorld = mat4_identity();
            Mat4 groundMVP = viewProj;
            m_renderer->setUniformMat4(m_shader, "uMVP", groundMVP);
            m_renderer->setUniformMat4(m_shader, "uWorld", groundWorld);
            m_renderer->setUniformInt(m_shader, "uUseTexture", m_groundTexture ? 1 : 0);
            m_renderer->drawMesh(m_groundMesh, m_groundTexture);
        }
        
        // If using scene system (100 planes), render from m_scene
        if (m_useSceneMode) {
            m_renderer->setUniformMat4(m_shader, "uMVP", viewProj);
            m_scene.render(m_renderer, m_modelMeshHandles, m_modelTextureHandles);
            
            if (m_showStats || m_debugMode) {
                auto sceneStats = m_scene.getRenderStats();
                m_stats.drawCalls = sceneStats.drawCalls;
                m_stats.meshesDrawn = sceneStats.instancesDrawn;
            }
        } else {
            // Render single object from m_meshes with transform
            Mat4 world;
            
            if (m_flightMode) {
                // Use flight dynamics transform
                world = m_flightDynamics.getTransformMatrix();
            } else {
                // Use scene file transform
                Mat4 translation = mat4_translate(m_objectPosition.x, m_objectPosition.y, m_objectPosition.z);
                Mat4 rotYaw = mat4_rotate_y(m_objectRotation.y * 3.14159f / 180.0f);
                Mat4 rotPitch = mat4_rotate_x(m_objectRotation.x * 3.14159f / 180.0f);
                Mat4 rotRoll = mat4_rotate_z(m_objectRotation.z * 3.14159f / 180.0f);
                Mat4 scale = mat4_scale(m_objectScale.x, m_objectScale.y, m_objectScale.z);
                
                world = mat4_mul(translation, mat4_mul(rotYaw, mat4_mul(rotPitch, mat4_mul(rotRoll, scale))));
            }
            
            Mat4 mvp = mat4_mul(viewProj, world);
            
            m_renderer->setUniformMat4(m_shader, "uMVP", mvp);
            m_renderer->setUniformMat4(m_shader, "uWorld", world);
            
            for (size_t i = 0; i < m_meshes.size(); i++) {
                const auto& mesh = m_meshes[i];
                m_renderer->setUniformInt(m_shader, "uUseTexture", mesh.textureHandle ? 1 : 0);
                m_renderer->drawMesh(mesh.meshHandle, mesh.textureHandle);
                
                if (m_showStats || m_debugMode) {
                    m_stats.drawCalls++;
                    m_stats.meshesDrawn++;
                }
            }
        }
    } else {
        // Orbit camera mode
        Mat4 view = mat4_lookAtRH({0, 0, m_distance}, {0, 0, 0}, {0, 1, 0});
        Mat4 proj = mat4_perspectiveRH_NO(60.0f * 3.14159265359f / 180.0f, aspect, 0.1f, 100.0f);
        Mat4 viewProj = mat4_mul(proj, view);
        
        // Draw ground first if enabled
        if (m_showGround && m_groundMesh) {
            Mat4 groundWorld = mat4_identity();
            Mat4 groundMVP = viewProj;
            m_renderer->setUniformMat4(m_shader, "uMVP", groundMVP);
            m_renderer->setUniformMat4(m_shader, "uWorld", groundWorld);
            m_renderer->setUniformInt(m_shader, "uUseTexture", m_groundTexture ? 1 : 0);
            m_renderer->drawMesh(m_groundMesh, m_groundTexture);
        }
        
        // Apply auto-rotation if enabled
        Mat4 RyAuto = m_autoRotate ? mat4_rotate_y(t * 0.3f) : mat4_identity();
        Mat4 Ry     = mat4_rotate_y(m_yaw);
        Mat4 Rx     = mat4_rotate_x(m_pitch);
        
        // Apply object transform from scene file
        Mat4 translation = mat4_translate(m_objectPosition.x, m_objectPosition.y, m_objectPosition.z);
        Mat4 rotYaw = mat4_rotate_y(m_objectRotation.y * 3.14159f / 180.0f);
        Mat4 rotPitch = mat4_rotate_x(m_objectRotation.x * 3.14159f / 180.0f);
        Mat4 rotRoll = mat4_rotate_z(m_objectRotation.z * 3.14159f / 180.0f);
        
        // Combine: translation * scene_rotation * user_rotation * auto_rotation
        Mat4 world = mat4_mul(translation, mat4_mul(rotYaw, mat4_mul(rotPitch, mat4_mul(rotRoll, mat4_mul(RyAuto, mat4_mul(Ry, Rx))))));
        
        Mat4 mvp = mat4_mul(proj, mat4_mul(view, world));
        
        m_renderer->setUniformMat4(m_shader, "uMVP", mvp);
        m_renderer->setUniformMat4(m_shader, "uWorld", world);
        
        // Draw all meshes
        if (m_meshes.empty()) {
            LOG_DEBUG("No meshes to render!");
        }
        for (size_t i = 0; i < m_meshes.size(); i++) {
            const auto& mesh = m_meshes[i];
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
    if (m_cameraType == CAMERA_FPS) {
        // FPS camera look - only when left button pressed
        if (!m_dragging) return;
        
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
    
    // Orbit camera mode: drag to rotate
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
    
    // Flight controls (arrow keys + Delete/PageDown for rudder)
    if (key == GLFW_KEY_UP) m_arrowUpPressed = (action != GLFW_RELEASE);          // Pitch down
    if (key == GLFW_KEY_DOWN) m_arrowDownPressed = (action != GLFW_RELEASE);      // Pitch up
    if (key == GLFW_KEY_LEFT) m_arrowLeftPressed = (action != GLFW_RELEASE);      // Roll left
    if (key == GLFW_KEY_RIGHT) m_arrowRightPressed = (action != GLFW_RELEASE);    // Roll right
    if (key == GLFW_KEY_DELETE) m_deletePressed = (action != GLFW_RELEASE);       // Rudder left
    if (key == GLFW_KEY_PAGE_DOWN) m_pageDownPressed = (action != GLFW_RELEASE);  // Rudder right
    
    // Toggle flight mode with 'F' key
    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        m_flightMode = !m_flightMode;
        LOG_INFO("Flight mode: %s", m_flightMode ? "ENABLED" : "DISABLED");
        
        if (m_flightMode) {
            // Initialize flight dynamics at object's current position
            Vec3 startPos = m_objectPosition;
            if (startPos.y < 50.0f) startPos.y = 50.0f;  // Ensure minimum altitude
            
            float startHeading = m_objectRotation.y * 3.14159f / 180.0f;  // Convert to radians
            m_flightDynamics.initialize(startPos, startHeading);
            
            // Initialize chase camera at aircraft position
            m_chaseCameraPos = startPos;
            m_chaseCameraPos.z += m_chaseDistance;  // Start behind
            m_chaseCameraPos.y += m_chaseHeight;    // Start above
            m_chaseCameraTarget = startPos;
            
            LOG_INFO("Flight initialized at (%.1f, %.1f, %.1f)", startPos.x, startPos.y, startPos.z);
            LOG_INFO("Flight controls: Arrows=pitch/roll, Del/PgDn=rudder, +/-=throttle");
        } else {
            // Restore to FPS camera mode - don't change camera type
            LOG_INFO("Flight mode disabled - FPS camera restored");
        }
    }
    
    // Throttle controls (when in flight mode)
    if (m_flightMode) {
        if (key == GLFW_KEY_EQUAL && action == GLFW_PRESS) {  // + key
            float& throttle = m_flightDynamics.getControlInputs().throttle;
            throttle = std::min(1.0f, throttle + 0.1f);
            LOG_INFO("Throttle: %.0f%%", throttle * 100);
        }
        if (key == GLFW_KEY_MINUS && action == GLFW_PRESS) {  // - key
            float& throttle = m_flightDynamics.getControlInputs().throttle;
            throttle = std::max(0.0f, throttle - 0.1f);
            LOG_INFO("Throttle: %.0f%%", throttle * 100);
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
// Create ground plane - a grid to show spatial reference
void CubeApp::createGroundPlane(const SceneFileGround& groundConfig) {
    LOG_INFO("Creating ground plane%s...", groundConfig.hasRunway ? " with runway" : "");
    
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    
    const float groundY = 0.0f;  // Ground at Y=0
    
    // ==================== LOAD TEXTURES ====================
    
    m_groundTexture = 0;
    m_runwayTexture = 0;
    
    if (!groundConfig.texturePath.empty()) {
        LOG_INFO("  Loading ground texture: %s", groundConfig.texturePath.c_str());
        m_groundTexture = m_renderer->createTexture(groundConfig.texturePath.c_str());
        if (!m_groundTexture) {
            LOG_WARNING("Failed to load ground texture, using solid color");
        }
    }
    
    if (groundConfig.hasRunway && !groundConfig.runwayTexturePath.empty()) {
        LOG_INFO("  Loading runway texture: %s", groundConfig.runwayTexturePath.c_str());
        m_runwayTexture = m_renderer->createTexture(groundConfig.runwayTexturePath.c_str());
        if (!m_runwayTexture) {
            LOG_WARNING("Failed to load runway texture, using solid color");
        }
    }
    
    // ==================== GROUND SURFACE ====================
    
    float surfSize = groundConfig.size;  // Use size from scene file
    
    Vertex sv0, sv1, sv2, sv3;
    
    // Vertex positions (large quad)
    sv0.px = -surfSize; sv0.py = groundY; sv0.pz = -surfSize;
    sv1.px = -surfSize; sv1.py = groundY; sv1.pz =  surfSize;
    sv2.px =  surfSize; sv2.py = groundY; sv2.pz =  surfSize;
    sv3.px =  surfSize; sv3.py = groundY; sv3.pz = -surfSize;
    
    // Normals (all pointing up)
    sv0.nx = sv1.nx = sv2.nx = sv3.nx = 0.0f;
    sv0.ny = sv1.ny = sv2.ny = sv3.ny = 1.0f;
    sv0.nz = sv1.nz = sv2.nz = sv3.nz = 0.0f;
    
    // Ground color from scene file
    float gr = groundConfig.color[0];
    float gg = groundConfig.color[1];
    float gb = groundConfig.color[2];
    float ga = groundConfig.color[3];
    sv0.r = sv1.r = sv2.r = sv3.r = gr;
    sv0.g = sv1.g = sv2.g = sv3.g = gg;
    sv0.b = sv1.b = sv2.b = sv3.b = gb;
    sv0.a = sv1.a = sv2.a = sv3.a = ga;
    
    // Texture coordinates - tiled
    float texRepeat = surfSize / 500.0f;  // Repeat every 500m
    sv0.u = 0.0f;       sv0.v = 0.0f;
    sv1.u = 0.0f;       sv1.v = texRepeat;
    sv2.u = texRepeat;  sv2.v = texRepeat;
    sv3.u = texRepeat;  sv3.v = 0.0f;
    
    // Tangents and bitangents (for normal mapping, if used)
    sv0.tx = sv1.tx = sv2.tx = sv3.tx = 1.0f;
    sv0.ty = sv1.ty = sv2.ty = sv3.ty = 0.0f;
    sv0.tz = sv1.tz = sv2.tz = sv3.tz = 0.0f;
    sv0.bx = sv1.bx = sv2.bx = sv3.bx = 0.0f;
    sv0.by = sv1.by = sv2.by = sv3.by = 0.0f;
    sv0.bz = sv1.bz = sv2.bz = sv3.bz = 1.0f;
    
    // Add ground quad
    uint16_t baseIdx = 0;
    vertices.push_back(sv0);
    vertices.push_back(sv1);
    vertices.push_back(sv2);
    vertices.push_back(sv3);
    
    // Ground indices (2 triangles)
    indices.push_back(baseIdx + 0);
    indices.push_back(baseIdx + 1);
    indices.push_back(baseIdx + 2);
    
    indices.push_back(baseIdx + 0);
    indices.push_back(baseIdx + 2);
    indices.push_back(baseIdx + 3);
    
    // ==================== RUNWAY STRIP (Optional) ====================
    
    if (groundConfig.hasRunway) {
        float stripWidth = groundConfig.runwayWidth / 2.0f;    // Half-width (±)
        float stripLength = groundConfig.runwayLength / 2.0f;  // Half-length (±)
        float stripY = 0.1f;  // Slightly above ground (z-fighting prevention)
        
        Vertex rs0, rs1, rs2, rs3;
        
        // Runway positions (centered, runs along Z axis)
        rs0.px = -stripWidth; rs0.py = stripY; rs0.pz = -stripLength;
        rs1.px = -stripWidth; rs1.py = stripY; rs1.pz =  stripLength;
        rs2.px =  stripWidth; rs2.py = stripY; rs2.pz =  stripLength;
        rs3.px =  stripWidth; rs3.py = stripY; rs3.pz = -stripLength;
        
        // Normals (up)
        rs0.nx = rs1.nx = rs2.nx = rs3.nx = 0.0f;
        rs0.ny = rs1.ny = rs2.ny = rs3.ny = 1.0f;
        rs0.nz = rs1.nz = rs2.nz = rs3.nz = 0.0f;
        
        // Runway color from scene file
        float rr = groundConfig.runwayColor[0];
        float rg = groundConfig.runwayColor[1];
        float rb = groundConfig.runwayColor[2];
        float ra = groundConfig.runwayColor[3];
        rs0.r = rs1.r = rs2.r = rs3.r = rr;
        rs0.g = rs1.g = rs2.g = rs3.g = rg;
        rs0.b = rs1.b = rs2.b = rs3.b = rb;
        rs0.a = rs1.a = rs2.a = rs3.a = ra;
        
        // Texture coordinates (repeats every 100m)
        float runwayRepeat = groundConfig.runwayLength / 100.0f;
        rs0.u = 0.0f;           rs0.v = 0.0f;
        rs1.u = 0.0f;           rs1.v = runwayRepeat;
        rs2.u = runwayRepeat;   rs2.v = runwayRepeat;
        rs3.u = runwayRepeat;   rs3.v = 0.0f;
        
        // Tangents and bitangents
        rs0.tx = rs1.tx = rs2.tx = rs3.tx = 1.0f;
        rs0.ty = rs1.ty = rs2.ty = rs3.ty = 0.0f;
        rs0.tz = rs1.tz = rs2.tz = rs3.tz = 0.0f;
        rs0.bx = rs1.bx = rs2.bx = rs3.bx = 0.0f;
        rs0.by = rs1.by = rs2.by = rs3.by = 0.0f;
        rs0.bz = rs1.bz = rs2.bz = rs3.bz = 1.0f;
        
        // Add runway quad
        baseIdx = (uint16_t)vertices.size();
        vertices.push_back(rs0);
        vertices.push_back(rs1);
        vertices.push_back(rs2);
        vertices.push_back(rs3);
        
        // Runway indices (2 triangles)
        indices.push_back(baseIdx + 0);
        indices.push_back(baseIdx + 1);
        indices.push_back(baseIdx + 2);
        
        indices.push_back(baseIdx + 0);
        indices.push_back(baseIdx + 2);
        indices.push_back(baseIdx + 3);
        
        LOG_INFO("  Runway: %.0fm × %.0fm", groundConfig.runwayWidth, groundConfig.runwayLength);
    }
    
    // ==================== CREATE MESH ====================
    
    m_groundMesh = m_renderer->createMesh(
        vertices.data(), (uint32_t)vertices.size(),
        indices.data(), (uint32_t)indices.size()
    );
    
    LOG_INFO("Ground created: %.0fm×%.0fm terrain (%zu verts, %zu tris)", 
             surfSize * 2.0f, surfSize * 2.0f, vertices.size(), indices.size() / 3);
    LOG_INFO("  Ground texture: %u, Runway texture: %u", m_groundTexture, m_runwayTexture);
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

void CubeApp::updateChaseCamera(float deltaTime) {
    const AircraftState& state = m_flightDynamics.getState();
    
    LOG_DEBUG("Aircraft state: pos(%.1f, %.1f, %.1f) yaw=%.2f pitch=%.2f", 
             state.position.x, state.position.y, state.position.z,
             state.yaw, state.pitch);
    
    // Calculate ideal camera position behind and above aircraft
    // Camera offset in aircraft body frame
    Vec3 offsetBody = {0, m_chaseHeight, m_chaseDistance};  // Behind (+Z) and above (+Y)
    
    // Transform offset from body frame to world frame
    Mat4 aircraftTransform = m_flightDynamics.getTransformMatrix();
    
    // Extract rotation from aircraft transform (ignoring translation)
    // Simplified: just use the aircraft's yaw for now
    float yaw = state.yaw;
    float pitch = state.pitch;
    
    // Rotate offset vector by aircraft orientation
    // Apply yaw rotation
    float cosY = std::cos(yaw);
    float sinY = std::sin(yaw);
    Vec3 offsetYaw = {
        offsetBody.x * cosY - offsetBody.z * sinY,
        offsetBody.y,
        offsetBody.x * sinY + offsetBody.z * cosY
    };
    
    // Apply pitch rotation (reduced effect for camera stability)
    float pitchDamped = pitch * 0.3f;  // Reduce pitch effect on camera
    float cosP = std::cos(pitchDamped);
    float sinP = std::sin(pitchDamped);
    Vec3 offsetWorld = {
        offsetYaw.x,
        offsetYaw.y * cosP - offsetYaw.z * sinP,
        offsetYaw.y * sinP + offsetYaw.z * cosP
    };
    
    // Calculate ideal camera position
    Vec3 idealCameraPos = {
        state.position.x + offsetWorld.x,
        state.position.y + offsetWorld.y,
        state.position.z + offsetWorld.z
    };
    
    // Smooth camera movement (exponential smoothing)
    // On first frame or large jumps, snap to position
    float distance = std::sqrt(
        (idealCameraPos.x - m_chaseCameraPos.x) * (idealCameraPos.x - m_chaseCameraPos.x) +
        (idealCameraPos.y - m_chaseCameraPos.y) * (idealCameraPos.y - m_chaseCameraPos.y) +
        (idealCameraPos.z - m_chaseCameraPos.z) * (idealCameraPos.z - m_chaseCameraPos.z)
    );
    
    if (distance > 100.0f || deltaTime > 0.5f) {
        // Snap to position (first frame or teleport)
        m_chaseCameraPos = idealCameraPos;
    } else {
        // Smooth interpolation
        float smoothFactor = 1.0f - std::pow(m_chaseSmoothness, deltaTime * 60.0f);
        m_chaseCameraPos.x += (idealCameraPos.x - m_chaseCameraPos.x) * smoothFactor;
        m_chaseCameraPos.y += (idealCameraPos.y - m_chaseCameraPos.y) * smoothFactor;
        m_chaseCameraPos.z += (idealCameraPos.z - m_chaseCameraPos.z) * smoothFactor;
    }
    
    // Camera looks at a point slightly ahead of aircraft
    Vec3 lookAheadBody = {0, 0, -10.0f};  // 10 meters forward
    Vec3 lookAheadWorld = {
        lookAheadBody.x * cosY - lookAheadBody.z * sinY,
        lookAheadBody.y,
        lookAheadBody.x * sinY + lookAheadBody.z * cosY
    };
    
    m_chaseCameraTarget = {
        state.position.x + lookAheadWorld.x,
        state.position.y + lookAheadWorld.y,
        state.position.z + lookAheadWorld.z
    };
    
    // Keep camera above ground
    if (m_chaseCameraPos.y < 2.0f) {
        m_chaseCameraPos.y = 2.0f;
    }
    
    LOG_DEBUG("Chase camera: pos(%.1f, %.1f, %.1f) target(%.1f, %.1f, %.1f)",
             m_chaseCameraPos.x, m_chaseCameraPos.y, m_chaseCameraPos.z,
             m_chaseCameraTarget.x, m_chaseCameraTarget.y, m_chaseCameraTarget.z);
}

// ==================== Scene Loading ====================
bool CubeApp::loadScene(const SceneFile& scene) {
    LOG_INFO("Applying scene: %s", scene.name.c_str());
    
    // Store the scene file for toggling with 'T' key
    m_sceneFile = scene;
    m_hasSceneFile = true;
    
    // Set camera type
    m_cameraType = (scene.camera.type == SceneFileCamera::FPS) ? CAMERA_FPS : CAMERA_ORBIT;
    LOG_DEBUG("Camera type: %s", (m_cameraType == CAMERA_FPS) ? "FPS" : "ORBIT");
    
    if (m_cameraType == CAMERA_FPS) {
        // FPS camera setup
        m_cameraPos.x = scene.camera.position[0];
        m_cameraPos.y = scene.camera.position[1];
        m_cameraPos.z = scene.camera.position[2];
        
        // Calculate orientation from position and target
        Vec3 target = {scene.camera.target[0], scene.camera.target[1], scene.camera.target[2]};
        Vec3 direction = {
            target.x - m_cameraPos.x,
            target.y - m_cameraPos.y,
            target.z - m_cameraPos.z
        };
        
        // Calculate yaw and pitch (in RADIANS)
        float length = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
        if (length > 0.001f) {
            direction.x /= length;
            direction.y /= length;
            direction.z /= length;
            
            m_cameraYaw = std::atan2(direction.x, -direction.z);
            m_cameraPitch = -std::asin(direction.y);
            
            // Update camera vectors
            updateFPSCamera(0.0f);
        }
        
        LOG_DEBUG("  FPS: pos(%.1f, %.1f, %.1f) yaw=%.2f pitch=%.2f",
                 m_cameraPos.x, m_cameraPos.y, m_cameraPos.z, m_cameraYaw, m_cameraPitch);
    } else {
        // Orbit camera setup
        m_distance = scene.camera.distance;
        m_yaw = scene.camera.yaw;
        m_pitch = scene.camera.pitch;
        m_autoRotate = scene.camera.autoRotate;
        
        LOG_DEBUG("  ORBIT: distance=%.1f yaw=%.2f pitch=%.2f autoRotate=%s",
                 m_distance, m_yaw, m_pitch, m_autoRotate ? "yes" : "no");
    }
    
    // Apply first light if available
    if (!scene.lights.empty()) {
        const auto& light = scene.lights[0];
        Vec3 lightDir = {light.direction[0], light.direction[1], light.direction[2]};
        m_renderer->setUniformVec3(m_shader, "uLightDir", lightDir);
        LOG_DEBUG("Light: dir(%.2f, %.2f, %.2f)", lightDir.x, lightDir.y, lightDir.z);
    }
    
    // Apply ground settings - recreate ground with scene configuration
    m_showGround = scene.ground.enabled;
    if (m_showGround) {
        // Recreate ground mesh with scene file settings
        if (m_groundMesh) {
            m_renderer->destroyMesh(m_groundMesh);
        }
        createGroundPlane(scene.ground);
    }
    LOG_DEBUG("Ground: %s%s", 
             m_showGround ? "enabled" : "disabled",
             scene.ground.hasRunway ? " with runway" : "");
    
    // Apply background settings
    m_useLightBackground = scene.background.enabled;
    LOG_DEBUG("Background: %s", m_useLightBackground ? "light" : "dark");
    
    // Load objects
    if (!scene.objects.empty()) {
        // Determine if we need scene system (multiple objects)
        bool isMultiObject = scene.objects.size() > 1;
        
        if (isMultiObject) {
            LOG_INFO("Multi-object scene: %zu objects", scene.objects.size());
            
            // Load the model from first object
            if (!scene.objects[0].modelPath.empty()) {
                LOG_INFO("Loading model: %s", scene.objects[0].modelPath.c_str());
                if (!loadModel(scene.objects[0].modelPath.c_str())) {
                    LOG_ERROR("Failed to load model: %s", scene.objects[0].modelPath.c_str());
                    return false;
                }
            }
            
            // Now populate Scene system with all objects
            m_scene.clear();
            
            // Store model mesh/texture handles for scene rendering
            m_modelMeshHandles[&m_model].clear();
            m_modelTextureHandles[&m_model].clear();
            
            for (const auto& mesh : m_meshes) {
                m_modelMeshHandles[&m_model].push_back(mesh.meshHandle);
                m_modelTextureHandles[&m_model].push_back(mesh.textureHandle);
            }
            
            // Add each scene file object to the Scene
            for (size_t i = 0; i < scene.objects.size(); i++) {
                const auto& sceneObj = scene.objects[i];
                if (!sceneObj.visible) continue;
                
                SceneObject obj;
                obj.model = &m_model;
                
                // Create transform matrix from position/rotation/scale
                float rotY = sceneObj.rotation[1] * 3.14159f / 180.0f; // Yaw in radians
                obj.transform = createTransformMatrix(
                    sceneObj.position[0], 
                    sceneObj.position[1], 
                    sceneObj.position[2], 
                    rotY, 
                    sceneObj.scale[0]  // Uniform scale
                );
                
                obj.colorTint = {1, 1, 1, 1}; // Default white tint
                obj.visible = true;
                
                m_scene.addObject(obj);
            }
            
            // Enable scene mode for multi-object rendering
            m_useSceneMode = true;
            LOG_INFO("Scene system populated: %zu objects", m_scene.getObjectCount());
            
        } else {
            // Single object - just load to m_meshes
            const auto& obj = scene.objects[0];
            if (obj.visible && !obj.modelPath.empty()) {
                LOG_INFO("Loading single model: %s", obj.modelPath.c_str());
                if (loadModel(obj.modelPath.c_str())) {
                    LOG_INFO("Model loaded successfully: %zu meshes", m_meshes.size());
                    
                    // Store object transform from scene file
                    m_objectPosition = {obj.position[0], obj.position[1], obj.position[2]};
                    m_objectRotation = {obj.rotation[0], obj.rotation[1], obj.rotation[2]};
                    m_objectScale = {obj.scale[0], obj.scale[1], obj.scale[2]};
                    
                    LOG_DEBUG("Object transform: pos(%.1f, %.1f, %.1f) rot(%.1f, %.1f, %.1f)",
                             m_objectPosition.x, m_objectPosition.y, m_objectPosition.z,
                             m_objectRotation.x, m_objectRotation.y, m_objectRotation.z);
                } else {
                    LOG_ERROR("Failed to load model: %s", obj.modelPath.c_str());
                    return false;
                }
            }
            
            // Single object - scene mode OFF
            m_useSceneMode = false;
        }
    }
    
    LOG_INFO("Scene applied successfully (camera=%s, objects=%zu, scene_mode=%s)",
             m_cameraType == CAMERA_FPS ? "FPS" : "ORBIT",
             scene.objects.size(),
             m_useSceneMode ? "yes" : "no");
    return true;
}

