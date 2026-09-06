#version 450
#extension GL_KHR_vulkan_memory_model : enable

layout(set = 0, binding = 0) uniform Uniforms {
    mat4 uProjection;
    vec2 uResolution;
} uniforms;

// Mask uniforms - animated via KeyframeTrack
layout(set = 1, binding = 0) uniform MaskParams {
    float uFeather;       // feather edge in pixels (0.0 = hard edge)
    float uInvert;        // 0.0 = normal, 1.0 = inverted
    float uOpacity;       // mask opacity multiplier (0.0 to 1.0)
} maskParams;

layout(set = 2, binding = 0) uniform sampler2D uTexture0; // source
layout(set = 2, binding = 1) uniform sampler2D uTexture1; // mask (single channel / red channel)

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

void main() {
    vec4 src = texture(uTexture0, vUv);
    float mask = texture(uTexture1, vUv).r; // use red channel as mask

    if (maskParams.uInvert > 0.5) {
        mask = 1.0 - mask;
    }

    if (maskParams.uFeather > 0.0) {
        // Simple feather: smoothstep around 0.5
        float feather = maskParams.uFeather / max(uniforms.uResolution.x, uniforms.uResolution.y);
        mask = smoothstep(0.5 - feather, 0.5 + feather, mask);
    }

    mask *= maskParams.uOpacity;
    outColor = vec4(src.rgb, src.a * mask);
}