// Motion blur vertex shader
#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec2 vTexCoordPrev;
layout(location = 2) out vec2 vTexCoordNext;

layout(set = 0, binding = 0) uniform MotionBlurUniforms {
    mat3 prevTransform;
    mat3 nextTransform;
    vec2 prevTranslation;
    vec2 nextTranslation;
    float shutterAngle;      // degrees
    float shutterPhase;      // degrees
    int sampleCount;
} uniforms;

void main() {
    vTexCoord = inTexCoord;
    
    // Compute previous and next frame texture coordinates based on transform
    vec3 pos = vec3(inTexCoord * 2.0 - 1.0, 1.0);
    vec3 posPrev = uniforms.prevTransform * pos;
    vec3 posNext = uniforms.nextTransform * pos;
    
    vTexCoordPrev = (posPrev.xy * 0.5 + 0.5) + uniforms.prevTranslation;
    vTexCoordNext = (posNext.xy * 0.5 + 0.5) + uniforms.nextTranslation;
    
    gl_Position = vec4(inPosition, 0.0, 1.0);
}