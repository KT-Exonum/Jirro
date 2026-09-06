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
                sscanf(vertex.c_str(), "%d/%d/%d", &idx, &uvIdx, &normIdx);
                faceVerts.push_back(static_cast<uint32_t>(std::abs(idx)) - 1);
            }
            for (size_t i = 1; i + 1 < faceVerts.size(); ++i) {
                indices.push_back(faceVerts[0]);
                indices.push_back(faceVerts[i]);
                indices.push_back(faceVerts[i + 1]);
            }
        }
    }
    
    mesh.vertexCount = positions.size() / 3;
    mesh.indexCount = indices.size();
    
    for (size_t i = 0; i < mesh.vertexCount; ++i) {
        Vertex v{};
        v.position[0] = positions[i * 3];
        v.position[1] = positions[i * 3 + 1];
        v.position[2] = positions[i * 3 + 2];
        
        if (includeNormals_ && i * 3 + 2 < normals.size()) {
            v.normal[0] = normals[i * 3];
            v.normal[1] = normals[i * 3 + 1];
            v.normal[2] = normals[i * 3 + 2];
        }
        
        if (i * 2 + 1 < texcoords.size()) {
            v.texcoord[0] = texcoords[i * 2];
            v.texcoord[1] = texcoords[i * 2 + 1];
        }
        
        mesh.vertices.push_back(v);
    }
    
    mesh.indices = indices;
    model->meshes.push_back(std::move(mesh));
    LOGI("Loaded OBJ: %zu vertices, %zu indices", model->meshes.back().vertexCount, model->meshes.back().indexCount);
    return model;
}

} // namespace vfx
