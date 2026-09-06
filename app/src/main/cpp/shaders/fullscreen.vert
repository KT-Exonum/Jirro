#version 450
#extension GL_KHR_vulkan_memory_model : enable

// Fullscreen triangle vertex shader - generates positions via gl_VertexIndex
// No vertex buffers needed; classic 3-vertex fullscreen triangle covering [-1,1]x[-1,1]
// with UVs in [0,1]x[0,1] (Vulkan Y-down convention for texture sampling).

layout(set = 0, binding = 0) uniform Uniforms {
    mat4 uProjection;
    vec2 uResolution;
} uniforms;

layout(location = 0) out vec2 vUv;

void main() {
    // Fullscreen triangle vertex positions (CCW winding for Vulkan frontFace = clockwise)
    // Vertex 0: bottom-left (-1, -1) -> UV (0, 1)
    // Vertex 1: top-left    (-1,  3) -> UV (0, -1) -> clamped to 0
    // Vertex 2: bottom-right( 3, -1) -> UV (2, 1) -> clamped to 1
    // This covers the entire screen with a single triangle.
    vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2(-1.0,  3.0),
        vec2( 3.0, -1.0)
    );

    vec2 uvs[3] = vec2[3](
        vec2(0.0, 1.0),
        vec2(0.0, -1.0),
        vec2(2.0, 1.0)
    );

    vec2 pos = positions[gl_VertexIndex];
    vUv = uvs[gl_VertexIndex];

    gl_Position = uniforms.uProjection * vec4(pos, 0.0, 1.0);
}