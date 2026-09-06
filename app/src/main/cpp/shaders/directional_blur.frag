// Directional blur fragment shader - for shake/turbulence effects
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inputTexture;

layout(set = 0, binding = 1) uniform DirectionalBlurUniforms {
    vec2 blurDirection;      // normalized direction vector
    float blurLength;        // blur distance in pixels
    int samples;             // number of samples
    float falloff;           // 0=uniform, 1=center-weighted, 2=gaussian
    bool radialBlur;         // if true, blurDirection is center point for radial blur
    vec2 center;             // center for radial blur (0-1)
    float angleOffset;       // rotation offset for spiral blur
} uniforms;

float sampleWeight(float t, float falloff) {
    if (falloff <= 0.0) return 1.0;
    if (falloff >= 2.0) return exp(-8.0 * t * t); // gaussian
    // Center-weighted (smoothstep)
    return 1.0 - smoothstep(0.0, 1.0, t);
}

void main() {
    vec2 uv = vTexCoord;
    vec4 color = vec4(0.0);
    float totalWeight = 0.0;
    
    vec2 dir = uniforms.blurDirection;
    float len = uniforms.blurLength;
    int samples = max(uniforms.samples, 1);
    
    if (uniforms.radialBlur) {
        // Radial/zoom blur
        vec2 toCenter = uniforms.center - uv;
        float dist = length(toCenter);
        if (dist > 0.0001) {
            dir = normalize(toCenter);
        } else {
            dir = vec2(0.0);
        }
        
        // Spiral component
        if (uniforms.angleOffset != 0.0) {
            float angle = uniforms.angleOffset * dist * 10.0;
            float ca = cos(angle);
            float sa = sin(angle);
            dir = vec2(dir.x * ca - dir.y * sa, dir.x * sa + dir.y * ca);
        }
        
        len *= dist * 2.0; // Scale by distance from center
    }
    
    // Sample along blur direction (both directions for centered blur)
    for (int i = -samples; i <= samples; i++) {
        float t = float(i) / float(samples);
        float weight = sampleWeight(abs(t), uniforms.falloff);
        
        vec2 sampleUV = uv + dir * len * t;
        vec4 sample = texture(inputTexture, sampleUV);
        color += sample * weight;
        totalWeight += weight;
    }
    
    outColor = color / max(totalWeight, 0.0001);
}