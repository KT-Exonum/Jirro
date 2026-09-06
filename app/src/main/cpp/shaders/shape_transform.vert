// Shape Transform vertex shader
#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 vTexCoord;

layout(set = 0, binding = 0) uniform ShapeTransformUniforms {
    mat3 transform;          // 2D transform matrix
    vec2 anchor;             // transform anchor (0-1)
    vec2 resolution;         // frame resolution
} uniforms;

void main() {
    vTexCoord = inTexCoord;
    
    // Apply transform in vertex shader for vector-perfect results
    vec2 p = inPosition; // NDC: -1 to 1
    vec2 centered = p - (uniforms.anchor * 2.0 - 1.0);
    vec3 transformed = uniforms.transform * vec3(centered, 1.0);
    vec2 result = transformed.xy + (uniforms.anchor * 2.0 - 1.0);
    
    gl_Position = vec4(result, 0.0, 1.0);
}