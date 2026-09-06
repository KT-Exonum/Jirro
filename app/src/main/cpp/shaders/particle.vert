// Particle system vertex shader - instanced rendering
#version 450

// Instance attributes (per-particle)
layout(location = 0) in vec2 inPosition;      // quad corner (-1,-1 to 1,1)
layout(location = 1) in vec2 inTexCoord;      // texture coordinate

// Instance data (provided via vertex buffers)
layout(location = 2) in vec2 inParticlePos;      // world position
layout(location = 3) in float inParticleSize;    // size in pixels
layout(location = 4) in vec4 inParticleColor;    // color
layout(location = 5) in float inParticleRotation; // rotation in radians
layout(location = 6) in float inParticleLife;    // normalized life 0-1

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec4 vColor;

layout(set = 0, binding = 0) uniform ParticleUniforms {
    vec2 resolution;          // frame resolution
    float pointSizeScale;     // global size multiplier
    bool useAdditiveBlend;    // affects output alpha
} uniforms;

void main() {
    vTexCoord = inTexCoord;
    vColor = inParticleColor;
    
    // Compute screen-space position and size
    vec2 ndcPos = inParticlePos / uniforms.resolution * 2.0 - 1.0;
    float size = inParticleSize * uniforms.pointSizeScale;
    
    // Rotate quad corners
    float ca = cos(inParticleRotation);
    float sa = sin(inParticleRotation);
    vec2 corner = inPosition * size;
    vec2 rotated = vec2(
        corner.x * ca - corner.y * sa,
        corner.x * sa + corner.y * ca
    );
    
    gl_Position = vec4(ndcPos + rotated / uniforms.resolution * 2.0, 0.0, 1.0);
    gl_PointSize = size;
}