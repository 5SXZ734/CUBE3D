// camera_entity.h - Camera as an entity in the world
#ifndef CAMERA_ENTITY_H
#define CAMERA_ENTITY_H

#include "entity.h"

// ==================== Camera Entity ====================
// Camera is now an entity that can have position, rotation, and behaviors
class CameraEntity : public Entity {
public:
    CameraEntity(EntityID id, const std::string& name)
        : Entity(id, name)
        , m_fov(75.0f)
        , m_nearPlane(0.1f)
        , m_farPlane(10000.0f)
        , m_isActive(false)
    {}
    
    // Camera-specific properties
    float getFOV() const { return m_fov; }
    float getNearPlane() const { return m_nearPlane; }
    float getFarPlane() const { return m_farPlane; }
    bool isActiveCamera() const { return m_isActive; }
    
    void setFOV(float fov) { m_fov = fov; }
    void setNearPlane(float near) { m_nearPlane = near; }
    void setFarPlane(float far) { m_farPlane = far; }
    void setActive(bool active) { m_isActive = active; }
    
    // Get view target (where camera is looking)
    // For cameras with behaviors, this is managed by the behavior
    Vec3 getTarget() const { return m_target; }
    void setTarget(const Vec3& target) { m_target = target; }
    
private:
    float m_fov;
    float m_nearPlane;
    float m_farPlane;
    bool m_isActive;
    Vec3 m_target;  // Where the camera is looking
};

#endif // CAMERA_ENTITY_H
