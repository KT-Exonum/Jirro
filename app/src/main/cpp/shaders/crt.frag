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
    vec2 center = vec2(0.5);
    
    // Barrel distortion (CRT curvature)
    vec2 dir = uv - center;
    float dist = length(dir);
    float barrel = 1.0 + dist * dist * pc.uDistortion * 0.1;
    uv = center + dir * barrel;
    
    // Scanlines
    float scanline = sin(uv.y * ubo.uResolution[1] * 1.2) * 0.5 + 0.5;
    float scanlineAlpha = mix(1.0, 0.6, scanline * pc.uScanlineAmount);
    
    // Noise
    float time = ubo.uTime;
    float noiseVal = noise(uv * 80.0 + vec2(time * 0.5, time * 0.3));
    float noiseOverlay = (noiseVal - 0.5) * 2.0 * pc.uNoiseAmount * 0.05;
    
    // Color bleed (chromatic aberration at edges)
    float chroma = pc.uColorBleed * 0.002 * dist;
    float r = texture(uInputTexture, uv + vec2(chroma, 0.0)).r;
    float g = texture(uInputTexture, uv).g;
    float b = texture(uInputTexture, uv - vec2(chroma, 0.0)).b;
    
    vec3 color = vec3(r, g, b);
    color += noiseOverlay;
    color *= scanlineAlpha;
    
    // Vignette
    float vignette = 1.0 - dist * dist * 0.5;
    color *= vignette * pc.uIntensity;
    
    // Gamma curve (CRT gamma ~2.2)
    color = pow(color, vec3(1.0 / 2.2));
    
    outColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}