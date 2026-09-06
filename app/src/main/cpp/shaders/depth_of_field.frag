// Depth of Field fragment shader - post-process DOF using depth texture
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D colorTexture;
layout(set = 0, binding = 1) uniform sampler2D depthTexture;

layout(set = 0, binding = 2) uniform DOFUniforms {
    float focalLength;      // mm
    float aperture;         // f-stop
    float focusDistance;    // mm
    float nearPlane;
    float farPlane;
    int samples;            // bokeh sample count
    float maxCoC;           // max circle of confusion (normalized 0-1)
    bool useBokehShape;     // polygonal bokeh
    int bokehSides;         // polygon sides for bokeh
    float bokehRotation;    // rotation of bokeh shape
    vec2 resolution;        // frame resolution
} uniforms;

// Convert depth buffer value to linear depth (0-1)
float linearizeDepth(float depth) {
    float n = uniforms.nearPlane;
    float f = uniforms.farPlane;
    return (2.0 * n) / (f + n - depth * (f - n));
}

// Circle of confusion calculation
float calcCoC(float depth) {
    float linearDepth = linearizeDepth(depth);
    float coc = abs(linearDepth - uniforms.focusDistance) * uniforms.aperture / 
                (uniforms.focalLength * linearDepth);
    return clamp(coc / uniforms.maxCoC, 0.0, 1.0);
}

// Polygon bokeh shape
vec2 polygonBokeh(vec2 uv, int sides, float rotation) {
    float angle = 6.28318530718 / float(sides);
    float halfAngle = angle * 0.5;
    float rotated = atan(uv.y, uv.x) + rotation;
    float r = length(uv);
    float a = mod(rotated, angle) - halfAngle;
    return uv / cos(a);
}

// Bokeh blur using randomized sampling
vec3 bokehBlur(sampler2D tex, vec2 uv, float coc) {
    if (coc < 0.01) return texture(tex, uv).rgb;
    
    vec3 color = vec3(0.0);
    float totalWeight = 0.0;
    
    // Poisson disk samples for better quality
    vec2 poisson[16] = vec2[](
        vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725),
        vec2(-0.094184101, -0.92938875), vec2(0.34495938, 0.2938776),
        vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
        vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.4737342),
        vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
        vec2(-0.2418884, 0.99706507), vec2(-0.81409955, 0.9143759),
        vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.1410079)
    );
    
    int sampleCount = min(uniforms.samples, 16);
    float radius = coc * 0.02; // Scale CoC to pixel radius
    
    for (int i = 0; i < sampleCount; i++) {
        vec2 offset = poisson[i] * radius;
        vec2 sampleUV = uv + offset;
        
        if (uniforms.useBokehShape) {
            // Apply bokeh shape to sample distribution
            sampleUV = uv + polygonBokeh(poisson[i], uniforms.bokehSides, uniforms.bokehRotation) * radius;
        }
        
        vec4 sample = texture(tex, sampleUV);
        float weight = sample.a;
        color += sample.rgb * weight;
        totalWeight += weight;
    }
    
    return color / max(totalWeight, 0.0001);
}

void main() {
    float depth = texture(depthTexture, vTexCoord).r;
    float coc = calcCoC(depth);
    
    vec3 color = texture(colorTexture, vTexCoord).rgb;
    
    if (coc > 0.01) {
        color = bokehBlur(colorTexture, vTexCoord, coc);
    }
    
    outColor = vec4(color, 1.0);
}