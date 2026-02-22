# Entity-Component-System Architecture Documentation

## Overview

The game engine has been re-architected to use an Entity-Component-System (ECS) pattern with behavior-based components. This provides a flexible, data-driven approach to game object management.

## Core Components

### 1. Entity System

**entity.h / entity.cpp**
- Base class for all game objects
- Stores transform (position, rotation, scale)
- Stores physics state (velocity, angular velocity)
- Reference to visual model
- Base update() method (can be overridden)

```cpp
Entity* entity = entityRegistry.createEntity("MyPlane");
entity->setPosition({0, 50, 0});
entity->setRotation({0, 0, 0});
entity->setModel(modelRegistry.getModel("L-39"));
```

### 2. Behavior System

**behavior.h**
- Base class for all entity behaviors
- Behaviors control how entities act
- Lifecycle: attach() → initialize() → update() → shutdown() → detach()
- Can be enabled/disabled at runtime

**Built-in Behaviors:**

**FlightDynamicsBehavior** (flight_dynamics_behavior.h)
- Controls entity using realistic flight physics
- Wraps existing FlightDynamics class
- Syncs flight state with entity transform
- Supports user control via control inputs

**ChaseCameraBehavior** (chase_camera_behavior.h)
- Camera that smoothly follows an entity
- Configurable distance, height, smoothness
- Perfect for chase cam or third-person view

### 3. Registry System

**EntityRegistry** (entity_registry.h)
- Central manager for all entities
- Creates/destroys entities with unique IDs
- Attaches/detaches behaviors to entities
- Updates all active entities and behaviors
- Type-safe behavior lookup with templates

**ModelRegistry** (model_registry.h)
- Manages 3D models with key-based access
- Register models: `registerModel("L-39", "models/L-39.X")`
- Retrieve models: `getModel("L-39")`
- Prevents duplicate loading
- Cleaner than file paths everywhere

### 4. Scene Loading V2

**SceneLoaderV2** (scene_loader_v2.h)
- New JSON scene format
- Declarative entity definition
- Model registry with keys
- Behavior assignment in JSON
- Behavior parameters in JSON

## New Scene File Format

### Example: scene_flight_v2.json

```json
{
  "name": "Flight Simulation - Entity System",
  
  "models": {
    "L-39": "models/L-39.X",
    "Cessna172": "models/Cessna172.X"
  },
  
  "entities": [
    {
      "name": "PlayerAircraft",
      "model": "L-39",
      "position": [0, 50, 90],
      "rotation": [0, 0, 0],
      "behaviors": ["FlightDynamics", "ChaseCamera"],
      "behaviorParams": {
        "userControlled": true,
        "cameraDistance": 25.0,
        "cameraHeight": 8.0
      }
    }
  ]
}
```

## Usage Examples

### Creating Entities Programmatically

```cpp
// Create registries
ModelRegistry modelRegistry;
EntityRegistry entityRegistry;

// Register models
modelRegistry.registerModel("L-39", "models/L-39.X");

// Create entity
Entity* plane = entityRegistry.createEntity("MyPlane");
plane->setPosition({0, 50, 0});
plane->setModel(modelRegistry.getModel("L-39"));

// Add behaviors
auto* flight = entityRegistry.addBehavior<FlightDynamicsBehavior>(plane->getID());
flight->setUserControlled(true);

auto* camera = entityRegistry.addBehavior<ChaseCameraBehavior>(plane->getID());
camera->setDistance(30.0f);

// Update loop
while (running) {
    entityRegistry.update(deltaTime);
    
    // Get camera position for rendering
    Vec3 camPos = camera->getCameraPosition();
    Vec3 camTarget = camera->getCameraTarget();
}
```

### Loading from Scene File

```cpp
ModelRegistry modelRegistry;
EntityRegistry entityRegistry;

SceneConfigV2 scene;
if (SceneLoaderV2::loadScene("scene_flight_v2.json", scene)) {
    SceneLoaderV2::applyScene(scene, modelRegistry, entityRegistry);
}

// Update loop
while (running) {
    entityRegistry.update(deltaTime);
    
    // Find entity by name
    Entity* plane = entityRegistry.findEntityByName("PlayerAircraft");
    
    // Get behavior
    auto* flight = entityRegistry.getBehavior<FlightDynamicsBehavior>(plane->getID());
    if (flight && flight->isUserControlled()) {
        // Apply user input
        flight->getControlInputs().throttle = currentThrottle;
    }
}
```

### Creating Custom Behaviors

```cpp
// Define custom behavior
class PatrolBehavior : public Behavior {
public:
    PatrolBehavior() : Behavior("Patrol") {}
    
    void initialize() override {
        // Setup patrol points
    }
    
    void update(float deltaTime) override {
        if (!m_enabled || !m_entity) return;
        
        // Move entity along patrol path
        // ...
        m_entity->setPosition(newPos);
    }
};

// Use it
auto* patrol = entityRegistry.addBehavior<PatrolBehavior>(entityID);
```

## Architecture Benefits

### ✅ Separation of Concerns
- Entities = data (what it is)
- Behaviors = logic (what it does)
- Models = visuals (how it looks)

### ✅ Reusability
- Same behavior works on different entities
- Behaviors are independent and composable

### ✅ Data-Driven
- Scenes defined in JSON
- No hard-coded entity creation
- Easy to add new entity types

### ✅ Flexibility
- Add/remove behaviors at runtime
- Easy to extend with new behavior types
- Multiple behaviors per entity

### ✅ Clean Code
- No God classes
- Single responsibility principle
- Easy to test individual behaviors

## Migration Guide

### Old Way (Hard-coded)

```cpp
// In app.cpp
Vec3 m_objectPosition;
FlightDynamics m_flightDynamics;
bool m_flightMode;

// Scattered logic
if (m_flightMode) {
    m_flightDynamics.update(deltaTime);
    m_objectPosition = m_flightDynamics.getState().position;
}
```

### New Way (ECS)

```cpp
// In app.cpp
ModelRegistry m_modelRegistry;
EntityRegistry m_entityRegistry;

// Clean update
m_entityRegistry.update(deltaTime);

// Get entity when needed
Entity* plane = m_entityRegistry.findEntityByName("PlayerAircraft");
auto* flight = m_entityRegistry.getBehavior<FlightDynamicsBehavior>(plane->getID());
```

## File Structure

```
New Files:
├── entity.h                          // Base entity class
├── entity.cpp                        // Entity implementation
├── behavior.h                        // Base behavior class
├── flight_dynamics_behavior.h        // Flight behavior
├── chase_camera_behavior.h           // Chase camera behavior
├── entity_registry.h                 // Entity manager
├── model_registry.h                  // Model manager
├── scene_loader_v2.h                 // New scene loader
└── scene_flight_v2.json              // Example scene

Existing Files (still used):
├── flight_dynamics.h/cpp             // Wrapped by FlightDynamicsBehavior
├── model.h/cpp                       // Used by ModelRegistry
└── scene_loader.h/cpp                // Old format (backward compatible)
```

## Next Steps

1. **Add to app.cpp**: Integrate EntityRegistry and ModelRegistry
2. **Update rendering**: Iterate entities for rendering instead of hard-coded objects
3. **Migrate controls**: Route input to user-controlled behaviors
4. **Create more behaviors**: CarBehavior, BirdBehavior, etc.
5. **Add scripting**: Lua/Python integration for behavior scripting

## Example: Multi-Entity Scene

```json
{
  "models": {
    "L-39": "models/L-39.X",
    "F-16": "models/F-16.X",
    "Tower": "models/Tower.X"
  },
  
  "entities": [
    {
      "name": "PlayerJet",
      "model": "L-39",
      "behaviors": ["FlightDynamics", "ChaseCamera"],
      "behaviorParams": { "userControlled": true }
    },
    {
      "name": "AIJet1",
      "model": "F-16",
      "position": [100, 60, 0],
      "behaviors": ["FlightDynamics", "PatrolPath"],
      "behaviorParams": { "userControlled": false }
    },
    {
      "name": "ControlTower",
      "model": "Tower",
      "position": [0, 0, -200],
      "behaviors": []
    }
  ]
}
```

## Benefits Over Old System

| Old System | New System |
|------------|------------|
| Hard-coded entities | JSON-defined entities |
| Scattered logic | Behavior components |
| File paths in code | Model registry keys |
| One aircraft only | Multiple entities |
| Difficult to extend | Easy to add behaviors |
| Tight coupling | Loose coupling |

This architecture sets the foundation for a professional game engine!
