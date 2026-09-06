#version 450

layout(set = 0, binding = 0) uniform Uniforms {
    mat4 uProjection;
    mat4 uView;
    float uTime;
    float uResolution[2];
    int uFrameIndex;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D uInputTexture;
layout(set = 0, binding = 2) uniform sampler2D uInputTexture2;

layout(location = 0) in vec2 fTexCoord;
layout(location = 1) in vec2 fCenterDist;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    float uTransform[9];
    float uFrequency;
    float uMagnitude;
    float uAngle;
    float uPhase;
    int uWaveType;
    float uDecay;
    float uRotation;
    float uSeed;
    float uAmount;
    float uSpeed;
    float uScale;
    float uOctaves;
    float uIntensity;
    float uBlockSize;
    float uChromatic;
    float uNoiseAmount;
    float uScanlineAmount;
    float uDistortion;
    float uColorBleed;
    float uJitter;
} pc;

void main() {
    vec2 uv = fTexCoord;
    vec2 center = vec2(0.5);
    vec2 dir = uv - center;
    float dist = length(dir);
    
    // Center-weighted radial blur - stronger at edges
    float blurAmount = dist * pc.uAmount * pc.uMagnitude * 0.02;
    int samples = 8;
    vec4 color = vec4(0.0);
    float totalWeight = 0.0;
    
    for (int i = 0; i < samples; i++) {
        float t = float(i) / float(samples - 1);
        vec2 sampleUV = center + dir * (1.0 + t * blurAmount);
        sampleUV = clamp(sampleUV, vec2(0.0), vec2(1.0));
        float weight = 1.0 - t * 0.7;
        color += texture(uInputTexture, sampleUV) * weight;
        totalWeight += weight;
    }
    
    color /= totalWeight;
    color.rgb *= pc.uIntensity;
    
    outColor = color;
}