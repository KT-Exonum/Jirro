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
    
    // Simple bezier warp using 4 control points (corners)
    // In a real implementation, these would come from the node's uniform values
    // For now, apply a simple bulge as placeholder
    vec2 dir = uv - center;
    float dist = length(dir);
    
    float warp = pc.uAmount * pc.uIntensity * 0.5;
    float scale = 1.0 + warp * (1.0 - dist * 2.0);
    
    vec2 newUV = center + dir * scale;
    newUV = clamp(newUV, vec2(0.0), vec2(1.0));
    
    outColor = texture(uInputTexture, newUV);
}