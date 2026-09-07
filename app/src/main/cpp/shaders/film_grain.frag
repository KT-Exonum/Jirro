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

float hash11(float p) {
    return fract(sin(p * 0.1031) * 43758.5453);
}

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

void main() {
    vec2 uv = fTexCoord;
    vec4 color = texture(uInputTexture, uv);
    
    // Monochromatic film grain
    float time = ubo.uTime;
    float frameSeed = pc.uSeed + ubo.uFrameIndex;
    float grain = (hash12(uv * 200.0 + vec2(frameSeed, time)) - 0.5) * 2.0;
    
    // Scale grain by luminance (more visible in midtones)
    float lum = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    float grainAmount = pc.uIntensity * pc.uNoiseAmount * (0.5 + lum * 0.5);
    
    color.rgb += grain * grainAmount * 0.1;
    
    // Slight vignette
    float vignette = 1.0 - length(uv - vec2(0.5)) * 0.3;
    color.rgb *= vignette;
    
    outColor = vec4(clamp(color.rgb, 0.0, 1.0), 1.0);
}