// Output fragment shader with onion skinning
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in int vLayer;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D currentFrame;
layout(set = 0, binding = 1) uniform sampler2DArray onionFrames; // Array of previous/next frames

layout(set = 0, binding = 2) uniform OnionSkinUniforms {
    bool enabled;
    int framesBefore;
    int framesAfter;
    float opacityBefore;
    float opacityAfter;
    vec3 colorBefore; // Red tint for past
    vec3 colorAfter;  // Blue tint for future
    float frameStep;  // Time between frames (for motion blur simulation)
} uniforms;

void main() {
    vec4 color = texture(currentFrame, vTexCoord);
    
    if (uniforms.enabled) {
        // Blend previous frames (before)
        for (int i = 0; i < uniforms.framesBefore; i++) {
            int layer = -(i + 1);
            if (layer >= -uniforms.framesBefore) {
                vec4 onion = texture(onionFrames, vec3(vTexCoord, -layer - 1));
                float opacity = uniforms.opacityBefore * (1.0 - float(i) / float(uniforms.framesBefore));
                color = mix(color, vec4(onion.rgb * uniforms.colorBefore, onion.a), opacity * onion.a);
            }
        }
        
        // Blend next frames (after)
        for (int i = 0; i < uniforms.framesAfter; i++) {
            int layer = i + 1;
            if (layer <= uniforms.framesAfter) {
                vec4 onion = texture(onionFrames, vec3(vTexCoord, layer - 1));
                float opacity = uniforms.opacityAfter * (1.0 - float(i) / float(uniforms.framesAfter));
                color = mix(color, vec4(onion.rgb * uniforms.colorAfter, onion.a), opacity * onion.a);
            }
        }
    }
    
    outColor = color;
}