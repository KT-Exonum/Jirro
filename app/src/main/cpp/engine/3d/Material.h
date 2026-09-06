#pragma once
// PBR material system with texture maps and shader parameter binding.

#include <string>
#include <vector>
#include <unordered_map>
#include <array>

namespace vfx {

enum class BlendMode {
    Opaque,
    AlphaBlend,
    AlphaTest,
    Additive,
    Multiply
};

enum class CullMode {
    None,
    Front,
    Back
};

struct TextureSlot {
    std::string path;
    int slot = 0; // texture unit index
    bool sRGB = false;
};

struct Material {
    std::string name;
    std::string materialId;
    
    // PBR base parameters
    float baseColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float specular = 0.5f;
    float emissive[3] = {0.0f, 0.0f, 0.0f};
    float emissiveIntensity = 1.0f;
    float occlusion = 1.0f;
    float alphaCutoff = 0.5f;
    
    // Textures
    TextureSlot baseColorMap;
    TextureSlot metallicRoughnessMap;
    TextureSlot normalMap;
    TextureSlot occlusionMap;
    TextureSlot emissiveMap;
    TextureSlot heightMap;
    
    // Render state
    BlendMode blendMode = BlendMode::Opaque;
    CullMode cullMode = CullMode::Back;
    bool doubleSided = false;
    bool castShadows = true;
    bool receiveShadows = true;
    int renderPriority = 0;
    
    // Tessellation
    float tessellationFactor = 1.0f;
    
    // Animation
    bool emissiveAnimated = false;
    float emissiveFlickerSpeed = 0.0f;
};

using MaterialHandle = std::string;

class MaterialLibrary {
public:
    MaterialLibrary() = default;
    ~MaterialLibrary() = default;
    
    MaterialHandle AddMaterial(const Material& material);
    void RemoveMaterial(const MaterialHandle& handle);
    Material* GetMaterial(const MaterialHandle& handle);
    const Material* GetMaterial(const MaterialHandle& handle) const;
    std::vector<Material*> GetAllMaterials();
    
    // Default materials
    static Material DefaultPBR();
    static Material DefaultUnlit();
    static Material DefaultWireframe();
    
private:
    std::unordered_map<MaterialHandle, Material> materials_;
    std::mutex mutex_;
};

} // namespace vfx
