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

layout(location = 0) in vec2 vPosition;   // -1 to 1 clip space
layout(location = 1) in vec2 vTexCoord;   // 0 to 1 texture space
layout(location = 0) out vec2 fTexCoord;
layout(location = 1) out vec2 fCenterDist; // for vignette etc.

layout(push_constant) uniform PushConstants {
    // 3x3 transform matrix (column-major): [a b c; d e f; g h i]
    // Applied as: uv' = mat3 * vec3(uv, 1.0)
    float uTransform[9];
    
    // Effect-specific params (for fragment shader)
    float uFrequency;
    float uMagnitude;
    float uAngle;
    float uPhase;
    int uWaveType;        // 0=Sine, 1=Triangle
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
    gl_Position = vec4(vPosition, 0.0, 1.0);
    
    // Apply 3x3 transform matrix to texture coordinates
    // Matrix is column-major: [m0 m3 m6; m1 m4 m7; m2 m5 m8]
    // Result: uv' = M * vec3(uv, 1.0)
    mat3 M = mat3(
        pc.uTransform[0], pc.uTransform[3], pc.uTransform[6],
        pc.uTransform[1], pc.uTransform[4], pc.uTransform[7],
        pc.uTransform[2], pc.uTransform[5], pc.uTransform[8]
    );
    
    vec3 uv3 = M * vec3(vTexCoord, 1.0);
    fTexCoord = uv3.xy / uv3.z;
    
    // Distance from center for vignette/radial effects
    fCenterDist = fTexCoord - vec2(0.5);
}