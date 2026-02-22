// input_controller.h - Base class for different input schemes
#ifndef INPUT_CONTROLLER_H
#define INPUT_CONTROLLER_H

#include <string>

// Forward declaration
class Entity;

// ==================== Input Controller Base ====================
// Different controllable entities use different input schemes
class InputController {
public:
    InputController(const std::string& name) : m_name(name), m_entity(nullptr) {}
    virtual ~InputController() = default;
    
    // Attach to an entity
    virtual void attach(Entity* entity) { m_entity = entity; }
    virtual void detach() { m_entity = nullptr; }
    
    // Input events
    virtual void onKeyPress(int key) = 0;
    virtual void onKeyRelease(int key) = 0;
    virtual void update(float deltaTime) = 0;
    
    const std::string& getName() const { return m_name; }
    Entity* getEntity() const { return m_entity; }
    
protected:
    std::string m_name;
    Entity* m_entity;
};

#endif // INPUT_CONTROLLER_H
