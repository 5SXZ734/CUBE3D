// entity_registry.h - Central registry for all game entities
#ifndef ENTITY_REGISTRY_H
#define ENTITY_REGISTRY_H

#include "entity.h"
#include "behavior.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>

// ==================== Entity Registry ====================
// Manages all entities and their behaviors
class EntityRegistry {
public:
    EntityRegistry() : m_nextEntityID(1) {}
    
    ~EntityRegistry() {
        clear();
    }
    
    // Entity management
    Entity* createEntity(const std::string& name) {
        EntityID id = m_nextEntityID++;
        Entity* entity = new Entity(id, name);
        m_entities[id] = entity;
        return entity;
    }
    
    void destroyEntity(EntityID id) {
        auto it = m_entities.find(id);
        if (it != m_entities.end()) {
            // Remove all behaviors
            removeBehaviors(id);
            
            // Delete entity
            delete it->second;
            m_entities.erase(it);
        }
    }
    
    Entity* getEntity(EntityID id) {
        auto it = m_entities.find(id);
        return (it != m_entities.end()) ? it->second : nullptr;
    }
    
    Entity* findEntityByName(const std::string& name) {
        for (auto& pair : m_entities) {
            if (pair.second->getName() == name) {
                return pair.second;
            }
        }
        return nullptr;
    }
    
    // Behavior management
    template<typename T>
    T* addBehavior(EntityID entityID) {
        T* behavior = new T();
        Entity* entity = getEntity(entityID);
        if (entity) {
            behavior->attach(entity);
            behavior->initialize();
            m_behaviors[entityID].push_back(behavior);
        }
        return behavior;
    }
    
    void removeBehaviors(EntityID entityID) {
        auto it = m_behaviors.find(entityID);
        if (it != m_behaviors.end()) {
            for (Behavior* behavior : it->second) {
                behavior->shutdown();
                behavior->detach();
                delete behavior;
            }
            m_behaviors.erase(it);
        }
    }
    
    template<typename T>
    T* getBehavior(EntityID entityID) {
        auto it = m_behaviors.find(entityID);
        if (it != m_behaviors.end()) {
            for (Behavior* behavior : it->second) {
                T* typed = dynamic_cast<T*>(behavior);
                if (typed) return typed;
            }
        }
        return nullptr;
    }
    
    std::vector<Behavior*> getBehaviors(EntityID entityID) {
        auto it = m_behaviors.find(entityID);
        if (it != m_behaviors.end()) {
            return it->second;
        }
        return {};
    }
    
    // Update all entities and behaviors
    void update(float deltaTime) {
        // Update all entities
        for (auto& pair : m_entities) {
            if (pair.second->isActive()) {
                pair.second->update(deltaTime);
            }
        }
        
        // Update all behaviors
        for (auto& pair : m_behaviors) {
            for (Behavior* behavior : pair.second) {
                if (behavior->isEnabled()) {
                    behavior->update(deltaTime);
                }
            }
        }
    }
    
    // Get all entities
    const std::unordered_map<EntityID, Entity*>& getAllEntities() const {
        return m_entities;
    }
    
    // Clear everything
    void clear() {
        // Clean up behaviors first
        for (auto& pair : m_behaviors) {
            for (Behavior* behavior : pair.second) {
                behavior->shutdown();
                behavior->detach();
                delete behavior;
            }
        }
        m_behaviors.clear();
        
        // Clean up entities
        for (auto& pair : m_entities) {
            delete pair.second;
        }
        m_entities.clear();
        
        m_nextEntityID = 1;
    }
    
    // Stats
    size_t getEntityCount() const { return m_entities.size(); }
    size_t getBehaviorCount() const {
        size_t count = 0;
        for (const auto& pair : m_behaviors) {
            count += pair.second.size();
        }
        return count;
    }
    
private:
    EntityID m_nextEntityID;
    std::unordered_map<EntityID, Entity*> m_entities;
    std::unordered_map<EntityID, std::vector<Behavior*>> m_behaviors;
};

#endif // ENTITY_REGISTRY_H
