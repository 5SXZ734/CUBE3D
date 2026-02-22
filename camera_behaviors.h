// camera_behaviors.h - Camera behaviors that target other entities
#ifndef CAMERA_BEHAVIORS_H
#define CAMERA_BEHAVIORS_H

#include "behavior.h"
#include "camera_entity.h"
#include "entity_registry.h"
#include <cmath>

// ==================== Chase Camera Behavior (for camera entities) ====================
// Camera follows and looks at a target entity
class ChaseCameraTargetBehavior : public Behavior {
public:
    ChaseCameraTargetBehavior(EntityRegistry* registry, EntityID targetID)
        : Behavior("ChaseCameraTarget")
        , m_registry(registry)
        , m_targetID(targetID)
        , m_distance(25.0f)
        , m_height(8.0f)
        , m_smoothness(0.92f)
    {}
    
    void update(float deltaTime) override {
        (void)deltaTime;
        if (!m_enabled || !m_entity || !m_registry) return;
        
        Entity* target = m_registry->getEntity(m_targetID);
        if (!target) return;
        
        Vec3 targetPos = target->getPosition();
        Vec3 targetRot = target->getRotation();
        float yaw = targetRot.y;
        
        // Calculate ideal camera position behind target
        Vec3 idealPos;
        idealPos.x = targetPos.x + m_distance * sinf(yaw);
        idealPos.y = targetPos.y + m_height;
        idealPos.z = targetPos.z + m_distance * cosf(yaw);
        
        // Smooth interpolation
        Vec3 currentPos = m_entity->getPosition();
        currentPos.x = currentPos.x * m_smoothness + idealPos.x * (1.0f - m_smoothness);
        currentPos.y = currentPos.y * m_smoothness + idealPos.y * (1.0f - m_smoothness);
        currentPos.z = currentPos.z * m_smoothness + idealPos.z * (1.0f - m_smoothness);
        m_entity->setPosition(currentPos);
        
        // Look at target (slightly ahead)
        Vec3 lookTarget;
        lookTarget.x = targetPos.x - 10.0f * sinf(yaw);
        lookTarget.y = targetPos.y;
        lookTarget.z = targetPos.z - 10.0f * cosf(yaw);
        
        // Update camera target (for CameraEntity)
        if (CameraEntity* cam = dynamic_cast<CameraEntity*>(m_entity)) {
            cam->setTarget(lookTarget);
        }
    }
    
    void setDistance(float distance) { m_distance = distance; }
    void setHeight(float height) { m_height = height; }
    void setSmoothness(float smoothness) { m_smoothness = smoothness; }
    
private:
    EntityRegistry* m_registry;
    EntityID m_targetID;
    float m_distance;
    float m_height;
    float m_smoothness;
};

// ==================== Orbit Camera Behavior (for camera entities) ====================
// Camera orbits around a target entity
class OrbitCameraTargetBehavior : public Behavior {
public:
    OrbitCameraTargetBehavior(EntityRegistry* registry, EntityID targetID)
        : Behavior("OrbitCameraTarget")
        , m_registry(registry)
        , m_targetID(targetID)
        , m_distance(12.0f)
        , m_yaw(0.6f)
        , m_pitch(-0.4f)
        , m_autoRotate(true)
        , m_rotationSpeed(0.3f)
    {}
    
    void update(float deltaTime) override {
        if (!m_enabled || !m_entity || !m_registry) return;
        
        Entity* target = m_registry->getEntity(m_targetID);
        if (!target) return;
        
        // Auto-rotate
        if (m_autoRotate) {
            m_yaw += deltaTime * m_rotationSpeed;
        }
        
        // Calculate camera position
        Vec3 targetPos = target->getPosition();
        Vec3 camPos;
        camPos.x = targetPos.x + m_distance * sinf(m_yaw) * cosf(m_pitch);
        camPos.y = targetPos.y + m_distance * sinf(m_pitch);
        camPos.z = targetPos.z + m_distance * cosf(m_yaw) * cosf(m_pitch);
        
        m_entity->setPosition(camPos);
        
        // Update camera target
        if (CameraEntity* cam = dynamic_cast<CameraEntity*>(m_entity)) {
            cam->setTarget(targetPos);
        }
    }
    
    void rotate(float deltaYaw, float deltaPitch) {
        m_yaw += deltaYaw;
        m_pitch += deltaPitch;
        m_pitch = std::max(-1.5f, std::min(1.5f, m_pitch));
    }
    
    void zoom(float delta) {
        m_distance += delta;
        m_distance = std::max(2.0f, std::min(50.0f, m_distance));
    }
    
    void setDistance(float distance) { m_distance = distance; }
    void setYaw(float yaw) { m_yaw = yaw; }
    void setPitch(float pitch) { m_pitch = pitch; }
    void setAutoRotate(bool autoRotate) { m_autoRotate = autoRotate; }
    void setRotationSpeed(float speed) { m_rotationSpeed = speed; }
    
    float getYaw() const { return m_yaw; }
    float getPitch() const { return m_pitch; }
    
private:
    EntityRegistry* m_registry;
    EntityID m_targetID;
    float m_distance;
    float m_yaw;
    float m_pitch;
    bool m_autoRotate;
    float m_rotationSpeed;
};

#endif // CAMERA_BEHAVIORS_H
