# Advanced ECS Architecture - Multi-Camera & Input System

## Overview

The game engine now features:
1. **Cameras as Entities** - Cameras are entities with behaviors
2. **Multiple Active Cameras** - Switch between different viewpoints
3. **Pluggable Input Controllers** - Different vehicle types have different controls
4. **Current Entity System** - Track which entity receives user input
5. **Runtime Entity Switching** - Change controlled entity on the fly
6. **Scene Reloading** - Reset to initial state

## Core Components

### 1. CameraEntity (camera_entity.h)
Cameras are now full entities that can:
- Have position and rotation like any entity
- Attach behaviors (chase, orbit, stationary)
- Be activated/deactivated
- Have FOV, near/far planes

### 2. InputController (input_controller.h)
Base class for vehicle-specific input:
- **AircraftInputController** - Stick + rudder + throttle
- **CarInputController** - Accelerate/brake + steering (future)
- **FPSInputController** - WASD + mouse (future)

### 3. SceneManager (scene_manager.h)
Central coordinator:
- Manages all cameras
- Tracks controllable entities
- Switches between cameras (C key)
- Switches between controllables (Tab key)
- Handles scene reloading (R key)

### 4. Camera Behaviors (camera_behaviors.h)
- **ChaseCameraTargetBehavior** - Follows target entity
- **OrbitCameraTargetBehavior** - Orbits around target
- **StationaryCameraBehavior** - Fixed position (future)

## Scene File Format

### Multiple Cameras Example:

```json
{
  "cameras": [
    {
      "name": "ChaseCamera",
      "type": "chase",
      "targetEntity": "PlayerAircraft",
      "fov": 75,
      "behaviorParams": {
        "distance": 25.0,
        "height": 8.0
      }
    },
    {
      "name": "OrbitCamera",
      "type": "orbit",
      "targetEntity": "PlayerAircraft",
      "behaviorParams": {
        "distance": 40.0,
        "autoRotate": true
      }
    },
    {
      "name": "TowerCamera",
      "type": "stationary",
      "position": [0, 30, -200],
      "target": [0, 50, 0]
    }
  ]
}
```

### Controllable Entities:

```json
{
  "entities": [
    {
      "name": "PlayerAircraft",
      "model": "L-39",
      "controllable": true,
      "controllerType": "aircraft",
      "behaviors": ["FlightDynamics"]
    },
    {
      "name": "PlayerCar",
      "model": "SportsCar",
      "controllable": true,
      "controllerType": "car",
      "behaviors": ["CarPhysics"]
    }
  ]
}
```

## Controls

### Global Controls:
- **C** - Cycle through cameras
- **Tab** - Switch to next controllable entity
- **Shift+Tab** - Switch to previous controllable entity
- **R** - Reload scene (reset to initial state)
- **ESC** - Quit

### Aircraft Controls:
- **↑/↓** - Pitch up/down
- **←/→** - Roll left/right
- **Delete/PageDown** - Rudder left/right
- **+/-** - Increase/decrease throttle

### Car Controls (future):
- **↑/↓** - Accelerate/brake
- **←/→** - Steer left/right
- **Space** - Handbrake

## Implementation Notes

### Camera Switching Flow:
1. User presses **C**
2. SceneManager deactivates current camera
3. SceneManager activates next camera
4. Rendering uses new active camera's view

### Entity Switching Flow:
1. User presses **Tab**
2. SceneManager detaches input controller from current entity
3. SceneManager switches to next controllable entity
4. Input controller reattaches to new entity
5. All camera behaviors update their targets

### Scene Reloading Flow:
1. User presses **R**
2. Clear all entities and behaviors
3. Reload scene file from disk
4. Recreate all entities, cameras, behaviors
5. Reset to initial camera and controllable entity

## Code Example

### Creating Cameras in Scene Loader:

```cpp
// Create camera entity
CameraEntity* camera = new CameraEntity(id, "ChaseCamera");
camera->setPosition(camPos);
camera->setFOV(75.0f);
entityRegistry.addEntity(camera);

// Attach chase behavior
auto* behavior = new ChaseCameraTargetBehavior(&entityRegistry, targetID);
behavior->setDistance(25.0f);
entityRegistry.addBehavior(camera->getID(), behavior);

// Register with scene manager
sceneManager.addCamera(camera->getID());
```

### Using Input Controllers:

```cpp
// Create input controller for aircraft
auto* controller = new AircraftInputController(&entityRegistry);

// Attach to scene manager
sceneManager.setInputController(controller);

// In main loop
sceneManager.getInputController()->update(deltaTime);
```

## Benefits

### 1. Flexibility
- Easy to add new camera types
- Easy to add new vehicle types
- Easy to create cinematic cameras

### 2. Modularity
- Input handling separated from entity logic
- Camera logic separated from rendering
- Clean separation of concerns

### 3. Designer-Friendly
- All configuration in JSON
- No code changes needed for new cameras
- No code changes for camera switching

### 4. Debugging
- Switch cameras to see from different angles
- Switch vehicles to test different physics
- Reset scene to test reproducibility

## Future Extensions

### Possible Additions:
1. **CinematicCameraBehavior** - Scripted camera paths
2. **FirstPersonCameraBehavior** - Attach to entity with offset
3. **SpectatorCameraBehavior** - Free-flying camera
4. **CarInputController** - Vehicle driving controls
5. **CharacterInputController** - Third-person character
6. **Camera transitions** - Smooth blending between cameras
7. **Camera shake** - Dynamic effects
8. **Save/load system** - Save entity states

## Migration from Old System

### Old Way:
```cpp
// Hard-coded camera
if (m_flightMode) {
    updateChaseCamera(deltaTime);
}
```

### New Way:
```cpp
// Automatic camera update
sceneManager.getActiveCamera()->update(deltaTime);
```

The new system provides a professional, extensible architecture for complex simulations with multiple viewpoints and vehicle types!
