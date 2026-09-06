#pragma once
// glTF/OBJ loader with PBR material extraction and mesh buffer upload.

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace vfx {

struct Vertex {
    float position[3];
    float normal[3];
    float tangent[4];
    float texcoord[2];
    float color[4];
    uint32_t joints[4];
    float weights[4];
};

static_assert(sizeof(Vertex) == 80, "Vertex must be 80 bytes for std140 alignment");

struct SubMesh {
    std::string name;
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    uint32_t vertexOffset = 0;
    uint32_t vertexCount = 0;
    std::string materialName;
};

struct Mesh {
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<SubMesh> subMeshes;
    float boundingSphere[4] = {0.0f, 0.0f, 0.0f, 1.0f}; // x,y,z,radius
    float boundingBoxMin[3] = {0.0f};
    float boundingBoxMax[3] = {0.0f};
};

struct SceneNode {
    std::string name;
    std::string meshName;
    float transform[16]; // column-major 4x4
    std::vector<SceneNode> children;
};

struct Model {
    std::string name;
    std::vector<Mesh> meshes;
    std::vector<SceneNode> scene;
    std::unordered_map<std::string, std::string> materialAssignments; // mesh_name -> material_id
};

class glTFLoader {
public:
    glTFLoader();
    ~glTFLoader();
    
    // Load glTF/GLB or OBJ
    std::shared_ptr<Model> Load(const std::string& filePath);
    
    // Supported formats
    static bool IsSupported(const std::string& filePath);
    
    // Extensions
    void SetIncludeNormals(bool include) { includeNormals_ = include; }
    void SetIncludeTangents(bool include) { includeTangents_ = include; }
    void SetIncludeColors(bool include) { includeColors_ = include; }
    void SetScale(float scale) { scale_ = scale; }
    
private:
    std::shared_ptr<Model> LoadGLTF(const std::string& path);
    std::shared_ptr<Model> LoadGLB(const std::string& path);
    std::shared_ptr<Model> LoadOBJ(const std::string& path);
    
    bool includeNormals_ = true;
    bool includeTangents_ = true;
    bool includeColors_ = false;
    float scale_ = 1.0f;
};

} // namespace vfx
