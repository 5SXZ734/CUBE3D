cmake_minimum_required(VERSION 3.25)
project(CubeViewer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

# -------------------- Sources --------------------
set(SOURCES
    app_v3.cpp
    entity.cpp
    flight_dynamics.cpp
    main_v3.cpp
    model_assimp.cpp
    renderer_d3d11.cpp
    renderer_d3d12.cpp
    renderer_opengl.cpp
    stb_image_impl.cpp
    text_renderer_gl.cpp
    aircraft_input_controller.h
    app_v3.h
    behavior.h
    camera_behaviors.h
    camera_entity.h
    chase_camera_behavior.h
    dds_loader.h
    debug.h
    entity.h
    entity_registry.h
    flight_dynamics.h
    flight_dynamics_behavior.h
    input_controller.h
    math_utils.h
    model.h
    model_registry.h
    normal_map_gen.h
    orbit_camera_behavior.h
    osd.h
    renderer.h
    scene.h
    scene_loader_v2.h
    scene_manager.h
    text_renderer.h
    text_renderer_gl.h
    texture_cache.h
)

if(WIN32)
  list(APPEND SOURCES renderer_d3d11.cpp renderer_d3d12.cpp)
endif()

add_executable(cube_viewer ${SOURCES})

target_include_directories(cube_viewer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  #../../external/glad/include
)

# -------------------- vcpkg dependencies --------------------
find_package(nlohmann_json CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)

target_link_libraries(cube_viewer PRIVATE
  glad::glad
  glfw
  assimp::assimp
  nlohmann_json::nlohmann_json
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

# -------------------- ACDyn (installed static lib) --------------------
set(ACDYN_PREFIX "D:/WORK/aero/_install/acdyn" CACHE PATH "ACDyn install prefix")

add_library(acdyn::acdyn STATIC IMPORTED GLOBAL)

set_target_properties(acdyn::acdyn PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${ACDYN_PREFIX}/include"
  IMPORTED_LOCATION_RELEASE     "${ACDYN_PREFIX}/lib/acdyn.lib"
  IMPORTED_LOCATION_DEBUG       "${ACDYN_PREFIX}/lib/debug/acdyn.lib"
)

target_link_libraries(cube_viewer PRIVATE acdyn::acdyn)

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

