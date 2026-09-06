#version 450
#extension GL_KHR_vulkan_memory_model : enable

layout(set = 0, binding = 0) uniform Uniforms {
    mat4 uProjection;
    vec2 uResolution;
} uniforms;

// Color correction uniforms - animated via KeyframeTrack
layout(set = 1, binding = 0) uniform ColorCorrectionParams {
    float uExposure;      // -5.0 to 5.0, default 0
    float uContrast;      // -1.0 to 1.0, default 0
    float uSaturation;    // 0.0 to 2.0, default 1
    float uTemperature;   // -1.0 to 1.0, default 0 (blue to orange)
    float uTint;          // -1.0 to 1.0, default 0 (green to magenta)
    float uHighlights;    // -1.0 to 1.0, default 0
    float uShadows;       // -1.0 to 1.0, default 0
    float uGamma;         // 0.1 to 3.0, default 1.0
} colorParams;

layout(set = 2, binding = 0) uniform sampler2D uTexture0;

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

// Simple luminance-preserving saturation adjustment
vec3 Saturation(vec3 color, float saturation) {
    vec3 luminance = vec3(0.2126, 0.7152, 0.0722);
    float l = dot(color, luminance);
    return mix(vec3(l), color, saturation);
}

// Temperature: shift toward blue (negative) or orange (positive)
vec3 Temperature(vec3 color, float temp) {
    if (temp < 0.0) {
        // Blue shift
        return mix(color, color * vec3(0.8, 0.9, 1.2), -temp);
    } else {
        // Orange shift
        return mix(color, color * vec3(1.2, 1.0, 0.8), temp);
    }
}

// Tint: shift toward green (negative) or magenta (positive)
vec3 Tint(vec3 color, float tint) {
    if (tint < 0.0) {
        // Green shift
        return mix(color, color * vec3(0.9, 1.1, 0.9), -tint);
    } else {
        // Magenta shift
        return mix(color, color * vec3(1.1, 0.9, 1.1), tint);
    }
}

// Lift/gamma/gain style shadows/highlights
vec3 LiftGammaGain(vec3 color, float shadows, float highlights, float gamma) {
    // Shadows affect dark tones, highlights affect bright tones
    float l = dot(color, vec3(0.2126, 0.7152, 0.0722));
    vec3 shadowMask = vec3(1.0) - smoothstep(0.0, 0.5, l);
    vec3 highlightMask = smoothstep(0.5, 1.0, l);

    color = color * (1.0 + shadows * shadowMask);
    color = color * (1.0 + highlights * highlightMask);
    color = pow(max(color, vec3(0.0)), vec3(1.0 / max(gamma, 0.01)));
    return color;
}

void main() {
    vec4 src = texture(uTexture0, vUv);
    vec3 color = src.rgb;

    // Exposure (multiplicative)
    color *= exp2(colorParams.uExposure);

    // Contrast (around 0.5 midpoint)
    color = (color - 0.5) * (1.0 + colorParams.uContrast) + 0.5;

    // Saturation
    color = Saturation(color, colorParams.uSaturation);

    // Temperature & Tint
    color = Temperature(color, colorParams.uTemperature);
    color = Tint(color, colorParams.uTint);

    // Shadows/Highlights/Gamma
    color = LiftGammaGain(color, colorParams.uShadows, colorParams.uHighlights, colorParams.uGamma);

    outColor = vec4(clamp(color, 0.0, 1.0), src.a);
}