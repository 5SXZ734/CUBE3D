# Multi-API 3D Renderer

A cross-platform 3D cube renderer demonstrating clean separation between application logic and graphics API implementation. Supports **OpenGL**, **Direct3D 11**, and **Direct3D 12** using the same application code.

![Architecture](https://img.shields.io/badge/Architecture-Multi--API-blue)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)

## Features

- ✅ **OpenGL 3.3** - Cross-platform support (Windows, Linux, macOS)
- ✅ **Direct3D 11** - Windows native API
- ✅ **Direct3D 12** - Modern Windows API with command lists
- ✅ **Single codebase** - Same app logic for all APIs
- ✅ **Clean architecture** - Easy to add new rendering backends

## Project Structure

```
parent_directory/
├── external/          # Shared dependencies (one level up)
│   ├── glfw/         # Window/input library
│   └── glad/         # OpenGL loader
│       ├── include/
│       └── src/
│
└── cube_viewer/       # This project
    ├── CMakeLists.txt
    ├── main.cpp       # Entry point
    ├── app.h/cpp      # Application logic (API-independent)
    ├── renderer.h     # Abstract renderer interface
    ├── renderer_opengl.cpp    # OpenGL implementation
    ├── renderer_d3d11.cpp     # Direct3D 11 implementation
    └── renderer_d3d12.cpp     # Direct3D 12 implementation
```

## Prerequisites

### All Platforms
- CMake 3.25+
- C++17 compiler

### Windows
- Visual Studio 2019+ or MinGW-w64
- Windows 10 SDK (for DirectX)

### Linux
- GCC 7+ or Clang 5+
- OpenGL development libraries: `sudo apt install libgl1-mesa-dev libx11-dev`

### macOS
- Xcode Command Line Tools
- OpenGL framework (included with macOS)

## Setup

### 1. Clone the repository

```bash
git clone https://github.com/yourusername/cube_viewer.git
cd cube_viewer
```

### 2. Setup external dependencies

**Option A: Use the parent directory structure (recommended)**

Place GLFW and GLAD in `../external/`:

```bash
cd ..
mkdir external
cd external

# Get GLFW
git clone https://github.com/glfw/glfw.git
# or download from: https://www.glfw.org/download.html

# Get GLAD
# Visit: https://glad.dav1d.de/
# - Language: C/C++
# - Specification: OpenGL
# - gl: Version 3.3+
# - Profile: Core
# Download and extract to external/glad/
```

**Option B: Use local external/ directory**

If you prefer dependencies inside the project:
1. Change `../external/` to `external/` in CMakeLists.txt
2. Place GLFW and GLAD in `cube_viewer/external/`

### 3. Build

```bash
cd cube_viewer
mkdir build
cd build
cmake ..
cmake --build .
```

**Windows with Visual Studio:**
```bash
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

## Usage

### Run with different renderers

**OpenGL (default):**
```bash
./cube_viewer
# or explicitly:
./cube_viewer opengl
./cube_viewer gl
```

**Direct3D 11 (Windows only):**
```bash
./cube_viewer d3d11
./cube_viewer dx11
```

**Direct3D 12 (Windows only):**
```bash
./cube_viewer d3d12
./cube_viewer dx12
```

### Controls

- **Left Mouse Drag** - Rotate the cube
- **Mouse Wheel** - Zoom in/out
- **ESC** - Exit

## Architecture

### Renderer Interface (`renderer.h`)

Defines a graphics API-agnostic interface:

```cpp
class IRenderer {
    virtual bool initialize(GLFWwindow* window) = 0;
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual uint32_t createMesh(...) = 0;
    virtual void drawMesh(uint32_t handle) = 0;
    // ...
};
```

### Application Layer (`app.cpp`)

Contains all application logic - camera, input, scene management. **Zero graphics API knowledge.**

### Renderer Implementations

- `renderer_opengl.cpp` - OpenGL 3.3 Core
- `renderer_d3d11.cpp` - Direct3D 11
- `renderer_d3d12.cpp` - Direct3D 12 with command lists

## Adding a New Renderer

1. Create `renderer_newapi.cpp`
2. Implement `IRenderer` interface
3. Add to `RendererAPI` enum in `renderer.h`
4. Update factory function
5. Update CMakeLists.txt

**No changes needed to `app.cpp` or `main.cpp`!**

## Technical Details

### OpenGL Renderer
- Uses GLAD for function loading
- Core profile (no deprecated features)
- Vertex Array Objects (VAO)

### Direct3D 11 Renderer
- Immediate context rendering
- Constant buffer for uniforms
- Simplified GLSL→HLSL conversion

### Direct3D 12 Renderer
- Command list recording
- Double buffering (2 frames)
- Fence-based synchronization
- No external helper libraries (no d3dx12.h)

## Known Issues

- D3D12 depth testing may need tuning on some hardware
- Window resize on D3D12 may cause brief flicker

## Contributing

Contributions welcome! Possible additions:
- Vulkan renderer
- Metal renderer (macOS)
- Texture support
- More complex geometry
- Shadow mapping

## License

MIT License - See LICENSE file

## Credits

- GLFW - https://www.glfw.org/
- GLAD - https://glad.dav1d.de/
- DirectXMath - Microsoft

---

**Note:** This is a learning/demonstration project showing clean API abstraction. For production use, consider established engines like Godot, Unity, or Unreal Engine.
