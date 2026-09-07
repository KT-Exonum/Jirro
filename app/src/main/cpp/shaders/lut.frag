// LUT (Look-Up Table) fragment shader - 3D LUT color grading
// Applies a 3D color lookup table for professional color grading

#version 450
#extension GL_KHR_vulkan_memory_model : enable

layout(set = 0, binding = 0) uniform Uniforms {
    mat4 uProjection;
    vec2 uResolution;
} uniforms;

layout(set = 1, binding = 0) uniform LUTParams {
    float uIntensity;         // LUT intensity (0-1)
    float uContrast;          // Contrast adjustment
    float uSaturation;        // Saturation adjustment
    int uInterpolation;       // 0=Nearest, 1=Trilinear, 2=Tetrahedral
    int uColorSpace;          // 0=sRGB, 1=Rec709, 2=LogC, 3=S-Log
} lutParams;

layout(set = 2, binding = 0) uniform sampler2D uInputTexture;
layout(set = 2, binding = 1) uniform sampler3D uLUTTexture;

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

// Color space conversions
vec3 srgbToLinear(vec3 c) {
    return pow(max(c, vec3(0.0)), vec3(2.2));
}

vec3 linearToSrgb(vec3 c) {
    return pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
}

vec3 rec709ToLinear(vec3 c) {
    return vec3(
        c.r <= 0.04045 ? c.r / 12.92 : pow((c.r + 0.055) / 1.055, 2.4),
        c.g <= 0.04045 ? c.g / 12.92 : pow((c.g + 0.055) / 1.055, 2.4),
        c.b <= 0.04045 ? c.b / 12.92 : pow((c.b + 0.055) / 1.055, 2.4)
    );
}

vec3 logCToLinear(vec3 c) {
    // ARRI LogC to linear (approximate)
    const float cut = 0.010591;
    const float a = 5.555556;
    const float b = 0.052272;
    const float c = 0.247190;
    const float d = 0.385537;
    const float e = 5.367655;
    const float f = 0.092819;
    
    vec3 linear;
    linear.r = c.r < cut ? (c.r - b) / a : exp((c.r - d) * e) - f;
    linear.g = c.g < cut ? (c.g - b) / a : exp((c.g - d) * e) - f;
    linear.b = c.b < cut ? (c.b - b) / a : exp((c.b - d) * e) - f;
    return max(linear, vec3(0.0));
}

vec3 slogToLinear(vec3 c) {
    // Sony S-Log to linear (approximate)
    return pow(10.0, (c * 0.432699 - 0.616596) / 0.6) - 0.037584;
}

vec3 toLinear(vec3 c) {
    if (lutParams.uColorSpace == 0) return srgbToLinear(c);
    if (lutParams.uColorSpace == 1) return rec709ToLinear(c);
    if (lutParams.uColorSpace == 2) return logCToLinear(c);
    if (lutParams.uColorSpace == 3) return slogToLinear(c);
    return srgbToLinear(c);
}

vec3 fromLinear(vec3 c) {
    if (lutParams.uColorSpace == 0) return linearToSrgb(c);
    if (lutParams.uColorSpace == 1) return linearToSrgb(c); // Rec709 display is same as sRGB for output
    if (lutParams.uColorSpace == 2) return linearToSrgb(c);
    if (lutParams.uColorSpace == 3) return linearToSrgb(c);
    return linearToSrgb(c);
}

// Trilinear interpolation in 3D texture
vec3 sampleLUTTrilinear(sampler3D lut, vec3 uv) {
    return texture(lut, uv).rgb;
}

// Tetrahedral interpolation for better quality
vec3 sampleLUTTetrahedral(sampler3D lut, vec3 uv) {
    // Simplified - use trilinear for now
    // Real tetrahedral would sample 4 vertices of the tetrahedron
    return texture(lut, uv).rgb;
}

void main() {
    vec4 input = texture(uInputTexture, vUv);
    vec3 color = input.rgb;
    
    // Convert to linear space
    color = toLinear(color);
    
    // Apply contrast
    color = (color - 0.5) * (1.0 + lutParams.uContrast) + 0.5;
    
    // Apply saturation
    float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(lum), color, lutParams.uSaturation);
    
    // Clamp to valid range
    color = clamp(color, 0.0, 1.0);
    
    // Sample 3D LUT
    vec3 lutColor;
    if (lutParams.uInterpolation == 0) {
        lutColor = texture(uLUTTexture, color).rgb;
    } else if (lutParams.uInterpolation == 1) {
        lutColor = sampleLUTTrilinear(uLUTTexture, color);
    } else {
        lutColor = sampleLUTTetrahedral(uLUTTexture, color);
    }
    
    // Convert back from linear
    lutColor = fromLinear(lutColor);
    
    // Blend with original based on intensity
    vec3 result = mix(color, lutColor, lutParams.uIntensity);
    
    // Final contrast/saturation from original
    result = (result - 0.5) * (1.0 + lutParams.uContrast) + 0.5;
    lum = dot(result, vec3(0.2126, 0.7152, 0.0722));
    result = mix(vec3(lum), result, lutParams.uSaturation);
    
    outColor = vec4(clamp(result, 0.0, 1.0), input.a);
}