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

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    float uFrequency;
    float uMagnitude;
    float uDecay;
    float uRotation;
    float uSeed;
} pc;

float hash11(float p) {
    return fract(sin(p * 0.1031) * 43758.5453);
}

float hash12(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise2D(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash12(i + vec2(0.0, 0.0)), hash12(i + vec2(1.0, 0.0)), f.x),
               mix(hash12(i + vec2(0.0, 1.0)), hash12(i + vec2(1.0, 1.0)), f.x), f.y);
}

void main() {
    vec4 color = texture(uInputTexture, vTexCoord);
    
    // Generate shake offset
    float time = ubo.uTime * pc.uFrequency;
    float decay = exp(-ubo.uTime * pc.uDecay);
    
    vec2 shakeOffset = vec2(
        (noise2D(vec2(time, pc.uSeed)) - 0.5) * 2.0,
        (noise2D(vec2(time + 100.0, pc.uSeed)) - 0.5) * 2.0
    ) * pc.uMagnitude * decay * 0.002;
    
    // Rotation shake
    float rotShake = (noise2D(vec2(time + 200.0, pc.uSeed)) - 0.5) * 2.0 * pc.uRotation * decay;
    
    // Apply shake to texture coordinates
    vec2 center = vec2(0.5);
    vec2 uv = vTexCoord - center;
    
    // Rotate
    float c = cos(rotShake);
    float s = sin(rotShake);
    uv = vec2(uv.x * c - uv.y * s, uv.x * s + uv.y * c);
    
    // Translate
    uv += shakeOffset;
    uv += center;
    
    // Clamp
    uv = clamp(uv, vec2(0.0), vec2(1.0));
    
    outColor = texture(uInputTexture, uv);
}