// flight_dynamics_behavior.h - Flight dynamics behavior for aircraft entities
#ifndef FLIGHT_DYNAMICS_BEHAVIOR_H
#define FLIGHT_DYNAMICS_BEHAVIOR_H

#include "behavior.h"
#include "flight_dynamics.h"

// ==================== Flight Dynamics Behavior ====================
// Controls an entity using realistic flight physics
class FlightDynamicsBehavior : public Behavior {
public:
    FlightDynamicsBehavior() 
        : Behavior("FlightDynamics")
        , m_userControlled(false)
    {}
    
    void initialize() override {
        if (m_entity) {
            // Initialize flight dynamics at entity's position
            Vec3 pos = m_entity->getPosition();
            float heading = m_entity->getRotation().y;  // Yaw in radians
            
            // FlightDynamics.initialize() sets up initial state
            m_flightDynamics.initialize(pos, heading);
            
            // IMPORTANT: FlightDynamics reset() sets velocity to {0,0,-50} regardless of heading
            // We need to rotate this velocity to match the actual heading
            AircraftState& state = m_flightDynamics.getState();
            float speed = 50.0f;  // Match the default speed from FlightDynamics
            
            // Velocity in heading direction (-Z when heading=0)
            state.velocity.x = -speed * sinf(heading);
            state.velocity.y = 0.0f;
            state.velocity.z = -speed * cosf(heading);
            state.speed = speed;
            
            // Set initial throttle for stable cruise flight
            m_flightDynamics.getControlInputs().throttle = 0.7f;
            
            printf("DEBUG: Flight initialized at (%.1f, %.1f, %.1f)\n", pos.x, pos.y, pos.z);
            printf("       Heading %.1f deg, velocity (%.1f, %.1f, %.1f)\n",
                   heading * 57.2958f, state.velocity.x, state.velocity.y, state.velocity.z);
        }
    }
    
    void update(float deltaTime) override {
        if (!m_enabled || !m_entity) return;
        
        // Update flight physics
        m_flightDynamics.update(deltaTime);
        
        // Apply flight state to entity
        const AircraftState& state = m_flightDynamics.getState();
        m_entity->setPosition(state.position);
        m_entity->setRotation({state.pitch, state.yaw, state.roll});
        m_entity->setVelocity(state.velocity);
        m_entity->setAngularVelocity({state.pitchRate, state.yawRate, state.rollRate});
    }
    
    // Control interface
    void setControlInputs(const ControlInputs& inputs) {
        m_flightDynamics.setControlInputs(inputs);
    }
    
    ControlInputs& getControlInputs() {
        return m_flightDynamics.getControlInputs();
    }
    
    const AircraftState& getState() const {
        return m_flightDynamics.getState();
    }
    
    void setUserControlled(bool controlled) {
        m_userControlled = controlled;
    }
    
    bool isUserControlled() const {
        return m_userControlled;
    }
    
    void reset() {
        if (m_entity) {
            Vec3 pos = m_entity->getPosition();
            float heading = m_entity->getRotation().y;
            m_flightDynamics.initialize(pos, heading);
        }
    }
    
    FlightDynamics& getFlightDynamics() { return m_flightDynamics; }
    
private:
    FlightDynamics m_flightDynamics;
    bool m_userControlled;
};

#endif // FLIGHT_DYNAMICS_BEHAVIOR_H
