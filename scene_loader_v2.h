// scene_loader_v2.h - New entity-based scene loader
#ifndef SCENE_LOADER_V2_H
#define SCENE_LOADER_V2_H

#include "entity_registry.h"
#include "model_registry.h"
#include "flight_dynamics_behavior.h"
#include "chase_camera_behavior.h"
#include "orbit_camera_behavior.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <fstream>
#include <map>

using json = nlohmann::json;

// ==================== Scene Configuration ====================
struct SceneConfigV2 {
    std::string name;
    
    // Model registrations (key -> filepath)
    std::map<std::string, std::string> models;
    
    // Camera settings
    std::string cameraType;  // "fps", "orbit", "chase"
    Vec3 cameraPosition;
    Vec3 cameraTarget;
    float cameraFOV;
    
    // Entities
    struct EntityConfig {
        std::string name;
        std::string modelKey;  // Key into model registry (e.g., "L-39")
        Vec3 position;
        Vec3 rotation;
        Vec3 scale;
        bool visible;
        
        // Behaviors to attach
        std::vector<std::string> behaviors;  // e.g., ["FlightDynamics", "ChaseCamera"]
        
        // Behavior parameters (optional)
        json behaviorParams;
    };
    std::vector<EntityConfig> entities;
    
    // Lights
    struct LightConfig {
        std::string type;
        Vec3 direction;
        Vec3 color;
        float intensity;
    };
    std::vector<LightConfig> lights;
    
    // Ground
    struct GroundConfig {
        bool enabled;
        float size;
        float color[4];
        std::string texturePath;
        bool hasRunway;
        float runwayWidth;
        float runwayLength;
        float runwayColor[4];
        std::string runwayTexturePath;
    } ground;
    
    // Background
    struct BackgroundConfig {
        bool enabled;
        Vec3 colorTop;
        Vec3 colorBottom;
    } background;
};

// ==================== Scene Loader V2 ====================
class SceneLoaderV2 {
public:
    // Load scene from JSON file
    static bool loadScene(const char* filepath, SceneConfigV2& outScene) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }
        
        json j;
        try {
            file >> j;
        } catch (const std::exception&) {
            return false;
        }
        
        // Parse scene name
        if (j.contains("name") && j["name"].is_string()) {
            outScene.name = j["name"].get<std::string>();
        }
        
        // Parse model registry
        if (j.contains("models") && j["models"].is_object()) {
            for (auto& [key, path] : j["models"].items()) {
                if (path.is_string()) {
                    outScene.models[key] = path.get<std::string>();
                }
            }
        }
        
        // Parse camera
        if (j.contains("camera") && j["camera"].is_object()) {
            auto& cam = j["camera"];
            
            if (cam.contains("type") && cam["type"].is_string()) {
                outScene.cameraType = cam["type"].get<std::string>();
            }
            
            if (cam.contains("position") && cam["position"].is_array() && cam["position"].size() == 3) {
                outScene.cameraPosition.x = cam["position"][0].get<float>();
                outScene.cameraPosition.y = cam["position"][1].get<float>();
                outScene.cameraPosition.z = cam["position"][2].get<float>();
            }
            
            if (cam.contains("target") && cam["target"].is_array() && cam["target"].size() == 3) {
                outScene.cameraTarget.x = cam["target"][0].get<float>();
                outScene.cameraTarget.y = cam["target"][1].get<float>();
                outScene.cameraTarget.z = cam["target"][2].get<float>();
            }
            
            if (cam.contains("fov") && cam["fov"].is_number()) {
                outScene.cameraFOV = cam["fov"].get<float>();
            }
        }
        
        // Parse entities
        if (j.contains("entities") && j["entities"].is_array()) {
            for (auto& entityJson : j["entities"]) {
                SceneConfigV2::EntityConfig entity;
                
                // Name
                if (entityJson.contains("name") && entityJson["name"].is_string()) {
                    entity.name = entityJson["name"].get<std::string>();
                }
                
                // Model key
                if (entityJson.contains("model") && entityJson["model"].is_string()) {
                    entity.modelKey = entityJson["model"].get<std::string>();
                }
                
                // Position
                if (entityJson.contains("position") && entityJson["position"].is_array() && entityJson["position"].size() == 3) {
                    entity.position.x = entityJson["position"][0].get<float>();
                    entity.position.y = entityJson["position"][1].get<float>();
                    entity.position.z = entityJson["position"][2].get<float>();
                }
                
                // Rotation
                if (entityJson.contains("rotation") && entityJson["rotation"].is_array() && entityJson["rotation"].size() == 3) {
                    entity.rotation.x = entityJson["rotation"][0].get<float>() * 0.0174533f;  // deg to rad
                    entity.rotation.y = entityJson["rotation"][1].get<float>() * 0.0174533f;
                    entity.rotation.z = entityJson["rotation"][2].get<float>() * 0.0174533f;
                }
                
                // Scale
                entity.scale = {1, 1, 1};
                if (entityJson.contains("scale") && entityJson["scale"].is_array() && entityJson["scale"].size() == 3) {
                    entity.scale.x = entityJson["scale"][0].get<float>();
                    entity.scale.y = entityJson["scale"][1].get<float>();
                    entity.scale.z = entityJson["scale"][2].get<float>();
                }
                
                // Visible
                entity.visible = true;
                if (entityJson.contains("visible") && entityJson["visible"].is_boolean()) {
                    entity.visible = entityJson["visible"].get<bool>();
                }
                
                // Behaviors
                if (entityJson.contains("behaviors") && entityJson["behaviors"].is_array()) {
                    for (auto& behavior : entityJson["behaviors"]) {
                        if (behavior.is_string()) {
                            entity.behaviors.push_back(behavior.get<std::string>());
                        }
                    }
                }
                
                // Behavior parameters
                if (entityJson.contains("behaviorParams") && entityJson["behaviorParams"].is_object()) {
                    entity.behaviorParams = entityJson["behaviorParams"];
                }
                
                outScene.entities.push_back(entity);
            }
        }
        
        // Parse lights (same as before)
        if (j.contains("lights") && j["lights"].is_array()) {
            for (auto& lightJson : j["lights"]) {
                SceneConfigV2::LightConfig light;
                
                if (lightJson.contains("type") && lightJson["type"].is_string()) {
                    light.type = lightJson["type"].get<std::string>();
                }
                
                if (lightJson.contains("direction") && lightJson["direction"].is_array() && lightJson["direction"].size() == 3) {
                    light.direction.x = lightJson["direction"][0].get<float>();
                    light.direction.y = lightJson["direction"][1].get<float>();
                    light.direction.z = lightJson["direction"][2].get<float>();
                }
                
                if (lightJson.contains("color") && lightJson["color"].is_array() && lightJson["color"].size() == 3) {
                    light.color.x = lightJson["color"][0].get<float>();
                    light.color.y = lightJson["color"][1].get<float>();
                    light.color.z = lightJson["color"][2].get<float>();
                }
                
                light.intensity = 1.0f;
                if (lightJson.contains("intensity") && lightJson["intensity"].is_number()) {
                    light.intensity = lightJson["intensity"].get<float>();
                }
                
                outScene.lights.push_back(light);
            }
        }
        
        // Parse ground
        if (j.contains("ground") && j["ground"].is_object()) {
            auto& gr = j["ground"];
            outScene.ground.enabled = gr.value("enabled", true);
            outScene.ground.size = gr.value("size", 5000.0f);
            
            // Color
            if (gr.contains("color") && gr["color"].is_array() && gr["color"].size() == 4) {
                outScene.ground.color[0] = gr["color"][0].get<float>();
                outScene.ground.color[1] = gr["color"][1].get<float>();
                outScene.ground.color[2] = gr["color"][2].get<float>();
                outScene.ground.color[3] = gr["color"][3].get<float>();
            }
            
            // Texture path
            if (gr.contains("texturePath") && gr["texturePath"].is_string()) {
                outScene.ground.texturePath = gr["texturePath"].get<std::string>();
            }
            
            // Runway
            outScene.ground.hasRunway = gr.value("hasRunway", false);
            outScene.ground.runwayWidth = gr.value("runwayWidth", 50.0f);
            outScene.ground.runwayLength = gr.value("runwayLength", 1000.0f);
            
            // Runway color
            if (gr.contains("runwayColor") && gr["runwayColor"].is_array() && gr["runwayColor"].size() == 4) {
                outScene.ground.runwayColor[0] = gr["runwayColor"][0].get<float>();
                outScene.ground.runwayColor[1] = gr["runwayColor"][1].get<float>();
                outScene.ground.runwayColor[2] = gr["runwayColor"][2].get<float>();
                outScene.ground.runwayColor[3] = gr["runwayColor"][3].get<float>();
            }
            
            // Runway texture path
            if (gr.contains("runwayTexturePath") && gr["runwayTexturePath"].is_string()) {
                outScene.ground.runwayTexturePath = gr["runwayTexturePath"].get<std::string>();
            }
        }
        
        // Parse background
        if (j.contains("background") && j["background"].is_object()) {
            auto& bg = j["background"];
            outScene.background.enabled = bg.value("enabled", true);
            // ... background parsing
        }
        
        return true;
    }
    
    // Apply scene to registries
    static bool applyScene(const SceneConfigV2& scene, 
                          ModelRegistry& modelRegistry, 
                          EntityRegistry& entityRegistry) {
        // Register all models
        for (const auto& [key, filepath] : scene.models) {
            if (!modelRegistry.registerModel(key, filepath)) {
                printf("Warning: Failed to load model '%s' from '%s'\n", 
                       key.c_str(), filepath.c_str());
            }
        }
        
        // Create entities
        for (const auto& entityConfig : scene.entities) {
            // Create entity
            Entity* entity = entityRegistry.createEntity(entityConfig.name);
            entity->setPosition(entityConfig.position);
            entity->setRotation(entityConfig.rotation);
            entity->setScale(entityConfig.scale);
            entity->setVisible(entityConfig.visible);
            
            // Assign model
            const Model* model = modelRegistry.getModel(entityConfig.modelKey);
            if (model) {
                entity->setModel(model);
            } else {
                printf("Warning: Model key '%s' not found for entity '%s'\n",
                       entityConfig.modelKey.c_str(), entityConfig.name.c_str());
            }
            
            // Attach behaviors
            for (const std::string& behaviorName : entityConfig.behaviors) {
                if (behaviorName == "FlightDynamics") {
                    auto* behavior = entityRegistry.addBehavior<FlightDynamicsBehavior>(entity->getID());
                    
                    // Apply behavior parameters if specified
                    if (entityConfig.behaviorParams.contains("userControlled") &&
                        entityConfig.behaviorParams["userControlled"].is_boolean()) {
                        behavior->setUserControlled(
                            entityConfig.behaviorParams["userControlled"].get<bool>());
                    }
                    
                } else if (behaviorName == "ChaseCamera") {
                    auto* behavior = entityRegistry.addBehavior<ChaseCameraBehavior>(entity->getID());
                    
                    // Apply camera parameters
                    if (entityConfig.behaviorParams.contains("cameraDistance") &&
                        entityConfig.behaviorParams["cameraDistance"].is_number()) {
                        behavior->setDistance(
                            entityConfig.behaviorParams["cameraDistance"].get<float>());
                    }
                    if (entityConfig.behaviorParams.contains("cameraHeight") &&
                        entityConfig.behaviorParams["cameraHeight"].is_number()) {
                        behavior->setHeight(
                            entityConfig.behaviorParams["cameraHeight"].get<float>());
                    }
                    
                } else if (behaviorName == "OrbitCamera") {
                    auto* behavior = entityRegistry.addBehavior<OrbitCameraBehavior>(entity->getID());
                    
                    // Apply orbit camera parameters
                    if (entityConfig.behaviorParams.contains("orbitDistance") &&
                        entityConfig.behaviorParams["orbitDistance"].is_number()) {
                        behavior->setDistance(
                            entityConfig.behaviorParams["orbitDistance"].get<float>());
                    }
                    if (entityConfig.behaviorParams.contains("orbitYaw") &&
                        entityConfig.behaviorParams["orbitYaw"].is_number()) {
                        behavior->setYaw(
                            entityConfig.behaviorParams["orbitYaw"].get<float>());
                    }
                    if (entityConfig.behaviorParams.contains("orbitPitch") &&
                        entityConfig.behaviorParams["orbitPitch"].is_number()) {
                        behavior->setPitch(
                            entityConfig.behaviorParams["orbitPitch"].get<float>());
                    }
                    if (entityConfig.behaviorParams.contains("autoRotate") &&
                        entityConfig.behaviorParams["autoRotate"].is_boolean()) {
                        behavior->setAutoRotate(
                            entityConfig.behaviorParams["autoRotate"].get<bool>());
                    }
                    if (entityConfig.behaviorParams.contains("rotationSpeed") &&
                        entityConfig.behaviorParams["rotationSpeed"].is_number()) {
                        behavior->setRotationSpeed(
                            entityConfig.behaviorParams["rotationSpeed"].get<float>());
                    }
                }
            }
        }
        
        return true;
    }
};

#endif // SCENE_LOADER_V2_H
