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
    float uIntensity;
    float uBlockSize;
    float uFrequency;
    float uSeed;
    float uChromatic;
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
    vec2 uv = vTexCoord;
    vec4 color = vec4(0.0);
    
    // Random displacement for glitch blocks
    float time = ubo.uTime * pc.uFrequency;
    float blockX = floor(uv.x / pc.uBlockSize);
    float blockY = floor(uv.y / pc.uBlockSize);
    float blockHash = hash12(vec2(blockX, blockY) + vec2(pc.uSeed, time));
    
    // Block displacement
    float displace = (blockHash - 0.5) * 2.0 * pc.uIntensity * 0.1;
    float displaceY = (hash12(vec2(blockX, blockY + 1.0) + vec2(pc.uSeed, time + 100.0)) - 0.5) * 2.0 * pc.uIntensity * 0.1;
    
    vec2 newUV = uv + vec2(displace, displaceY);
    
    // Chromatic aberration
    float chroma = pc.uChromatic * 0.01 * pc.uIntensity;
    float r = texture(uInputTexture, uv + vec2(chroma, 0.0)).r;
    float g = texture(uInputTexture, uv).g;
    float b = texture(uInputTexture, uv - vec2(chroma, 0.0)).b;
    
    // Scanline effect
    float scanline = sin(uv.y * ubo.uResolution[1] * 2.0 + time * 10.0) * 0.02 * pc.uIntensity;
    
    // RGB channel shifts
    vec3 rgbShift = vec3(
        texture(uInputTexture, uv + vec2(chroma + (hash12(vec2(blockX + 1.0, blockY) + pc.uSeed) - 0.5) * 0.02, 0.0)).r,
        texture(uInputTexture, uv).g,
        texture(uInputTexture, uv - vec2(chroma + (hash12(vec2(blockX, blockY + 1.0) + pc.uSeed) - 0.5) * 0.02, 0.0)).b
    );
    
    // Random horizontal line displacement (glitch lines)
    float lineGlitch = step(0.98, hash12(vec2(floor(uv.y * 100.0), time))) * pc.uIntensity * 0.05;
    uv.x += lineGlitch;
    
    color.rgb = rgbShift;
    color.a = texture(uInputTexture, uv).a;
    
    // Add scanlines
    color.rgb += scanline;
    
    outColor = color;
}