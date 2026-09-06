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

float fbm(vec2 p, int octaves) {
    float value = 0.0;
    float amplitude = 1.0;
    for (int i = 0; i < 4; i++) {
        if (i >= octaves) break;
        value += amplitude * noise(p);
        p *= 2.0;
        amplitude *= 0.5;
    }
    return value;
}

void main() {
    vec2 uv = fTexCoord;
    
    // Horizontal jitter (VHS tracking error)
    float time = ubo.uTime;
    float line = floor(uv.y * ubo.uResolution[1]);
    float jitter = (hash12(vec2(line, time * 30.0 + pc.uSeed)) - 0.5) * 2.0 * pc.uJitter * 0.005;
    uv.x += jitter;
    
    // Horizontal distortion waves
    float distortion = sin(uv.y * 50.0 + time * 2.0) * pc.uDistortion * 0.002;
    uv.x += distortion;
    
    // Noise overlay
    float noiseVal = fbm(uv * 50.0 + vec2(time * 0.5, time * 0.3), 3);
    float noiseOverlay = (noiseVal - 0.5) * 2.0 * pc.uNoiseAmount * 0.1;
    
    // Scanlines
    float scanline = sin(uv.y * ubo.uResolution[1] * 1.5) * 0.5 + 0.5;
    float scanlineAlpha = mix(1.0, 0.7, scanline * pc.uScanlineAmount);
    
    // Color bleed (chromatic aberration)
    float chroma = pc.uChromatic * 0.003;
    float r = texture(uInputTexture, uv + vec2(chroma, 0.0)).r;
    float g = texture(uInputTexture, uv).g;
    float b = texture(uInputTexture, uv - vec2(chroma, 0.0)).b;
    
    vec3 color = vec3(r, g, b);
    
    // Add noise
    color += noiseOverlay;
    
    // Apply scanlines
    color *= scanlineAlpha;
    
    // Vignette
    float vignette = 1.0 - length(fCenterDist) * 0.5;
    color *= vignette;
    
    outColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}