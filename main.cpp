// main.cpp - Entry point
#include "app.h"
#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    // Parse command line for renderer selection and optional model path
    RendererAPI api = RendererAPI::OpenGL;
    const char* modelPath = nullptr;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "opengl") == 0 || strcmp(argv[i], "gl") == 0) {
            api = RendererAPI::OpenGL;
        } else if (strcmp(argv[i], "d3d11") == 0 || strcmp(argv[i], "dx11") == 0) {
            api = RendererAPI::Direct3D11;
        } else if (strcmp(argv[i], "d3d12") == 0 || strcmp(argv[i], "dx12") == 0) {
            api = RendererAPI::Direct3D12;
        } else {
            // Assume it's a model file path
            modelPath = argv[i];
        }
    }

    // Print usage
    if (modelPath) {
        std::fprintf(stdout, "Loading model: %s\n", modelPath);
    } else {
        std::fprintf(stdout, "Usage: %s [opengl|d3d11|d3d12] [model.x]\n", argv[0]);
        std::fprintf(stdout, "No model specified, using default cube\n");
    }

    // Create and run application
    CubeApp app;
    
    if (!app.initialize(api, modelPath)) {
        std::fprintf(stderr, "Failed to initialize application\n");
        return 1;
    }

    app.run();
    app.shutdown();

    return 0;
}
