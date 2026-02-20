// scene_loader.h - JSON scene file loader
#ifndef SCENE_LOADER_H
#define SCENE_LOADER_H

#include <string>
#include <vector>

// ==================== Scene Description Structures ====================

struct SceneFileCamera {
    float position[3] = {0, 5, 20};
    float target[3] = {0, 0, 0};
    float fov = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
};

struct SceneFileLight {
    enum Type { DIRECTIONAL, POINT, SPOT } type = DIRECTIONAL;
    float direction[3] = {-0.6f, -1.0f, -0.4f};  // For directional
    float position[3] = {0, 10, 0};               // For point/spot
    float color[3] = {1, 1, 1};
    float intensity = 1.0f;
    float range = 100.0f;                         // For point/spot
};

struct SceneFileObject {
    std::string name;
    std::string modelPath;
    float position[3] = {0, 0, 0};
    float rotation[3] = {0, 0, 0};  // Euler angles in degrees (yaw, pitch, roll)
    float scale[3] = {1, 1, 1};
    bool visible = true;
};

struct SceneFileGround {
    bool enabled = false;
    float size = 100.0f;
    float color[4] = {0.3f, 0.3f, 0.3f, 1.0f};
};

struct SceneFileBackground {
    bool enabled = false;
    float colorTop[3] = {0.5f, 0.7f, 1.0f};
    float colorBottom[3] = {0.8f, 0.9f, 1.0f};
};

struct SceneFile {
    std::string name;
    SceneFileCamera camera;
    std::vector<SceneFileLight> lights;
    std::vector<SceneFileObject> objects;
    SceneFileGround ground;
    SceneFileBackground background;
};

// ==================== Scene Loader ====================

class SceneLoader {
public:
    // Load scene from JSON file
    static bool loadScene(const char* filepath, SceneFile& outScene);
    
    // Save scene to JSON file
    static bool saveScene(const char* filepath, const SceneFile& scene);
    
    // Get last error message
    static const char* getLastError();
    
private:
    static std::string s_lastError;
};

#endif // SCENE_LOADER_H
