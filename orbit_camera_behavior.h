// orbit_camera_behavior.h - Camera that orbits around an entity
#ifndef ORBIT_CAMERA_BEHAVIOR_H
#define ORBIT_CAMERA_BEHAVIOR_H

#include "behavior.h"
#include "math_utils.h"
#include <cmath>

// ==================== Orbit Camera Behavior ====================
// Camera that orbits around an entity with auto-rotation and manual control
class OrbitCameraBehavior : public Behavior {
public:
    OrbitCameraBehavior() 
        : Behavior("OrbitCamera")
        , m_distance(10.0f)
        , m_yaw(0.6f)
        , m_pitch(-0.4f)
        , m_autoRotate(true)
        , m_rotationSpeed(0.3f)
        , m_cameraPosition({0, 0, 0})
        , m_cameraTarget({0, 0, 0})
    {}
    
    void initialize() override {
        if (m_entity) {
            m_cameraTarget = m_entity->getPosition();
            updateCameraPosition();
        }
    }
    
    void update(float deltaTime) override {
        if (!m_enabled || !m_entity) return;
        
        // Update target to follow entity
        m_cameraTarget = m_entity->getPosition();
        
        // Auto-rotate if enabled
        if (m_autoRotate) {
            m_yaw += deltaTime * m_rotationSpeed;
        }
        
        // Update camera position
        updateCameraPosition();
    }
    
    // Manual control methods
    void rotate(float deltaYaw, float deltaPitch) {
        m_yaw += deltaYaw;
        m_pitch += deltaPitch;
        
        // Clamp pitch to avoid gimbal lock
        m_pitch = std::max(-1.5f, std::min(1.5f, m_pitch));
    }
    
    void zoom(float delta) {
        m_distance += delta;
        m_distance = std::max(2.0f, std::min(50.0f, m_distance));
    }
    
    // Getters
    Vec3 getCameraPosition() const { return m_cameraPosition; }
    Vec3 getCameraTarget() const { return m_cameraTarget; }
    
    float getDistance() const { return m_distance; }
    float getYaw() const { return m_yaw; }
    float getPitch() const { return m_pitch; }
    bool isAutoRotate() const { return m_autoRotate; }
    
    // Setters
    void setDistance(float distance) { m_distance = distance; }
    void setYaw(float yaw) { m_yaw = yaw; }
    void setPitch(float pitch) { m_pitch = pitch; }
    void setAutoRotate(bool autoRotate) { m_autoRotate = autoRotate; }
    void setRotationSpeed(float speed) { m_rotationSpeed = speed; }
    
private:
    void updateCameraPosition() {
        // Spherical to Cartesian conversion
        m_cameraPosition.x = m_cameraTarget.x + m_distance * sinf(m_yaw) * cosf(m_pitch);
        m_cameraPosition.y = m_cameraTarget.y + m_distance * sinf(m_pitch);
        m_cameraPosition.z = m_cameraTarget.z + m_distance * cosf(m_yaw) * cosf(m_pitch);
    }
    
    float m_distance;       // Distance from target
    float m_yaw;            // Horizontal rotation (radians)
    float m_pitch;          // Vertical rotation (radians)
    bool m_autoRotate;      // Auto-rotate around entity
    float m_rotationSpeed;  // Auto-rotation speed (rad/sec)
    
    Vec3 m_cameraPosition;
    Vec3 m_cameraTarget;
};

#endif // ORBIT_CAMERA_BEHAVIOR_H
