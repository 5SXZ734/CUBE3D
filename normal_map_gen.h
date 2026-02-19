// normal_map_gen.h - Procedural normal map generation
#ifndef NORMAL_MAP_GEN_H
#define NORMAL_MAP_GEN_H

#include <vector>
#include <cmath>
#include <cstdint>

// Generate a procedural normal map with a bumpy pattern
// Returns RGBA data (RGB = normal, A = unused)
inline std::vector<uint8_t> generateProceduralNormalMap(int width, int height, float bumpScale = 1.0f) {
    std::vector<uint8_t> data(width * height * 4);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;
            
            // Create a bumpy pattern using sine waves
            float u = (float)x / width;
            float v = (float)y / height;
            
            // Multiple frequency bumps for interesting surface detail
            float bump = 0.0f;
            bump += std::sin(u * 20.0f * 3.14159f) * 0.3f;  // Horizontal waves
            bump += std::sin(v * 20.0f * 3.14159f) * 0.3f;  // Vertical waves
            bump += std::sin((u + v) * 40.0f * 3.14159f) * 0.2f;  // Diagonal detail
            bump += std::sin((u - v) * 40.0f * 3.14159f) * 0.2f;  // Opposite diagonal
            bump *= bumpScale;
            
            // Calculate surface normal from height field
            // Sample neighboring points to get gradient
            float h_right = bump + std::sin((u + 0.01f) * 20.0f * 3.14159f) * 0.3f;
            float h_up = bump + std::sin((v + 0.01f) * 20.0f * 3.14159f) * 0.3f;
            
            // Tangent space normal (perturbed from flat 0,0,1)
            float nx = -(h_right - bump) * 10.0f;  // dHeight/dx
            float ny = -(h_up - bump) * 10.0f;     // dHeight/dy
            float nz = 1.0f;
            
            // Normalize
            float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 0.0001f) {
                nx /= len;
                ny /= len;
                nz /= len;
            }
            
            // Convert from [-1,1] to [0,255]
            data[idx + 0] = (uint8_t)((nx * 0.5f + 0.5f) * 255.0f);  // R
            data[idx + 1] = (uint8_t)((ny * 0.5f + 0.5f) * 255.0f);  // G
            data[idx + 2] = (uint8_t)((nz * 0.5f + 0.5f) * 255.0f);  // B
            data[idx + 3] = 255;  // A
        }
    }
    
    return data;
}

// Generate a simpler rivet/panel pattern (looks more mechanical)
inline std::vector<uint8_t> generateRivetNormalMap(int width, int height) {
    std::vector<uint8_t> data(width * height * 4);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * 4;
            
            float u = (float)x / width;
            float v = (float)y / height;
            
            // Create rivet pattern (circular bumps in a grid)
            float rivetSpacing = 8.0f;  // Rivets every 1/8th of texture
            float rivetRadius = 0.03f;
            
            float cellX = std::fmod(u * rivetSpacing, 1.0f);
            float cellY = std::fmod(v * rivetSpacing, 1.0f);
            
            // Distance from center of cell
            float dx = cellX - 0.5f;
            float dy = cellY - 0.5f;
            float dist = std::sqrt(dx * dx + dy * dy);
            
            // Create circular bump
            float bump = 0.0f;
            if (dist < rivetRadius) {
                // Spherical bump
                float t = dist / rivetRadius;
                bump = std::cos(t * 3.14159f * 0.5f) * 0.5f;
            }
            
            // Add panel lines (horizontal grooves)
            float panelLine = std::fmod(v * 4.0f, 1.0f);
            if (panelLine > 0.48f && panelLine < 0.52f) {
                bump -= 0.3f;  // Groove
            }
            
            // Calculate normal from bump
            float nx = (dist < rivetRadius) ? -dx / (rivetRadius * 2.0f) : 0.0f;
            float ny = (dist < rivetRadius) ? -dy / (rivetRadius * 2.0f) : 0.0f;
            float nz = 1.0f - std::abs(bump);
            
            // Normalize
            float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 0.0001f) {
                nx /= len;
                ny /= len;
                nz /= len;
            }
            
            // Convert from [-1,1] to [0,255]
            data[idx + 0] = (uint8_t)((nx * 0.5f + 0.5f) * 255.0f);
            data[idx + 1] = (uint8_t)((ny * 0.5f + 0.5f) * 255.0f);
            data[idx + 2] = (uint8_t)((nz * 0.5f + 0.5f) * 255.0f);
            data[idx + 3] = 255;
        }
    }
    
    return data;
}

// Generate a flat normal map (for testing - should look identical to no normal map)
inline std::vector<uint8_t> generateFlatNormalMap(int width, int height) {
    std::vector<uint8_t> data(width * height * 4);
    
    for (int i = 0; i < width * height; i++) {
        data[i * 4 + 0] = 128;  // X = 0
        data[i * 4 + 1] = 128;  // Y = 0
        data[i * 4 + 2] = 255;  // Z = 1 (pointing up in tangent space)
        data[i * 4 + 3] = 255;  // Alpha
    }
    
    return data;
}

#endif // NORMAL_MAP_GEN_H
