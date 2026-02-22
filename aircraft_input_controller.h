// aircraft_input_controller.h - Input controller for aircraft
#ifndef AIRCRAFT_INPUT_CONTROLLER_H
#define AIRCRAFT_INPUT_CONTROLLER_H

#include "input_controller.h"
#include "flight_dynamics_behavior.h"
#include "entity_registry.h"
#include <GLFW/glfw3.h>

// ==================== Aircraft Input Controller ====================
// Handles stick (elevator/aileron/rudder) and throttle controls
class AircraftInputController : public InputController {
public:
    AircraftInputController(EntityRegistry* registry) 
        : InputController("Aircraft")
        , m_registry(registry)
        , m_upPressed(false)
        , m_downPressed(false)
        , m_leftPressed(false)
        , m_rightPressed(false)
        , m_rudderLeftPressed(false)
        , m_rudderRightPressed(false)
    {}
    
    void onKeyPress(int key) override {
        if (key == GLFW_KEY_UP) m_upPressed = true;
        if (key == GLFW_KEY_DOWN) m_downPressed = true;
        if (key == GLFW_KEY_LEFT) m_leftPressed = true;
        if (key == GLFW_KEY_RIGHT) m_rightPressed = true;
        if (key == GLFW_KEY_DELETE) m_rudderLeftPressed = true;
        if (key == GLFW_KEY_PAGE_DOWN) m_rudderRightPressed = true;
        
        // Throttle controls
        if (m_entity && (key == GLFW_KEY_EQUAL || key == GLFW_KEY_MINUS)) {
            auto* flight = m_registry->getBehavior<FlightDynamicsBehavior>(m_entity->getID());
            if (flight) {
                float& throttle = flight->getControlInputs().throttle;
                if (key == GLFW_KEY_EQUAL) {
                    throttle = std::min(1.0f, throttle + 0.1f);
                    printf("Throttle: %.0f%%\n", throttle * 100);
                } else {
                    throttle = std::max(0.0f, throttle - 0.1f);
                    printf("Throttle: %.0f%%\n", throttle * 100);
                }
            }
        }
    }
    
    void onKeyRelease(int key) override {
        if (key == GLFW_KEY_UP) m_upPressed = false;
        if (key == GLFW_KEY_DOWN) m_downPressed = false;
        if (key == GLFW_KEY_LEFT) m_leftPressed = false;
        if (key == GLFW_KEY_RIGHT) m_rightPressed = false;
        if (key == GLFW_KEY_DELETE) m_rudderLeftPressed = false;
        if (key == GLFW_KEY_PAGE_DOWN) m_rudderRightPressed = false;
    }
    
    void update(float deltaTime) override {
        (void)deltaTime;
        
        if (!m_entity) return;
        
        auto* flight = m_registry->getBehavior<FlightDynamicsBehavior>(m_entity->getID());
        if (!flight) return;
        
        ControlInputs& controls = flight->getControlInputs();
        
        // Pitch and roll - 50% deflection for smooth control
        controls.elevator = 0.0f;
        controls.aileron = 0.0f;
        
        if (m_upPressed) controls.elevator += 0.5f;
        if (m_downPressed) controls.elevator -= 0.5f;
        if (m_leftPressed) controls.aileron += 0.5f;
        if (m_rightPressed) controls.aileron -= 0.5f;
        
        // Rudder
        controls.rudder = 0.0f;
        if (m_rudderLeftPressed) controls.rudder += 0.5f;
        if (m_rudderRightPressed) controls.rudder -= 0.5f;
    }
    
private:
    EntityRegistry* m_registry;
    bool m_upPressed, m_downPressed;
    bool m_leftPressed, m_rightPressed;
    bool m_rudderLeftPressed, m_rudderRightPressed;
};

#endif // AIRCRAFT_INPUT_CONTROLLER_H
