// behavior.h - Base behavior class for entity components
#ifndef BEHAVIOR_H
#define BEHAVIOR_H

#include <string>

// Forward declaration
class Entity;

// ==================== Behavior Base Class ====================
// Behaviors control how entities behave and respond to input
class Behavior {
public:
    Behavior(const std::string& name) 
        : m_name(name)
        , m_entity(nullptr)
        , m_enabled(true)
    {}
    
    virtual ~Behavior() = default;
    
    // Attach to an entity
    virtual void attach(Entity* entity) { m_entity = entity; }
    virtual void detach() { m_entity = nullptr; }
    
    // Lifecycle
    virtual void initialize() {}
    virtual void update(float deltaTime) = 0;  // Pure virtual - must implement
    virtual void shutdown() {}
    
    // Getters
    const std::string& getName() const { return m_name; }
    Entity* getEntity() const { return m_entity; }
    bool isEnabled() const { return m_enabled; }
    
    // Setters
    void setEnabled(bool enabled) { m_enabled = enabled; }
    
protected:
    std::string m_name;
    Entity* m_entity;
    bool m_enabled;
};

#endif // BEHAVIOR_H
