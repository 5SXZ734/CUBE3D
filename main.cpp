// main.cpp - Entry point
#include "app.h"
#include "debug.h"
#include <cstdio>
#include <cstring>

void printUsage(const char* programName) {
    printf("Usage: %s [options] [model.x]\n", programName);
    printf("\nRenderer Selection:\n");
    printf("  opengl, gl     Use OpenGL 3.3\n");
    printf("  d3d11, dx11    Use Direct3D 11\n");
    printf("  d3d12, dx12    Use Direct3D 12\n");
    printf("\nDebug Options:\n");
    printf("  --debug        Enable debug output\n");
    printf("  --verbose      Enable verbose logging (implies --debug)\n");
    printf("  --trace        Enable trace logging (very verbose)\n");
    printf("  --stats        Show performance statistics on exit\n");
    printf("  --validate     Enable strict validation checks\n");
    printf("\nExamples:\n");
    printf("  %s opengl airplane.x\n", programName);
    printf("  %s --debug d3d12 model.x\n", programName);
    printf("  %s --verbose --stats opengl\n", programName);
    printf("\n");
}

int main(int argc, char** argv) {
    // Parse command line arguments
    RendererAPI api = RendererAPI::OpenGL;
    const char* modelPath = nullptr;
    bool debugMode = false;
    bool verboseMode = false;
    bool traceMode = false;
    bool showStats = false;
    bool strictValidation = false;
    bool showHelp = false;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            showHelp = true;
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
    
    // Create and run application
    CubeApp app;
    app.setDebugMode(debugMode);
    app.setStrictValidation(strictValidation);
    app.setShowStats(showStats);
    
    LOG_DEBUG("Initializing application...");
    if (!app.initialize(api, modelPath)) {
        LOG_ERROR("Failed to initialize application");
        DebugManager::shutdown();
        return 1;
    }
    
    LOG_INFO("Application initialized successfully");
    LOG_INFO("Press ESC to exit, S to toggle scene mode (100 airplanes), B to toggle background, G to toggle ground");
    LOG_INFO("Mouse drag to rotate, scroll to zoom");
    
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
