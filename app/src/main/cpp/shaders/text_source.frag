// Text source fragment shader - SDF font rendering
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform Uniforms {
    vec4 textColor;
    vec4 backgroundColor;
    vec4 strokeColor;
    float strokeWidth;
    float feather;
    float fontSize;
    int alignment; // 0=left, 1=center, 2=right
    float lineHeight;
    vec2 resolution;
} uniforms;

layout(set = 0, binding = 1) uniform sampler2D fontAtlas; // SDF font atlas

// Simple SDF text rendering - in production would use a proper text layout engine
// This is a placeholder that renders a single line of text
void main() {
    vec2 uv = vTexCoord;
    
    // Background
    vec4 bgColor = uniforms.backgroundColor;
    
    // For now, just render a placeholder - real implementation would:
    // 1. Layout text using HarfBuzz/FreeType
    // 2. Generate glyph positions
    // 3. Sample SDF font atlas
    // 4. Apply stroke, shadow, etc.
    
    // Placeholder: render a simple rectangle as text placeholder
    float dist = max(abs(uv.x - 0.5) - 0.4, abs(uv.y - 0.5) - 0.1);
    float alpha = 1.0 - smoothstep(-uniforms.feather, uniforms.feather, dist);
    
    outColor = mix(bgColor, uniforms.textColor, alpha);
}