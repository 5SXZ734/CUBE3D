// flight_dynamics.cpp - Aircraft flight dynamics implementation
#include "flight_dynamics.h"
#include "renderer.h"  // For Vertex struct used in debug.h
#include "debug.h"
#include <cmath>

FlightDynamics::FlightDynamics()
    : m_initialPosition({0, 100, 0})  // Start at 100m altitude
    , m_initialHeading(0)
{
    reset();
}

FlightDynamics::~FlightDynamics() {
}

void FlightDynamics::initialize(Vec3 position, float heading) {
    m_initialPosition = position;
    m_initialHeading = heading;
    reset();
}

void FlightDynamics::reset() {
    m_state = AircraftState();
    m_state.position = m_initialPosition;
    m_state.yaw = m_initialHeading;
    
    // Start with forward velocity
    m_state.velocity = {0, 0, -50.0f};  // 50 m/s forward (~180 km/h)
    m_state.speed = 50.0f;
    
    m_controls.reset();
    
    LOG_DEBUG("Flight dynamics reset: pos(%.1f, %.1f, %.1f) heading=%.2f",
             m_state.position.x, m_state.position.y, m_state.position.z,
             m_state.yaw);
}

void FlightDynamics::setControlInputs(const ControlInputs& inputs) {
    m_controls = inputs;
}

void FlightDynamics::update(float deltaTime) {
    // TODO: This is where your legacy flight dynamics model will go!
    // For now, simple placeholder physics
    
    // Compute forces and torques
    Vec3 force, torque;
    computeForces(force, torque);
    
    // Integrate state
    integrateState(deltaTime);
    
    // Keep above ground
    if (m_state.position.y < 10.0f) {
        m_state.position.y = 10.0f;
        if (m_state.velocity.y < 0) {
            m_state.velocity.y = 0;
        }
    }
}

void FlightDynamics::computeForces(Vec3& force, Vec3& torque) {
    // TODO: Replace with your legacy flight model
    
    // Placeholder: Simple physics for testing
    
    // Thrust (body frame, forward is -Z)
    float thrust = m_controls.throttle * m_params.maxThrust;
    Vec3 thrustForce = {0, 0, -thrust};
    
    // Gravity (world frame)
    Vec3 gravity = {0, -m_params.mass * GRAVITY, 0};
    
    // Simple lift based on speed (very simplified)
    float dynamicPressure = 0.5f * AIR_DENSITY * m_state.speed * m_state.speed;
    float lift = dynamicPressure * m_params.wingArea * m_params.liftCoeff;
    Vec3 liftForce = {0, lift, 0};  // Up in world frame
    
    // Drag opposing velocity
    float drag = dynamicPressure * m_params.wingArea * m_params.dragCoeff;
    Vec3 dragForce = {
        -m_state.velocity.x * drag * 0.01f,
        -m_state.velocity.y * drag * 0.01f,
        -m_state.velocity.z * drag * 0.01f
    };
    
    // Total force (mix of body and world frames - will be fixed in real model)
    force = {
        thrustForce.x + gravity.x + liftForce.x + dragForce.x,
        thrustForce.y + gravity.y + liftForce.y + dragForce.y,
        thrustForce.z + gravity.z + liftForce.z + dragForce.z
    };
    
    // Control torques (body frame)
    torque = {
        m_controls.elevator * m_params.elevatorPower * 10000.0f,  // pitch
        m_controls.rudder * m_params.rudderPower * 10000.0f,      // yaw
        m_controls.aileron * m_params.aileronPower * 10000.0f     // roll
    };
    
    // Add damping
    torque.x -= m_state.pitchRate * m_params.pitchStability * 1000.0f;
    torque.y -= m_state.yawRate * m_params.yawStability * 1000.0f;
    torque.z -= m_state.rollRate * m_params.rollStability * 1000.0f;
}

void FlightDynamics::integrateState(float dt) {
    // Simple Euler integration (will be improved with legacy model)
    
    // Angular rates from torques (simplified moment of inertia)
    float I = m_params.mass * m_params.wingspan * m_params.wingspan * 0.1f;
    
    Vec3 torque;
    Vec3 force;
    computeForces(force, torque);
    
    // Update angular velocities
    m_state.pitchRate += (torque.x / I) * dt;
    m_state.yawRate += (torque.y / I) * dt;
    m_state.rollRate += (torque.z / I) * dt;
    
    // Update orientation
    m_state.pitch += m_state.pitchRate * dt;
    m_state.yaw += m_state.yawRate * dt;
    m_state.roll += m_state.rollRate * dt;
    
    // Update linear velocity
    Vec3 accel = {
        force.x / m_params.mass,
        force.y / m_params.mass,
        force.z / m_params.mass
    };
    
    m_state.velocity.x += accel.x * dt;
    m_state.velocity.y += accel.y * dt;
    m_state.velocity.z += accel.z * dt;
    
    // Update position
    m_state.position.x += m_state.velocity.x * dt;
    m_state.position.y += m_state.velocity.y * dt;
    m_state.position.z += m_state.velocity.z * dt;
    
    // Update speed
    m_state.speed = std::sqrt(
        m_state.velocity.x * m_state.velocity.x +
        m_state.velocity.y * m_state.velocity.y +
        m_state.velocity.z * m_state.velocity.z
    );
}

Mat4 FlightDynamics::getTransformMatrix() const {
    // Create transformation matrix from position and orientation
    // Order: Translate * RotateY(yaw) * RotateX(pitch) * RotateZ(roll)
    
    Mat4 translation = mat4_translate(m_state.position.x, m_state.position.y, m_state.position.z);
    Mat4 rotYaw = mat4_rotate_y(m_state.yaw);
    Mat4 rotPitch = mat4_rotate_x(m_state.pitch);
    Mat4 rotRoll = mat4_rotate_z(m_state.roll);
    
    return mat4_mul(translation, mat4_mul(rotYaw, mat4_mul(rotPitch, rotRoll)));
}

Vec3 FlightDynamics::bodyToWorld(const Vec3& bodyVec) const {
    // TODO: Proper rotation matrix transformation
    // For now, simplified
    return bodyVec;
}

Vec3 FlightDynamics::worldToBody(const Vec3& worldVec) const {
    // TODO: Proper rotation matrix transformation
    // For now, simplified
    return worldVec;
}

void FlightDynamics::printDebugInfo() const {
    LOG_INFO("=== Flight State ===");
    LOG_INFO("Position: (%.1f, %.1f, %.1f)", 
             m_state.position.x, m_state.position.y, m_state.position.z);
    LOG_INFO("Orientation: pitch=%.2f yaw=%.2f roll=%.2f", 
             m_state.pitch, m_state.yaw, m_state.roll);
    LOG_INFO("Speed: %.1f m/s (%.1f km/h)", m_state.speed, m_state.speed * 3.6f);
    LOG_INFO("Velocity: (%.1f, %.1f, %.1f)", 
             m_state.velocity.x, m_state.velocity.y, m_state.velocity.z);
    LOG_INFO("Controls: elev=%.2f ail=%.2f rud=%.2f thr=%.2f",
             m_controls.elevator, m_controls.aileron, m_controls.rudder, m_controls.throttle);
}
