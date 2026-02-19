# Debug System Usage Examples

## Quick Start

### Basic Usage (No Debug Output)
```bash
./cube_viewer opengl airplane.x
```

### Enable Debug Mode
```bash
./cube_viewer --debug opengl airplane.x
```

Output:
```
[INFO ] === Model Viewer Starting ===
[DEBUG] Debug mode enabled (level: 3)
[INFO ] Renderer: OpenGL 3.3
[INFO ] Model: airplane.x
[DEBUG] Model file size: 45.23 KB
[DEBUG] Initializing application...
[INFO ] OpenGL Renderer initialized
[INFO ] OpenGL Version: 4.6.0
[DEBUG] Model loaded successfully:
[DEBUG]   Meshes: 8
[DEBUG]   Mesh 0: 236 vertices, 504 indices
[DEBUG]     Loading texture: bihull.bmp
[DEBUG]   Mesh 1: 192 vertices, 288 indices
...
[INFO ] Application initialized successfully
[INFO ] Press ESC to exit, mouse to rotate, scroll to zoom
```

### Verbose Mode (with Timestamps)
```bash
./cube_viewer --verbose opengl airplane.x
```

Output:
```
[     0 ms] [INFO ] === Model Viewer Starting ===
[     1 ms] [DEBUG] Debug mode enabled (level: 3)
[     1 ms] [INFO ] Renderer: OpenGL 3.3
[     1 ms] [INFO ] Model: airplane.x
[     2 ms] [DEBUG] Model file size: 45.23 KB
[    15 ms] [DEBUG] Texture loaded: bihull.bmp 256x256 (3 channels, 192.00 KB)
[    23 ms] [DEBUG] Texture loaded: wings.bmp 128x128 (3 channels, 48.00 KB)
...
```

### Trace Mode (Maximum Detail)
```bash
./cube_viewer --trace d3d12 model.x
```

Shows every operation including:
- Frame timing
- Draw call details
- Resource binding
- State changes

### Performance Statistics
```bash
./cube_viewer --stats opengl airplane.x
```

On exit, prints:
```
=== Performance Stats ===
FPS:           59.8 (16.73 ms/frame)
Frame Time:    Avg: 16.71 ms, Min: 15.12 ms, Max: 18.45 ms
Draw Calls:    8
Triangles:     3.5K
Meshes Drawn:  8
Textures:      3 (0.5 MB)
Mesh Memory:   1.2 MB
Total Frames:  3580
========================
```

### Strict Validation Mode
```bash
./cube_viewer --validate opengl model.x
```

Enables:
- File existence checks before loading
- Texture format validation
- Mesh vertex/index validation
- Out-of-bounds index detection
- Degenerate geometry warnings

Example output:
```
[INFO ] Strict validation enabled
[DEBUG] Validating model file: model.x
[DEBUG] Model file exists, size: 45.23 KB
[DEBUG] Mesh validated: 236 vertices, 504 indices (168 triangles)
[WARNING] Texture validation failed: texture_missing.bmp (file not found)
[ERROR] Index 502 references vertex 240 (out of bounds, max 235)
[ERROR] Mesh validation failed, aborting
```

### Combining Options
```bash
./cube_viewer --debug --stats --validate d3d11 airplane.x
```

Gets you:
- Debug output during runtime
- Validation of all assets
- Performance statistics on exit

### Help
```bash
./cube_viewer --help
```

```
Usage: cube_viewer [options] [model.x]

Renderer Selection:
  opengl, gl     Use OpenGL 3.3
  d3d11, dx11    Use Direct3D 11
  d3d12, dx12    Use Direct3D 12

Debug Options:
  --debug        Enable debug output
  --verbose      Enable verbose logging (implies --debug)
  --trace        Enable trace logging (very verbose)
  --stats        Show performance statistics on exit
  --validate     Enable strict validation checks

Examples:
  cube_viewer opengl airplane.x
  cube_viewer --debug d3d12 model.x
  cube_viewer --verbose --stats opengl
```

## Log File

When debug mode is enabled, a `debug_log.txt` file is created with all output:

```
=== Debug Log Started ===
[INFO ] === Model Viewer Starting ===
[DEBUG] Debug mode enabled (level: 3)
[INFO ] Renderer: OpenGL 3.3
...
[INFO ] === Model Viewer Exiting ===
=== Debug Log Ended ===
```

This file can be attached to bug reports or reviewed later.

## In-Code Usage

### Adding Debug Output to Renderers

```cpp
// In renderer code:
uint32_t createTexture(const char* filepath) override {
    LOG_DEBUG("Loading texture: %s", filepath);
    
    int w, h, ch;
    unsigned char* data = stbi_load(filepath, &w, &h, &ch, 0);
    
    if (!data) {
        LOG_ERROR("Texture load failed: %s (reason: %s)", 
                 filepath, stbi_failure_reason());
        return 0;
    }
    
    LOG_DEBUG("Texture loaded: %dx%d, %d channels (%.2f KB)", 
             w, h, ch, (w * h * ch) / 1024.0f);
    
    // ... rest of implementation
}
```

### Validation Example

```cpp
// Validate before use:
if (DebugManager::isEnabled()) {
    if (!FileValidator::exists(texturePath)) {
        LOG_ERROR("Texture file not found: %s", texturePath);
        return 0;
    }
    
    if (!MeshValidator::validate(vertices, vCount, indices, iCount)) {
        LOG_ERROR("Mesh validation failed");
        return 0;
    }
}
```

### Performance Tracking

```cpp
// In app.cpp render loop:
void CubeApp::render() {
    m_stats.reset();  // Reset per-frame counters
    
    for (const auto& mesh : m_meshes) {
        m_renderer->drawMesh(mesh.meshHandle);
        m_stats.drawCalls++;
        m_stats.triangles += mesh.triangleCount;
    }
    
    double frameTime = glfwGetTime() - frameStart;
    m_stats.updateFrameTime(frameTime);
}
```

## Common Debug Scenarios

### "Texture Not Loading"
```bash
./cube_viewer --debug --validate opengl model.x
```
Look for:
- `[ERROR] Texture file not found: ...`
- `[WARNING] Unknown texture extension: ...`
- `[ERROR] Texture load failed: ... (reason: ...)`

### "Slow Performance"
```bash
./cube_viewer --stats --verbose opengl model.x
```
Check:
- FPS (should be ~60)
- Draw calls (should be minimized)
- Frame time spikes (max frame time)

### "Crash or Corruption"
```bash
./cube_viewer --validate --trace d3d12 model.x
```
Enables all validation and maximum logging to catch:
- Out-of-bounds indices
- Invalid vertex data
- Resource binding errors

### "D3D12 Debug Layer"
The code automatically enables D3D12 debug layer in debug builds.
Look for validation errors in the output or debug_log.txt.

## Tips

1. **Always use `--debug` when filing bug reports**
   - Provides essential context
   - Shows exactly what failed

2. **Use `--validate` when developing**
   - Catches errors early
   - Prevents crashes

3. **Use `--stats` to track progress**
   - Measure optimization impact
   - Identify bottlenecks

4. **Check `debug_log.txt` for past sessions**
   - Review what happened
   - Compare before/after changes
