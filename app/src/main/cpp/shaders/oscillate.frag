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
    float uAngle;
    float uPhase;
    int uWaveType;  // 0 = Sine, 1 = Triangle
} pc;

float triangleWave(float x) {
    return abs(fract(x + 0.5) * 2.0 - 1.0);
}

float waveFunc(float x) {
    if (pc.uWaveType == 0) {
        return sin(x);
    } else {
        return triangleWave(x);
    }
}

void main() {
    vec4 color = texture(uInputTexture, vTexCoord);
    
    // Calculate oscillation offset
    float angleRad = radians(pc.uAngle);
    vec2 dir = vec2(cos(angleRad), sin(angleRad));
    float phase = ubo.uTime * pc.uFrequency * 6.2831853 + pc.uPhase * 0.01;
    float offset = waveFunc(phase) * pc.uMagnitude;
    
    // Apply offset to texture coordinates
    vec2 offsetUV = vTexCoord + dir * offset * 0.001;
    
    // Clamp to valid range
    offsetUV = clamp(offsetUV, vec2(0.0), vec2(1.0));
    
    outColor = texture(uInputTexture, offsetUV);
}