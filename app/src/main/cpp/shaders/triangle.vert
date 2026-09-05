#version 450

layout(location = 0) out vec3 vColor;

// Classic no-vertex-buffer triangle: position and color are derived purely
// from gl_VertexIndex, matching how a Phase 3 fullscreen composite pass will
// also work (no per-node vertex buffers needed for image-space effects).
vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.2, 0.2),
    vec3(0.2, 1.0, 0.3),
    vec3(0.3, 0.4, 1.0)
);

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    vColor = colors[gl_VertexIndex];
}
