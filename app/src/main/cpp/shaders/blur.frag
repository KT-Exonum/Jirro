#version 450
#extension GL_KHR_vulkan_memory_model : enable

layout(set = 0, binding = 0) uniform Uniforms {
    mat4 uProjection;
    vec2 uResolution;
} uniforms;

// Blur uniforms - animated via KeyframeTrack
layout(set = 1, binding = 0) uniform BlurParams {
    float uRadius;        // blur radius in pixels (0.0 = no blur)
    float uDirection;     // 0.0 = horizontal, 1.0 = vertical, 2.0 = both (box)
    int uSamples;         // number of samples (odd number, max 15)
} blurParams;

layout(set = 2, binding = 0) uniform sampler2D uTexture0;

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

void main() {
    if (blurParams.uRadius <= 0.0 || blurParams.uSamples <= 1) {
        outColor = texture(uTexture0, vUv);
        return;
    }

    vec2 texelSize = 1.0 / uniforms.uResolution;
    vec2 direction = vec2(0.0);

    if (blurParams.uDirection == 0.0) {
        direction = vec2(1.0, 0.0); // horizontal
    } else if (blurParams.uDirection == 1.0) {
        direction = vec2(0.0, 1.0); // vertical
    } else {
        direction = vec2(1.0, 1.0); // both (box blur approximation)
    }

    int samples = min(blurParams.uSamples, 15);
    // Ensure odd number for symmetric sampling
    if (samples % 2 == 0) samples++;

    float halfSamples = float(samples - 1) * 0.5;
    vec4 sum = vec4(0.0);
    float weightSum = 0.0;

    // Gaussian weights (precomputed for sigma = radius/3)
    float sigma = blurParams.uRadius / 3.0;
    for (int i = 0; i < samples; i++) {
        float offset = float(i) - halfSamples;
        vec2 sampleUv = vUv + direction * offset * texelSize * blurParams.uRadius;

        // Gaussian weight
        float weight = exp(-0.5 * (offset * offset) / (sigma * sigma));
        sum += texture(uTexture0, sampleUv) * weight;
        weightSum += weight;
    }

    outColor = sum / weightSum;
}