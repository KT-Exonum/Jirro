#version 450
#extension GL_EXT_debug_printf : enable

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
    float level = ub.audioLevel * ub.sensitivity;
    level = clamp(level, 0.0, 1.0);

    vec3 col = vec3(level * 0.2, level * 0.5, level);
    col += vec3(0.05) * (1.0 - level);

    outColor = vec4(col, 1.0);
}
