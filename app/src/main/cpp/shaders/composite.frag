#version 450
#extension GL_KHR_vulkan_memory_model : enable

layout(set = 0, binding = 0) uniform Uniforms {
    mat4 uProjection;
    vec2 uResolution;
} uniforms;

// Composite uniforms
layout(set = 1, binding = 0) uniform CompositeParams {
    int uMode;           // 0=over, 1=in, 2=out, 3=atop, 4=xor, 5=plus
    float uOpacity;      // foreground opacity multiplier
} compositeParams;

layout(set = 2, binding = 0) uniform sampler2D uTexture0; // background (dst)
layout(set = 2, binding = 1) uniform sampler2D uTexture1; // foreground (src)

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

// Porter-Duff compositing operators (assuming premultiplied alpha)
// For non-premultiplied input, we premultiply in the shader

vec4 Premultiply(vec4 c) {
    return vec4(c.rgb * c.a, c.a);
}

vec4 Unpremultiply(vec4 c) {
    return c.a > 0.0 ? vec4(c.rgb / c.a, c.a) : vec4(0.0);
}

void main() {
    vec4 dst = texture(uTexture0, vUv);
    vec4 src = texture(uTexture1, vUv);

    // Apply opacity to foreground
    src.a *= compositeParams.uOpacity;
    src = Premultiply(src);
    dst = Premultiply(dst);

    vec4 result;
    switch (compositeParams.uMode) {
        case 0: // Over (normal)
            result = src + dst * (1.0 - src.a);
            break;
        case 1: // In
            result = src * dst.a;
            break;
        case 2: // Out
            result = src * (1.0 - dst.a);
            break;
        case 3: // Atop
            result = src * dst.a + dst * (1.0 - src.a);
            break;
        case 4: // Xor
            result = src * (1.0 - dst.a) + dst * (1.0 - src.a);
            break;
        case 5: // Plus (additive)
            result = src + dst;
            break;
        default:
            result = src + dst * (1.0 - src.a);
    }

    outColor = Unpremultiply(result);
}