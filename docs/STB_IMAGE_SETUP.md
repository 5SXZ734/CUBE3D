# STB Image Setup

## What is STB Image?

**stb_image.h** is a single-header image loading library that supports:
- JPEG, PNG, BMP, TGA, GIF, PSD, HDR, PIC
- No dependencies, just drop in and use
- Public domain / MIT licensed

## Getting stb_image.h

### Option 1: Download directly

Visit: https://raw.githubusercontent.com/nothings/stb/master/stb_image.h

Right-click → Save As → `stb_image.h` in your project directory

### Option 2: Clone STB repository

```bash
git clone https://github.com/nothings/stb.git
cp stb/stb_image.h /path/to/your/project/
```

### Option 3: Use from external directory

```bash
cd ../../external
git clone https://github.com/nothings/stb.git
```

Then in your project, include as:
```cpp
#include "../../external/stb/stb_image.h"
```

## Where to Place stb_image.h

Put `stb_image.h` in the **same directory** as your renderer files:

```
cube_viewer/
├── renderer_opengl.cpp  ← Uses stb_image
├── renderer_d3d11.cpp   ← Uses stb_image  
├── renderer_d3d12.cpp   ← Uses stb_image
├── stb_image.h          ← Place here
└── ...
```

## How It's Used in the Code

Each renderer includes stb_image:

```cpp
// renderer_opengl.cpp
#define STB_IMAGE_IMPLEMENTATION  // Only in ONE .cpp file!
#include "stb_image.h"
```

⚠️ **IMPORTANT**: Only define `STB_IMAGE_IMPLEMENTATION` in **ONE** translation unit!

Currently defined in:
- `renderer_opengl.cpp` ✅
- `renderer_d3d11.cpp` ✅ (OK - separate conditional compilation)
- `renderer_d3d12.cpp` ✅ (OK - separate conditional compilation)

This works because each renderer is only compiled when needed (Windows has D3D11/D3D12, others only have OpenGL).

## Verification

After placing stb_image.h, test the build:

```bash
cd build
cmake ..
cmake --build .
```

If you see errors like:
```
fatal error: stb_image.h: No such file or directory
```

Make sure:
1. File is named exactly `stb_image.h` (case-sensitive on Linux/Mac)
2. File is in the same directory as the renderer .cpp files
3. File is actually the stb_image.h header (not a webpage or readme)

## What's Implemented

### ✅ OpenGL Renderer
- Full texture loading with mipmaps
- Automatic format detection (RGB/RGBA)
- Proper texture binding in drawMesh

### ✅ D3D11 Renderer  
- Full texture loading with mipmaps
- Shader resource view creation
- Texture binding to pixel shader

### ⚠️ D3D12 Renderer
- Stub implementation (returns 0)
- TODO: Requires descriptor heap setup
- Models will work but textures won't show

## Supported Texture Formats

From .X files, these formats work:
- ✅ BMP (most common in old .X files)
- ✅ TGA
- ✅ PNG
- ✅ JPEG
- ✅ DDS (via stb_image extensions)

## Testing

```bash
# Test with default cube (no texture)
./cube_viewer opengl

# Test with textured model
./cube_viewer opengl aircraft.x

# If textures don't load, check console for errors like:
# "Failed to load texture: model.bmp - file not found"
```

## Troubleshooting

**Problem**: Textures appear black or white
**Solution**: Check texture paths in .X file match actual file locations

**Problem**: "Failed to load texture"
**Solution**: Texture file must be relative to .X file location or absolute path

**Problem**: Upside-down textures
**Solution**: Already handled with `stbi_set_flip_vertically_on_load()`
- OpenGL: `true` (bottom-left origin)
- D3D11/D3D12: `false` (top-left origin)

## Next Steps

1. Download `stb_image.h` and place in project directory
2. Rebuild project
3. Test with your .X models
4. Textures should now load and display!

The texture system is fully integrated with Assimp - texture paths from .X files are automatically loaded.
