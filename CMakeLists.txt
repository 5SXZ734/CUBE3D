cmake_minimum_required(VERSION 3.25)
project(CubeViewer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------- GLFW (from source in ../external/glfw) ----------
# Configure GLFW options to keep it minimal
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(../external/glfw ${CMAKE_BINARY_DIR}/glfw)

# ---------- Cube Viewer Application ----------
set(SOURCES
    main.cpp
    app.cpp
    renderer_opengl.cpp
)

# Add GLAD on all platforms
list(APPEND SOURCES ../external/glad/src/glad.c)

# Add D3D11 and D3D12 renderers on Windows only
if(WIN32)
    list(APPEND SOURCES renderer_d3d11.cpp renderer_d3d12.cpp)
endif()

add_executable(cube_viewer ${SOURCES})

# Include directories
target_include_directories(cube_viewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../external/glad/include
)

# Link libraries
target_link_libraries(cube_viewer PRIVATE
    glfw
)

# Platform-specific settings
if(WIN32)
    # Windows: Link OpenGL and D3D11/D3D12 libraries
    target_link_libraries(cube_viewer PRIVATE 
        opengl32
        d3d11
        d3d12
        dxgi
        d3dcompiler
    )
    
    # Expose native window handles for D3D11/D3D12
    target_compile_definitions(cube_viewer PRIVATE GLFW_EXPOSE_NATIVE_WIN32)
elseif(APPLE)
    # macOS: Link OpenGL framework
    target_link_libraries(cube_viewer PRIVATE
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
    )
else()
    # Linux: Link OpenGL and X11 libraries
    target_link_libraries(cube_viewer PRIVATE
        GL
        dl
        pthread
    )
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(cube_viewer PRIVATE /W4 /permissive-)
else()
    target_compile_options(cube_viewer PRIVATE -Wall -Wextra)
endif()
