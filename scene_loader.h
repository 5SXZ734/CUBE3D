// scene_loader.h - JSON scene file loader
#ifndef SCENE_LOADER_H
#define SCENE_LOADER_H

#include <string>
#include <vector>

// ==================== Scene File Structures ====================

struct SceneFileCamera {
    enum Type { ORBIT, FPS };
    Type type;
    float position[3];
    float target[3];
    float fov;
    float nearPlane;
    float farPlane;
    
    // Orbit camera specific
    float distance;
    float yaw;
    float pitch;
    bool autoRotate;
    
    SceneFileCamera()
        : type(ORBIT)
        , position{0, 5, 20}
        , target{0, 0, 0}
        , fov(60.0f)
        , nearPlane(0.1f)
        , farPlane(10000.0f)
        , distance(20.0f)
        , yaw(0.0f)
        , pitch(-0.3f)
        , autoRotate(false)
    {}
};

struct SceneFileLight {
    enum Type { DIRECTIONAL, POINT, SPOT };
    Type type;
    float direction[3];
    float position[3];
    float color[3];
    float intensity;
    float range;
    
    SceneFileLight()
        : type(DIRECTIONAL)
        , direction{-0.6f, -1.0f, -0.4f}
        , position{0, 10, 0}
        , color{1, 1, 1}
        , intensity(1.0f)
        , range(100.0f)
    {}
};

struct SceneFileObject {
    std::string name;
    std::string modelPath;
    float position[3];
    float rotation[3];
    float scale[3];
    bool visible;
    
    SceneFileObject()
        : position{0, 0, 0}
        , rotation{0, 0, 0}
        , scale{1, 1, 1}
        , visible(true)
    {}
};

struct SceneFileGround {
    bool enabled;
    float size;
    float color[4];
    bool hasRunway;
    float runwayWidth;
    float runwayLength;
    float runwayColor[4];
    
    // TEXTURE SUPPORT
    std::string texturePath;        // Path to ground texture (e.g., "scenes/models/surf.bmp")
    std::string runwayTexturePath;  // Path to runway texture (e.g., "scenes/models/strip.bmp")
    
    SceneFileGround()
        : enabled(true)
        , size(5000.0f)
        , color{0.3f, 0.3f, 0.3f, 1.0f}
        , hasRunway(false)
        , runwayWidth(50.0f)
        , runwayLength(1000.0f)
        , runwayColor{0.5f, 0.5f, 0.5f, 1.0f}
        , texturePath("")
        , runwayTexturePath("")
    {}
};

struct SceneFileBackground {
    bool enabled;
    float colorTop[3];
    float colorBottom[3];
    
    SceneFileBackground()
        : enabled(true)
        , colorTop{0.5f, 0.7f, 1.0f}
        , colorBottom{0.8f, 0.9f, 1.0f}
    {}
};

struct SceneFile {
    std::string name;
    SceneFileCamera camera;
    std::vector<SceneFileLight> lights;
    std::vector<SceneFileObject> objects;
    SceneFileGround ground;
    SceneFileBackground background;
    
    SceneFile() : name("Untitled Scene") {}
};

// ==================== Scene Loader ====================

class SceneLoader {
public:
    static bool loadScene(const char* filepath, SceneFile& outScene);
    static bool saveScene(const char* filepath, const SceneFile& scene);
    static const char* getLastError();
    
private:
    static std::string s_lastError;
};

#endif // SCENE_LOADER_H
