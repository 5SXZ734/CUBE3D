cmake_minimum_required(VERSION 3.25)
project(CubeViewer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------- GLFW (prebuilt/installed in ../../external/glfw/_install) ----------
# vcpkg-like layout supported:
#   <prefix>/lib + <prefix>/include
#   <prefix>/debug/lib + <prefix>/debug/include (optional)
#
# Prefer package config if present: <prefix>/lib/cmake/glfw3 or <prefix>/share/glfw3
set(GLFW_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../../external/glfw/_install")

# Make both prefixes visible. CMake will choose debug/release libs via imported targets.
list(PREPEND CMAKE_PREFIX_PATH
  "${GLFW_PREFIX}"
  "${GLFW_PREFIX}/debug"
)

# glfw3 provides a config package named "glfw3" and imported target "glfw"
# (some installs expose "glfw3::glfw3" or "glfw")
find_package(glfw3 CONFIG REQUIRED)

# ---------- Cube Viewer Application ----------
set(SOURCES
  main.cpp
  app.cpp
  renderer_opengl.cpp
  ../../external/glad/src/glad.c
)

if(WIN32)
  list(APPEND SOURCES renderer_d3d11.cpp renderer_d3d12.cpp)
endif()

add_executable(cube_viewer ${SOURCES})

target_include_directories(cube_viewer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ../../external/glad/include
)

# Link GLFW imported target
target_link_libraries(cube_viewer PRIVATE glfw)

# Platform-specific settings
if(WIN32)
  target_link_libraries(cube_viewer PRIVATE
    opengl32
    d3d11
    d3d12
    dxgi
    d3dcompiler
  )
  target_compile_definitions(cube_viewer PRIVATE GLFW_EXPOSE_NATIVE_WIN32)
elseif(APPLE)
  target_link_libraries(cube_viewer PRIVATE
    "-framework OpenGL"
    "-framework Cocoa"
    "-framework IOKit"
  )
else()
  target_link_libraries(cube_viewer PRIVATE
    GL
    dl
    pthread
  )
endif()

if(MSVC)
  target_compile_options(cube_viewer PRIVATE /W4 /permissive-)
else()
  target_compile_options(cube_viewer PRIVATE -Wall -Wextra)
endif()
