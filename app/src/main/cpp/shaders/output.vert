// Output vertex shader with onion skinning support
#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out int vLayer; // 0=current, -1=previous, 1=next, etc.

void main() {
    vTexCoord = inTexCoord;
    gl_Position = vec4(inPosition, 0.0, 1.0);
    vLayer = 0; // Base layer
}