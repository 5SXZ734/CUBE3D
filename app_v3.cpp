// app_v3.cpp - Complete application implementation with ECS
#include "app_v3.h"
#include "scene_loader_v2.h"
#include "flight_dynamics_behavior.h"
#include "chase_camera_behavior.h"
#include "orbit_camera_behavior.h"
#include "text_renderer_gl.h"
#include "math_utils.h"
#include "debug.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cmath>
#include <algorithm>

// GLFW callbacks
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

static void glfw_scroll_callback(GLFWwindow* win, double xoffset, double yoffset) {
    CubeApp* app = (CubeApp*)glfwGetWindowUserPointer(win);
    if (app) app->onScroll(xoffset, yoffset);
}

static void glfw_key_callback(GLFWwindow* win, int key, int scancode, int action, int mods) {
    CubeApp* app = (CubeApp*)glfwGetWindowUserPointer(win);
    if (app) app->onKey(key, scancode, action, mods);
}

// ==================== CONSTRUCTOR ====================
CubeApp::CubeApp()
    : m_window(nullptr)
    , m_width(1280)
    , m_height(720)
    , m_renderer(nullptr)
    , m_shader(0)
    , m_proceduralNormalMap(0)
    , m_useNormalMapping(false)
    , m_arrowUpPressed(false)
    , m_arrowDownPressed(false)
    , m_arrowLeftPressed(false)
    , m_arrowRightPressed(false)
    , m_deletePressed(false)
    , m_pageDownPressed(false)
    , m_lastFrameTime(0.0)
    , m_startTime(0.0)
    , m_deltaTime(0.0f)
    , m_debugMode(false)
    , m_strictValidation(false)
    , m_showStats(false)
    , m_textRenderer(nullptr)
{
    // Initialize environment
    m_environment.groundMesh = 0;
    m_environment.runwayMesh = 0;
    m_environment.groundTexture = 0;
    m_environment.runwayTexture = 0;
    m_environment.showGround = true;
    m_environment.useLightBackground = false;
    m_environment.lightDirection = {-0.3f, -1.0f, -0.2f};
    m_environment.lightColor = {1.0f, 1.0f, 0.95f};
    
    m_cameraPos = {0, 20, 80};
    m_cameraTarget = {0, 0, 0};
    
    // Create scene manager
    m_sceneManager = new SceneManager(m_entityRegistry, m_modelRegistry);
    
    m_orbitDistance = 10.0f;
    m_orbitYaw = 0.6f;
    m_orbitPitch = -0.4f;
    m_orbitMode = false;
    m_dragging = false;
    m_lastX = 0.0;
    m_lastY = 0.0;
}

CubeApp::~CubeApp() {
    shutdown();
}

void CubeApp::printStats() const {
    m_stats.print();
    m_textureCache.printStats();
}

// ==================== INITIALIZATION ====================
bool CubeApp::initialize(RendererAPI api, const char* sceneFile) {
    LOG_DEBUG("Initializing GLFW...");
    
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
    m_window = glfwCreateWindow(m_width, m_height, "Flight Simulator", nullptr, nullptr);
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
    
    // Initialize texture cache
    m_textureCache.setRenderer(m_renderer);
    LOG_DEBUG("Texture cache initialized");
    
    // Initialize text renderer
    printf("=== TEXT RENDERER INITIALIZATION ===\n");
    m_textRenderer = new GLTextRenderer();
    if (m_textRenderer && m_textRenderer->initialize()) {
        LOG_INFO("Text renderer initialized successfully");
    } else {
        LOG_WARNING("Text renderer failed to initialize");
        delete m_textRenderer;
        m_textRenderer = nullptr;
    }
    
    // Set initial viewport
    int fbW, fbH;
    glfwGetFramebufferSize(m_window, &fbW, &fbH);
    onFramebufferSize(fbW, fbH);
    
    // Create shader
    LOG_DEBUG("Creating shader...");
    m_shader = m_renderer->createShader(OPENGL_VERTEX_SHADER, OPENGL_FRAGMENT_SHADER);
    if (!m_shader) {
        LOG_ERROR("Failed to create shader");
        return false;
    }
    
    m_renderer->useShader(m_shader);
    m_renderer->setUniformInt(m_shader, "uTexture", 0);
    m_renderer->setUniformInt(m_shader, "uNormalMap", 1);
    m_renderer->setDepthTest(true);
    m_renderer->setCulling(false);
    
    // Create simple procedural normal map (flat blue = no bumps)
    std::vector<unsigned char> normalMapData(256 * 256 * 4);
    for (int i = 0; i < 256 * 256; i++) {
        normalMapData[i * 4 + 0] = 128;  // R = 0.5 (x)
        normalMapData[i * 4 + 1] = 128;  // G = 0.5 (y)
        normalMapData[i * 4 + 2] = 255;  // B = 1.0 (z pointing up)
        normalMapData[i * 4 + 3] = 255;  // A = 1.0
    }
    
    m_proceduralNormalMap = m_renderer->createTextureFromData(
        normalMapData.data(), 256, 256, 4);
    
    if (m_proceduralNormalMap) {
        LOG_INFO("Procedural normal map created");
    }

    m_startTime = glfwGetTime();
    m_lastFrameTime = m_startTime;
    
    // Load scene from JSON
    if (sceneFile) {
        SceneConfigV2 scene;
        printf("DEBUG: Attempting to load scene file: %s\n", sceneFile);
        
        if (SceneLoaderV2::loadScene(sceneFile, scene)) {
            LOG_INFO("Loading scene: %s", scene.name.c_str());
            
            printf("DEBUG: Scene loaded, models count: %zu\n", scene.models.size());
            printf("DEBUG: Entities count: %zu\n", scene.entities.size());
            
            // Apply scene to registries
            if (SceneLoaderV2::applyScene(scene, m_modelRegistry, m_entityRegistry)) {
                LOG_INFO("Scene loaded successfully");
                LOG_INFO("  Entities: %zu", m_entityRegistry.getEntityCount());
                LOG_INFO("  Behaviors: %zu", m_entityRegistry.getBehaviorCount());
                LOG_INFO("  Models: %zu", m_modelRegistry.getModelCount());
                
                // Create mesh/texture handles for all models
                printf("DEBUG: Creating render data for models...\n");
                for (const auto& [key, filepath] : scene.models) {
                    printf("DEBUG: Loading model '%s' from '%s'\n", key.c_str(), filepath.c_str());
                    const Model* model = m_modelRegistry.getModel(key);
                    if (model) {
                        printf("DEBUG: Model has %zu meshes\n", model->meshes.size());
                        createModelRenderData(model);
                        printf("DEBUG: Render data created for '%s'\n", key.c_str());
                    } else {
                        printf("ERROR: Model '%s' not found in registry!\n", key.c_str());
                    }
                }
                
                // Store scene file for reloading
                m_sceneFilePath = sceneFile;
                m_sceneManager->setSceneFilePath(sceneFile);
                
                // Create cameras from scene
                printf("DEBUG: Creating cameras from scene...\n");
                for (const auto& camConfig : scene.cameras) {
                    CameraEntity* camera = m_entityRegistry.createCameraEntity(camConfig.name);
                    camera->setPosition(camConfig.position);
                    camera->setFOV(camConfig.fov);
                    
                    // Find target entity
                    EntityID targetID = 0;
                    if (!camConfig.targetEntity.empty()) {
                        Entity* target = m_entityRegistry.findEntityByName(camConfig.targetEntity);
                        if (target) {
                            targetID = target->getID();
                            printf("DEBUG: Camera '%s' targets entity '%s' (ID: %u)\n", camConfig.name.c_str(), camConfig.targetEntity.c_str(), targetID);
                        } else {
                            printf("WARNING: Target entity '%s' not found for camera '%s'\n", camConfig.targetEntity.c_str(), camConfig.name.c_str());
                        }
                    }
                    
                    // Attach camera behavior
                    if (camConfig.type == "chase" && targetID != 0) {
                        auto* behavior = new ChaseCameraTargetBehavior(&m_entityRegistry, targetID);
                        if (camConfig.behaviorParams.contains("distance"))
                            behavior->setDistance(camConfig.behaviorParams["distance"].get<float>());
                        if (camConfig.behaviorParams.contains("height"))
                            behavior->setHeight(camConfig.behaviorParams["height"].get<float>());
                        if (camConfig.behaviorParams.contains("smoothness"))
                            behavior->setSmoothness(camConfig.behaviorParams["smoothness"].get<float>());
                        
                        behavior->attach(camera);
                        behavior->initialize();
                        m_entityRegistry.addBehaviorManual(camera->getID(), behavior);
                        printf("DEBUG: Created chase camera: %s\n", camConfig.name.c_str());
                        
                    } else if (camConfig.type == "orbit" && targetID != 0) {
                        auto* behavior = new OrbitCameraTargetBehavior(&m_entityRegistry, targetID);
                        if (camConfig.behaviorParams.contains("distance"))
                            behavior->setDistance(camConfig.behaviorParams["distance"].get<float>());
                        if (camConfig.behaviorParams.contains("yaw"))
                            behavior->setYaw(camConfig.behaviorParams["yaw"].get<float>());
                        if (camConfig.behaviorParams.contains("pitch"))
                            behavior->setPitch(camConfig.behaviorParams["pitch"].get<float>());
                        if (camConfig.behaviorParams.contains("autoRotate"))
                            behavior->setAutoRotate(camConfig.behaviorParams["autoRotate"].get<bool>());
                        if (camConfig.behaviorParams.contains("rotationSpeed"))
                            behavior->setRotationSpeed(camConfig.behaviorParams["rotationSpeed"].get<float>());
                        
                        behavior->attach(camera);
                        behavior->initialize();
                        m_entityRegistry.addBehaviorManual(camera->getID(), behavior);
                        printf("DEBUG: Created orbit camera: %s\n", camConfig.name.c_str());
                        
                    } else if (camConfig.type == "stationary") {
                        camera->setTarget(camConfig.target);
                        printf("DEBUG: Created stationary camera: %s\n", camConfig.name.c_str());
                    }
                    
                    m_sceneManager->addCamera(camera->getID());
                }
                
                // Register controllable entities
                printf("DEBUG: Registering controllable entities...\n");
                for (const auto& entityConfig : scene.entities) {
                    if (entityConfig.controllable) {
                        Entity* entity = m_entityRegistry.findEntityByName(entityConfig.name);
                        if (entity) {
                            m_sceneManager->addControllable(entity->getID());
                            printf("DEBUG: Registered controllable: %s (type: %s)\n", entityConfig.name.c_str(), entityConfig.controllerType.c_str());
                        }
                    }
                }
                
                // Create input controller
                if (m_sceneManager->getCurrentControllable()) {
                    auto* controller = new AircraftInputController(&m_entityRegistry);
                    m_sceneManager->setInputController(controller);
                    printf("DEBUG: Created aircraft input controller\n");
                }
                
                // Initialize camera from scene manager
                CameraEntity* activeCamera = m_sceneManager->getActiveCamera();
                if (activeCamera) {
                    m_cameraPos = activeCamera->getPosition();
                    m_cameraTarget = activeCamera->getTarget();
                    printf("DEBUG: Active camera: %s\n", activeCamera->getName().c_str());
                }
                
                printf("DEBUG: Scene Manager: %zu cameras, %zu controllables\n", m_sceneManager->getCameraCount(), m_sceneManager->getControllableCount());
                
                // Create ground if specified
                if (scene.ground.enabled) {
                    printf("DEBUG: Creating ground plane...\n");
                    createGroundPlane(scene.ground);
                    printf("DEBUG: Ground mesh: %u, texture: %u\n", 
                           m_environment.groundMesh, m_environment.groundTexture);
                    printf("DEBUG: Runway mesh: %u, texture: %u\n",
                           m_environment.runwayMesh, m_environment.runwayTexture);
                }
                
                // Setup lighting
                if (!scene.lights.empty()) {
                    m_environment.lightDirection = scene.lights[0].direction;
                    m_environment.lightColor = scene.lights[0].color;
                    printf("DEBUG: Light direction: (%.2f, %.2f, %.2f)\n",
                           m_environment.lightDirection.x,
                           m_environment.lightDirection.y,
                           m_environment.lightDirection.z);
                }
                
                // Debug: List all entities
                printf("DEBUG: Entities in registry:\n");
                const auto& entities = m_entityRegistry.getAllEntities();
                for (const auto& [id, entity] : entities) {
                    printf("  - Entity ID %u: '%s' at (%.1f, %.1f, %.1f)\n",
                           id, entity->getName().c_str(),
                           entity->getPosition().x,
                           entity->getPosition().y,
                           entity->getPosition().z);
                    printf("    Model: %p, Visible: %d\n", 
                           (void*)entity->getModel(), entity->isVisible());
                }
                
            } else {
                LOG_ERROR("Failed to apply scene");
                printf("ERROR: Scene application failed!\n");
                return false;
            }
        } else {
            LOG_ERROR("Failed to load scene file: %s", sceneFile);
            printf("ERROR: Could not load scene file '%s'\n", sceneFile);
            printf("Make sure the file exists in the working directory!\n");
            return false;
        }
    } else {
        printf("WARNING: No scene file specified!\n");
    }
    
    LOG_INFO("===========================================");
    LOG_INFO("Flight Simulator Ready!");
    LOG_INFO("Controls: Arrows=pitch/roll, Del/PgDn=rudder, +/-=throttle");
    LOG_INFO("          O=OSD, I=detail, G=ground, N=normals, ESC=quit");
    LOG_INFO("===========================================");
    
    return true;
}

// ==================== SHUTDOWN ====================
void CubeApp::shutdown() {
    // Cleanup scene manager
    if (m_sceneManager) {
        delete m_sceneManager;
        m_sceneManager = nullptr;
    }
    
    // Cleanup text renderer
    if (m_textRenderer) {
        m_textRenderer->shutdown();
        delete m_textRenderer;
        m_textRenderer = nullptr;
    }
    
    // Cleanup entities (this also cleans up behaviors)
    m_entityRegistry.clear();
    
    // Cleanup models
    m_modelRegistry.clear();
    
    if (m_renderer) {
        // Destroy model meshes/textures
        for (auto& pair : m_modelMeshHandles) {
            for (uint32_t handle : pair.second) {
                if (handle) m_renderer->destroyMesh(handle);
            }
        }
        for (auto& pair : m_modelTextureHandles) {
            for (uint32_t handle : pair.second) {
                if (handle) m_renderer->destroyTexture(handle);
            }
        }
        m_modelMeshHandles.clear();
        m_modelTextureHandles.clear();
        
        // Destroy environment resources
        if (m_environment.groundMesh) m_renderer->destroyMesh(m_environment.groundMesh);
        if (m_environment.runwayMesh) m_renderer->destroyMesh(m_environment.runwayMesh);
        if (m_environment.groundTexture) m_renderer->destroyTexture(m_environment.groundTexture);
        if (m_environment.runwayTexture) m_renderer->destroyTexture(m_environment.runwayTexture);
        if (m_proceduralNormalMap) m_renderer->destroyTexture(m_proceduralNormalMap);
        
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

// ==================== MAIN LOOP ====================
void CubeApp::run() {
    while (!glfwWindowShouldClose(m_window)) {
        double frameStart = glfwGetTime();
        
        glfwPollEvents();

        double currentTime = glfwGetTime();
        float deltaTime = (float)(currentTime - m_lastFrameTime);
        m_lastFrameTime = currentTime;
        m_deltaTime = deltaTime;

        update(deltaTime);
        render();

        if (m_showStats) {
            m_stats.frameTime = (float)(glfwGetTime() - frameStart) * 1000.0f;
            m_stats.fps = 1.0f / deltaTime;
        }
    }
}

// ==================== UPDATE ====================
void CubeApp::update(float deltaTime) {
    // Update all entities and their behaviors
    m_entityRegistry.update(deltaTime);
    
    // Update input controller
    if (m_sceneManager && m_sceneManager->getInputController()) {
        m_sceneManager->getInputController()->update(deltaTime);
    }
    
    // Update camera from scene manager
    CameraEntity* activeCamera = m_sceneManager ? m_sceneManager->getActiveCamera() : nullptr;
    if (activeCamera) {
        m_cameraPos = activeCamera->getPosition();
        m_cameraTarget = activeCamera->getTarget();
    }
}

// ==================== RENDER ====================
void CubeApp::render() {
    static int frameCount = 0;
    static bool debugOnce = true;
    
    if (debugOnce) {
        printf("\n=== FIRST RENDER DEBUG ===\n");
        printf("Camera pos: (%.1f, %.1f, %.1f)\n", m_cameraPos.x, m_cameraPos.y, m_cameraPos.z);
        printf("Camera target: (%.1f, %.1f, %.1f)\n", m_cameraTarget.x, m_cameraTarget.y, m_cameraTarget.z);
        printf("Ground mesh: %u, visible: %d\n", m_environment.groundMesh, m_environment.showGround);
        printf("Entities count: %zu\n", m_entityRegistry.getEntityCount());
        debugOnce = false;
    }
    
    m_renderer->beginFrame();
    
    // Setup view/projection matrices using working mat4 functions
    float aspect = (float)m_width / (float)m_height;
    Vec3 worldUp = {0, 1, 0};
    
    Mat4 view = mat4_lookAtRH(m_cameraPos, m_cameraTarget, worldUp);
    Mat4 proj = mat4_perspectiveRH_NO(75.0f * 3.14159265359f / 180.0f, aspect, 0.1f, 10000.0f);
    Mat4 viewProj = mat4_mul(proj, view);
    
    if (frameCount == 0) {
        printf("Using mat4 functions - Camera working correctly\n");
        frameCount++;
    }
    
    // Set lighting uniforms
    m_renderer->useShader(m_shader);
    m_renderer->setUniformVec3(m_shader, "uLightDir", m_environment.lightDirection);
    m_renderer->setUniformVec3(m_shader, "uLightColor", m_environment.lightColor);
    m_renderer->setUniformInt(m_shader, "uUseNormalMapping", m_useNormalMapping ? 1 : 0);
    
    // Draw ground
    if (m_environment.showGround && m_environment.groundMesh) {
        Mat4 groundWorld = mat4_identity();
        
        m_renderer->setUniformMat4(m_shader, "uMVP", viewProj);
        m_renderer->setUniformMat4(m_shader, "uWorld", groundWorld);
        m_renderer->setUniformInt(m_shader, "uUseTexture", m_environment.groundTexture ? 1 : 0);
        m_renderer->drawMesh(m_environment.groundMesh, m_environment.groundTexture);
        
        // Draw runway
        if (m_environment.runwayMesh) {
            m_renderer->setUniformInt(m_shader, "uUseTexture", m_environment.runwayTexture ? 1 : 0);
            m_renderer->drawMesh(m_environment.runwayMesh, m_environment.runwayTexture);
        }
    }
    
    // Draw all entities
    const auto& entities = m_entityRegistry.getAllEntities();
    for (const auto& [id, entity] : entities) {
        if (entity->isVisible() && entity->getModel()) {
            renderEntity(entity, viewProj);
        }
    }
    
    // Render OSD
    if (m_osd.isEnabled() && m_textRenderer) {
        Entity* player = getPlayerEntity();
        if (player) {
            auto* flight = m_entityRegistry.getBehavior<FlightDynamicsBehavior>(player->getID());
            if (flight) {
                const AircraftState& state = flight->getState();
                const ControlInputs& controls = flight->getControlInputs();
                
                auto osdLines = m_osd.generateOSDLines(state, controls);
                
                m_textRenderer->beginText(m_width, m_height);
                
                float y = 0.02f;
                float lineHeight = 0.04f;
                
                for (const auto& line : osdLines) {
                    TextColor color = TextColor::Green();
                    
                    if (line.find("===") != std::string::npos || 
                        line.find("---") != std::string::npos) {
                        color = TextColor::Cyan();
                    } else if (line.find("CLIMBING") != std::string::npos) {
                        color = TextColor::Green();
                    } else if (line.find("DESCENDING") != std::string::npos) {
                        color = TextColor::Yellow();
                    }
                    
                    m_textRenderer->renderText(line.c_str(), {0.02f, y}, color, 2.5f);
                    y += lineHeight;
                }
                
                m_textRenderer->endText();
            }
        }
    }
    
    m_renderer->endFrame();
}

// ==================== RENDER ENTITY ====================
void CubeApp::renderEntity(const Entity* entity, const Mat4& viewProj) {
    const Model* model = entity->getModel();
    if (!model) return;
    
    // Get mesh and texture handles
    auto meshIt = m_modelMeshHandles.find(model);
    auto texIt = m_modelTextureHandles.find(model);
    if (meshIt == m_modelMeshHandles.end()) return;
    
    // Get entity transform
    Mat4 world = entity->getTransformMatrix();
    Mat4 mvp = mat4_mul(viewProj, world);
    
    // Set uniforms
    m_renderer->setUniformMat4(m_shader, "uMVP", mvp);
    m_renderer->setUniformMat4(m_shader, "uWorld", world);
    
    // Draw all meshes
    const auto& meshHandles = meshIt->second;
    const auto& texHandles = (texIt != m_modelTextureHandles.end()) 
                             ? texIt->second 
                             : std::vector<uint32_t>();
    
    for (size_t i = 0; i < meshHandles.size(); i++) {
        uint32_t texHandle = (i < texHandles.size()) ? texHandles[i] : 0;
        m_renderer->setUniformInt(m_shader, "uUseTexture", texHandle ? 1 : 0);
        m_renderer->drawMesh(meshHandles[i], texHandle);
    }
}

// ==================== GET PLAYER ENTITY ====================
Entity* CubeApp::getPlayerEntity() {
    // Find first user-controlled entity
    const auto& entities = m_entityRegistry.getAllEntities();
    for (const auto& [id, entity] : entities) {
        auto* flight = m_entityRegistry.getBehavior<FlightDynamicsBehavior>(id);
        if (flight && flight->isUserControlled()) {
            return entity;
        }
    }
    return nullptr;
}

// ==================== CREATE MODEL RENDER DATA ====================
void CubeApp::createModelRenderData(const Model* model) {
    if (!model) return;
    
    std::vector<uint32_t> meshHandles;
    std::vector<uint32_t> texHandles;
    
    for (const auto& mesh : model->meshes) {
        // Convert ModelVertex to Vertex
        std::vector<Vertex> vertices;
        vertices.reserve(mesh.vertices.size());
        
        for (const auto& mv : mesh.vertices) {
            Vertex v;
            v.px = mv.px;
            v.py = mv.py;
            v.pz = mv.pz;
            v.nx = mv.nx;
            v.ny = mv.ny;
            v.nz = mv.nz;
            v.r = 1.0f;  // Default white
            v.g = 1.0f;
            v.b = 1.0f;
            v.a = 1.0f;
            v.u = mv.u;
            v.v = mv.v;
            v.tx = mv.tx;
            v.ty = mv.ty;
            v.tz = mv.tz;
            v.bx = mv.bx;
            v.by = mv.by;
            v.bz = mv.bz;
            vertices.push_back(v);
        }
        
        // Convert uint32_t indices to uint16_t
        std::vector<uint16_t> indices16;
        indices16.reserve(mesh.indices.size());
        for (uint32_t idx : mesh.indices) {
            indices16.push_back((uint16_t)idx);
        }
        
        // Create mesh
        uint32_t meshHandle = m_renderer->createMesh(
            vertices.data(), 
            (uint32_t)vertices.size(),
            indices16.data(), 
            (uint32_t)indices16.size()
        );
        meshHandles.push_back(meshHandle);
        
        // Create texture if available
        uint32_t texHandle = 0;
        if (!mesh.texturePath.empty()) {
            texHandle = m_renderer->createTexture(mesh.texturePath.c_str());
        }
        texHandles.push_back(texHandle);
    }
    
    m_modelMeshHandles[model] = meshHandles;
    m_modelTextureHandles[model] = texHandles;
}

// ==================== CREATE GROUND PLANE ====================
void CubeApp::createGroundPlane(const SceneConfigV2::GroundConfig& groundConfig) {
    LOG_INFO("Creating ground plane%s...", groundConfig.hasRunway ? " with runway" : "");
    
    std::vector<Vertex> groundVertices;
    std::vector<uint16_t> groundIndices;
    
    const float groundY = 0.0f;
    float surfSize = groundConfig.size;
    
    // Ground surface vertices
    Vertex sv0, sv1, sv2, sv3;
    
    sv0.px = -surfSize; sv0.py = groundY; sv0.pz = -surfSize;
    sv1.px = -surfSize; sv1.py = groundY; sv1.pz =  surfSize;
    sv2.px =  surfSize; sv2.py = groundY; sv2.pz =  surfSize;
    sv3.px =  surfSize; sv3.py = groundY; sv3.pz = -surfSize;
    
    sv0.nx = sv1.nx = sv2.nx = sv3.nx = 0.0f;
    sv0.ny = sv1.ny = sv2.ny = sv3.ny = 1.0f;
    sv0.nz = sv1.nz = sv2.nz = sv3.nz = 0.0f;
    
    float gr = groundConfig.color[0];
    float gg = groundConfig.color[1];
    float gb = groundConfig.color[2];
    float ga = groundConfig.color[3];
    sv0.r = sv1.r = sv2.r = sv3.r = gr;
    sv0.g = sv1.g = sv2.g = sv3.g = gg;
    sv0.b = sv1.b = sv2.b = sv3.b = gb;
    sv0.a = sv1.a = sv2.a = sv3.a = ga;
    
    float texRepeat = surfSize / 500.0f;
    sv0.u = 0.0f;       sv0.v = 0.0f;
    sv1.u = 0.0f;       sv1.v = texRepeat;
    sv2.u = texRepeat;  sv2.v = texRepeat;
    sv3.u = texRepeat;  sv3.v = 0.0f;
    
    sv0.tx = sv1.tx = sv2.tx = sv3.tx = 1.0f;
    sv0.ty = sv1.ty = sv2.ty = sv3.ty = 0.0f;
    sv0.tz = sv1.tz = sv2.tz = sv3.tz = 0.0f;
    sv0.bx = sv1.bx = sv2.bx = sv3.bx = 0.0f;
    sv0.by = sv1.by = sv2.by = sv3.by = 0.0f;
    sv0.bz = sv1.bz = sv2.bz = sv3.bz = 1.0f;
    
    groundVertices.push_back(sv0);
    groundVertices.push_back(sv1);
    groundVertices.push_back(sv2);
    groundVertices.push_back(sv3);
    
    groundIndices.push_back(0);
    groundIndices.push_back(1);
    groundIndices.push_back(2);
    groundIndices.push_back(0);
    groundIndices.push_back(2);
    groundIndices.push_back(3);
    
    m_environment.groundMesh = m_renderer->createMesh(
        groundVertices.data(), (uint32_t)groundVertices.size(),
        groundIndices.data(), (uint32_t)groundIndices.size()
    );
    
    if (!groundConfig.texturePath.empty()) {
        printf("DEBUG: Loading ground texture: %s\n", groundConfig.texturePath.c_str());
        m_environment.groundTexture = m_renderer->createTexture(groundConfig.texturePath.c_str());
        printf("DEBUG: Ground texture handle: %u\n", m_environment.groundTexture);
    } else {
        m_environment.groundTexture = 0;
    }
    
    // Create runway if needed
    if (groundConfig.hasRunway) {
        std::vector<Vertex> runwayVertices;
        std::vector<uint16_t> runwayIndices;
        
        float stripWidth = groundConfig.runwayWidth / 2.0f;
        float stripLength = groundConfig.runwayLength / 2.0f;
        float stripY = 0.2f;
        
        Vertex rs0, rs1, rs2, rs3;
        
        rs0.px = -stripWidth; rs0.py = stripY; rs0.pz = -stripLength;
        rs1.px = -stripWidth; rs1.py = stripY; rs1.pz =  stripLength;
        rs2.px =  stripWidth; rs2.py = stripY; rs2.pz =  stripLength;
        rs3.px =  stripWidth; rs3.py = stripY; rs3.pz = -stripLength;
        
        rs0.nx = rs1.nx = rs2.nx = rs3.nx = 0.0f;
        rs0.ny = rs1.ny = rs2.ny = rs3.ny = 1.0f;
        rs0.nz = rs1.nz = rs2.nz = rs3.nz = 0.0f;
        
        float rr = groundConfig.runwayColor[0];
        float rg = groundConfig.runwayColor[1];
        float rb = groundConfig.runwayColor[2];
        float ra = groundConfig.runwayColor[3];
        rs0.r = rs1.r = rs2.r = rs3.r = rr;
        rs0.g = rs1.g = rs2.g = rs3.g = rg;
        rs0.b = rs1.b = rs2.b = rs3.b = rb;
        rs0.a = rs1.a = rs2.a = rs3.a = ra;
        
        float runwayTexU = 1.0f;
        float runwayTexV = groundConfig.runwayLength / 100.0f;
        rs0.u = 0.0f;        rs0.v = 0.0f;
        rs1.u = 0.0f;        rs1.v = runwayTexV;
        rs2.u = runwayTexU;  rs2.v = runwayTexV;
        rs3.u = runwayTexU;  rs3.v = 0.0f;
        
        rs0.tx = rs1.tx = rs2.tx = rs3.tx = 1.0f;
        rs0.ty = rs1.ty = rs2.ty = rs3.ty = 0.0f;
        rs0.tz = rs1.tz = rs2.tz = rs3.tz = 0.0f;
        rs0.bx = rs1.bx = rs2.bx = rs3.bx = 0.0f;
        rs0.by = rs1.by = rs2.by = rs3.by = 0.0f;
        rs0.bz = rs1.bz = rs2.bz = rs3.bz = 1.0f;
        
        runwayVertices.push_back(rs0);
        runwayVertices.push_back(rs1);
        runwayVertices.push_back(rs2);
        runwayVertices.push_back(rs3);
        
        runwayIndices.push_back(0);
        runwayIndices.push_back(1);
        runwayIndices.push_back(2);
        runwayIndices.push_back(0);
        runwayIndices.push_back(2);
        runwayIndices.push_back(3);
        
        m_environment.runwayMesh = m_renderer->createMesh(
            runwayVertices.data(), (uint32_t)runwayVertices.size(),
            runwayIndices.data(), (uint32_t)runwayIndices.size()
        );
        
        if (!groundConfig.runwayTexturePath.empty()) {
            printf("DEBUG: Loading runway texture: %s\n", groundConfig.runwayTexturePath.c_str());
            m_environment.runwayTexture = m_renderer->createTexture(groundConfig.runwayTexturePath.c_str());
            printf("DEBUG: Runway texture handle: %u\n", m_environment.runwayTexture);
        } else {
            m_environment.runwayTexture = 0;
        }
        
        LOG_INFO("  Runway: %.0fm × %.0fm", groundConfig.runwayWidth, groundConfig.runwayLength);
    }
    
    LOG_INFO("Ground created: %.0fm×%.0fm", surfSize * 2.0f, surfSize * 2.0f);
}

// ==================== INPUT HANDLING ====================
void CubeApp::onKey(int key, int scancode, int action, int mods) {
    (void)scancode;
    
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(m_window, 1);
    }
    
    // Camera switching (C key)
    if (key == GLFW_KEY_C && action == GLFW_PRESS) {
        if (m_sceneManager) {
            m_sceneManager->nextCamera();
        }
    }
    
    // Entity switching (Tab)
    if (key == GLFW_KEY_TAB && action == GLFW_PRESS) {
        if (m_sceneManager) {
            if (mods & GLFW_MOD_SHIFT) {
                m_sceneManager->previousControllable();
            } else {
                m_sceneManager->nextControllable();
            }
        }
    }
    
    // Scene reload (R)
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        reloadScene();
    }
    
    // Toggle ground
    if (key == GLFW_KEY_G && action == GLFW_PRESS) {
        m_environment.showGround = !m_environment.showGround;
        LOG_INFO("Ground: %s", m_environment.showGround ? "VISIBLE" : "HIDDEN");
    }
    
    // Toggle OSD
    if (key == GLFW_KEY_O && action == GLFW_PRESS) {
        m_osd.toggle();
        LOG_INFO("OSD: %s", m_osd.isEnabled() ? "ENABLED" : "DISABLED");
    }
    
    // Toggle OSD detail
    if (key == GLFW_KEY_I && action == GLFW_PRESS) {
        m_osd.toggleDetailedMode();
        LOG_INFO("OSD mode: %s", m_osd.isDetailedMode() ? "DETAILED" : "SIMPLE");
    }
    
    // Toggle normal mapping
    if (key == GLFW_KEY_N && action == GLFW_PRESS) {
        m_useNormalMapping = !m_useNormalMapping;
        LOG_INFO("Normal mapping: %s", m_useNormalMapping ? "ENABLED" : "DISABLED");
    }
    
    // Pass keys to input controller
    if (m_sceneManager && m_sceneManager->getInputController()) {
        if (action == GLFW_PRESS) {
            m_sceneManager->getInputController()->onKeyPress(key);
        } else if (action == GLFW_RELEASE) {
            m_sceneManager->getInputController()->onKeyRelease(key);
        }
    }
}

void CubeApp::onFramebufferSize(int width, int height) {
    m_width = width;
    m_height = height;
    m_renderer->setViewport(width, height);
}

void CubeApp::onMouseButton(int button, int action, int mods) {
    (void)mods;
    
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            glfwGetCursorPos(m_window, &m_lastX, &m_lastY);
            m_dragging = true;
        } else if (action == GLFW_RELEASE) {
            m_dragging = false;
        }
    }
}

void CubeApp::onCursorPos(double x, double y) {
    if (m_dragging) {
        float dx = (float)(x - m_lastX);
        float dy = (float)(y - m_lastY);
        
        // Update orbit camera via scene manager
        CameraEntity* activeCamera = m_sceneManager ? m_sceneManager->getActiveCamera() : nullptr;
        if (activeCamera) {
            auto* orbitBehavior = m_entityRegistry.getBehavior<OrbitCameraTargetBehavior>(activeCamera->getID());
            if (orbitBehavior) {
                orbitBehavior->rotate(dx * 0.01f, -dy * 0.01f);
            }
        }
        
        m_lastX = x;
        m_lastY = y;
    }
}

void CubeApp::onScroll(double xoffset, double yoffset) {
    (void)xoffset;
    
    // Update orbit camera via scene manager
    CameraEntity* activeCamera = m_sceneManager ? m_sceneManager->getActiveCamera() : nullptr;
    if (activeCamera) {
        auto* orbitBehavior = m_entityRegistry.getBehavior<OrbitCameraTargetBehavior>(activeCamera->getID());
        if (orbitBehavior) {
            orbitBehavior->zoom(-(float)yoffset * 1.0f);
        }
    }
}

// ==================== SCENE RELOADING ====================
bool CubeApp::reloadScene() {
    if (m_sceneFilePath.empty()) {
        LOG_WARNING("No scene file to reload");
        return false;
    }
    
    LOG_INFO("Reloading scene: %s", m_sceneFilePath.c_str());
    
    // Clear scene manager
    m_sceneManager->clear();
    
    // Destroy render data
    for (auto& pair : m_modelMeshHandles) {
        for (uint32_t handle : pair.second) {
            if (handle) m_renderer->destroyMesh(handle);
        }
    }
    for (auto& pair : m_modelTextureHandles) {
        for (uint32_t handle : pair.second) {
            if (handle) m_renderer->destroyTexture(handle);
        }
    }
    m_modelMeshHandles.clear();
    m_modelTextureHandles.clear();
    
    // Destroy environment
    if (m_environment.groundMesh) m_renderer->destroyMesh(m_environment.groundMesh);
    if (m_environment.runwayMesh) m_renderer->destroyMesh(m_environment.runwayMesh);
    if (m_environment.groundTexture) m_renderer->destroyTexture(m_environment.groundTexture);
    if (m_environment.runwayTexture) m_renderer->destroyTexture(m_environment.runwayTexture);
    m_environment.groundMesh = 0;
    m_environment.runwayMesh = 0;
    m_environment.groundTexture = 0;
    m_environment.runwayTexture = 0;
    
    // Clear registries
    m_entityRegistry.clear();
    m_modelRegistry.clear();
    
    // Reload scene file
    SceneConfigV2 scene;
    if (!SceneLoaderV2::loadScene(m_sceneFilePath.c_str(), scene)) {
        LOG_ERROR("Failed to reload scene file");
        return false;
    }
    
    // Apply scene to registries
    if (!SceneLoaderV2::applyScene(scene, m_modelRegistry, m_entityRegistry)) {
        LOG_ERROR("Failed to apply scene");
        return false;
    }
    
    // Create mesh/texture handles for all models
    for (const auto& [key, filepath] : scene.models) {
        const Model* model = m_modelRegistry.getModel(key);
        if (model) {
            createModelRenderData(model);
        }
    }
    
    // Create cameras from scene
    for (const auto& camConfig : scene.cameras) {
        CameraEntity* camera = m_entityRegistry.createCameraEntity(camConfig.name);
        camera->setPosition(camConfig.position);
        camera->setFOV(camConfig.fov);
        
        // Find target entity
        EntityID targetID = 0;
        if (!camConfig.targetEntity.empty()) {
            Entity* target = m_entityRegistry.findEntityByName(camConfig.targetEntity);
            if (target) {
                targetID = target->getID();
            }
        }
        
        // Attach camera behavior
        if (camConfig.type == "chase" && targetID != 0) {
            auto* behavior = new ChaseCameraTargetBehavior(&m_entityRegistry, targetID);
            if (camConfig.behaviorParams.contains("distance"))
                behavior->setDistance(camConfig.behaviorParams["distance"].get<float>());
            if (camConfig.behaviorParams.contains("height"))
                behavior->setHeight(camConfig.behaviorParams["height"].get<float>());
            if (camConfig.behaviorParams.contains("smoothness"))
                behavior->setSmoothness(camConfig.behaviorParams["smoothness"].get<float>());
            
            behavior->attach(camera);
            behavior->initialize();
            m_entityRegistry.addBehaviorManual(camera->getID(), behavior);
            
        } else if (camConfig.type == "orbit" && targetID != 0) {
            auto* behavior = new OrbitCameraTargetBehavior(&m_entityRegistry, targetID);
            if (camConfig.behaviorParams.contains("distance"))
                behavior->setDistance(camConfig.behaviorParams["distance"].get<float>());
            if (camConfig.behaviorParams.contains("yaw"))
                behavior->setYaw(camConfig.behaviorParams["yaw"].get<float>());
            if (camConfig.behaviorParams.contains("pitch"))
                behavior->setPitch(camConfig.behaviorParams["pitch"].get<float>());
            if (camConfig.behaviorParams.contains("autoRotate"))
                behavior->setAutoRotate(camConfig.behaviorParams["autoRotate"].get<bool>());
            if (camConfig.behaviorParams.contains("rotationSpeed"))
                behavior->setRotationSpeed(camConfig.behaviorParams["rotationSpeed"].get<float>());
            
            behavior->attach(camera);
            behavior->initialize();
            m_entityRegistry.addBehaviorManual(camera->getID(), behavior);
            
        } else if (camConfig.type == "stationary") {
            camera->setTarget(camConfig.target);
        }
        
        m_sceneManager->addCamera(camera->getID());
    }
    
    // Register controllable entities
    for (const auto& entityConfig : scene.entities) {
        if (entityConfig.controllable) {
            Entity* entity = m_entityRegistry.findEntityByName(entityConfig.name);
            if (entity) {
                m_sceneManager->addControllable(entity->getID());
            }
        }
    }
    
    // Create input controller
    if (m_sceneManager->getCurrentControllable()) {
        auto* controller = new AircraftInputController(&m_entityRegistry);
        m_sceneManager->setInputController(controller);
    }
    
    // Initialize camera
    CameraEntity* activeCamera = m_sceneManager->getActiveCamera();
    if (activeCamera) {
        m_cameraPos = activeCamera->getPosition();
        m_cameraTarget = activeCamera->getTarget();
    }
    
    // Create ground if specified
    if (scene.ground.enabled) {
        createGroundPlane(scene.ground);
    }
    
    // Setup lighting
    if (!scene.lights.empty()) {
        m_environment.lightDirection = scene.lights[0].direction;
        m_environment.lightColor = scene.lights[0].color;
    }
    
    LOG_INFO("Scene reloaded successfully");
    return true;
}
