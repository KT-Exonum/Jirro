#version 450
#extension GL_KHR_vulkan_memory_model : enable

// Base fragment shader for simple texture sampling (Normal blend / passthrough)
// Used for: ImageSource, VideoSource passthrough, Shader nodes with custom SPIR-V

layout(set = 0, binding = 0) uniform Uniforms {
    mat4 uProjection;
    vec2 uResolution;
} uniforms;

layout(set = 1, binding = 0) uniform sampler2D uTexture0;
layout(set = 1, binding = 1) uniform sampler2D uTexture1;

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

// Blend mode: Normal (source over)
// outColor = src
void main() {
    vec4 src = texture(uTexture0, vUv);
    outColor = src;
}