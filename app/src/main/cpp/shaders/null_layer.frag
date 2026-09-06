// Null layer fragment shader - passes through input with transform applied in vertex shader
// In practice, transform is applied via push constants or uniform buffer in the vertex shader
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inputTexture;
layout(set = 0, binding = 1) uniform NullUniforms {
    // Transform matrix (3x3 for 2D)
    mat3 transform;
    vec2 pivot;
} uniforms;

void main() {
    // Apply inverse transform to sample from correct position
    vec2 uv = (uniforms.transform * vec3(vTexCoord, 1.0)).xy;
    outColor = texture(inputTexture, uv);
}