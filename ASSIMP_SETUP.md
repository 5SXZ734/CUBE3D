# Adding Assimp for .X File Loading

## What is Assimp?

**Assimp (Open Asset Import Library)** is a cross-platform library that loads 50+ 3D model formats including:
- DirectX .X files ✅
- Wavefront OBJ
- FBX, GLTF, Collada
- 3DS, Blender, and many more

**Website**: https://github.com/assimp/assimp
**License**: BSD (very permissive, free for commercial use)

## Setup

### 1. Get Assimp

```bash
cd ..  # Go to parent directory
cd external
git clone https://github.com/assimp/assimp.git
```

Or download from: https://github.com/assimp/assimp/releases

### 2. Directory Structure

```
parent_folder/
├── external/
│   ├── glfw/
│   ├── glad/
│   └── assimp/        # ← New!
└── cube_viewer/
    ├── model.h
    ├── model_assimp.cpp
    └── ...
```

### 3. Build

CMake will automatically build Assimp as part of your project:

```bash
cd cube_viewer/build
cmake ..
cmake --build .
```

## Usage in Your Code

### Loading a .X File

```cpp
#include "model.h"

Model model;
if (ModelLoader::LoadXFile("models/aircraft.x", model)) {
    // Model loaded successfully
    std::printf("Loaded %zu meshes\n", model.meshes.size());
    
    // Upload to renderer
    for (auto& mesh : model.meshes) {
        // Convert ModelVertex to renderer Vertex format
        std::vector<Vertex> vertices;
        for (auto& v : mesh.vertices) {
            vertices.push_back({
                v.px, v.py, v.pz,  // position
                v.nx, v.ny, v.nz,  // normal
                1.0f, 1.0f, 1.0f, 1.0f  // color (white for textured models)
            });
        }
        
        // Create mesh in renderer
        mesh.rendererMeshHandle = renderer->createMesh(
            vertices.data(), vertices.size(),
            (uint16_t*)mesh.indices.data(), mesh.indices.size()
        );
        
        // Load texture if present
        if (!mesh.texturePath.empty()) {
            mesh.rendererTextureHandle = renderer->createTexture(
                mesh.texturePath.c_str()
            );
        }
    }
}
```

### Supported .X File Features

Assimp can load from your .X files:
- ✅ Vertices (positions)
- ✅ Normals
- ✅ Texture coordinates (UVs)
- ✅ Materials
- ✅ Textures (BMP, TGA, DDS, etc.)
- ✅ Multiple meshes
- ✅ Animations (with additional code)
- ✅ Hierarchies/bones (with additional code)

## What Assimp Does

1. **Parses .X files** - Understands both text and binary formats
2. **Extracts geometry** - Vertices, normals, UVs, indices
3. **Loads materials** - Colors, textures, properties
4. **Post-processes** - Triangulation, normal generation, optimization
5. **Cross-platform** - Works on Windows, Linux, macOS

## Comparison with DirectX D3DRM

| Feature | D3DRM (Legacy) | Assimp (Modern) |
|---------|---------------|-----------------|
| Platform | Windows only | Cross-platform |
| File formats | .X only | 50+ formats |
| Maintained | Deprecated | Active development |
| License | Microsoft | BSD (open source) |
| Size | Part of DirectX | ~50MB compiled |
| Integration | COM interfaces | Simple C++ API |

## Alternative: Manual .X File Parsing

If you want to avoid the Assimp dependency, you can write a simple .X file parser:
- .X files in TEXT format are easy to parse
- You only need to extract vertices, normals, and texture names
- See: https://en.wikipedia.org/wiki/X_file_format

But Assimp is **highly recommended** because:
- Handles both text and binary .X formats
- Robust error handling
- Optimizes mesh data
- Widely used and tested

## Testing

Test with a simple .X file:

1. Create `test.x` or use one from your aircraft models
2. Place in `models/` folder
3. Run: `./cube_viewer models/test.x`

## Troubleshooting

**Build error: "Cannot find assimp"**
- Make sure you cloned Assimp into `../external/assimp`
- Check CMakeLists.txt paths

**Runtime error: "Failed to load model"**
- Check file path is correct
- Verify .X file is valid (open in Blender or similar)
- Check console for Assimp error messages

**Missing textures**
- Textures must be in same folder as .X file or use absolute paths
- Check texture paths in .X file are correct
- Supported formats: BMP, TGA, PNG, JPG

## Next Steps

After getting basic .X loading working, you can extend to:
1. Add texture loading to all three renderers
2. Support animations from .X files
3. Load hierarchical models (parent/child frames)
4. Add model caching to avoid reloading

The architecture is now ready for full 3D model support!
