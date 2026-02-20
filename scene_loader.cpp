// scene_loader.cpp - JSON scene file loader implementation
#include "scene_loader.h"
#include <nlohmann/json.hpp>  // Installed via vcpkg
#include <fstream>
#include <cstdio>

using json = nlohmann::json;

std::string SceneLoader::s_lastError;

// Helper to safely read array of floats with defaults
template<size_t N>
void readFloatArray(const json& j, const char* key, float (&arr)[N], const float (&defaultVal)[N]) {
    if (j.contains(key) && j[key].is_array()) {
        auto& jarr = j[key];
        for (size_t i = 0; i < N && i < jarr.size(); i++) {
            arr[i] = jarr[i].get<float>();
        }
    } else {
        for (size_t i = 0; i < N; i++) {
            arr[i] = defaultVal[i];
        }
    }
}

// Helper to write array of floats
template<size_t N>
void writeFloatArray(json& j, const char* key, const float (&arr)[N]) {
    j[key] = json::array();
    for (size_t i = 0; i < N; i++) {
        j[key].push_back(arr[i]);
    }
}

bool SceneLoader::loadScene(const char* filepath, SceneFile& outScene) {
    try {
        // Read file
        std::ifstream file(filepath);
        if (!file.is_open()) {
            s_lastError = std::string("Failed to open file: ") + filepath;
            return false;
        }
        
        json j;
        file >> j;
        
        // Scene name
        if (j.contains("name")) {
            outScene.name = j["name"].get<std::string>();
        }
        
        // Camera
        if (j.contains("camera")) {
            auto& cam = j["camera"];
            
            // Camera type
            if (cam.contains("type")) {
                std::string typeStr = cam["type"];
                if (typeStr == "fps" || typeStr == "FPS") outScene.camera.type = SceneFileCamera::FPS;
                else if (typeStr == "orbit" || typeStr == "ORBIT") outScene.camera.type = SceneFileCamera::ORBIT;
            }
            
            float defaultPos[3] = {0, 5, 20};
            float defaultTarget[3] = {0, 0, 0};
            
            readFloatArray(cam, "position", outScene.camera.position, defaultPos);
            readFloatArray(cam, "target", outScene.camera.target, defaultTarget);
            
            if (cam.contains("fov")) outScene.camera.fov = cam["fov"];
            if (cam.contains("nearPlane")) outScene.camera.nearPlane = cam["nearPlane"];
            if (cam.contains("farPlane")) outScene.camera.farPlane = cam["farPlane"];
            
            // Orbit camera specific
            if (cam.contains("distance")) outScene.camera.distance = cam["distance"];
            if (cam.contains("yaw")) outScene.camera.yaw = cam["yaw"];
            if (cam.contains("pitch")) outScene.camera.pitch = cam["pitch"];
            if (cam.contains("autoRotate")) outScene.camera.autoRotate = cam["autoRotate"];
        }
        
        // Lights
        if (j.contains("lights") && j["lights"].is_array()) {
            for (auto& jlight : j["lights"]) {
                SceneFileLight light;
                
                // Type
                if (jlight.contains("type")) {
                    std::string typeStr = jlight["type"];
                    if (typeStr == "directional") light.type = SceneFileLight::DIRECTIONAL;
                    else if (typeStr == "point") light.type = SceneFileLight::POINT;
                    else if (typeStr == "spot") light.type = SceneFileLight::SPOT;
                }
                
                // Direction (for directional lights)
                float defaultDir[3] = {-0.6f, -1.0f, -0.4f};
                readFloatArray(jlight, "direction", light.direction, defaultDir);
                
                // Position (for point/spot lights)
                float defaultPos[3] = {0, 10, 0};
                readFloatArray(jlight, "position", light.position, defaultPos);
                
                // Color
                float defaultColor[3] = {1, 1, 1};
                readFloatArray(jlight, "color", light.color, defaultColor);
                
                if (jlight.contains("intensity")) light.intensity = jlight["intensity"];
                if (jlight.contains("range")) light.range = jlight["range"];
                
                outScene.lights.push_back(light);
            }
        }
        
        // Objects
        if (j.contains("objects") && j["objects"].is_array()) {
            for (auto& jobj : j["objects"]) {
                SceneFileObject obj;
                
                if (jobj.contains("name")) obj.name = jobj["name"];
                if (jobj.contains("model")) obj.modelPath = jobj["model"];
                
                float defaultPos[3] = {0, 0, 0};
                float defaultRot[3] = {0, 0, 0};
                float defaultScale[3] = {1, 1, 1};
                
                readFloatArray(jobj, "position", obj.position, defaultPos);
                readFloatArray(jobj, "rotation", obj.rotation, defaultRot);
                readFloatArray(jobj, "scale", obj.scale, defaultScale);
                
                if (jobj.contains("visible")) obj.visible = jobj["visible"];
                
                outScene.objects.push_back(obj);
            }
        }
        
        // Ground
        if (j.contains("ground")) {
            auto& gr = j["ground"];
            if (gr.contains("enabled")) outScene.ground.enabled = gr["enabled"];
            if (gr.contains("size")) outScene.ground.size = gr["size"];
            
            float defaultColor[4] = {0.3f, 0.3f, 0.3f, 1.0f};
            readFloatArray(gr, "color", outScene.ground.color, defaultColor);
            
            // Runway configuration
            if (gr.contains("hasRunway")) outScene.ground.hasRunway = gr["hasRunway"];
            if (gr.contains("runwayWidth")) outScene.ground.runwayWidth = gr["runwayWidth"];
            if (gr.contains("runwayLength")) outScene.ground.runwayLength = gr["runwayLength"];
            
            float defaultRunwayColor[4] = {0.5f, 0.5f, 0.5f, 1.0f};
            readFloatArray(gr, "runwayColor", outScene.ground.runwayColor, defaultRunwayColor);
            
            // TEXTURE PATHS - NEW!
            if (gr.contains("texturePath") && gr["texturePath"].is_string()) {
                outScene.ground.texturePath = gr["texturePath"].get<std::string>();
            }
            if (gr.contains("runwayTexturePath") && gr["runwayTexturePath"].is_string()) {
                outScene.ground.runwayTexturePath = gr["runwayTexturePath"].get<std::string>();
            }
        }
        
        // Background
        if (j.contains("background")) {
            auto& bg = j["background"];
            if (bg.contains("enabled")) outScene.background.enabled = bg["enabled"];
            
            float defaultTop[3] = {0.5f, 0.7f, 1.0f};
            float defaultBottom[3] = {0.8f, 0.9f, 1.0f};
            
            readFloatArray(bg, "colorTop", outScene.background.colorTop, defaultTop);
            readFloatArray(bg, "colorBottom", outScene.background.colorBottom, defaultBottom);
        }
        
        std::printf("Scene loaded: %s (%zu objects, %zu lights)\n", 
                   filepath, outScene.objects.size(), outScene.lights.size());
        return true;
        
    } catch (const std::exception& e) {
        s_lastError = std::string("JSON parse error: ") + e.what();
        std::fprintf(stderr, "Failed to load scene: %s\n", s_lastError.c_str());
        return false;
    }
}

bool SceneLoader::saveScene(const char* filepath, const SceneFile& scene) {
    try {
        json j;
        
        // Scene name
        if (!scene.name.empty()) {
            j["name"] = scene.name;
        }
        
        // Camera
        j["camera"] = json::object();
        j["camera"]["type"] = (scene.camera.type == SceneFileCamera::FPS) ? "fps" : "orbit";
        writeFloatArray(j["camera"], "position", scene.camera.position);
        writeFloatArray(j["camera"], "target", scene.camera.target);
        j["camera"]["fov"] = scene.camera.fov;
        j["camera"]["nearPlane"] = scene.camera.nearPlane;
        j["camera"]["farPlane"] = scene.camera.farPlane;
        j["camera"]["distance"] = scene.camera.distance;
        j["camera"]["yaw"] = scene.camera.yaw;
        j["camera"]["pitch"] = scene.camera.pitch;
        j["camera"]["autoRotate"] = scene.camera.autoRotate;
        
        // Lights
        j["lights"] = json::array();
        for (const auto& light : scene.lights) {
            json jlight;
            
            if (light.type == SceneFileLight::DIRECTIONAL) jlight["type"] = "directional";
            else if (light.type == SceneFileLight::POINT) jlight["type"] = "point";
            else if (light.type == SceneFileLight::SPOT) jlight["type"] = "spot";
            
            writeFloatArray(jlight, "direction", light.direction);
            writeFloatArray(jlight, "position", light.position);
            writeFloatArray(jlight, "color", light.color);
            jlight["intensity"] = light.intensity;
            jlight["range"] = light.range;
            
            j["lights"].push_back(jlight);
        }
        
        // Objects
        j["objects"] = json::array();
        for (const auto& obj : scene.objects) {
            json jobj;
            
            if (!obj.name.empty()) jobj["name"] = obj.name;
            if (!obj.modelPath.empty()) jobj["model"] = obj.modelPath;
            
            writeFloatArray(jobj, "position", obj.position);
            writeFloatArray(jobj, "rotation", obj.rotation);
            writeFloatArray(jobj, "scale", obj.scale);
            jobj["visible"] = obj.visible;
            
            j["objects"].push_back(jobj);
        }
        
        // Ground
        j["ground"]["enabled"] = scene.ground.enabled;
        j["ground"]["size"] = scene.ground.size;
        writeFloatArray(j["ground"], "color", scene.ground.color);
        j["ground"]["hasRunway"] = scene.ground.hasRunway;
        j["ground"]["runwayWidth"] = scene.ground.runwayWidth;
        j["ground"]["runwayLength"] = scene.ground.runwayLength;
        writeFloatArray(j["ground"], "runwayColor", scene.ground.runwayColor);
        
        // TEXTURE PATHS - NEW!
        if (!scene.ground.texturePath.empty()) {
            j["ground"]["texturePath"] = scene.ground.texturePath;
        }
        if (!scene.ground.runwayTexturePath.empty()) {
            j["ground"]["runwayTexturePath"] = scene.ground.runwayTexturePath;
        }
        
        // Background
        j["background"]["enabled"] = scene.background.enabled;
        writeFloatArray(j["background"], "colorTop", scene.background.colorTop);
        writeFloatArray(j["background"], "colorBottom", scene.background.colorBottom);
        
        // Write to file
        std::ofstream file(filepath);
        if (!file.is_open()) {
            s_lastError = std::string("Failed to open file for writing: ") + filepath;
            return false;
        }
        
        file << j.dump(2);  // Pretty print with 2-space indent
        
        std::printf("Scene saved: %s\n", filepath);
        return true;
        
    } catch (const std::exception& e) {
        s_lastError = std::string("JSON write error: ") + e.what();
        std::fprintf(stderr, "Failed to save scene: %s\n", s_lastError.c_str());
        return false;
    }
}

const char* SceneLoader::getLastError() {
    return s_lastError.c_str();
}
