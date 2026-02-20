// main.cpp - Entry point
#include "app.h"
#include "debug.h"
#include "scene_loader.h"
#include <cstdio>
#include <cstring>

void printUsage(const char* programName) {
    printf("Usage: %s [options] [model.x]\n", programName);
    printf("\nRenderer Selection:\n");
    printf("  opengl, gl     Use OpenGL 3.3\n");
    printf("  d3d11, dx11    Use Direct3D 11\n");
    printf("  d3d12, dx12    Use Direct3D 12\n");
    printf("\nScene Options:\n");
    printf("  --scene FILE   Load scene from JSON file\n");
    printf("\nDebug Options:\n");
    printf("  --debug        Enable debug output\n");
    printf("  --verbose      Enable verbose logging (implies --debug)\n");
    printf("  --trace        Enable trace logging (very verbose)\n");
    printf("  --stats        Show performance statistics on exit\n");
    printf("  --validate     Enable strict validation checks\n");
    printf("\nExamples:\n");
    printf("  %s opengl airplane.x\n", programName);
    printf("  %s --scene example_scene.json d3d12\n", programName);
    printf("  %s --debug d3d12 model.x\n", programName);
    printf("  %s --verbose --stats opengl\n", programName);
    printf("\n");
}

int main(int argc, char** argv) {
    // Parse command line arguments
    RendererAPI api = RendererAPI::OpenGL;
    const char* modelPath = nullptr;
    const char* sceneFile = nullptr;
    bool debugMode = false;
    bool verboseMode = false;
    bool traceMode = false;
    bool showStats = false;
    bool strictValidation = false;
    bool showHelp = false;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            showHelp = true;
        } else if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            sceneFile = argv[++i];
        } else if (strcmp(argv[i], "--debug") == 0) {
            debugMode = true;
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            debugMode = true;
            verboseMode = true;
        } else if (strcmp(argv[i], "--trace") == 0) {
            debugMode = true;
            verboseMode = true;
            traceMode = true;
        } else if (strcmp(argv[i], "--stats") == 0) {
            showStats = true;
        } else if (strcmp(argv[i], "--validate") == 0) {
            strictValidation = true;
        } else if (strcmp(argv[i], "opengl") == 0 || strcmp(argv[i], "gl") == 0) {
            api = RendererAPI::OpenGL;
        } else if (strcmp(argv[i], "d3d11") == 0 || strcmp(argv[i], "dx11") == 0) {
            api = RendererAPI::Direct3D11;
        } else if (strcmp(argv[i], "d3d12") == 0 || strcmp(argv[i], "dx12") == 0) {
            api = RendererAPI::Direct3D12;
        } else if (argv[i][0] != '-') {
            // Assume it's a model file path
            modelPath = argv[i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            printUsage(argv[0]);
            return 1;
        }
    }
    
    if (showHelp) {
        printUsage(argv[0]);
        return 0;
    }
    
    // Initialize debug system
    LogLevel logLevel = LogLevel::INFO;
    if (traceMode) logLevel = LogLevel::TRACE;
    else if (verboseMode) logLevel = LogLevel::DEBUG;
    else if (debugMode) logLevel = LogLevel::DEBUG;
    
    DebugManager::initialize(debugMode, logLevel);
    DebugManager::enableTimestamps(verboseMode || traceMode);
    
    LOG_INFO("=== Model Viewer Starting ===");
    LOG_DEBUG("Debug mode enabled (level: %d)", (int)logLevel);
    
    // Print configuration
    const char* apiName = "Unknown";
    switch (api) {
        case RendererAPI::OpenGL: apiName = "OpenGL 3.3"; break;
        case RendererAPI::Direct3D11: apiName = "Direct3D 11"; break;
        case RendererAPI::Direct3D12: apiName = "Direct3D 12"; break;
    }
    
    LOG_INFO("Renderer: %s", apiName);
    
    if (modelPath) {
        LOG_INFO("Model: %s", modelPath);
        
        // Validate model file
        if (strictValidation || debugMode) {
            if (!FileValidator::validateModelPath(modelPath)) {
                LOG_ERROR("Model validation failed, aborting");
                DebugManager::shutdown();
                return 1;
            }
            size_t fileSize = FileValidator::getFileSize(modelPath);
            LOG_DEBUG("Model file size: %.2f KB", fileSize / 1024.0f);
        }
    } else {
        LOG_INFO("No model specified, using default cube");
    }
    
    if (strictValidation) {
        LOG_INFO("Strict validation enabled");
    }
    
    // Load scene file if specified
    SceneFile scene;
    bool hasScene = false;
    if (sceneFile) {
        LOG_INFO("Loading scene: %s", sceneFile);
        
        // Validate scene file exists
        if (strictValidation || debugMode) {
            if (!FileValidator::validateTexturePath(sceneFile)) {  // Reuse texture validator for JSON
                LOG_ERROR("Scene file validation failed, aborting");
                DebugManager::shutdown();
                return 1;
            }
        }
        
        if (SceneLoader::loadScene(sceneFile, scene)) {
            LOG_INFO("Scene loaded: %zu objects, %zu lights", scene.objects.size(), scene.lights.size());
            hasScene = true;
            
            // If scene specifies a model path for any object and no model was specified on command line,
            // use the first object's model
            if (!modelPath && !scene.objects.empty() && !scene.objects[0].modelPath.empty()) {
                modelPath = scene.objects[0].modelPath.c_str();
                LOG_INFO("Using model from scene: %s", modelPath);
            }
        } else {
            LOG_ERROR("Failed to load scene: %s", SceneLoader::getLastError());
            DebugManager::shutdown();
            return 1;
        }
    }
    
    // Create and run application
    CubeApp app;
    app.setDebugMode(debugMode);
    app.setStrictValidation(strictValidation);
    app.setShowStats(showStats);
    
    LOG_DEBUG("Initializing application...");
    // If we have a scene file, don't pass modelPath - scene will load its own models
    const char* initModelPath = hasScene ? nullptr : modelPath;
    if (!app.initialize(api, initModelPath)) {
        LOG_ERROR("Failed to initialize application");
        DebugManager::shutdown();
        return 1;
    }
    
    // Apply scene settings if a scene was loaded
    if (hasScene) {
        LOG_DEBUG("Applying scene settings...");
        if (!app.loadScene(scene)) {
            LOG_WARNING("Failed to apply scene settings");
        }
    }
    
    // If still no model loaded, create default cube
    if (!app.hasModel()) {
        LOG_INFO("No model loaded - creating default rotating cube");
        if (!app.createDefaultCube()) {
            LOG_ERROR("Failed to create default cube");
            DebugManager::shutdown();
            return 1;
        }
    }
    
    LOG_INFO("Application initialized successfully");
    LOG_INFO("Controls: B=background, G=ground, N=normal mapping, ESC=exit");
    if (scene.camera.type == SceneFileCamera::FPS) {
        LOG_INFO("FPS Camera: WASD to move, Left-click+drag to look, Space/Shift for up/down");
    }
    
    app.run();
    
    LOG_DEBUG("Shutting down application...");
    app.shutdown();
    
    if (showStats) {
        app.printStats();
    }
    
    LOG_INFO("=== Model Viewer Exiting ===");
    DebugManager::shutdown();

    return 0;
}
