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
    float uPhase;
    float uAnchorX;
    float uAnchorY;
} pc;

void main() {
    vec2 uv = vTexCoord;
    vec4 color = texture(uInputTexture, uv);
    
    // Calculate pulse scale
    float phase = ubo.uTime * pc.uFrequency * 6.2831853 + pc.uPhase * 0.01;
    float pulse = sin(phase) * 0.5 + 0.5;  // 0 to 1
    float scale = 1.0 + pulse * pc.uMagnitude;
    
    // Apply scale around anchor point
    vec2 anchor = vec2(pc.uAnchorX, pc.uAnchorY);
    uv = anchor + (uv - anchor) / scale;
    
    // Clamp
    uv = clamp(uv, vec2(0.0), vec2(1.0));
    
    outColor = texture(uInputTexture, uv);
}