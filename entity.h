// entity.h - Base entity class for game objects
#ifndef ENTITY_H
#define ENTITY_H

#include "math_utils.h"
#include <string>
#include <cstdint>

// Forward declarations
class Behavior;
struct Model;  // Model is a struct, not a class

// Unique entity ID
typedef uint32_t EntityID;

// ==================== Entity Base Class ====================
// Represents any object in the game world
class Entity {
public:
    Entity(EntityID id, const std::string& name)
        : m_id(id)
        , m_name(name)
        , m_position({0, 0, 0})
        , m_rotation({0, 0, 0})  // Euler angles (pitch, yaw, roll) in radians
        , m_scale({1, 1, 1})
        , m_velocity({0, 0, 0})
        , m_angularVelocity({0, 0, 0})
        , m_model(nullptr)
        , m_visible(true)
        , m_active(true)
    {}
    
    virtual ~Entity() = default;
    
    // Getters
    EntityID getID() const { return m_id; }
    const std::string& getName() const { return m_name; }
    
    Vec3 getPosition() const { return m_position; }
    Vec3 getRotation() const { return m_rotation; }
    Vec3 getScale() const { return m_scale; }
    Vec3 getVelocity() const { return m_velocity; }
    Vec3 getAngularVelocity() const { return m_angularVelocity; }
    
    const Model* getModel() const { return m_model; }
    bool isVisible() const { return m_visible; }
    bool isActive() const { return m_active; }
    
    // Setters
    void setPosition(const Vec3& pos) { m_position = pos; }
    void setRotation(const Vec3& rot) { m_rotation = rot; }
    void setScale(const Vec3& scale) { m_scale = scale; }
    void setVelocity(const Vec3& vel) { m_velocity = vel; }
    void setAngularVelocity(const Vec3& angVel) { m_angularVelocity = angVel; }
    
    void setModel(const Model* model) { m_model = model; }
    void setVisible(bool visible) { m_visible = visible; }
    void setActive(bool active) { m_active = active; }
    
    // Transform matrix for rendering
    Mat4 getTransformMatrix() const;
    
    // Update (can be overridden)
    virtual void update(float deltaTime) { (void)deltaTime; }
    
protected:
    EntityID m_id;
    std::string m_name;
    
    // Transform
    Vec3 m_position;
    Vec3 m_rotation;      // Euler angles in radians
    Vec3 m_scale;
    
    // Physics
    Vec3 m_velocity;
    Vec3 m_angularVelocity;
    
    // Rendering
    const Model* m_model;
    bool m_visible;
    bool m_active;
};

#endif // ENTITY_H
