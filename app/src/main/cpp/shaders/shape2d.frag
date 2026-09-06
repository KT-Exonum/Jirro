#version 450

layout(set = 0, binding = 0) uniform Uniforms {
    mat4 uProjection;
    mat4 uView;
    float uTime;
    float uResolution[2];
    int uFrameIndex;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D uTexture0;
layout(set = 0, binding = 2) uniform sampler2D uTexture1;
layout(set = 0, binding = 3) uniform sampler2D uTexture2;
layout(set = 0, binding = 4) uniform sampler2D uTexture3;
layout(set = 0, binding = 5) uniform sampler2D uTexture3;
layout(set = 0, binding = 6) uniform sampler2D uTexture4;
layout(set = 0, binding = 7) uniform sampler2D uTexture5;
layout(set = 0, binding = 8) uniform sampler2D uTexture6;
layout(set = 0, binding = 9) uniform sampler2D uTexture7;

// Fill gradient texture (1D texture with gradient stops)
layout(set = 0, binding = 10) uniform sampler2D uGradientTex;

layout(location = 0) in vec4 vColor;
layout(location = 1) in float vLayerId;
layout(location = 2) in float vPathId;

layout(location = 0) out vec4 oColor;

layout(push_constant) uniform PushConstants {
    float uTransform[9];
    float uTime;
    float uResolution[2];
    int uLayerCount;
} pc;

// Gradient evaluation
vec4 evalGradient(sampler2D gradTex, float t) {
    t = clamp(t, 0.0, 1.0);
    return texture(gradTex, vec2(t, 0.5));
}

// SDF for rounded rect
float sdRoundedRect(vec2 p, vec2 size, float r) {
    vec2 d = abs(p) - size + vec2(r);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - r;
}

// SDF for ellipse
float sdEllipse(vec2 p, vec2 r) {
    vec2 k = vec2(1.0);
    p = p / r;
    return length(p) - 1.0;
}

// SDF for polygon
float sdPolygon(vec2 p, vec2 v) {
    // Simplified - distance to edge
    float d = dot(p, v) - length(v);
    return d;
}

void main() {
    // For now, just pass through vertex color
    // Real implementation would:
    // 1. Compute SDF for fill
    // 2. Evaluate gradient if needed
    // 3. Compute stroke SDF
    // 4. Apply fill rule (even-odd / non-zero)
    // 5. Blend with stroke
    
    oColor = vColor;
}