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
    
    // Polar coordinate transform
    // Rectangular -> Polar: x = angle, y = radius
    float angle = atan(dir.y, dir.x);
    float radius = length(dir);
    
    // Normalize
    float polarX = (angle + 3.14159265) / 6.2831853; // -pi..pi -> 0..1
    float polarY = radius * 2.0; // 0..0.5 -> 0..1
    
    // Apply amount
    vec2 polarUV = mix(uv, vec2(polarX, polarY), pc.uAmount);
    polarUV = clamp(polarUV, vec2(0.0), vec2(1.0));
    
    outColor = texture(uInputTexture, polarUV);
}