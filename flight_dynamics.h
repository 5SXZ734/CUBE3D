// flight_dynamics.h - Aircraft flight dynamics and physics simulation
#ifndef FLIGHT_DYNAMICS_H
#define FLIGHT_DYNAMICS_H

#include "math_utils.h"

// ==================== Aircraft State ====================

struct AircraftState {
    // Position (world space)
    Vec3 position;          // meters
    
    // Orientation (Euler angles in radians)
    float pitch;            // rotation around X axis (nose up/down)
    float yaw;              // rotation around Y axis (nose left/right)
    float roll;             // rotation around Z axis (wing left/right)
    
    // Linear velocity (body frame)
    Vec3 velocity;          // m/s
    float speed;            // total speed magnitude m/s
    
    // Angular velocity (body frame, radians/sec)
    float pitchRate;        // rad/s
    float yawRate;          // rad/s
    float rollRate;         // rad/s
    
    // Acceleration (for integration)
    Vec3 acceleration;      // m/s²
    Vec3 angularAcceleration; // rad/s²
    
    AircraftState() 
        : position({0, 0, 0})
        , pitch(0), yaw(0), roll(0)
        , velocity({0, 0, 0}), speed(0)
        , pitchRate(0), yawRate(0), rollRate(0)
        , acceleration({0, 0, 0})
        , angularAcceleration({0, 0, 0})
    {}
};

// ==================== Control Inputs ====================

struct ControlInputs {
    float elevator;         // pitch control: -1 (nose down) to +1 (nose up)
    float aileron;          // roll control: -1 (left) to +1 (right)
    float rudder;           // yaw control: -1 (left) to +1 (right)
    float throttle;         // engine power: 0 (idle) to 1 (full)
    
    ControlInputs()
        : elevator(0), aileron(0), rudder(0), throttle(0.5f)
    {}
    
    void reset() {
        elevator = 0;
        aileron = 0;
        rudder = 0;
        throttle = 0.5f;
    }
};

// ==================== Aircraft Parameters ====================

struct AircraftParams {
    // Mass properties
    float mass;             // kg
    float wingArea;         // m²
    float wingspan;         // m
    
    // Aerodynamic coefficients
    float liftCoeff;        // CL
    float dragCoeff;        // CD
    float sideForceCoeff;   // CY
    
    // Control effectiveness
    float elevatorPower;    // pitch authority
    float aileronPower;     // roll authority
    float rudderPower;      // yaw authority
    
    // Engine
    float maxThrust;        // Newtons
    
    // Stability derivatives (simplified)
    float pitchStability;   // damping
    float rollStability;    // damping
    float yawStability;     // damping
    
    // Default L-39 Albatros parameters (jet trainer)
    AircraftParams()
        : mass(4700.0f)                 // 4.7 tons empty
        , wingArea(18.8f)               // m²
        , wingspan(9.46f)               // m
        , liftCoeff(0.5f)
        , dragCoeff(0.025f)
        , sideForceCoeff(0.0f)
        , elevatorPower(2.0f)
        , aileronPower(3.0f)
        , rudderPower(1.5f)
        , maxThrust(16870.0f)           // 1720 kgf
        , pitchStability(0.8f)
        , rollStability(0.9f)
        , yawStability(0.7f)
    {}
};

// ==================== Flight Dynamics System ====================

class FlightDynamics {
public:
    FlightDynamics();
    ~FlightDynamics();
    
    // Initialize with starting position and orientation
    void initialize(Vec3 position, float heading);
    
    // Update physics simulation
    void update(float deltaTime);
    
    // Control input
    void setControlInputs(const ControlInputs& inputs);
    ControlInputs& getControlInputs() { return m_controls; }
    
    // State access
    const AircraftState& getState() const { return m_state; }
    AircraftState& getState() { return m_state; }
    
    // Parameters
    void setParameters(const AircraftParams& params) { m_params = params; }
    const AircraftParams& getParameters() const { return m_params; }
    
    // Reset to initial state
    void reset();
    
    // Get transformation matrix for rendering
    Mat4 getTransformMatrix() const;
    
    // Debug info
    void printDebugInfo() const;
    
private:
    // Physics calculations
    void computeForces(Vec3& force, Vec3& torque);
    void integrateState(float dt);
    
    // Coordinate transforms
    Vec3 bodyToWorld(const Vec3& bodyVec) const;
    Vec3 worldToBody(const Vec3& worldVec) const;
    
    AircraftState m_state;
    AircraftParams m_params;
    ControlInputs m_controls;
    
    // Initial state for reset
    Vec3 m_initialPosition;
    float m_initialHeading;
    
    // Constants
    static constexpr float GRAVITY = 9.81f;        // m/s²
    static constexpr float AIR_DENSITY = 1.225f;   // kg/m³ at sea level
};

#endif // FLIGHT_DYNAMICS_H
