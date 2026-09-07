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
layout(set = 0, binding = 5) uniform sampler2D uTexture4;
layout(set = 0, binding = 6) uniform sampler2D uTexture5;
layout(set = 0, binding = 7) uniform sampler2D uTexture6;
layout(set = 0, binding = 8) uniform sampler2D uTexture7;

layout(location = 0) in vec2 vPos;
layout(location = 1) in vec2 vUV;
layout(location = 2) in vec4 vColor;
layout(location = 3) in int vLayerId;
layout(location = 4) in int vPathId;

layout(location = 0) out vec4 oColor;

layout(push_constant) uniform PushConstants {
    float uTransform[9];
    float uTime;
    float uResolution[2];
    int uLayerCount;
} pc;

void main() {
    gl_Position = vec4(vPos, 0.0, 1.0);
    
    // Apply transform
    mat3 M = mat3(
        pc.uTransform[0], pc.uTransform[3], pc.uTransform[6],
        pc.uTransform[1], pc.uTransform[4], pc.uTransform[7],
        pc.uTransform[2], pc.uTransform[5], pc.uTransform[8]
    );
    
    vec3 pos = M * vec3(vPos, 1.0);
    gl_Position = vec4(pos.xy, 0.0, pos.z);
    
    // Pass through color
    oColor = vColor;
}