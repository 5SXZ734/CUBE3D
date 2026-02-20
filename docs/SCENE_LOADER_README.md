# JSON Scene Loader - Integration Guide

## Step 1: Download nlohmann/json Library

### Option A: Direct Download (Recommended)
1. Go to: https://github.com/nlohmann/json/releases/latest
2. Download the file named `json.hpp`
3. Place it in: `D:\WORK\aero\CUBE3D\json.hpp`

### Option B: Using wget/curl
```bash
cd D:\WORK\aero\CUBE3D
curl -O https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp
```

## Step 2: Add Files to Your Project

The following files have been created in `/mnt/user-data/outputs/`:
- `scene_loader.h` - Header file
- `scene_loader.cpp` - Implementation
- `example_scene.json` - Example scene file

Copy them to your project:
```
D:\WORK\aero\CUBE3D\scene_loader.h
D:\WORK\aero\CUBE3D\scene_loader.cpp
D:\WORK\aero\CUBE3D\example_scene.json
```

## Step 3: Add to CMakeLists.txt

Add `scene_loader.cpp` to your source files:

```cmake
set(SOURCES
    main.cpp
    app.cpp
    model_assimp.cpp
    scene_loader.cpp        # <-- ADD THIS LINE
    renderer_opengl.cpp
    renderer_d3d11.cpp
    renderer_d3d12.cpp
)
```

## Step 4: Update main.cpp to Accept Scene Files

Add command line argument for scene files:

```cpp
#include "scene_loader.h"

int main(int argc, char** argv) {
    // ... existing arg parsing ...
    
    const char* sceneFile = nullptr;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--scene") == 0 && i + 1 < argc) {
            sceneFile = argv[++i];
        }
        // ... rest of arg parsing ...
    }
    
    // Load scene if specified
    Scene scene;
    if (sceneFile) {
        if (!SceneLoader::loadScene(sceneFile, scene)) {
            fprintf(stderr, "Failed to load scene: %s\n", SceneLoader::getLastError());
            return 1;
        }
    }
    
    // Pass scene to app initialization
    // (You'll need to update CubeApp to accept Scene)
}
```

## Step 5: Update CubeApp to Use Scene Data

In `app.h`, add:
```cpp
#include "scene_loader.h"

class CubeApp {
    // ...
    bool loadScene(const Scene& scene);
};
```

In `app.cpp`, implement:
```cpp
bool CubeApp::loadScene(const Scene& scene) {
    // Clear existing objects
    m_meshes.clear();
    
    // Set camera
    m_camera.position.x = scene.camera.position[0];
    m_camera.position.y = scene.camera.position[1];
    m_camera.position.z = scene.camera.position[2];
    m_camera.target.x = scene.camera.target[0];
    m_camera.target.y = scene.camera.target[1];
    m_camera.target.z = scene.camera.target[2];
    m_camera.fov = scene.camera.fov;
    
    // Set light
    if (!scene.lights.empty()) {
        const auto& light = scene.lights[0];
        Vec3 lightDir = {light.direction[0], light.direction[1], light.direction[2]};
        m_renderer->setUniformVec3(m_shader, "uLightDir", lightDir);
    }
    
    // Load objects
    for (const auto& sceneObj : scene.objects) {
        if (!sceneObj.visible) continue;
        
        // Load model
        if (!loadModel(sceneObj.modelPath.c_str())) {
            fprintf(stderr, "Failed to load model: %s\n", sceneObj.modelPath.c_str());
            continue;
        }
        
        // Store transform (you'll need to apply these in rendering)
        // Position: sceneObj.position[0,1,2]
        // Rotation: sceneObj.rotation[0,1,2] (degrees)
        // Scale: sceneObj.scale[0,1,2]
    }
    
    // Set ground
    m_showGround = scene.ground.enabled;
    
    // Set background
    m_showBackground = scene.background.enabled;
    
    return true;
}
```

## Usage Examples

### Load a scene file:
```bash
cube_viewer.exe --scene example_scene.json --api opengl
```

### Scene file format:
See `example_scene.json` for a complete example with:
- Camera position and orientation
- Directional lights
- Multiple objects with transforms
- Ground plane
- Background gradient

## Scene File Format Reference

```json
{
  "name": "Scene Name",
  "camera": {
    "position": [x, y, z],
    "target": [x, y, z],
    "fov": 45,
    "nearPlane": 0.1,
    "farPlane": 1000
  },
  "lights": [
    {
      "type": "directional|point|spot",
      "direction": [x, y, z],     // for directional
      "position": [x, y, z],      // for point/spot
      "color": [r, g, b],
      "intensity": 1.0,
      "range": 100                // for point/spot
    }
  ],
  "objects": [
    {
      "name": "ObjectName",
      "model": "path/to/model.3ds",
      "position": [x, y, z],
      "rotation": [yaw, pitch, roll],  // degrees
      "scale": [x, y, z],
      "visible": true
    }
  ],
  "ground": {
    "enabled": true,
    "size": 100,
    "color": [r, g, b, a]
  },
  "background": {
    "enabled": true,
    "colorTop": [r, g, b],
    "colorBottom": [r, g, b]
  }
}
```

## Notes

- All file paths in scene are relative to the scene file location
- Rotation angles are in degrees
- Colors are normalized 0.0-1.0
- The library is header-only, no linking required
- JSON parsing errors are reported via `SceneLoader::getLastError()`
