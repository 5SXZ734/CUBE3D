// main_v3.cpp - Entry point with ECS system
#include "app_v3.h"
#include "debug.h"
#include <cstdio>

int main(int argc, char** argv) {
    // Parse command line arguments
    RendererAPI api = RendererAPI::OpenGL;  // Default
    const char* sceneFile = "scene_flight_v2.json";  // Default scene
    std::string sceneFilePath;  // For constructing path with .json
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--d3d11") == 0) {
            api = RendererAPI::Direct3D11;
        } else if (strcmp(argv[i], "--d3d12") == 0) {
            api = RendererAPI::Direct3D12;
        } else if (strcmp(argv[i], "--opengl") == 0 || strcmp(argv[i], "--gl") == 0) {
            api = RendererAPI::OpenGL;
        } else if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            // Check if the scene name already has .json extension
            const char* sceneName = argv[++i];
            std::string sceneStr(sceneName);
            
            if (sceneStr.size() >= 5 && sceneStr.substr(sceneStr.size() - 5) == ".json") {
                // Already has .json extension
                sceneFilePath = sceneStr;
            } else {
                // Add .json extension
                sceneFilePath = sceneStr + ".json";
            }
            sceneFile = sceneFilePath.c_str();
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --opengl, --gl     Use OpenGL renderer (default)\n");
            printf("  --d3d11            Use Direct3D 11 renderer\n");
            printf("  --d3d12            Use Direct3D 12 renderer\n");
            printf("  --scene <name>     Load scene file (.json extension optional)\n");
            printf("                     Examples: --scene scene_flight_v2\n");
            printf("                               --scene scene_orbit_v2.json\n");
            printf("  --help             Show this help\n");
            printf("\n");
            printf("Controls:\n");
            printf("  Arrow Keys         Pitch and roll\n");
            printf("  Delete/PageDown    Rudder\n");
            printf("  +/-                Throttle\n");
            printf("  O                  Toggle OSD\n");
            printf("  I                  Toggle OSD detail mode\n");
            printf("  G                  Toggle ground\n");
            printf("  N                  Toggle normal mapping\n");
            printf("  ESC                Exit\n");
            return 0;
        }
    }
    
    printf("===========================================\n");
    printf("  Flight Simulator - Entity System Demo\n");
    printf("===========================================\n");
    printf("Renderer: %s\n", 
           api == RendererAPI::OpenGL ? "OpenGL" :
           api == RendererAPI::Direct3D11 ? "Direct3D 11" :
           "Direct3D 12");
    printf("Scene: %s\n", sceneFile);
    printf("===========================================\n\n");
    
    // Create and initialize application
    CubeApp app;
    
    if (!app.initialize(api, sceneFile)) {
        LOG_ERROR("Failed to initialize application");
        return 1;
    }
    
    // Run main loop
    app.run();
    
    // Cleanup
    app.shutdown();
    
    printf("\nGoodbye!\n");
    return 0;
}
