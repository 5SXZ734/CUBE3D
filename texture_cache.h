// texture_cache.h - Texture caching system to avoid duplicate loads
#ifndef TEXTURE_CACHE_H
#define TEXTURE_CACHE_H

#include "renderer.h"
#include <string>
#include <unordered_map>

// ==================== Texture Cache ====================
class TextureCache {
private:
    IRenderer* m_renderer;
    std::unordered_map<std::string, uint32_t> m_pathToHandle;
    
    // Statistics
    uint32_t m_totalRequests;
    uint32_t m_cacheHits;
    uint32_t m_cacheMisses;
    
public:
    TextureCache() 
        : m_renderer(nullptr)
        , m_totalRequests(0)
        , m_cacheHits(0)
        , m_cacheMisses(0)
    {}
    
    void setRenderer(IRenderer* renderer) {
        m_renderer = renderer;
    }
    
    // Get or load texture - main API
    uint32_t getOrLoad(const char* path) {
        if (!m_renderer) {
            LOG_ERROR("TextureCache: No renderer set!");
            return 0;
        }
        
        if (!path || path[0] == '\0') {
            LOG_WARNING("TextureCache: Empty path provided");
            return 0;
        }
        
        m_totalRequests++;
        
        // Normalize path (convert to lowercase for case-insensitive comparison)
        std::string normalizedPath = normalizePath(path);
        
        // Check if already loaded
        auto it = m_pathToHandle.find(normalizedPath);
        if (it != m_pathToHandle.end()) {
            m_cacheHits++;
            LOG_TRACE("TextureCache: Cache HIT for %s (handle %u)", path, it->second);
            return it->second;
        }
        
        // Not in cache - load it
        m_cacheMisses++;
        LOG_DEBUG("TextureCache: Cache MISS for %s, loading...", path);
        
        uint32_t handle = m_renderer->createTexture(path);
        if (handle == 0) {
            LOG_WARNING("TextureCache: Failed to load texture: %s", path);
            return 0;
        }
        
        // Add to cache
        m_pathToHandle[normalizedPath] = handle;
        LOG_DEBUG("TextureCache: Cached %s as handle %u (%zu textures cached)", 
                 path, handle, m_pathToHandle.size());
        
        return handle;
    }
    
    // Check if texture is already loaded
    bool isLoaded(const char* path) const {
        if (!path) return false;
        std::string normalizedPath = normalizePath(path);
        return m_pathToHandle.find(normalizedPath) != m_pathToHandle.end();
    }
    
    // Get handle without loading (returns 0 if not cached)
    uint32_t getHandle(const char* path) const {
        if (!path) return 0;
        std::string normalizedPath = normalizePath(path);
        auto it = m_pathToHandle.find(normalizedPath);
        return (it != m_pathToHandle.end()) ? it->second : 0;
    }
    
    // Clear cache (doesn't destroy textures in renderer)
    void clear() {
        LOG_INFO("TextureCache: Clearing cache (%zu textures)", m_pathToHandle.size());
        m_pathToHandle.clear();
        m_totalRequests = 0;
        m_cacheHits = 0;
        m_cacheMisses = 0;
    }
    
    // Get cache statistics
    struct Stats {
        uint32_t totalRequests;
        uint32_t cacheHits;
        uint32_t cacheMisses;
        uint32_t uniqueTextures;
        float hitRate;
    };
    
    Stats getStats() const {
        Stats stats;
        stats.totalRequests = m_totalRequests;
        stats.cacheHits = m_cacheHits;
        stats.cacheMisses = m_cacheMisses;
        stats.uniqueTextures = (uint32_t)m_pathToHandle.size();
        stats.hitRate = (m_totalRequests > 0) 
                        ? (float)m_cacheHits / (float)m_totalRequests * 100.0f 
                        : 0.0f;
        return stats;
    }
    
    void printStats() const {
        Stats stats = getStats();
        printf("\n=== Texture Cache Statistics ===\n");
        printf("Total Requests:   %u\n", stats.totalRequests);
        printf("Cache Hits:       %u\n", stats.cacheHits);
        printf("Cache Misses:     %u\n", stats.cacheMisses);
        printf("Unique Textures:  %u\n", stats.uniqueTextures);
        printf("Hit Rate:         %.1f%%\n", stats.hitRate);
        
        if (stats.totalRequests > 0) {
            float savings = (float)stats.cacheHits / (float)stats.totalRequests * 100.0f;
            printf("Memory Saved:     ~%.1f%% (avoided reloading %u textures)\n", 
                   savings, stats.cacheHits);
        }
        printf("================================\n\n");
    }
    
private:
    // Normalize path for consistent lookups (lowercase, forward slashes)
    static std::string normalizePath(const char* path) {
        std::string normalized = path;
        
        // Convert to lowercase for case-insensitive comparison
        for (char& c : normalized) {
            if (c >= 'A' && c <= 'Z') {
                c = c - 'A' + 'a';
            }
            // Normalize path separators
            if (c == '\\') {
                c = '/';
            }
        }
        
        return normalized;
    }
};

#endif // TEXTURE_CACHE_H
