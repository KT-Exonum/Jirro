// Text vertex shader - SDF glyph rendering
// Each glyph is a quad with position + UV coordinates
// Instance data: glyph index, position offset, color, effects

#version 450
#extension GL_KHR_vulkan_memory_model : enable

layout(set = 0, binding = 0) uniform Uniforms {
    mat4 uProjection;
    vec2 uResolution;
} uniforms;

layout(set = 1, binding = 0) uniform Params {
    vec4 textColor;
    vec4 strokeColor;
    vec4 shadowColor;
    float strokeWidth;
    float feather;
    float shadowOffsetX;
    float shadowOffsetY;
    float shadowBlur;
    float fontSize;
    int alignment;
    float lineHeight;
    float letterSpacing;
    float wordSpacing;
    int renderMode; // 0=fill, 1=stroke, 2=fill+stroke, 3=shadow+fill
} params;

layout(location = 0) in vec2 inPosition;      // Quad position (0-1)
layout(location = 1) in vec2 inTexCoord;      // Atlas UV coordinates
layout(location = 2) in vec4 inGlyphData;     // atlasPage, glyphWidth, glyphHeight, advanceX
layout(location = 3) in vec4 inInstanceData;  // x, y, color, effects

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec4 vColor;
layout(location = 2) out float vStrokeWidth;
layout(location = 3) out float vFeather;
layout(location = 4) out vec4 vStrokeColor;
layout(location = 5) out vec4 vShadowColor;
layout(location = 6) out vec2 vShadowOffset;
layout(location = 7) out float vShadowBlur;
layout(location = 8) out int vRenderMode;

void main() {
    // Instance position
    vec2 glyphPos = inInstanceData.xy;
    vec4 glyphColor = inInstanceData.z == 0.0 ? params.textColor : 
                      vec4(inInstanceData.z, inInstanceData.w, 0.0, 1.0); // packed color
    
    // Quad position in pixels
    vec2 quadSize = inGlyphData.zw; // glyphWidth, glyphHeight
    vec2 pos = glyphPos + inPosition * quadSize;
    
    // Convert to NDC
    vec2 ndc = (pos / uniforms.uResolution) * 2.0 - 1.0;
    ndc.y = -ndc.y; // Flip Y for Vulkan
    
    gl_Position = uniforms.uProjection * vec4(ndc, 0.0, 1.0);
    
    vTexCoord = inTexCoord;
    vColor = glyphColor;
    vStrokeWidth = params.strokeWidth;
    vFeather = params.feather;
    vStrokeColor = params.strokeColor;
    vShadowColor = params.shadowColor;
    vShadowOffset = vec2(params.shadowOffsetX, params.shadowOffsetY);
    vShadowBlur = params.shadowBlur;
    vRenderMode = params.renderMode;
}