#include "Shadow.h"

namespace vfx {

void ShadowManager::CalculateCascades(const ShadowMap& shadowMap, float cameraNear, float cameraFar) {
    float lambda = cascadeSplitLambda_;
    float range = cameraFar - cameraNear;
    
    for (int i = 0; i < shadowMap.cascadeCount; ++i) {
        float logDepth = cameraNear * std::pow(range / cameraNear, static_cast<float>(i + 1) / shadowMap.cascadeCount);
        float uniformDepth = cameraNear + range * (static_cast<float>(i + 1) / shadowMap.cascadeCount);
        float splitDepth = std::mix(uniformDepth, logDepth, lambda);
        
        shadowMap.cascades[i].splitDepth = splitDepth;
        shadowMap.cascades[i].near = i == 0 ? cameraNear : shadowMap.cascades[i - 1].far;
        shadowMap.cascades[i].far = splitDepth;
    }
}

} // namespace vfx
