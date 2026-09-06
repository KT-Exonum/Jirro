// Chroma Key fragment shader - professional green/blue screen keying
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D foregroundTexture;
layout(set = 0, binding = 1) uniform sampler2D backgroundTexture;

layout(set = 0, binding = 2) uniform ChromaKeyUniforms {
    // Key color (RGB)
    vec3 keyColorRGB;
    // Key color in HSV
    vec3 keyColorHSV;
    
    // Tolerance
    float similarity;       // How close to key color (0-1)
    float smoothness;       // Edge softness (0-1)
    
    // Spill suppression
    float spillReduction;   // Desaturate spill color
    bool advancedSpill;     // Use color difference method
    float spillThreshold;   // Threshold for spill detection
    
    // Edge refinement
    float edgeFeather;      // Feather edges
    float edgeExpand;       // Expand/contract matte
    float edgeBlur;         // Blur matte edges
    
    // Light wrap
    float lightWrap;        // Wrap background light onto foreground
    float lightWrapSize;    // Size of light wrap
    
    // Color correction on result
    float foregroundGain;
    float foregroundGamma;
    float foregroundSaturation;
    
    // View mode
    int viewMode;           // 0=Composite, 1=Matte, 2=Foreground, 3=Background, 4=Spill
    
    // Key method
    int keyMethod;          // 0=ColorDifference, 1=HSV, 2=Luminance
    
    vec2 resolution;
} uniforms;

// RGB to HSV conversion
vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0/3.0, 2.0/3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

// HSV to RGB conversion
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// Color difference keying (blue/green screen)
// Returns alpha (0 = transparent, 1 = opaque)
float colorDifferenceKey(vec3 fg, vec3 keyColor, float similarity, float smoothness) {
    // Classic color difference: alpha = 1 - (|fg - key| / similarity)
    // With smoothstep for soft edges
    vec3 diff = abs(fg - keyColor);
    float maxDiff = max(max(diff.r, diff.g), diff.b);
    float alpha = 1.0 - smoothstep(0.0, similarity, maxDiff);
    return clamp(alpha, 0.0, 1.0);
}

// HSV keying
float hsvKey(vec3 fg, vec3 keyHSV, float similarity, float smoothness) {
    vec3 fgHSV = rgb2hsv(fg);
    float hueDiff = abs(fgHSV.x - keyHSV.x);
    hueDiff = min(hueDiff, 1.0 - hueDiff); // Wrap around
    float satDiff = abs(fgHSV.y - keyHSV.y);
    float valDiff = abs(fgHSV.z - keyHSV.z);
    float diff = max(hueDiff, max(satDiff, valDiff));
    float alpha = 1.0 - smoothstep(0.0, similarity, diff);
    return clamp(alpha, 0.0, 1.0);
}

// Luminance keying
float luminanceKey(vec3 fg, float similarity, float smoothness) {
    float lum = dot(fg, vec3(0.2126, 0.7152, 0.0722));
    float alpha = 1.0 - smoothstep(0.0, similarity, lum);
    return clamp(alpha, 0.0, 1.0);
}

// Spill suppression - simple desaturation
vec3 suppressSpillSimple(vec3 fg, vec3 keyColor, float reduction) {
    vec3 gray = vec3(dot(fg, vec3(0.2126, 0.7152, 0.0722)));
    return mix(fg, gray, reduction);
}

// Advanced spill suppression - color difference method
vec3 suppressSpillAdvanced(vec3 fg, vec3 keyColor, float threshold, float reduction) {
    vec3 diff = fg - keyColor;
    float spillAmount = length(diff);
    if (spillAmount < threshold) return fg;
    
    // Project spill onto key color axis and remove
    float keyLen = length(keyColor);
    if (keyLen < 0.001) return fg;
    
    vec3 keyNorm = keyColor / keyLen;
    float spillProjection = dot(fg, keyNorm);
    vec3 spillComponent = keyNorm * spillProjection;
    vec3 cleaned = fg - spillComponent * reduction;
    return cleaned;
}

// Edge refinement on matte
float refineMatte(float alpha, float feather, float expand, float blur, vec2 uv, sampler2D matteTex) {
    if (expand != 0.0) {
        alpha = clamp(alpha + expand, 0.0, 1.0);
    }
    if (feather > 0.0) {
        alpha = smoothstep(-feather, feather, alpha - 0.5) * 0.5 + 0.5; // Simplified
    }
    // Blur would require a separate pass - simplified here
    return alpha;
}

// Light wrap - wraps background luminance onto foreground edges
vec3 lightWrap(vec3 fg, vec3 bg, float alpha, float wrapAmount, float wrapSize) {
    if (wrapAmount <= 0.0) return fg;
    
    // Simple edge detection on alpha
    // In practice, would sample neighboring alpha values
    float edgeFactor = alpha * (1.0 - alpha) * 4.0; // Peak at 0.5 alpha
    vec3 bgLum = vec3(dot(bg, vec3(0.2126, 0.7152, 0.0722)));
    return fg + bgLum * edgeFactor * wrapAmount;
}

// Apply foreground color correction
vec3 correctForeground(vec3 fg, float gain, float gamma, float saturation) {
    fg *= gain;
    fg = pow(max(fg, vec3(0.0)), vec3(1.0 / max(gamma, 0.001)));
    float lum = dot(fg, vec3(0.2126, 0.7152, 0.0722));
    return mix(vec3(lum), fg, saturation);
}

void main() {
    vec4 fg = texture(foregroundTexture, vTexCoord);
    vec4 bg = texture(backgroundTexture, vTexCoord);
    
    // Premultiply alpha for calculations
    vec3 fgRGB = fg.rgb;
    float fgAlpha = fg.a;
    
    // Generate matte (alpha) based on key method
    float matte = 1.0;
    
    if (uniforms.keyMethod == 0) { // Color Difference
        matte = colorDifferenceKey(fgRGB, uniforms.keyColorRGB, uniforms.similarity, uniforms.smoothness);
    } else if (uniforms.keyMethod == 1) { // HSV
        matte = hsvKey(fgRGB, uniforms.keyColorHSV, uniforms.similarity, uniforms.smoothness);
    } else if (uniforms.keyMethod == 2) { // Luminance
        matte = luminanceKey(fgRGB, uniforms.similarity, uniforms.smoothness);
    }
    
    // Apply edge refinement
    matte = refineMatte(matte, uniforms.edgeFeather, uniforms.edgeExpand, uniforms.edgeBlur, vTexCoord, foregroundTexture);
    
    // Spill suppression
    vec3 fgClean = fgRGB;
    if (uniforms.advancedSpill) {
        fgClean = suppressSpillAdvanced(fgRGB, uniforms.keyColorRGB, uniforms.spillThreshold, uniforms.spillReduction);
    } else if (uniforms.spillReduction > 0.0) {
        fgClean = suppressSpillSimple(fgRGB, uniforms.keyColorRGB, uniforms.spillReduction);
    }
    
    // Foreground color correction
    fgClean = correctForeground(fgClean, uniforms.foregroundGain, uniforms.foregroundGamma, uniforms.foregroundSaturation);
    
    // Composite
    vec3 compositeRGB;
    float compositeAlpha;
    
    if (uniforms.viewMode == 1) { // Matte view
        compositeRGB = vec3(matte);
        compositeAlpha = 1.0;
    } else if (uniforms.viewMode == 2) { // Foreground only
        compositeRGB = fgClean;
        compositeAlpha = matte;
    } else if (uniforms.viewMode == 3) { // Background only
        compositeRGB = bg.rgb;
        compositeAlpha = 1.0;
    } else if (uniforms.viewMode == 4) { // Spill view
        compositeRGB = fgRGB - fgClean;
        compositeAlpha = 1.0;
    } else { // Composite (default)
        compositeAlpha = matte * fgAlpha;
        compositeRGB = fgClean * matte + bg.rgb * (1.0 - matte);
        
        // Light wrap
        if (uniforms.lightWrap > 0.0) {
            compositeRGB = lightWrap(compositeRGB, bg.rgb, matte, uniforms.lightWrap, uniforms.lightWrapSize);
        }
    }
    
    outColor = vec4(compositeRGB, compositeAlpha);
}