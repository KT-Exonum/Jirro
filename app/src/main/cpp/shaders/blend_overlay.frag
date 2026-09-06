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

// Blend mode: Overlay
// Combines Multiply and Screen: if base < 0.5 use Multiply, else Screen
void main() {
    vec4 dst = texture(uTexture0, vUv);
    vec4 src = texture(uTexture1, vUv);
    outColor = mix(
        2.0 * src * dst,           // Multiply for dark base
        1.0 - 2.0 * (1.0 - src) * (1.0 - dst), // Screen for light base
        step(0.5, dst)
    );
}