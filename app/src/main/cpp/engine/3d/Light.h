#pragma once
// 3D lighting system: directional, point, spot lights with shadow support.

#include <string>
#include <vector>
#include <array>
#include <memory>

namespace vfx {

struct Transform {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float rotation[3] = {0.0f, 0.0f, 0.0f}; // Euler radians
    float scale[3] = {1.0f, 1.0f, 1.0f};
};

enum class LightType {
    Directional,
    Point,
    Spot,
    Ambient
};

struct Light {
    LightType type = LightType::Directional;
    Transform transform;
    
    // Color and intensity
    float color[3] = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    
    // Attenuation (Point/Spot)
    float attenuationConstant = 1.0f;
    float attenuationLinear = 0.0f;
    float attenuationQuadratic = 0.0f;
    float range = 100.0f;
    
    // Spot
    float spotInnerCone = 30.0f; // degrees
    float spotOuterCone = 45.0f; // degrees
    float spotFalloff = 1.0f;
    
    // Shadows
    bool castShadows = true;
    float shadowBias = 0.001f;
    float shadowNormalBias = 0.02f;
    int shadowMapSize = 2048;
    float shadowNear = 0.1f;
    float shadowFar = 100.0f;
    
    // Animation
    float flickerIntensity = 0.0f;
    float flickerSpeed = 1.0f;
};

struct LightProbe {
    float position[3] = {0.0f, 0.0f, 0.0f};
    std::array<float, 9> shCoefficients; // spherical harmonics L2
    float intensity = 1.0f;
};

class LightManager {
public:
    LightManager();
    ~LightManager();
    
    // Add/remove lights
    std::string AddLight(const Light& light);
    void RemoveLight(const std::string& lightId);
    Light* GetLight(const std::string& lightId);
    const Light* GetLight(const std::string& lightId) const;
    std::vector<Light*> GetAllLights();
    
    // Ambient
    void SetAmbientColor(float r, float g, float b) { ambientColor_ = {r, g, b}; }
    const float* GetAmbientColor() const { return ambientColor_.data(); }
    
    // Light probes
    std::string AddLightProbe(const LightProbe& probe);
    void RemoveLightProbe(const std::string& probeId);
    
    // Limits
    static constexpr size_t kMaxLights = 64;
    static constexpr size_t kMaxShadowCastingLights = 4;
    
private:
    std::unordered_map<std::string, Light> lights_;
    std::unordered_map<std::string, LightProbe> probes_;
    std::array<float, 3> ambientColor_{0.03f, 0.03f, 0.03f};
    mutable std::mutex mutex_;
};

} // namespace vfx
