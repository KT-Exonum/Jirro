#version 450

layout(set = 0, binding = 0) uniform UniformBuffer {
    float time;
    float audioLevel;
    float beatPhase;
    float sensitivity;
    float smoothing;
    vec2  resolution;
} ub;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 uv = inTexCoord;
    float wave = sin(uv.x * 20.0 + ub.time * 5.0) * 0.1;
    wave += sin(uv.x * 40.0 - ub.time * 3.0) * 0.05;
    wave *= ub.audioLevel * ub.sensitivity;

    float d = abs(uv.y - 0.5 - wave);
    float line = 1.0 - smoothstep(0.0, 0.005, d);
    vec3 col = vec3(0.0, line, line * 0.5);

    outColor = vec4(col, 1.0);
}
