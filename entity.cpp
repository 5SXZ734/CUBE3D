// entity.cpp - Entity implementation
#include "entity.h"
#include <cmath>

Mat4 Entity::getTransformMatrix() const {
    // Build proper transform matrix: T * R * S
    
    float cosY = std::cos(m_rotation.y);  // yaw
    float sinY = std::sin(m_rotation.y);
    float cosP = std::cos(m_rotation.x);  // pitch
    float sinP = std::sin(m_rotation.x);
    float cosR = std::cos(m_rotation.z);  // roll
    float sinR = std::sin(m_rotation.z);
    
    // Build combined rotation matrix (YXZ order: Yaw * Pitch * Roll)
    Mat4 mat;
    
    // First row
    mat.m[0] = (cosY * cosR + sinY * sinP * sinR) * m_scale.x;
    mat.m[1] = cosP * sinR * m_scale.x;
    mat.m[2] = (-sinY * cosR + cosY * sinP * sinR) * m_scale.x;
    mat.m[3] = 0.0f;
    
    // Second row
    mat.m[4] = (-cosY * sinR + sinY * sinP * cosR) * m_scale.y;
    mat.m[5] = cosP * cosR * m_scale.y;
    mat.m[6] = (sinY * sinR + cosY * sinP * cosR) * m_scale.y;
    mat.m[7] = 0.0f;
    
    // Third row
    mat.m[8] = sinY * cosP * m_scale.z;
    mat.m[9] = -sinP * m_scale.z;
    mat.m[10] = cosY * cosP * m_scale.z;
    mat.m[11] = 0.0f;
    
    // Fourth row (translation)
    mat.m[12] = m_position.x;
    mat.m[13] = m_position.y;
    mat.m[14] = m_position.z;
    mat.m[15] = 1.0f;
    
    return mat;
}

