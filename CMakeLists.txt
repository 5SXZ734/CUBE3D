cmake_minimum_required(VERSION 3.25)
project(CubeViewer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -------------------- Sources --------------------
set(SOURCES
  main.cpp
  app.cpp
  app.h
  renderer_opengl.cpp
  renderer.h
  model_assimp.cpp
  model.h
  ../../external/glad/src/glad.c
  stb_image_impl.cpp
)

if(WIN32)
  list(APPEND SOURCES renderer_d3d11.cpp renderer_d3d12.cpp)
endif()

add_executable(cube_viewer ${SOURCES})

target_include_directories(cube_viewer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ../../external/glad/include
)

# -------------------- vcpkg dependencies --------------------
find_package(glfw3 CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)

target_link_libraries(cube_viewer PRIVATE
  glfw
  assimp::assimp
)

# stb is header-only; vcpkg may not provide a CMake package config for it.
# Just add vcpkg include dir so <stb_image.h> resolves.
if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
  target_include_directories(cube_viewer PRIVATE
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include"
  )
else()
  set(VCPKG_ROOT "D:/WORK/external/vcpkg" CACHE PATH "Path to vcpkg root")
  set(VCPKG_TRIPLET "x64-windows" CACHE STRING "vcpkg triplet")
  target_include_directories(cube_viewer PRIVATE
    "${VCPKG_ROOT}/installed/${VCPKG_TRIPLET}/include"
  )
endif()

# -------------------- Platform specifics --------------------
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

# -------------------- Warnings --------------------
if(MSVC)
  target_compile_options(cube_viewer PRIVATE /W4 /permissive-)
else()
  target_compile_options(cube_viewer PRIVATE -Wall -Wextra)
endif()

if(0 AND WIN32)
  set(COPY_RUNTIME_DEPS_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/cmake/copy_runtime_deps.cmake")

  # Provide vcpkg bin dirs explicitly (handles the case where the script can't see toolchain vars)
  set(_VCPKG_BIN "D:/WORK/external/vcpkg/installed/x64-windows/bin")
  set(_VCPKG_DBG_BIN "D:/WORK/external/vcpkg/installed/x64-windows/debug/bin")

  add_custom_command(TARGET cube_viewer POST_BUILD
    COMMAND ${CMAKE_COMMAND}
      -Dexe=$<TARGET_FILE:cube_viewer>
      -Dout_dir=$<TARGET_FILE_DIR:cube_viewer>
      -Dvcpkg_bin=${_VCPKG_BIN}
      -Dvcpkg_debug_bin=${_VCPKG_DBG_BIN}
      -P "${COPY_RUNTIME_DEPS_SCRIPT}"
    VERBATIM
  )
endif()

