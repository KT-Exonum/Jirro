// Camera3D vertex shader - outputs view-projection matrix for other nodes
#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 vTexCoord;

layout(set = 0, binding = 0) uniform Camera3DUniforms {
    mat4 viewMatrix;
    mat4 projMatrix;
    mat4 viewProjMatrix;
    vec3 cameraPos;
    float focalLength;
    float aperture;
    float focusDistance;
    float nearPlane;
    float farPlane;
    vec2 resolution;
} uniforms;

void main() {
    vTexCoord = inTexCoord;
    gl_Position = vec4(inPosition, 0.0, 1.0);
}