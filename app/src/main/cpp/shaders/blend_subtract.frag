#version 450
#extension GL_KHR_vulkan_memory_model : enable

layout(set = 0, binding = 0) uniform Uniforms {
    mat4 uProjection;
    vec2 uResolution;
} uniforms;

layout(set = 1, binding = 0) uniform sampler2D uTexture0; // base (dst)
layout(set = 1, binding = 1) uniform sampler2D uTexture1; // overlay (src)

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

// Blend mode: Subtract
// outColor = dst - src (clamped to 0.0)
void main() {
    vec4 dst = texture(uTexture0, vUv);
    vec4 src = texture(uTexture1, vUv);
    outColor = max(dst - src, vec4(0.0));
}