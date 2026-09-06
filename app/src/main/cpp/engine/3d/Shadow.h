#pragma once
// Shadow mapping: cascaded shadow maps for directional lights, PCF filtering.

#include <array>
#include <cstdint>

namespace vfx {

static constexpr int kShadowMapMaxSize = 4096;
static constexpr int kMaxCascades = 4;
static constexpr int kMaxShadowCastingLights = 4;

struct ShadowCascade {
    float splitDepth = 0.0f;
    float near = 0.1f;
    float far = 100.0f;
    float viewProj[16]; // column-major 4x4
};

struct ShadowMap {
    std::string lightId;
    int width = 2048;
    int height = 2048;
    bool cascaded = false;
    std::array<ShadowCascade, kMaxCascades> cascades;
    int cascadeCount = 1;
    float depthBias = 0.001f;
    float normalBias = 0.02f;
    int pcfSamples = 4;
};

class ShadowManager {
public:
    ShadowManager() = default;
    ~ShadowManager() = default;
    
    void SetCascadeSplitLambda(float lambda) { cascadeSplitLambda_ = lambda; }
    [[nodiscard]] float GetCascadeSplitLambda() const { return cascadeSplitLambda_; }
    
    void CalculateCascades(const ShadowMap& shadowMap, float cameraNear, float cameraFar);
    
private:
    float cascadeSplitLambda_ = 0.5f;
};

} // namespace vfx
