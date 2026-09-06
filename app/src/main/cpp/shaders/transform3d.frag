// Transform3D fragment shader - samples input texture with 3D transform
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in float vDepth;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inputTexture;

layout(set = 0, binding = 1) uniform Transform3DUniforms {
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projMatrix;
    vec2 resolution;
    float opacity;
    int blendMode;  // 0=normal, 1=add, 2=multiply, etc.
} uniforms;

void main() {
    vec4 color = texture(inputTexture, vTexCoord);
    color.a *= uniforms.opacity;
    
    // Apply blend mode if needed (handled by blend node typically)
    outColor = color;
}