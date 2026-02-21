// osd.h - On-Screen Display for flight information
#ifndef OSD_H
#define OSD_H

#include "flight_dynamics.h"
#include <string>
#include <vector>
#include <cstdio>

// ==================== OSD Display System ====================

class FlightOSD {
public:
    FlightOSD() 
        : m_enabled(false)
        , m_detailedMode(false)
    {}
    
    // Toggle OSD on/off
    void toggle() { m_enabled = !m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
    
    // Toggle between simple and detailed mode
    void toggleDetailedMode() { m_detailedMode = !m_detailedMode; }
    bool isDetailedMode() const { return m_detailedMode; }
    
    // Generate OSD text lines
    std::vector<std::string> generateOSDLines(const AircraftState& state, const ControlInputs& controls) {
        std::vector<std::string> lines;
        
        if (!m_enabled) return lines;
        
        char buffer[256];
        
        // Convert speed to km/h and knots
        float speedKmh = state.speed * 3.6f;
        float speedKnots = state.speed * 1.94384f;
        
        // Convert altitude to feet
        float altitudeFeet = state.position.y * 3.28084f;
        
        // Convert angles to degrees
        float pitchDeg = state.pitch * 57.2958f;
        float rollDeg = state.roll * 57.2958f;
        float yawDeg = state.yaw * 57.2958f;
        
        // Compute vertical speed (simplified - would need history for accuracy)
        float verticalSpeed = state.velocity.y;
        float verticalSpeedFpm = verticalSpeed * 60.0f * 3.28084f; // feet per minute
        
        if (m_detailedMode) {
            // ==================== DETAILED MODE ====================
            lines.push_back("=== FLIGHT DATA ===");
            
            // Airspeed
            snprintf(buffer, sizeof(buffer), "AIRSPEED: %3.0f kt (%3.0f km/h)", speedKnots, speedKmh);
            lines.push_back(buffer);
            
            // Altitude
            snprintf(buffer, sizeof(buffer), "ALTITUDE: %4.0f ft (%4.0f m)", altitudeFeet, state.position.y);
            lines.push_back(buffer);
            
            // Vertical speed
            snprintf(buffer, sizeof(buffer), "V/S:      %+5.0f fpm", verticalSpeedFpm);
            lines.push_back(buffer);
            
            // Attitude
            snprintf(buffer, sizeof(buffer), "PITCH:    %+6.1f deg", pitchDeg);
            lines.push_back(buffer);
            
            snprintf(buffer, sizeof(buffer), "ROLL:     %+6.1f deg", rollDeg);
            lines.push_back(buffer);
            
            snprintf(buffer, sizeof(buffer), "HEADING:  %6.1f deg", normalizeHeading(yawDeg));
            lines.push_back(buffer);
            
            // Throttle
            snprintf(buffer, sizeof(buffer), "THROTTLE: %3.0f%%", controls.throttle * 100.0f);
            lines.push_back(buffer);
            
            // Angular rates
            lines.push_back("--- RATES ---");
            snprintf(buffer, sizeof(buffer), "PITCH RATE: %+5.2f deg/s", state.pitchRate * 57.2958f);
            lines.push_back(buffer);
            
            snprintf(buffer, sizeof(buffer), "ROLL RATE:  %+5.2f deg/s", state.rollRate * 57.2958f);
            lines.push_back(buffer);
            
            snprintf(buffer, sizeof(buffer), "YAW RATE:   %+5.2f deg/s", state.yawRate * 57.2958f);
            lines.push_back(buffer);
            
            // Controls
            lines.push_back("--- CONTROLS ---");
            snprintf(buffer, sizeof(buffer), "ELEVATOR: %+5.2f", controls.elevator);
            lines.push_back(buffer);
            
            snprintf(buffer, sizeof(buffer), "AILERON:  %+5.2f", controls.aileron);
            lines.push_back(buffer);
            
            snprintf(buffer, sizeof(buffer), "RUDDER:   %+5.2f", controls.rudder);
            lines.push_back(buffer);
            
        } else {
            // ==================== SIMPLE MODE ====================
            
            // Compact HUD-style display
            snprintf(buffer, sizeof(buffer), "SPD %3.0fkt  ALT %4.0fft  THR %3.0f%%", 
                     speedKnots, altitudeFeet, controls.throttle * 100.0f);
            lines.push_back(buffer);
            
            snprintf(buffer, sizeof(buffer), "HDG %3.0f  PITCH %+4.0f  ROLL %+4.0f", 
                     normalizeHeading(yawDeg), pitchDeg, rollDeg);
            lines.push_back(buffer);
            
            if (verticalSpeedFpm > 100.0f) {
                snprintf(buffer, sizeof(buffer), "CLIMBING %+.0f fpm", verticalSpeedFpm);
                lines.push_back(buffer);
            } else if (verticalSpeedFpm < -100.0f) {
                snprintf(buffer, sizeof(buffer), "DESCENDING %+.0f fpm", verticalSpeedFpm);
                lines.push_back(buffer);
            }
        }
        
        return lines;
    }
    
private:
    bool m_enabled;
    bool m_detailedMode;
    
    // Normalize heading to 0-360 range
    float normalizeHeading(float heading) const {
        while (heading < 0.0f) heading += 360.0f;
        while (heading >= 360.0f) heading -= 360.0f;
        return heading;
    }
};

#endif // OSD_H
