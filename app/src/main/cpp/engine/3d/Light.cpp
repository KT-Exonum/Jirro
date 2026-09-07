#include "Light.h"
#include <algorithm>

namespace vfx {

LightManager::LightManager() = default;

LightManager::~LightManager() = default;

std::string LightManager::AddLight(const Light& light) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = "light_" + std::to_string(lights_.size());
    lights_[id] = light;
    return id;
}

void LightManager::RemoveLight(const std::string& lightId) {
    std::lock_guard<std::mutex> lock(mutex_);
    lights_.erase(lightId);
}

Light* LightManager::GetLight(const std::string& lightId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = lights_.find(lightId);
    return it != lights_.end() ? &it->second : nullptr;
}

const Light* LightManager::GetLight(const std::string& lightId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = lights_.find(lightId);
    return it != lights_.end() ? &it->second : nullptr;
}

std::vector<Light*> LightManager::GetAllLights() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Light*> result;
    result.reserve(lights_.size());
    for (auto& [id, light] : lights_) result.push_back(&light);
    return result;
}

std::string LightManager::AddLightProbe(const LightProbe& probe) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = "probe_" + std::to_string(probes_.size());
    probes_[id] = probe;
    return id;
}

void LightManager::RemoveLightProbe(const std::string& probeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    probes_.erase(probeId);
}

} // namespace vfx
