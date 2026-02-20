# L-39 Flight Dynamics Integration Plan

## Summary
Integrating legacy L-39 Albatros flight simulator code into our modern OpenGL/D3D viewer.

## Legacy Code Analysis

### Core Components Present:
1. **Control Surface Physics** (CONTROL.cpp)
   - Aerodynamic forces on control surfaces
   - Control loading feedback
   - Trim system

2. **6DOF Integration** (Dynamics.cpp)  
   - Uses external ACDyn_t DLL for physics
   - We need to replace this with self-contained implementation

3. **Support Libraries:**
   - Vector/Matrix math (Vectors.cpp/h)
   - Graph interpolation (gphfast.cpp/h)
   - Inertial smoothing (Inerval.cpp/h)
   - World coordinate system (wcs.cpp/h)

## Integration Strategy

### Phase 1: Core Math & Utilities (READY TO IMPLEMENT)
Extract and adapt:
- Vector/matrix functions we need
- Simple aerodynamic curve system
- Basic inertial lag for smooth controls

### Phase 2: Control Surface Model (ADAPT FROM CONTROL.cpp)
Port the control surface aerodynamics:
- CtrlSurf_t::Think() - main aero calculation
- Control loading feedback
- Simplified for our needs

### Phase 3: 6DOF Physics (NEW IMPLEMENTATION)
Implement rigid body dynamics:
- Forces: Thrust, Lift, Drag, Weight
- Moments: Control surfaces, gyroscopic effects
- Euler integration with proper coordinate transforms

### Phase 4: Aircraft Systems (SIMPLIFIED)
Minimal subsystems:
- Engine thrust model (simplified from AI_25TL)
- Gear/flap positions
- Basic fuel consumption

## Key Simplifications

1. **No DLL dependency** - All physics self-contained
2. **Simplified aerodynamics** - Essential forces only
3. **No complex subsystems** - Engine, fuel, oil are basic models
4. **Direct control** - No hydraulics simulation
5. **Standard atmosphere** - ISA model, no weather

## File Structure

```
flight_dynamics.h       - Interface (EXISTING)
flight_dynamics.cpp     - Implementation (UPDATE)
flight_aero.h/cpp       - Control surfaces & aerodynamics (NEW)
flight_utils.h/cpp      - Math utilities from legacy code (NEW)
```

## Next Steps

1. Create flight_utils with essential legacy functions
2. Create flight_aero with control surface model
3. Update flight_dynamics.cpp with 6DOF physics
4. Test with arrow key controls

Ready to proceed!
