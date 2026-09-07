#include "glTFLoader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>

#define LOG_TAG "glTFLoader"
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vfx {

glTFLoader::glTFLoader() = default;

glTFLoader::~glTFLoader() = default;

bool glTFLoader::IsSupported(const std::string& filePath) {
    std::string lower;
    lower.resize(filePath.size());
    std::transform(filePath.begin(), filePath.end(), lower.begin(), ::tolower);
    return lower.ends_with(".gltf") || lower.ends_with(".glb") || lower.ends_with(".obj");
}

std::shared_ptr<Model> glTFLoader::Load(const std::string& filePath) {
    std::string lower;
    lower.resize(filePath.size());
    std::transform(filePath.begin(), filePath.end(), lower.begin(), ::tolower);
    
    if (lower.ends_with(".glb")) return LoadGLB(filePath);
    if (lower.ends_with(".gltf")) return LoadGLTF(filePath);
    if (lower.ends_with(".obj")) return LoadOBJ(filePath);
    
    LOGE("Unsupported format: %s", filePath.c_str());
    return nullptr;
}

std::shared_ptr<Model> glTFLoader::LoadGLTF(const std::string& path) {
    LOGI("Loading glTF: %s", path.c_str());
    auto model = std::make_shared<Model>();
    model->name = path;
    return model;
}

std::shared_ptr<Model> glTFLoader::LoadGLB(const std::string& path) {
    LOGI("Loading GLB: %s", path.c_str());
    auto model = std::make_shared<Model>();
    model->name = path;
    return model;
}

std::shared_ptr<Model> glTFLoader::LoadOBJ(const std::string& path) {
    LOGI("Loading OBJ: %s", path.c_str());
    auto model = std::make_shared<Model>();
    
    std::ifstream file(path);
    if (!file) {
        LOGE("Failed to open OBJ: %s", path.c_str());
        return nullptr;
    }
    
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> texcoords;
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;
    
    Mesh mesh;
    mesh.name = "OBJ_Mesh";
    
    struct IndexKey {
        uint32_t posIdx;
        uint32_t normIdx;
        uint32_t uvIdx;
        bool operator==(const IndexKey& other) const {
            return posIdx == other.posIdx && normIdx == other.normIdx && uvIdx == other.uvIdx;
        }
    };
    
    struct IndexKeyHash {
        size_t operator()(const IndexKey& k) const {
            return ((k.posIdx * 73856093) ^ (k.normIdx * 19349663) ^ (k.uvIdx * 83492791));
        }
    };
    
    std::unordered_map<IndexKey, uint32_t, IndexKeyHash> vertexMap;
    std::vector<uint32_t> indices;
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;
        
        if (prefix == "v") {
            float x, y, z;
            iss >> x >> y >> z;
            positions.push_back(x * scale_);
            positions.push_back(y * scale_);
            positions.push_back(z * scale_);
        } else if (prefix == "vn" && includeNormals_) {
            float x, y, z;
            iss >> x >> y >> z;
            normals.push_back(x); normals.push_back(y); normals.push_back(z);
        } else if (prefix == "vt") {
            float u, v;
            iss >> u >> v;
            texcoords.push_back(u); texcoords.push_back(v);
        } else if (prefix == "f") {
            std::string vertex;
            std::vector<uint32_t> faceVerts;
            while (iss >> vertex) {
                uint32_t idx = 0, uvIdx = 0, normIdx = 0;
                int matched = sscanf(vertex.c_str(), "%d/%d/%d", &idx, &uvIdx, &normIdx);
                if (matched < 1) continue;
                
                // Handle negative indices (relative to end)
                if (idx < 0) idx = static_cast<uint32_t>(positions.size() / 3) + idx;
                if (normIdx < 0 && includeNormals_) normIdx = static_cast<uint32_t>(normals.size() / 3) + normIdx;
                if (uvIdx < 0) uvIdx = static_cast<uint32_t>(texcoords.size() / 2) + uvIdx;
                
                IndexKey key{idx - 1, normIdx - 1, uvIdx - 1};
                auto [it, inserted] = vertexMap.emplace(key, static_cast<uint32_t>(vertices.size()));
                if (inserted) {
                    Vertex v{};
                    v.position[0] = positions[(idx - 1) * 3];
                    v.position[1] = positions[(idx - 1) * 3 + 1];
                    v.position[2] = positions[(idx - 1) * 3 + 2];
                    
                    if (includeNormals_ && (normIdx - 1) * 3 + 2 < normals.size()) {
                        v.normal[0] = normals[(normIdx - 1) * 3];
                        v.normal[1] = normals[(normIdx - 1) * 3 + 1];
                        v.normal[2] = normals[(normIdx - 1) * 3 + 2];
                    }
                    
                    if ((uvIdx - 1) * 2 + 1 < texcoords.size()) {
                        v.texcoord[0] = texcoords[(uvIdx - 1) * 2];
                        v.texcoord[1] = texcoords[(uvIdx - 1) * 2 + 1];
                    }
                    
                    vertices.push_back(v);
                }
                faceVerts.push_back(it->second);
            }
            
            for (size_t i = 1; i + 1 < faceVerts.size(); ++i) {
                indices.push_back(faceVerts[0]);
                indices.push_back(faceVerts[i]);
                indices.push_back(faceVerts[i + 1]);
            }
        }
    }
    
    mesh.vertexCount = vertices.size();
    mesh.indexCount = indices.size();
    mesh.vertices = std::move(vertices);
    mesh.indices = std::move(indices);
    model->meshes.push_back(std::move(mesh));
    LOGI("Loaded OBJ: %zu vertices, %zu indices", model->meshes.back().vertexCount, model->meshes.back().indexCount);
    return model;
}

} // namespace vfx
