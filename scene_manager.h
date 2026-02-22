// scene_manager.h - Manages cameras, current entity, and scene state
#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H

#include "entity_registry.h"
#include "model_registry.h"
#include "camera_entity.h"
#include "camera_behaviors.h"
#include "input_controller.h"
#include <vector>
#include <string>

// ==================== Scene Manager ====================
// Central manager for scene state, cameras, and controllable entities
class SceneManager {
public:
    SceneManager(EntityRegistry& entityRegistry, ModelRegistry& modelRegistry)
        : m_entityRegistry(entityRegistry)
        , m_modelRegistry(modelRegistry)
        , m_currentCameraIndex(0)
        , m_currentControllableIndex(0)
        , m_inputController(nullptr)
    {}
    
    ~SceneManager() {
        delete m_inputController;
    }
    
    // Camera management
    void addCamera(EntityID cameraID) {
        CameraEntity* cam = dynamic_cast<CameraEntity*>(m_entityRegistry.getEntity(cameraID));
        if (cam) {
            m_cameraIDs.push_back(cameraID);
            if (m_cameraIDs.size() == 1) {
                setActiveCamera(0);  // First camera is active by default
            }
        }
    }
    
    void nextCamera() {
        if (m_cameraIDs.empty()) return;
        m_currentCameraIndex = (m_currentCameraIndex + 1) % m_cameraIDs.size();
        setActiveCamera(m_currentCameraIndex);
    }
    
    void previousCamera() {
        if (m_cameraIDs.empty()) return;
        m_currentCameraIndex = (m_currentCameraIndex + m_cameraIDs.size() - 1) % m_cameraIDs.size();
        setActiveCamera(m_currentCameraIndex);
    }
    
    CameraEntity* getActiveCamera() {
        if (m_cameraIDs.empty()) return nullptr;
        return dynamic_cast<CameraEntity*>(m_entityRegistry.getEntity(m_cameraIDs[m_currentCameraIndex]));
    }
    
    // Controllable entity management
    void addControllable(EntityID entityID) {
        m_controllableIDs.push_back(entityID);
        if (m_controllableIDs.size() == 1) {
            setCurrentControllable(0);  // First controllable is current by default
        }
    }
    
    void nextControllable() {
        if (m_controllableIDs.empty()) return;
        m_currentControllableIndex = (m_currentControllableIndex + 1) % m_controllableIDs.size();
        setCurrentControllable(m_currentControllableIndex);
        printf("Switched to controllable entity #%zu\n", m_currentControllableIndex + 1);
    }
    
    void previousControllable() {
        if (m_controllableIDs.empty()) return;
        m_currentControllableIndex = (m_currentControllableIndex + m_controllableIDs.size() - 1) % m_controllableIDs.size();
        setCurrentControllable(m_currentControllableIndex);
        printf("Switched to controllable entity #%zu\n", m_currentControllableIndex + 1);
    }
    
    Entity* getCurrentControllable() {
        if (m_controllableIDs.empty()) return nullptr;
        return m_entityRegistry.getEntity(m_controllableIDs[m_currentControllableIndex]);
    }
    
    EntityID getCurrentControllableID() const {
        if (m_controllableIDs.empty()) return 0;
        return m_controllableIDs[m_currentControllableIndex];
    }
    
    // Input controller
    void setInputController(InputController* controller) {
        delete m_inputController;
        m_inputController = controller;
        
        // Attach to current controllable
        if (m_inputController && !m_controllableIDs.empty()) {
            Entity* current = getCurrentControllable();
            if (current) {
                m_inputController->attach(current);
            }
        }
    }
    
    InputController* getInputController() { return m_inputController; }
    
    // Scene reloading
    void setSceneFilePath(const std::string& path) { m_sceneFilePath = path; }
    const std::string& getSceneFilePath() const { return m_sceneFilePath; }
    
    void clear() {
        m_cameraIDs.clear();
        m_controllableIDs.clear();
        m_currentCameraIndex = 0;
        m_currentControllableIndex = 0;
        delete m_inputController;
        m_inputController = nullptr;
    }
    
    // Stats
    size_t getCameraCount() const { return m_cameraIDs.size(); }
    size_t getControllableCount() const { return m_controllableIDs.size(); }
    
private:
    void setActiveCamera(size_t index) {
        // Deactivate all cameras
        for (EntityID id : m_cameraIDs) {
            CameraEntity* cam = dynamic_cast<CameraEntity*>(m_entityRegistry.getEntity(id));
            if (cam) cam->setActive(false);
        }
        
        // Activate selected camera
        if (index < m_cameraIDs.size()) {
            CameraEntity* cam = dynamic_cast<CameraEntity*>(m_entityRegistry.getEntity(m_cameraIDs[index]));
            if (cam) {
                cam->setActive(true);
                printf("Camera switched to: %s\n", cam->getName().c_str());
            }
        }
    }
    
    void setCurrentControllable(size_t index) {
        if (index >= m_controllableIDs.size()) return;
        
        EntityID newTargetID = m_controllableIDs[index];
        Entity* current = m_entityRegistry.getEntity(newTargetID);
        if (!current) return;
        
        // Reattach input controller
        if (m_inputController) {
            m_inputController->detach();
            m_inputController->attach(current);
        }
        
        // Update all camera behaviors to target the new entity
        for (EntityID cameraID : m_cameraIDs) {
            CameraEntity* camera = dynamic_cast<CameraEntity*>(m_entityRegistry.getEntity(cameraID));
            if (!camera) continue;
            
            // Check for chase camera behavior
            auto* chaseBehavior = m_entityRegistry.getBehavior<ChaseCameraTargetBehavior>(cameraID);
            if (chaseBehavior) {
                // Recreate behavior with new target
                m_entityRegistry.removeBehavior<ChaseCameraTargetBehavior>(cameraID);
                auto* newBehavior = new ChaseCameraTargetBehavior(&m_entityRegistry, newTargetID);
                newBehavior->setDistance(25.0f);  // Use defaults or store original params
                newBehavior->setHeight(8.0f);
                newBehavior->setSmoothness(0.92f);
                newBehavior->attach(camera);
                newBehavior->initialize();
                m_entityRegistry.addBehaviorManual(cameraID, newBehavior);
            }
            
            // Check for orbit camera behavior
            auto* orbitBehavior = m_entityRegistry.getBehavior<OrbitCameraTargetBehavior>(cameraID);
            if (orbitBehavior) {
                // Recreate behavior with new target
                m_entityRegistry.removeBehavior<OrbitCameraTargetBehavior>(cameraID);
                auto* newBehavior = new OrbitCameraTargetBehavior(&m_entityRegistry, newTargetID);
                newBehavior->setDistance(40.0f);
                newBehavior->setYaw(0.6f);
                newBehavior->setPitch(-0.3f);
                newBehavior->setAutoRotate(true);
                newBehavior->setRotationSpeed(0.2f);
                newBehavior->attach(camera);
                newBehavior->initialize();
                m_entityRegistry.addBehaviorManual(cameraID, newBehavior);
            }
        }
        
        printf("Cameras retargeted to: %s\n", current->getName().c_str());
    }
    
    EntityRegistry& m_entityRegistry;
    ModelRegistry& m_modelRegistry;
    
    std::vector<EntityID> m_cameraIDs;
    size_t m_currentCameraIndex;
    
    std::vector<EntityID> m_controllableIDs;
    size_t m_currentControllableIndex;
    
    InputController* m_inputController;
    std::string m_sceneFilePath;
};

#endif // SCENE_MANAGER_H
