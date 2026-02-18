// dds_loader.h - Simple DDS texture loader for DX SDK models
#ifndef DDS_LOADER_H
#define DDS_LOADER_H

#include <cstdint>
#include <cstdio>
#include <cstring>

// DDS file structures
#pragma pack(push, 1)
struct DDS_PIXELFORMAT {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwFourCC;
    uint32_t dwRGBBitCount;
    uint32_t dwRBitMask;
    uint32_t dwGBitMask;
    uint32_t dwBBitMask;
    uint32_t dwABitMask;
};

struct DDS_HEADER {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwHeight;
    uint32_t dwWidth;
    uint32_t dwPitchOrLinearSize;
    uint32_t dwDepth;
    uint32_t dwMipMapCount;
    uint32_t dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t dwCaps;
    uint32_t dwCaps2;
    uint32_t dwCaps3;
    uint32_t dwCaps4;
    uint32_t dwReserved2;
};
#pragma pack(pop)

#define DDS_MAGIC 0x20534444  // "DDS "
#define DDPF_FOURCC 0x00000004
#define DDPF_RGB    0x00000040
#define DDPF_RGBA   0x00000041

#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
    ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) | \
     ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24))

// Common DDS formats
#define FOURCC_DXT1 MAKEFOURCC('D','X','T','1')
#define FOURCC_DXT3 MAKEFOURCC('D','X','T','3')
#define FOURCC_DXT5 MAKEFOURCC('D','X','T','5')

class DDSLoader {
public:
    // Load DDS file and return RGBA8 data (decompressed if needed)
    // Returns nullptr on failure
    // width, height, and channels are set on success
    static unsigned char* Load(const char* filepath, int* width, int* height, int* channels) {
        FILE* f = fopen(filepath, "rb");
        if (!f) {
            std::fprintf(stderr, "Failed to open DDS file: %s\n", filepath);
            return nullptr;
        }

        // Read magic number
        uint32_t magic;
        fread(&magic, sizeof(uint32_t), 1, f);
        if (magic != DDS_MAGIC) {
            std::fprintf(stderr, "Invalid DDS file (bad magic): %s\n", filepath);
            fclose(f);
            return nullptr;
        }

        // Read header
        DDS_HEADER header;
        fread(&header, sizeof(DDS_HEADER), 1, f);

        *width = header.dwWidth;
        *height = header.dwHeight;

        // Check format
        bool isDXT1 = (header.ddspf.dwFlags & DDPF_FOURCC) && (header.ddspf.dwFourCC == FOURCC_DXT1);
        bool isDXT3 = (header.ddspf.dwFlags & DDPF_FOURCC) && (header.ddspf.dwFourCC == FOURCC_DXT3);
        bool isDXT5 = (header.ddspf.dwFlags & DDPF_FOURCC) && (header.ddspf.dwFourCC == FOURCC_DXT5);

        if (isDXT1 || isDXT3 || isDXT5) {
            // Compressed format - decompress to RGBA8
            *channels = 4;
            
            // Calculate compressed size
            int blockSize = (isDXT1) ? 8 : 16;
            int numBlocksWide = (*width + 3) / 4;
            int numBlocksHigh = (*height + 3) / 4;
            int compressedSize = numBlocksWide * numBlocksHigh * blockSize;

            // Read compressed data
            unsigned char* compressedData = new unsigned char[compressedSize];
            fread(compressedData, 1, compressedSize, f);
            fclose(f);

            // Decompress
            unsigned char* rgba = new unsigned char[(*width) * (*height) * 4];
            
            if (isDXT1) {
                DecompressDXT1(compressedData, rgba, *width, *height);
            } else if (isDXT5) {
                DecompressDXT5(compressedData, rgba, *width, *height);
            } else {
                // DXT3 - for now, treat like DXT5 (similar structure)
                DecompressDXT5(compressedData, rgba, *width, *height);
            }

            delete[] compressedData;
            return rgba;
            
        } else if (header.ddspf.dwFlags & DDPF_RGB) {
            // Uncompressed RGB/RGBA
            int bpp = header.ddspf.dwRGBBitCount / 8;
            *channels = bpp;
            
            int dataSize = (*width) * (*height) * bpp;
            unsigned char* data = new unsigned char[dataSize];
            fread(data, 1, dataSize, f);
            fclose(f);

            // If BGR/BGRA, swap to RGB/RGBA
            if (bpp >= 3) {
                for (int i = 0; i < (*width) * (*height); i++) {
                    unsigned char temp = data[i * bpp + 0];
                    data[i * bpp + 0] = data[i * bpp + 2];
                    data[i * bpp + 2] = temp;
                }
            }

            return data;
        } else {
            std::fprintf(stderr, "Unsupported DDS format: %s\n", filepath);
            fclose(f);
            return nullptr;
        }
    }

private:
    // Simple DXT1 decompression
    static void DecompressDXT1(const unsigned char* src, unsigned char* dst, int width, int height) {
        int numBlocksWide = (width + 3) / 4;
        int numBlocksHigh = (height + 3) / 4;

        for (int by = 0; by < numBlocksHigh; by++) {
            for (int bx = 0; bx < numBlocksWide; bx++) {
                const unsigned char* block = src + (by * numBlocksWide + bx) * 8;
                
                uint16_t c0 = block[0] | (block[1] << 8);
                uint16_t c1 = block[2] | (block[3] << 8);
                uint32_t bits = block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24);

                uint32_t colors[4];
                colors[0] = RGB565ToRGBA8(c0, 255);
                colors[1] = RGB565ToRGBA8(c1, 255);

                if (c0 > c1) {
                    colors[2] = BlendRGBA(colors[0], colors[1], 2, 1);
                    colors[3] = BlendRGBA(colors[0], colors[1], 1, 2);
                } else {
                    colors[2] = BlendRGBA(colors[0], colors[1], 1, 1);
                    colors[3] = 0x00000000; // Transparent
                }

                for (int py = 0; py < 4; py++) {
                    for (int px = 0; px < 4; px++) {
                        int x = bx * 4 + px;
                        int y = by * 4 + py;
                        if (x < width && y < height) {
                            int index = (bits >> ((py * 4 + px) * 2)) & 0x3;
                            uint32_t color = colors[index];
                            int offset = (y * width + x) * 4;
                            dst[offset + 0] = (color >> 0) & 0xFF;
                            dst[offset + 1] = (color >> 8) & 0xFF;
                            dst[offset + 2] = (color >> 16) & 0xFF;
                            dst[offset + 3] = (color >> 24) & 0xFF;
                        }
                    }
                }
            }
        }
    }

    // Simple DXT5 decompression (with alpha)
    static void DecompressDXT5(const unsigned char* src, unsigned char* dst, int width, int height) {
        int numBlocksWide = (width + 3) / 4;
        int numBlocksHigh = (height + 3) / 4;

        for (int by = 0; by < numBlocksHigh; by++) {
            for (int bx = 0; bx < numBlocksWide; bx++) {
                const unsigned char* block = src + (by * numBlocksWide + bx) * 16;
                
                // Alpha block (8 bytes)
                uint8_t a0 = block[0];
                uint8_t a1 = block[1];
                uint64_t aBits = block[2] | ((uint64_t)block[3] << 8) | ((uint64_t)block[4] << 16) |
                                ((uint64_t)block[5] << 24) | ((uint64_t)block[6] << 32) | ((uint64_t)block[7] << 40);

                uint8_t alphas[8];
                alphas[0] = a0;
                alphas[1] = a1;
                if (a0 > a1) {
                    for (int i = 0; i < 6; i++)
                        alphas[2 + i] = ((6 - i) * a0 + (1 + i) * a1) / 7;
                } else {
                    for (int i = 0; i < 4; i++)
                        alphas[2 + i] = ((4 - i) * a0 + (1 + i) * a1) / 5;
                    alphas[6] = 0;
                    alphas[7] = 255;
                }

                // Color block (8 bytes)
                uint16_t c0 = block[8] | (block[9] << 8);
                uint16_t c1 = block[10] | (block[11] << 8);
                uint32_t bits = block[12] | (block[13] << 8) | (block[14] << 16) | (block[15] << 24);

                uint32_t colors[4];
                colors[0] = RGB565ToRGBA8(c0, 255);
                colors[1] = RGB565ToRGBA8(c1, 255);
                colors[2] = BlendRGBA(colors[0], colors[1], 2, 1);
                colors[3] = BlendRGBA(colors[0], colors[1], 1, 2);

                for (int py = 0; py < 4; py++) {
                    for (int px = 0; px < 4; px++) {
                        int x = bx * 4 + px;
                        int y = by * 4 + py;
                        if (x < width && y < height) {
                            int pixelIndex = py * 4 + px;
                            int colorIndex = (bits >> (pixelIndex * 2)) & 0x3;
                            int alphaIndex = (aBits >> (pixelIndex * 3)) & 0x7;
                            
                            uint32_t color = colors[colorIndex];
                            int offset = (y * width + x) * 4;
                            dst[offset + 0] = (color >> 0) & 0xFF;
                            dst[offset + 1] = (color >> 8) & 0xFF;
                            dst[offset + 2] = (color >> 16) & 0xFF;
                            dst[offset + 3] = alphas[alphaIndex];
                        }
                    }
                }
            }
        }
    }

    static uint32_t RGB565ToRGBA8(uint16_t rgb565, uint8_t alpha) {
        uint8_t r = ((rgb565 >> 11) & 0x1F) * 255 / 31;
        uint8_t g = ((rgb565 >> 5) & 0x3F) * 255 / 63;
        uint8_t b = (rgb565 & 0x1F) * 255 / 31;
        return r | (g << 8) | (b << 16) | (alpha << 24);
    }

    static uint32_t BlendRGBA(uint32_t c0, uint32_t c1, int w0, int w1) {
        uint8_t r = (((c0 >> 0) & 0xFF) * w0 + ((c1 >> 0) & 0xFF) * w1) / (w0 + w1);
        uint8_t g = (((c0 >> 8) & 0xFF) * w0 + ((c1 >> 8) & 0xFF) * w1) / (w0 + w1);
        uint8_t b = (((c0 >> 16) & 0xFF) * w0 + ((c1 >> 16) & 0xFF) * w1) / (w0 + w1);
        uint8_t a = (((c0 >> 24) & 0xFF) * w0 + ((c1 >> 24) & 0xFF) * w1) / (w0 + w1);
        return r | (g << 8) | (b << 16) | (a << 24);
    }
};

#endif // DDS_LOADER_H
