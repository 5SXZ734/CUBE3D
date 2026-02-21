// chase_camera_behavior.h - Camera that follows an entity
#ifndef CHASE_CAMERA_BEHAVIOR_H
#define CHASE_CAMERA_BEHAVIOR_H

#include "behavior.h"
#include "math_utils.h"

// ==================== Chase Camera Behavior ====================
// Camera that smoothly follows an entity from behind
class ChaseCameraBehavior : public Behavior {
public:
    ChaseCameraBehavior() 
        : Behavior("ChaseCamera")
        , m_distance(25.0f)
        , m_height(8.0f)
        , m_smoothness(0.92f)
        , m_cameraPosition({0, 0, 0})
        , m_cameraTarget({0, 0, 0})
    {}
    
    void initialize() override {
        if (m_entity) {
            Vec3 pos = m_entity->getPosition();
            m_cameraPosition = pos;
            m_cameraPosition.z += m_distance;
            m_cameraPosition.y += m_height;
            m_cameraTarget = pos;
        }
    }
    
    void update(float deltaTime) override {
        (void)deltaTime;  // Mark as intentionally unused
        if (!m_enabled || !m_entity) return;
        
        Vec3 entityPos = m_entity->getPosition();
        Vec3 entityRot = m_entity->getRotation();
        
        // Calculate ideal camera position behind the entity
        float yaw = entityRot.y;  // Yaw angle
        
        Vec3 idealPos;
        // Behind is OPPOSITE direction: add distance in negative yaw direction
        idealPos.x = entityPos.x + m_distance * sinf(yaw);
        idealPos.y = entityPos.y + m_height;
        idealPos.z = entityPos.z + m_distance * cosf(yaw);
        
        // Smooth interpolation
        m_cameraPosition.x = m_cameraPosition.x * m_smoothness + idealPos.x * (1.0f - m_smoothness);
        m_cameraPosition.y = m_cameraPosition.y * m_smoothness + idealPos.y * (1.0f - m_smoothness);
        m_cameraPosition.z = m_cameraPosition.z * m_smoothness + idealPos.z * (1.0f - m_smoothness);
        
        // Look at entity (with slight offset forward)
        Vec3 targetOffset;
        targetOffset.x = -10.0f * sinf(yaw);
        targetOffset.y = 0.0f;
        targetOffset.z = -10.0f * cosf(yaw);
        
        m_cameraTarget.x = entityPos.x + targetOffset.x;
        m_cameraTarget.y = entityPos.y + targetOffset.y;
        m_cameraTarget.z = entityPos.z + targetOffset.z;
    }
    
    // Getters
    Vec3 getCameraPosition() const { return m_cameraPosition; }
    Vec3 getCameraTarget() const { return m_cameraTarget; }
    
    // Camera parameters
    void setDistance(float distance) { m_distance = distance; }
    void setHeight(float height) { m_height = height; }
    void setSmoothness(float smoothness) { m_smoothness = smoothness; }
    
    float getDistance() const { return m_distance; }
    float getHeight() const { return m_height; }
    float getSmoothness() const { return m_smoothness; }
    
private:
    float m_distance;      // Distance behind entity
    float m_height;        // Height above entity
    float m_smoothness;    // Smoothing factor (0-1)
    
    Vec3 m_cameraPosition;
    Vec3 m_cameraTarget;
};

#endif // CHASE_CAMERA_BEHAVIOR_H
