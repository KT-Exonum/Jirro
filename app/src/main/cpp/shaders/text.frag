// Text fragment shader - SDF glyph rendering with stroke, shadow, effects
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColor;
layout(location = 2) in float vStrokeWidth;
layout(location = 3) in float vFeather;
layout(location = 4) in vec4 vStrokeColor;
layout(location = 5) in vec4 vShadowColor;
layout(location = 6) in vec2 vShadowOffset;
layout(location = 7) in float vShadowBlur;
layout(location = 8) in int vRenderMode;

layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D fontAtlas; // SDF font atlas

// SDF smoothing
float sdfSmooth(float distance, float feather) {
    return smoothstep(-feather, feather, distance);
}

// Stroke from SDF
vec4 applyStroke(vec4 fillColor, vec4 strokeColor, float distance, float strokeWidth, float feather) {
    float strokeDist = distance - strokeWidth;
    float strokeAlpha = sdfSmooth(strokeDist, feather);
    float fillAlpha = sdfSmooth(distance, feather);
    return mix(strokeColor, fillColor, fillAlpha) * max(fillAlpha, strokeAlpha);
}

// Shadow from SDF
vec4 applyShadow(vec4 color, float distance, vec2 shadowOffset, float shadowBlur, float feather) {
    // Sample SDF at offset position
    vec2 shadowUV = vTexCoord - shadowOffset * 0.01; // Offset in UV space
    float shadowDist = texture(fontAtlas, shadowUV).r;
    float shadowAlpha = sdfSmooth(shadowDist - shadowBlur, feather);
    return vec4(vShadowColor.rgb, vShadowColor.a * shadowAlpha);
}

void main() {
    // Sample SDF from font atlas
    float distance = texture(fontAtlas, vTexCoord).r;
    
    // Base alpha from SDF
    float baseAlpha = sdfSmooth(distance, vFeather);
    
    vec4 finalColor = vec4(0.0);
    float finalAlpha = 0.0;
    
    if (vRenderMode == 0) {
        // Fill only
        finalColor = vColor;
        finalAlpha = baseAlpha;
    } else if (vRenderMode == 1) {
        // Stroke only
        finalColor = vStrokeColor;
        finalAlpha = sdfSmooth(distance - vStrokeWidth, vFeather);
    } else if (vRenderMode == 2) {
        // Fill + Stroke
        finalColor = mix(vStrokeColor, vColor, baseAlpha);
        float strokeAlpha = sdfSmooth(distance - vStrokeWidth, vFeather);
        finalAlpha = max(baseAlpha, strokeAlpha);
    } else if (vRenderMode == 3) {
        // Shadow + Fill
        vec4 shadow = applyShadow(vColor, distance, vShadowOffset, vShadowBlur, vFeather);
        finalColor = mix(shadow, vColor, baseAlpha);
        finalAlpha = max(baseAlpha, shadow.a);
    } else if (vRenderMode == 4) {
        // Shadow + Fill + Stroke
        float fillAlpha = baseAlpha;
        float strokeAlpha = sdfSmooth(distance - vStrokeWidth, vFeather);
        vec4 fillColor = vColor;
        vec4 strokeColor = vStrokeColor;
        vec4 shadow = applyShadow(vColor, distance, vShadowOffset, vShadowBlur, vFeather);
        
        vec4 combined = mix(shadow, fillColor, fillAlpha);
        combined = mix(strokeColor, combined, fillAlpha);
        finalAlpha = max(max(fillAlpha, strokeAlpha), shadow.a);
        finalColor = combined;
    }
    
    // Premultiply alpha for proper blending
    outColor = vec4(finalColor.rgb * finalAlpha, finalAlpha);
}