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
    uv -= center;
    
    // Calculate pulse scale
    float phase = ubo.uTime * pc.uFrequency * 6.2831853 + pc.uPhase * 0.01;
    float pulse = sin(phase) * 0.5 + 0.5;  // 0 to 1
    float scale = 1.0 + pulse * pc.uMagnitude;
    
    // Apply scale around anchor point
    vec2 anchor = vec2(pc.uAmount, pc.uSpeed);  // reuse for anchor
    uv = anchor + (uv - anchor) / scale;
    
    uv += center;
    uv = clamp(uv, vec2(0.0), vec2(1.0));
    
    outColor = texture(uInputTexture, uv);
}