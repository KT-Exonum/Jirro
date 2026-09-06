// Transform3D vertex shader - 3D transform with perspective
#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out float vDepth;

layout(set = 0, binding = 0) uniform Transform3DUniforms {
    mat4 modelMatrix;      // Model matrix (translate * rotate * scale)
    mat4 viewMatrix;       // Camera view matrix
    mat4 projMatrix;       // Camera projection matrix
    vec2 resolution;       // Frame resolution
} uniforms;

void main() {
    vTexCoord = inTexCoord;
    
    // Apply model-view-projection
    vec4 worldPos = uniforms.modelMatrix * vec4(inPosition * uniforms.resolution, 0.0, 1.0);
    vec4 viewPos = uniforms.viewMatrix * worldPos;
    vec4 clipPos = uniforms.projMatrix * viewPos;
    
    vDepth = clipPos.z / clipPos.w;
    gl_Position = clipPos;
}