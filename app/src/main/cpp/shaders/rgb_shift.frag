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
    
    // Animated RGB channel shift
    float time = ubo.uTime * pc.uSpeed;
    float angleRad = radians(pc.uAngle);
    vec2 dir = vec2(cos(angleRad), sin(angleRad));
    
    float shiftAmount = pc.uChromatic * pc.uIntensity * 0.005;
    float rShift = sin(time) * shiftAmount;
    float bShift = cos(time * 1.3) * shiftAmount;
    
    vec2 rUV = uv + dir * rShift;
    vec2 bUV = uv - dir * bShift;
    rUV = clamp(rUV, vec2(0.0), vec2(1.0));
    bUV = clamp(bUV, vec2(0.0), vec2(1.0));
    
    float r = texture(uInputTexture, rUV).r;
    float g = texture(uInputTexture, uv).g;
    float b = texture(uInputTexture, bUV).b;
    
    vec3 color = vec3(r, g, b) * pc.uIntensity;
    
    outColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}