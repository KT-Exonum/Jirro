#include "Material.h"
#include <algorithm>

namespace vfx {

MaterialHandle MaterialLibrary::AddMaterial(const Material& material) {
    std::lock_guard<std::mutex> lock(mutex_);
    MaterialHandle handle = material.materialId.empty() ? "mat_" + std::to_string(materials_.size()) : material.materialId;
    materials_[handle] = material;
    return handle;
}

void MaterialLibrary::RemoveMaterial(const MaterialHandle& handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    materials_.erase(handle);
}

Material* MaterialLibrary::GetMaterial(const MaterialHandle& handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = materials_.find(handle);
    return it != materials_.end() ? &it->second : nullptr;
}

const Material* MaterialLibrary::GetMaterial(const MaterialHandle& handle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = materials_.find(handle);
    return it != materials_.end() ? &it->second : nullptr;
}

std::vector<Material*> MaterialLibrary::GetAllMaterials() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Material*> result;
    result.reserve(materials_.size());
    for (auto& [id, mat] : materials_) result.push_back(&mat);
    return result;
}

Material MaterialLibrary::DefaultPBR() {
    Material m;
    m.name = "Default PBR";
    m.materialId = "default_pbr";
    m.baseColor[0] = 1.0f; m.baseColor[1] = 1.0f; m.baseColor[2] = 1.0f; m.baseColor[3] = 1.0f;
    m.metallic = 0.0f;
    m.roughness = 0.5f;
    return m;
}

Material MaterialLibrary::DefaultUnlit() {
    Material m;
    m.name = "Default Unlit";
    m.materialId = "default_unlit";
    m.blendMode = MaterialBlendMode::Opaque;
    return m;
}

Material MaterialLibrary::DefaultWireframe() {
    Material m;
    m.name = "Default Wireframe";
    m.materialId = "default_wireframe";
    m.cullMode = CullMode::None;
    return m;
}

} // namespace vfx
