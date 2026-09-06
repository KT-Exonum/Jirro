// Particle fragment shader - life-based fading
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColor;
layout(location = 2) in float vLife;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D particleTexture; // optional sprite sheet
layout(set = 0, binding = 1) uniform ParticleUniforms {
    bool useTexture;          // use particleTexture instead of procedural
    bool softParticles;       // feather edges based on depth
    float softnessFactor;     // soft particle falloff distance
} uniforms;

void main() {
    vec2 uv = vTexCoord;
    vec4 color = vColor;
    
    if (uniforms.useTexture) {
        // Sample from particle texture / sprite sheet
        vec4 texColor = texture(particleTexture, uv);
        color *= texColor;
    } else {
        // Procedural circular particle with soft edge
        float dist = length(uv - 0.5) * 2.0;
        float alpha = 1.0 - smoothstep(0.7, 1.0, dist);
        color.a *= alpha;
    }
    
    // Fade out based on life
    color.a *= smoothstep(0.0, 0.1, vLife) * smoothstep(0.0, 0.1, vLife);
    
    // Premultiply alpha for additive blending
    if (color.a > 0.0) {
        color.rgb *= color.a;
    }
    
    outColor = color;
}