# Migration Guide: Old System → ECS System

## What Changed

### Removed from CubeApp:
- ❌ `m_flightMode` - No longer needed, flight is always active if entity has FlightDynamicsBehavior
- ❌ `m_flightDynamics` - Now part of FlightDynamicsBehavior attached to entity
- ❌ `m_objectPosition/Rotation/Scale` - Now stored in Entity
- ❌ `m_scene` - Replaced with EntityRegistry
- ❌ `m_model` - Now in ModelRegistry
- ❌ `m_chaseCameraPos/Target/Distance/Height/Smoothness` - Now in ChaseCameraBehavior
- ❌ `m_cameraType` - Determined by behaviors
- ❌ `m_useSceneMode` - Always in "scene mode" now
- ❌ FPS camera controls (W/A/S/D) - Not needed for flight sim

### Added to CubeApp:
- ✅ `ModelRegistry m_modelRegistry` - Manages all 3D models
- ✅ `EntityRegistry m_entityRegistry` - Manages all entities and behaviors
- ✅ `SceneEnvironment m_environment` - Consolidated environment settings
- ✅ Simplified camera state (position/target only)

## Step-by-Step Migration

### 1. Replace app.h

**Before:**
```cpp
bool m_flightMode;
FlightDynamics m_flightDynamics;
Vec3 m_objectPosition;
Scene m_scene;
```

**After:**
```cpp
ModelRegistry m_modelRegistry;
EntityRegistry m_entityRegistry;
SceneEnvironment m_environment;
```

### 2. Update Initialization

**Before:**
```cpp
bool initialize(RendererAPI api, const char* modelPath) {
    // Load single model
    if (!loadModel(modelPath)) return false;
    
    // Initialize flight dynamics manually
    m_flightDynamics.initialize(startPos, startHeading);
}
```

**After:**
```cpp
bool initialize(RendererAPI api, const char* sceneFile) {
    // Load scene with entities
    SceneConfigV2 scene;
    SceneLoaderV2::loadScene(sceneFile, scene);
    SceneLoaderV2::applyScene(scene, m_modelRegistry, m_entityRegistry);
    
    // Entities and behaviors auto-initialized from scene!
}
```

### 3. Update Game Loop

**Before:**
```cpp
void update(float deltaTime) {
    if (m_flightMode) {
        // Apply controls
        m_flightDynamics.getControlInputs().elevator = ...;
        
        // Update physics
        m_flightDynamics.update(deltaTime);
        
        // Get state
        m_objectPosition = m_flightDynamics.getState().position;
        
        // Update chase camera manually
        updateChaseCamera(deltaTime);
    }
}
```

**After:**
```cpp
void update(float deltaTime) {
    // Update ALL entities and behaviors in one call
    m_entityRegistry.update(deltaTime);
    
    // Get player entity
    Entity* player = getPlayerEntity();
    auto* flight = m_entityRegistry.getBehavior<FlightDynamicsBehavior>(player->getID());
    
    // Apply controls
    flight->getControlInputs().elevator = ...;
    
    // Camera updated automatically by ChaseCameraBehavior!
    auto* camera = m_entityRegistry.getBehavior<ChaseCameraBehavior>(player->getID());
    m_cameraPos = camera->getCameraPosition();
}
```

### 4. Update Rendering

**Before:**
```cpp
void render() {
    // Render single object
    Mat4 world = createTransformMatrix(
        m_objectPosition.x, 
        m_objectPosition.y, 
        m_objectPosition.z,
        m_objectRotation.y,
        1.0f
    );
    
    renderModel(&m_model, world);
}
```

**After:**
```cpp
void render() {
    // Render ALL entities
    const auto& entities = m_entityRegistry.getAllEntities();
    for (const auto& [id, entity] : entities) {
        if (entity->isVisible()) {
            Mat4 world = entity->getTransformMatrix();
            renderEntity(entity, viewProj);
        }
    }
}
```

### 5. Update Input Handling

**Before:**
```cpp
void onKey(int key, ...) {
    // Toggle flight mode
    if (key == GLFW_KEY_F) {
        m_flightMode = !m_flightMode;
        if (m_flightMode) {
            m_flightDynamics.initialize(...);
        }
    }
    
    // Apply controls only if flight mode
    if (m_flightMode) {
        if (key == GLFW_KEY_EQUAL) {
            m_flightDynamics.getControlInputs().throttle += 0.1f;
        }
    }
}
```

**After:**
```cpp
void onKey(int key, ...) {
    // No flight mode toggle needed!
    
    // Get player entity
    Entity* player = getPlayerEntity();
    auto* flight = m_entityRegistry.getBehavior<FlightDynamicsBehavior>(player->getID());
    
    // Apply controls directly
    if (key == GLFW_KEY_EQUAL) {
        flight->getControlInputs().throttle += 0.1f;
    }
}
```

## Scene File Changes

**Before (scene_flight.json):**
```json
{
  "objects": [
    {
      "name": "PlayerAircraft",
      "model": "models/L-39.X",     // ← File path
      "position": [0, 50, 90]
    }
  ]
}
```

**After (scene_flight_v2.json):**
```json
{
  "models": {
    "L-39": "models/L-39.X"         // ← Model registry
  },
  "entities": [
    {
      "name": "PlayerAircraft",
      "model": "L-39",                // ← Key reference
      "position": [0, 50, 90],
      "behaviors": ["FlightDynamics", "ChaseCamera"],  // ← Behavior attachment!
      "behaviorParams": {
        "userControlled": true
      }
    }
  ]
}
```

## Implementation Checklist

### Files to Add:
- [x] entity.h / entity.cpp
- [x] behavior.h
- [x] flight_dynamics_behavior.h
- [x] chase_camera_behavior.h
- [x] entity_registry.h
- [x] model_registry.h
- [x] scene_loader_v2.h

### Files to Replace:
- [ ] app.h → app_v2.h
- [ ] app.cpp → Merge with app_v2_implementation.cpp
- [ ] main.cpp → main_v2.cpp

### Files to Update:
- [ ] CMakeLists.txt - Add new source files
- [ ] Visual Studio project - Add new files

### Testing:
1. Compile with new files
2. Run: `./app --scene scene_flight_v2.json`
3. Should see L-39 flying immediately
4. Controls should work (arrows, +/-, O, I)
5. Camera should follow aircraft

## Key Benefits

### Before (Old System):
- Hard-coded flight mode toggle
- Manual state synchronization
- Single entity only
- Scattered logic

### After (ECS System):
- Always in flight (if entity has behavior)
- Automatic state sync
- Multiple entities supported
- Clean, modular code

## Troubleshooting

**Issue: Nothing appears**
- Check console: "Scene loaded successfully"
- Verify: scene_flight_v2.json in working directory
- Verify: models/L-39.X exists

**Issue: Controls don't work**
- Check: Entity has FlightDynamicsBehavior
- Check: userControlled = true in scene
- Check: getPlayerEntity() returns valid entity

**Issue: Camera doesn't follow**
- Check: Entity has ChaseCameraBehavior
- Check: Behavior is attached and initialized
- Check: Camera position/target being updated

**Issue: Compilation errors**
- Add all new .h files to project
- Add entity.cpp to compilation
- Link against existing libraries (nlohmann/json, GLFW, etc.)

## Result

After migration, starting the application will:
1. ✅ Load scene_flight_v2.json
2. ✅ Create L-39 entity with model
3. ✅ Attach FlightDynamicsBehavior (user-controlled)
4. ✅ Attach ChaseCameraBehavior
5. ✅ Start flying immediately - no mode toggle!
6. ✅ Arrow keys control aircraft
7. ✅ Camera follows automatically
8. ✅ OSD shows flight data

No more `m_flightMode` toggle - just pure flight simulation from the start! 🚀
