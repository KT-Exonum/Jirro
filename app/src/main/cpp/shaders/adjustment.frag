// Adjustment layer fragment shader - applies color correction to input
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inputTexture;
layout(set = 0, binding = 1) uniform AdjustmentUniforms {
    // Lift/Gamma/Gain (ASC CDL)
    vec3 lift;
    vec3 gamma;
    vec3 gain;
    
    // Offset (ASC CDL)
    vec3 offset;
    
    // Saturation
    float saturation;
    
    // Contrast
    float contrast;
    
    // Brightness
    float brightness;
    
    // Temperature/Tint
    float temperature;
    float tint;
    
    // Vignette
    float vignetteAmount;
    float vignetteSize;
    float vignetteRoundness;
    
    // Curves (simplified - 5 control points per channel)
    vec4 curveR;
    vec4 curveG;
    vec4 curveB;
    vec4 curveM; // Master
    
    // Enabled flags
    bool enableLiftGammaGain;
    bool enableOffset;
    bool enableSaturation;
    bool enableContrastBrightness;
    bool enableTemperatureTint;
    bool enableVignette;
    bool enableCurves;
} uniforms;

vec3 applyLiftGammaGain(vec3 color) {
    vec3 result = color;
    if (uniforms.enableLiftGammaGain) {
        result = (result + uniforms.lift) * uniforms.gain;
        result = pow(max(result, vec3(0.0)), vec3(1.0) / max(uniforms.gamma, vec3(0.001)));
    }
    return result;
}

vec3 applyOffset(vec3 color) {
    if (uniforms.enableOffset) {
        return color + uniforms.offset;
    }
    return color;
}

vec3 applySaturation(vec3 color) {
    if (uniforms.enableSaturation) {
        float l = dot(color, vec3(0.2126, 0.7152, 0.0722));
        return mix(vec3(l), color, uniforms.saturation);
    }
    return color;
}

vec3 applyContrastBrightness(vec3 color) {
    if (uniforms.enableContrastBrightness) {
        return (color - 0.5) * uniforms.contrast + 0.5 + uniforms.brightness;
    }
    return color;
}

vec3 applyTemperatureTint(vec3 color) {
    if (uniforms.enableTemperatureTint) {
        // Temperature: blue-orange axis
        // Tint: green-magenta axis
        float temp = uniforms.temperature;
        float tnt = uniforms.tint;
        
        color.r *= 1.0 + temp * 0.1;
        color.b *= 1.0 - temp * 0.1;
        color.g *= 1.0 + tnt * 0.1;
    }
    return color;
}

float applyVignette(vec2 uv) {
    if (!uniforms.enableVignette) return 1.0;
    
    vec2 center = uv - 0.5;
    float dist = length(center * vec2(1.0 / uniforms.vignetteSize));
    dist = smoothstep(0.0, uniforms.vignetteRoundness, dist);
    return 1.0 - uniforms.vignetteAmount * dist;
}

vec3 applyCurves(vec3 color) {
    if (!uniforms.enableCurves) return color;
    
    // Simple 4-point curve interpolation
    auto curve = [](float x, vec4 pts) -> float {
        // pts.x = 0.0 value, pts.y = 0.33 value, pts.z = 0.66 value, pts.w = 1.0 value
        if (x < 0.33) return mix(pts.x, pts.y, x / 0.33);
        else if (x < 0.66) return mix(pts.y, pts.z, (x - 0.33) / 0.33);
        else return mix(pts.z, pts.w, (x - 0.66) / 0.34);
    };
    
    return vec3(
        curve(color.r, uniforms.curveR) * curve(color.r, uniforms.curveM),
        curve(color.g, uniforms.curveG) * curve(color.g, uniforms.curveM),
        curve(color.b, uniforms.curveB) * curve(color.b, uniforms.curveM)
    );
}

void main() {
    vec4 input = texture(inputTexture, vTexCoord);
    vec3 color = input.rgb;
    
    // Apply adjustments in order
    color = applyLiftGammaGain(color);
    color = applyOffset(color);
    color = applySaturation(color);
    color = applyContrastBrightness(color);
    color = applyTemperatureTint(color);
    color = applyCurves(color);
    
    // Vignette
    float vignette = applyVignette(vTexCoord);
    color *= vignette;
    
    outColor = vec4(clamp(color, 0.0, 1.0), input.a);
}