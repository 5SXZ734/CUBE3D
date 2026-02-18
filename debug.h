// debug.h - Debug utilities and logging
#ifndef DEBUG_H
#define DEBUG_H

#ifdef _MSC_VER
#pragma warning(disable: 4996)  // Disable fopen deprecation warning on MSVC
#endif

#include <cstdio>
#include <cstdarg>
#include <chrono>
#include <string>

// ==================== Debug Levels ====================
enum class LogLevel {
    ERROR = 0,
    WARNING = 1,
    INFO = 2,
    DEBUG = 3,
    TRACE = 4
};

// ==================== Debug Manager ====================
class DebugManager {
private:
    static bool s_enabled;
    static LogLevel s_logLevel;
    static FILE* s_logFile;
    static bool s_showTimestamp;
    static std::chrono::high_resolution_clock::time_point s_startTime;

public:
    static void initialize(bool enabled = false, LogLevel level = LogLevel::INFO) {
        s_enabled = enabled;
        s_logLevel = level;
        s_startTime = std::chrono::high_resolution_clock::now();
        
        if (enabled) {
            s_logFile = fopen("debug_log.txt", "w");
            if (s_logFile) {
                fprintf(s_logFile, "=== Debug Log Started ===\n");
                fflush(s_logFile);
            }
        }
    }

    static void shutdown() {
        if (s_logFile) {
            fprintf(s_logFile, "=== Debug Log Ended ===\n");
            fclose(s_logFile);
            s_logFile = nullptr;
        }
    }

    static void setEnabled(bool enabled) { s_enabled = enabled; }
    static bool isEnabled() { return s_enabled; }
    static void setLogLevel(LogLevel level) { s_logLevel = level; }
    static void enableTimestamps(bool enable) { s_showTimestamp = enable; }

    static void log(LogLevel level, const char* format, ...) {
        if (!s_enabled || level > s_logLevel) return;

        char buffer[4096];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        const char* levelStr = getLevelString(level);
        const char* colorCode = getColorCode(level);
        
        if (s_showTimestamp) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_startTime).count();
            fprintf(stdout, "%s[%6lld ms] [%s]%s %s\n", colorCode, elapsed, levelStr, "\033[0m", buffer);
        } else {
            fprintf(stdout, "%s[%s]%s %s\n", colorCode, levelStr, "\033[0m", buffer);
        }
        fflush(stdout);

        if (s_logFile) {
            if (s_showTimestamp) {
                auto now = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_startTime).count();
                fprintf(s_logFile, "[%6lld ms] [%s] %s\n", elapsed, levelStr, buffer);
            } else {
                fprintf(s_logFile, "[%s] %s\n", levelStr, buffer);
            }
            fflush(s_logFile);
        }
    }

private:
    static const char* getLevelString(LogLevel level) {
        switch (level) {
            case LogLevel::ERROR:   return "ERROR";
            case LogLevel::WARNING: return "WARN ";
            case LogLevel::INFO:    return "INFO ";
            case LogLevel::DEBUG:   return "DEBUG";
            case LogLevel::TRACE:   return "TRACE";
            default:                return "?????";
        }
    }

    static const char* getColorCode(LogLevel level) {
        switch (level) {
            case LogLevel::ERROR:   return "\033[1;31m"; // Red
            case LogLevel::WARNING: return "\033[1;33m"; // Yellow
            case LogLevel::INFO:    return "\033[1;32m"; // Green
            case LogLevel::DEBUG:   return "\033[1;36m"; // Cyan
            case LogLevel::TRACE:   return "\033[1;37m"; // White
            default:                return "\033[0m";
        }
    }
};

// Static member declarations (definition in debug.cpp)
// Note: If you don't want a separate .cpp file, see alternative below

// ==================== Convenience Macros ====================
#define LOG_ERROR(...)   DebugManager::log(LogLevel::ERROR, __VA_ARGS__)
#define LOG_WARNING(...) DebugManager::log(LogLevel::WARNING, __VA_ARGS__)
#define LOG_INFO(...)    DebugManager::log(LogLevel::INFO, __VA_ARGS__)
#define LOG_DEBUG(...)   DebugManager::log(LogLevel::DEBUG, __VA_ARGS__)
#define LOG_TRACE(...)   DebugManager::log(LogLevel::TRACE, __VA_ARGS__)

// ==================== Static Member Storage ====================
// Implementation detail: These are defined inline to avoid needing a separate .cpp file
// This requires C++17 or later. If using C++14 or earlier, move these to a .cpp file.
inline bool DebugManager::s_enabled = false;
inline LogLevel DebugManager::s_logLevel = LogLevel::INFO;
inline FILE* DebugManager::s_logFile = nullptr;
inline bool DebugManager::s_showTimestamp = true;
inline std::chrono::high_resolution_clock::time_point DebugManager::s_startTime = {};


// ==================== Performance Counters ====================
struct PerformanceStats {
    // Frame timing
    double frameTime = 0.0;
    double fps = 0.0;
    double minFrameTime = 999999.0;
    double maxFrameTime = 0.0;
    double avgFrameTime = 0.0;
    
    // Rendering stats
    uint32_t drawCalls = 0;
    uint32_t triangles = 0;
    uint32_t meshesDrawn = 0;
    
    // Resource stats
    uint32_t texturesLoaded = 0;
    uint32_t textureMemoryKB = 0;
    uint32_t meshesLoaded = 0;
    uint32_t meshMemoryKB = 0;
    
    // Frame counter for averaging
    uint32_t frameCount = 0;
    double totalFrameTime = 0.0;
    
    void reset() {
        drawCalls = 0;
        triangles = 0;
        meshesDrawn = 0;
        frameCount++;
    }
    
    void updateFrameTime(double dt) {
        frameTime = dt;
        fps = (dt > 0.0) ? (1.0 / dt) : 0.0;
        
        if (dt < minFrameTime) minFrameTime = dt;
        if (dt > maxFrameTime) maxFrameTime = dt;
        
        totalFrameTime += dt;
        avgFrameTime = totalFrameTime / frameCount;
    }
    
    void print() const {
        printf("\n=== Performance Stats ===\n");
        printf("FPS:           %.1f (%.2f ms/frame)\n", fps, frameTime * 1000.0);
        printf("Frame Time:    Avg: %.2f ms, Min: %.2f ms, Max: %.2f ms\n",
               avgFrameTime * 1000.0, minFrameTime * 1000.0, maxFrameTime * 1000.0);
        printf("Draw Calls:    %u\n", drawCalls);
        printf("Triangles:     %u (%.1fK)\n", triangles, triangles / 1000.0f);
        printf("Meshes Drawn:  %u\n", meshesDrawn);
        printf("Textures:      %u (%.1f MB)\n", texturesLoaded, textureMemoryKB / 1024.0f);
        printf("Mesh Memory:   %.1f MB\n", meshMemoryKB / 1024.0f);
        printf("Total Frames:  %u\n", frameCount);
        printf("========================\n\n");
    }
};

// ==================== File Validator ====================
class FileValidator {
public:
    static bool exists(const char* path) {
        FILE* f = fopen(path, "rb");
        if (f) {
            fclose(f);
            return true;
        }
        return false;
    }
    
    static bool validateTexturePath(const char* path) {
        if (!exists(path)) {
            LOG_ERROR("Texture file not found: %s", path);
            return false;
        }
        
        // Check extension
        const char* ext = strrchr(path, '.');
        if (!ext) {
            LOG_ERROR("Texture has no extension: %s", path);
            return false;
        }
        
        const char* validExts[] = {".bmp", ".BMP", ".dds", ".DDS", ".png", ".PNG", 
                                  ".jpg", ".JPG", ".jpeg", ".JPEG", ".tga", ".TGA"};
        bool validExt = false;
        for (const char* valid : validExts) {
            if (strcmp(ext, valid) == 0) {
                validExt = true;
                break;
            }
        }
        
        if (!validExt) {
            LOG_WARNING("Unknown texture extension: %s", ext);
        }
        
        return true;
    }
    
    static bool validateModelPath(const char* path) {
        if (!exists(path)) {
            LOG_ERROR("Model file not found: %s", path);
            return false;
        }
        
        const char* ext = strrchr(path, '.');
        if (!ext) {
            LOG_ERROR("Model has no extension: %s", path);
            return false;
        }
        
        if (strcmp(ext, ".x") != 0 && strcmp(ext, ".X") != 0) {
            LOG_WARNING("Expected .X model format, got: %s", ext);
        }
        
        return true;
    }
    
    static size_t getFileSize(const char* path) {
        FILE* f = fopen(path, "rb");
        if (!f) return 0;
        
        fseek(f, 0, SEEK_END);
        size_t size = ftell(f);
        fclose(f);
        return size;
    }
};

// ==================== Mesh Validator ====================
class MeshValidator {
public:
    static bool validate(const Vertex* vertices, uint32_t vertexCount,
                        const uint16_t* indices, uint32_t indexCount) {
        if (!vertices || vertexCount == 0) {
            LOG_ERROR("Mesh validation failed: null or empty vertices");
            return false;
        }
        
        if (!indices || indexCount == 0) {
            LOG_ERROR("Mesh validation failed: null or empty indices");
            return false;
        }
        
        if (indexCount % 3 != 0) {
            LOG_WARNING("Index count %u is not divisible by 3 (not all triangles?)", indexCount);
        }
        
        // Check for out-of-bounds indices
        for (uint32_t i = 0; i < indexCount; i++) {
            if (indices[i] >= vertexCount) {
                LOG_ERROR("Index %u references vertex %u (out of bounds, max %u)", 
                         i, indices[i], vertexCount - 1);
                return false;
            }
        }
        
        // Check for degenerate vertices
        uint32_t degenerateCount = 0;
        for (uint32_t i = 0; i < vertexCount; i++) {
            const Vertex& v = vertices[i];
            if (v.px == 0 && v.py == 0 && v.pz == 0) {
                degenerateCount++;
            }
        }
        
        if (degenerateCount > vertexCount / 2) {
            LOG_WARNING("Mesh has %u vertices at origin (%.1f%%)", 
                       degenerateCount, (degenerateCount * 100.0f) / vertexCount);
        }
        
        LOG_DEBUG("Mesh validated: %u vertices, %u indices (%u triangles)", 
                 vertexCount, indexCount, indexCount / 3);
        
        return true;
    }
};

#endif // DEBUG_H
