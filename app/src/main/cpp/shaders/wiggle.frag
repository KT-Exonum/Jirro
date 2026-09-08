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

float hash12(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i + vec2(0.0, 0.0)), hash12(i + vec2(1.0, 0.0)), f.x),
               mix(hash12(i + vec2(0.0, 1.0)), hash12(i + vec2(1.0, 1.0)), f.x), f.y);
}

float fbm(vec2 p, int octaves) {
    float value = 0.0;
    float amplitude = 1.0;
    for (int i = 0; i < 8; i++) {
        if (i >= octaves) break;
        value += amplitude * noise(p);
        p *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

void main() {
    vec2 uv = fTexCoord;
    
    // AE-style wiggle: position wiggle using FBM
    float time = ubo.uTime * pc.uFrequency;
    float freq = pc.uScale;
    float amp = pc.uAmount * pc.uMagnitude;
    
    vec2 offset = vec2(0.0);
    for (int i = 0; i < 4; i++) {
        if (i >= int(pc.uOctaves)) break;
        float f = freq * pow(2.0, float(i));
        float a = amp * 0.5 / freq;
        offset.x += (noise(vec2(time * f, float(pc.uSeed) + float(i) * 100.0)) - 0.5) * 2.0 * a;
        offset.y += (noise(vec2(time * f + 1000.0, float(pc.uSeed) + float(i) * 100.0)) - 0.5) * 2.0 * a;
    }
    
    uv += offset * 0.01;
    uv = clamp(uv, vec2(0.0), vec2(1.0));
    
    outColor = texture(uInputTexture, uv);
}