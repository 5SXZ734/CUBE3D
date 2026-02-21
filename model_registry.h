// model_registry.h - Registry for 3D models with key-based lookup
#ifndef MODEL_REGISTRY_H
#define MODEL_REGISTRY_H

#include "model.h"
#include <unordered_map>
#include <string>

// ==================== Model Registry ====================
// Manages all 3D models with key-based access
class ModelRegistry {
public:
    ModelRegistry() = default;
    ~ModelRegistry() {
        clear();
    }
    
    // Register a model with a key (e.g., "L-39", "Cessna172", "Car")
    bool registerModel(const std::string& key, const std::string& filepath) {
        // Check if already loaded
        if (m_models.find(key) != m_models.end()) {
            return true;  // Already registered
        }
        
        // Load the model
        Model* model = new Model();
        if (!ModelLoader::LoadXFile(filepath.c_str(), *model)) {
            delete model;
            return false;
        }
        
        m_models[key] = model;
        m_filepaths[key] = filepath;
        return true;
    }
    
    // Get model by key
    const Model* getModel(const std::string& key) const {
        auto it = m_models.find(key);
        return (it != m_models.end()) ? it->second : nullptr;
    }
    
    // Check if model exists
    bool hasModel(const std::string& key) const {
        return m_models.find(key) != m_models.end();
    }
    
    // Unregister a model
    void unregisterModel(const std::string& key) {
        auto it = m_models.find(key);
        if (it != m_models.end()) {
            delete it->second;
            m_models.erase(it);
            m_filepaths.erase(key);
        }
    }
    
    // Get filepath for a key
    std::string getFilepath(const std::string& key) const {
        auto it = m_filepaths.find(key);
        return (it != m_filepaths.end()) ? it->second : "";
    }
    
    // Get all registered keys
    std::vector<std::string> getRegisteredKeys() const {
        std::vector<std::string> keys;
        for (const auto& pair : m_models) {
            keys.push_back(pair.first);
        }
        return keys;
    }
    
    // Clear all models
    void clear() {
        for (auto& pair : m_models) {
            delete pair.second;
        }
        m_models.clear();
        m_filepaths.clear();
    }
    
    // Stats
    size_t getModelCount() const { return m_models.size(); }
    
private:
    std::unordered_map<std::string, Model*> m_models;
    std::unordered_map<std::string, std::string> m_filepaths;
};

#endif // MODEL_REGISTRY_H
