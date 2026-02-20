// flight_dynamics.cpp - Aircraft flight dynamics implementation
#include "flight_dynamics.h"
#include "renderer.h"  // For Vertex struct used in debug.h
#include "debug.h"
#include <cmath>
#include <algorithm>

// Helper functions
static float clamp(float v, float min, float max) {
    return (std::max)(min, (std::min)(max, v));
}

static float sign(float v) {
    return (v < 0.0f) ? -1.0f : 1.0f;
}

FlightDynamics::FlightDynamics()
    : m_initialPosition({0, 100, 0})
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
    
    // Start with forward velocity (50 m/s ~= 180 km/h)
    m_state.velocity = {0, 0, -50.0f};  // -Z is forward
    m_state.speed = 50.0f;
    
    m_controls.reset();
    
    LOG_DEBUG("Flight dynamics reset: pos(%.1f, %.1f, %.1f) heading=%.2f speed=%.1f",
             m_state.position.x, m_state.position.y, m_state.position.z,
             m_state.yaw, m_state.speed);
}

void FlightDynamics::setControlInputs(const ControlInputs& inputs) {
    m_controls = inputs;
}

void FlightDynamics::update(float deltaTime) {
    if (deltaTime <= 0.0f || deltaTime > 1.0f) return;  // Sanity check
    
    // Compute forces and torques ONCE
    Vec3 force, torque;
    computeForces(force, torque);
    
    // Integrate state using the computed forces
    integrateState(deltaTime, force, torque);
    
    // Enforce ground constraint
    if (m_state.position.y < 2.0f) {
        m_state.position.y = 2.0f;
        if (m_state.velocity.y < 0) {
            m_state.velocity.y = 0;
            // Add ground friction
            m_state.velocity.x *= 0.95f;
            m_state.velocity.z *= 0.95f;
        }
    }
}

void FlightDynamics::computeForces(Vec3& force, Vec3& torque) {
    // Debug: Log at entry
    static int frameCount = 0;
    if (++frameCount % 60 == 0 && (m_controls.elevator != 0 || m_controls.aileron != 0 || m_controls.rudder != 0)) {
        LOG_DEBUG("computeForces: controls(e=%.2f a=%.2f r=%.2f)",
                 m_controls.elevator, m_controls.aileron, m_controls.rudder);
    }
    
    // ==================== FORCES (Newtons) ====================
    
    // 1. THRUST (body frame, forward is -Z)
    float thrust = m_controls.throttle * m_params.maxThrust;
    Vec3 thrustForce = {0, 0, -thrust};  // Forward thrust
    
    // 2. GRAVITY (world frame, always down)
    Vec3 gravityWorld = {0, -m_params.mass * GRAVITY, 0};
    
    // Transform gravity to body frame for proper calculations
    Vec3 gravity = worldToBody(gravityWorld);
    
    // 3. AERODYNAMIC FORCES
    // Dynamic pressure: q = 0.5 * rho * V^2
    float q = 0.5f * AIR_DENSITY * m_state.speed * m_state.speed;
    
    // Lift (perpendicular to velocity, affected by angle of attack)
    // Simplified: more nose-up pitch = more lift
    float angleOfAttack = m_state.pitch + m_controls.elevator * 0.3f;  // radians
    float liftCoeff = m_params.liftCoeff * (1.0f + angleOfAttack * 3.0f);
    liftCoeff = clamp(liftCoeff, -0.5f, 1.5f);  // Realistic limits
    float lift = q * m_params.wingArea * liftCoeff;
    
    // Lift is perpendicular to velocity in body frame
    // Simplified: assume lift is primarily upward in body frame
    Vec3 liftForce = {0, lift, 0};
    
    // Drag (opposes velocity direction)
    float dragCoeff = m_params.dragCoeff * (1.0f + angleOfAttack * angleOfAttack * 5.0f);
    float drag = q * m_params.wingArea * dragCoeff;
    
    // Drag opposes velocity
    Vec3 dragForce = {0, 0, 0};
    if (m_state.speed > 0.1f) {
        Vec3 velDir = {
            m_state.velocity.x / m_state.speed,
            m_state.velocity.y / m_state.speed,
            m_state.velocity.z / m_state.speed
        };
        dragForce = {
            -velDir.x * drag,
            -velDir.y * drag,
            -velDir.z * drag
        };
    }
    
    // Total force (in body frame)
    force = v3_add(thrustForce, v3_add(gravity, v3_add(liftForce, dragForce)));
    
    // ==================== TORQUES (Newton-meters) ====================
    
    // Control surface effectiveness scales with dynamic pressure
    // Increased multiplier for jet trainer responsiveness
    float controlPower = q * 0.03f;  // Increased from 0.01 - more control authority
    controlPower = clamp(controlPower, 5.0f, 150.0f);  // Higher minimum for low-speed control
    
    // Pitch moment (elevator)
    // Positive elevator (nose up) creates negative pitch moment (nose down in our coord system)
    float pitchMoment = -m_controls.elevator * m_params.elevatorPower * controlPower;
    
    // Roll moment (ailerons)
    float rollMoment = m_controls.aileron * m_params.aileronPower * controlPower;
    
    // Yaw moment (rudder)
    float yawMoment = m_controls.rudder * m_params.rudderPower * controlPower;
    
    // Aerodynamic damping (opposes rotation)
    float dampingFactor = q * 0.001f;
    pitchMoment -= m_state.pitchRate * m_params.pitchStability * dampingFactor;
    rollMoment -= m_state.rollRate * m_params.rollStability * dampingFactor;
    yawMoment -= m_state.yawRate * m_params.yawStability * dampingFactor;
    
    torque = {pitchMoment, yawMoment, rollMoment};
}

void FlightDynamics::integrateState(float dt, const Vec3& force, const Vec3& torque) {
    // Moment of inertia for L-39 jet trainer (nimble aircraft)
    // Using very low values for high maneuverability
    float Ixx = m_params.mass * m_params.wingspan * m_params.wingspan * 0.0008f;  // Roll - very responsive
    float Iyy = m_params.mass * 12.0f * 12.0f * 0.0008f;  // Pitch - very responsive
    float Izz = m_params.mass * m_params.wingspan * m_params.wingspan * 0.0010f;  // Yaw - slightly heavier
    
    // Debug: Log inertia values once
    static bool logged = false;
    if (!logged) {
        LOG_INFO("Moments of inertia: Ixx=%.1f Iyy=%.1f Izz=%.1f kg⋅m²", Ixx, Iyy, Izz);
        logged = true;
    }
    
    // Debug: Log torques if any control input
    if (fabs(torque.x) > 0.1f || fabs(torque.y) > 0.1f || fabs(torque.z) > 0.1f) {
        LOG_DEBUG("Torques: pitch=%.2f yaw=%.2f roll=%.2f N⋅m", 
                 torque.x, torque.y, torque.z);
    }
    
    // Use the forces and torques passed in (no recomputation!)
    
    // ==================== ANGULAR INTEGRATION ====================
    
    // Angular acceleration = torque / moment of inertia
    // torque.x = pitch torque, torque.y = yaw torque, torque.z = roll torque
    float pitchAccel = torque.x / Iyy;  // Pitch uses Iyy (fuselage)
    float yawAccel = torque.y / Izz;    // Yaw uses Izz (wingspan)
    float rollAccel = torque.z / Ixx;   // Roll uses Ixx (wingspan)
    
    // Debug: Log accelerations if significant
    if (fabs(pitchAccel) > 0.01f || fabs(yawAccel) > 0.01f || fabs(rollAccel) > 0.01f) {
        LOG_DEBUG("Angular accel: pitch=%.3f yaw=%.3f roll=%.3f rad/s²",
                 pitchAccel, yawAccel, rollAccel);
    }
    
    // Update angular velocities
    m_state.pitchRate += pitchAccel * dt;
    m_state.yawRate += yawAccel * dt;
    m_state.rollRate += rollAccel * dt;
    
    // Debug: Show rates after update
    if (fabs(pitchAccel) > 0.01f || fabs(yawAccel) > 0.01f || fabs(rollAccel) > 0.01f) {
        LOG_DEBUG("Angular rates: pitch=%.3f yaw=%.3f roll=%.3f rad/s",
                 m_state.pitchRate, m_state.yawRate, m_state.rollRate);
    }
    
    // Clamp angular rates to realistic values for L-39 jet trainer
    m_state.pitchRate = clamp(m_state.pitchRate, -4.0f, 4.0f);  // ~230 deg/s (was 115)
    m_state.yawRate = clamp(m_state.yawRate, -3.0f, 3.0f);      // ~172 deg/s (was 86)
    m_state.rollRate = clamp(m_state.rollRate, -6.0f, 6.0f);    // ~344 deg/s (was 172) - fast rolls!
    
    // Update orientation (Euler angles)
    float oldPitch = m_state.pitch;
    float oldYaw = m_state.yaw;
    float oldRoll = m_state.roll;
    
    m_state.pitch += m_state.pitchRate * dt;
    m_state.yaw += m_state.yawRate * dt;
    m_state.roll += m_state.rollRate * dt;
    
    // Debug: Show orientation changes
    if (fabs(m_state.pitch - oldPitch) > 0.001f || 
        fabs(m_state.yaw - oldYaw) > 0.001f || 
        fabs(m_state.roll - oldRoll) > 0.001f) {
        LOG_DEBUG("Orientation: pitch=%.3f° yaw=%.3f° roll=%.3f°",
                 m_state.pitch * 57.2958f, m_state.yaw * 57.2958f, m_state.roll * 57.2958f);
    }
    
    // Normalize angles to -PI..PI
    const float PI = 3.14159265359f;
    while (m_state.pitch > PI) m_state.pitch -= 2.0f * PI;
    while (m_state.pitch < -PI) m_state.pitch += 2.0f * PI;
    while (m_state.yaw > PI) m_state.yaw -= 2.0f * PI;
    while (m_state.yaw < -PI) m_state.yaw += 2.0f * PI;
    while (m_state.roll > PI) m_state.roll -= 2.0f * PI;
    while (m_state.roll < -PI) m_state.roll += 2.0f * PI;
    
    // ==================== LINEAR INTEGRATION ====================
    
    // Transform force from body frame to world frame
    Vec3 forceWorld = bodyToWorld(force);
    
    // Acceleration = Force / Mass
    Vec3 accel = {
        forceWorld.x / m_params.mass,
        forceWorld.y / m_params.mass,
        forceWorld.z / m_params.mass
    };
    
    // Update velocity in world frame
    Vec3 velocityWorld = bodyToWorld(m_state.velocity);
    velocityWorld.x += accel.x * dt;
    velocityWorld.y += accel.y * dt;
    velocityWorld.z += accel.z * dt;
    
    // Transform back to body frame
    m_state.velocity = worldToBody(velocityWorld);
    
    // Update position (in world frame)
    m_state.position.x += velocityWorld.x * dt;
    m_state.position.y += velocityWorld.y * dt;
    m_state.position.z += velocityWorld.z * dt;
    
    // Update speed magnitude
    m_state.speed = v3_length(velocityWorld);
    
    // Minimum flying speed
    if (m_state.speed < 20.0f) {
        m_state.speed = 20.0f;
        // Maintain direction but set minimum speed
        if (v3_length(velocityWorld) > 0.1f) {
            Vec3 dir = v3_norm(velocityWorld);
            velocityWorld = v3_scale(dir, m_state.speed);
            m_state.velocity = worldToBody(velocityWorld);
        }
    }
}

Mat4 FlightDynamics::getTransformMatrix() const {
    // Create transformation matrix: Translation * Yaw * Pitch * Roll
    Mat4 translation = mat4_translate(m_state.position.x, m_state.position.y, m_state.position.z);
    Mat4 rotYaw = mat4_rotate_y(m_state.yaw);
    Mat4 rotPitch = mat4_rotate_x(m_state.pitch);
    Mat4 rotRoll = mat4_rotate_z(m_state.roll);
    
    return mat4_mul(translation, mat4_mul(rotYaw, mat4_mul(rotPitch, rotRoll)));
}

Vec3 FlightDynamics::bodyToWorld(const Vec3& bodyVec) const {
    // Rotate vector from body frame to world frame
    // Apply: Yaw * Pitch * Roll
    Vec3 result = bodyVec;
    
    // Roll rotation
    float cosR = std::cos(m_state.roll);
    float sinR = std::sin(m_state.roll);
    Vec3 afterRoll = {
        result.x * cosR - result.y * sinR,
        result.x * sinR + result.y * cosR,
        result.z
    };
    
    // Pitch rotation
    float cosP = std::cos(m_state.pitch);
    float sinP = std::sin(m_state.pitch);
    Vec3 afterPitch = {
        afterRoll.x,
        afterRoll.y * cosP - afterRoll.z * sinP,
        afterRoll.y * sinP + afterRoll.z * cosP
    };
    
    // Yaw rotation
    float cosY = std::cos(m_state.yaw);
    float sinY = std::sin(m_state.yaw);
    Vec3 afterYaw = {
        afterPitch.x * cosY - afterPitch.z * sinY,
        afterPitch.y,
        afterPitch.x * sinY + afterPitch.z * cosY
    };
    
    return afterYaw;
}

Vec3 FlightDynamics::worldToBody(const Vec3& worldVec) const {
    // Rotate vector from world frame to body frame
    // Apply inverse: Roll^-1 * Pitch^-1 * Yaw^-1
    
    // Yaw inverse
    float cosY = std::cos(-m_state.yaw);
    float sinY = std::sin(-m_state.yaw);
    Vec3 afterYaw = {
        worldVec.x * cosY - worldVec.z * sinY,
        worldVec.y,
        worldVec.x * sinY + worldVec.z * cosY
    };
    
    // Pitch inverse
    float cosP = std::cos(-m_state.pitch);
    float sinP = std::sin(-m_state.pitch);
    Vec3 afterPitch = {
        afterYaw.x,
        afterYaw.y * cosP - afterYaw.z * sinP,
        afterYaw.y * sinP + afterYaw.z * cosP
    };
    
    // Roll inverse
    float cosR = std::cos(-m_state.roll);
    float sinR = std::sin(-m_state.roll);
    Vec3 afterRoll = {
        afterPitch.x * cosR - afterPitch.y * sinR,
        afterPitch.x * sinR + afterPitch.y * cosR,
        afterPitch.z
    };
    
    return afterRoll;
}

void FlightDynamics::printDebugInfo() const {
    LOG_INFO("=== Flight State ===");
    LOG_INFO("Position: (%.1f, %.1f, %.1f) m", 
             m_state.position.x, m_state.position.y, m_state.position.z);
    LOG_INFO("Orientation: pitch=%.1f° yaw=%.1f° roll=%.1f°", 
             m_state.pitch * 57.2958f, m_state.yaw * 57.2958f, m_state.roll * 57.2958f);
    LOG_INFO("Speed: %.1f m/s (%.1f km/h)", m_state.speed, m_state.speed * 3.6f);
    LOG_INFO("Angular rates: pitch=%.2f yaw=%.2f roll=%.2f rad/s",
             m_state.pitchRate, m_state.yawRate, m_state.rollRate);
    LOG_INFO("Controls: elev=%.2f ail=%.2f rud=%.2f thr=%.1f%%",
             m_controls.elevator, m_controls.aileron, m_controls.rudder, m_controls.throttle * 100);
}
