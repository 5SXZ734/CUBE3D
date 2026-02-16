// main.cpp - Entry point
#include "app.h"
#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    // Parse command line for renderer selection
    RendererAPI api = RendererAPI::OpenGL;
    
    if (argc > 1) {
        if (strcmp(argv[1], "opengl") == 0 || strcmp(argv[1], "gl") == 0) {
            api = RendererAPI::OpenGL;
        } else if (strcmp(argv[1], "d3d11") == 0 || strcmp(argv[1], "dx11") == 0) {
            api = RendererAPI::Direct3D11;
        } else if (strcmp(argv[1], "d3d12") == 0 || strcmp(argv[1], "dx12") == 0) {
            api = RendererAPI::Direct3D12;
        } else {
            std::fprintf(stderr, "Usage: %s [opengl|d3d11|d3d12]\n", argv[0]);
            return 1;
        }
    }

    // Create and run application
    CubeApp app;
    
    if (!app.initialize(api)) {
        std::fprintf(stderr, "Failed to initialize application\n");
        return 1;
    }

    app.run();
    app.shutdown();

    return 0;
}
