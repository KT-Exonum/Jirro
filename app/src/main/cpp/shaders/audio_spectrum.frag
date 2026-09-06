#version 450

layout(set = 0, binding = 0) uniform UniformBuffer {
    float time;
    float audioLevel;
    float beatPhase;
    float sensitivity;
    float smoothing;
    vec2  resolution;
    float barCount;
    float barWidth;
    float barGap;
} ub;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 uv = inTexCoord;
    float barIndex = floor(uv.x * ub.barCount);
    float barLocal = fract(uv.x * ub.barCount);

    float height = fract(sin(barIndex * 12.9898 + ub.time) * 43758.5453);
    height *= ub.audioLevel * ub.sensitivity;

    float barX = barLocal / (ub.barWidth + ub.barGap);
    float inBar = step(barX, ub.barWidth / (ub.barWidth + ub.barGap));

    float top = 0.5 + height * 0.5;
    float bottom = 0.5 - height * 0.5;
    float inBarY = step(bottom, uv.y) * step(uv.y, top);

    vec3 col = vec3(0.1, 0.8, 0.3) * inBar * inBarY;
    outColor = vec4(col, 1.0);
}
