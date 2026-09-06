// TextAnimator vertex shader - per-character transform animation
#version 450
#extension GL_KHR_vulkan_memory_model : enable

layout(set = 0, binding = 0) uniform Uniforms {
    mat4 uProjection;
    vec2 uResolution;
} uniforms;

layout(set = 1, binding = 0) uniform Params {
    vec4 textColor;
    vec4 strokeColor;
    float strokeWidth;
    float feather;
    float time;
    float progress; // 0-1 for typewriter
    float charDelay;
    int animMode; // 0=none, 1=typewriter, 2=fade-in, 3=scale-in, 4=slide-in, 5=rotate-in, 6=random
    int easing; // 0=linear, 1=ease-in, 2=ease-out, 3=ease-in-out, 4=bounce, 5=elastic
    float animDuration;
    float waveFrequency;
    float waveAmplitude;
    float waveSpeed;
} params;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inGlyphData;     // atlasPage, glyphWidth, glyphHeight, advanceX
layout(location = 3) in vec4 inInstanceData;  // x, y, charIndex, effects

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec4 vColor;
layout(location = 2) out float vStrokeWidth;
layout(location = 3) out float vFeather;
layout(location = 4) out float vCharProgress;

float ease(float t, int easing) {
    if (easing == 1) return t * t; // ease-in
    if (easing == 2) return 1.0 - (1.0 - t) * (1.0 - t); // ease-out
    if (easing == 3) return t < 0.5 ? 2.0 * t * t : 1.0 - pow(-2.0 * t + 2.0, 2.0) / 2.0; // ease-in-out
    if (easing == 4) { // bounce
        if (t < 1.0/2.75) return 7.5625 * t * t;
        if (t < 2.0/2.75) return 7.5625 * (t - 1.5/2.75) * (t - 1.5/2.75) + 0.75;
        if (t < 2.5/2.75) return 7.5625 * (t - 2.25/2.75) * (t - 2.25/2.75) + 0.9375;
        return 7.5625 * (t - 2.625/2.75) * (t - 2.625/2.75) + 0.984375;
    }
    if (easing == 5) { // elastic
        if (t == 0.0 || t == 1.0) return t;
        return -pow(2.0, 10.0 * (t - 1.0)) * sin((t - 1.1) * 5.0 * 3.14159);
    }
    return t;
}

void main() {
    vec2 glyphPos = inInstanceData.xy;
    float charIndex = inInstanceData.z;
    vec4 glyphColor = vec4(inInstanceData.w / 255.0); // packed
    
    float quadW = inGlyphData.y;
    float quadH = inGlyphData.z;
    
    // Calculate character progress for animation
    float charProgress = 1.0;
    if (params.animMode > 0) {
        float delay = charIndex * params.charDelay;
        float t = (params.time - delay) / params.animDuration;
        t = clamp(t, 0.0, 1.0);
        charProgress = ease(t, params.easing);
    }
    
    // Typewriter effect
    if (params.animMode == 1) {
        float typeProgress = params.progress;
        float charStart = charIndex / 100.0; // approximate
        charProgress = smoothstep(charStart, charStart + 0.02, typeProgress);
    }
    
    vec2 pos = glyphPos;
    float scale = 1.0;
    float rotation = 0.0;
    
    // Apply animation based on mode
    if (params.animMode == 2) { // fade-in
        // handled by alpha
    } else if (params.animMode == 3) { // scale-in
        scale = mix(0.1, 1.0, charProgress);
    } else if (params.animMode == 4) { // slide-in (from bottom)
        pos.y += mix(50.0, 0.0, charProgress);
    } else if (params.animMode == 5) { // rotate-in
        rotation = mix(3.14159, 0.0, charProgress);
    } else if (params.animMode == 6) { // random/wave
        float wave = sin(params.time * params.waveSpeed + charIndex * params.waveFrequency) * params.waveAmplitude;
        pos.y += wave * (1.0 - charProgress);
        rotation = wave * 0.1 * (1.0 - charProgress);
    }
    
    // Apply transform
    vec2 localPos = inPosition * vec2(quadW, quadH);
    localPos *= scale;
    
    if (rotation != 0.0) {
        float c = cos(rotation);
        float s = sin(rotation);
        localPos = vec2(c * localPos.x - s * localPos.y, s * localPos.x + c * localPos.y);
    }
    
    vec2 worldPos = pos + localPos;
    
    // Convert to NDC
    vec2 ndc = (worldPos / uniforms.uResolution) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    
    gl_Position = uniforms.uProjection * vec4(ndc, 0.0, 1.0);
    
    vTexCoord = inTexCoord;
    vColor = params.textColor;
    vStrokeWidth = params.strokeWidth;
    vFeather = params.feather;
    vCharProgress = charProgress;
}