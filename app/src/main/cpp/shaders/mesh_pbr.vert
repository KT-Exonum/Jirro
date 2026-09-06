// MeshSource vertex shader - PBR with skinning support
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec4 inColor;      // vertex color
layout(location = 5) in ivec4 inJoints;    // skinning joints
layout(location = 6) in vec4 inWeights;    // skinning weights

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec3 vWorldPos;
layout(location = 2) out vec3 vNormal;
layout(location = 3) out vec3 vTangent;
layout(location = 4) out vec3 vBitangent;
layout(location = 5) out vec4 vColor;

layout(set = 0, binding = 0) uniform MeshUniforms {
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projMatrix;
    mat4 normalMatrix;
    vec3 cameraPos;
    float time;
    bool hasVertexColors;
    bool hasTangents;
    bool skinned;
    int maxJoints;
} uniforms;

layout(set = 0, binding = 1) uniform JointUniforms {
    mat4 jointMatrices[128];
} joints;

void main() {
    vTexCoord = inTexCoord;
    vColor = inColor;
    
    vec3 pos = inPosition;
    vec3 normal = inNormal;
    vec3 tangent = inTangent;
    
    // Skinning
    if (uniforms.skinned) {
        mat4 skinMatrix = mat4(0.0);
        for (int i = 0; i < 4; i++) {
            float w = inWeights[i];
            if (w > 0.0) {
                skinMatrix += w * joints.jointMatrices[inJoints[i]];
            }
        }
        pos = (skinMatrix * vec4(pos, 1.0)).xyz;
        normal = mat3(skinMatrix) * normal;
        tangent = mat3(skinMatrix) * tangent;
    }
    
    // World position
    vec4 worldPos4 = uniforms.modelMatrix * vec4(pos, 1.0);
    vWorldPos = worldPos4.xyz;
    
    // Normal in world space
    vNormal = normalize(mat3(uniforms.normalMatrix) * normal);
    
    // Tangent space
    if (uniforms.hasTangents) {
        vTangent = normalize(mat3(uniforms.normalMatrix) * tangent);
        vBitangent = cross(vNormal, vTangent) * inTangent.w; // w stores handedness
    } else {
        // Generate tangent from UV
        vTangent = normalize(cross(vNormal, vec3(0.0, 1.0, 0.0)));
        if (length(vTangent) < 0.1) vTangent = normalize(cross(vNormal, vec3(1.0, 0.0, 0.0)));
        vBitangent = cross(vNormal, vTangent);
    }
    
    // Clip position
    gl_Position = uniforms.projMatrix * uniforms.viewMatrix * worldPos4;
}