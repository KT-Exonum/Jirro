// Shape Merge / Boolean operations fragment shader
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D shapeA;
layout(set = 0, binding = 1) uniform sampler2D shapeB;

layout(set = 0, binding = 2) uniform ShapeMergeUniforms {
    int booleanOp;       // 0=Union, 1=Subtract, 2=Intersect, 3=Difference, 4=XOR
    bool invertA;
    bool invertB;
    float feather;       // edge feather for result
} uniforms;

void main() {
    vec4 a = texture(shapeA, vTexCoord);
    vec4 b = texture(shapeB, vTexCoord);
    
    if (uniforms.invertA) a = vec4(a.rgb, 1.0 - a.a);
    if (uniforms.invertB) b = vec4(b.rgb, 1.0 - a.a);
    
    float alphaA = a.a;
    float alphaB = b.a;
    float resultAlpha = 0.0;
    vec3 resultColor = vec3(0.0);
    
    if (uniforms.booleanOp == 0) { // Union (A over B)
        resultAlpha = alphaA + alphaB * (1.0 - alphaA);
        resultColor = (a.rgb * alphaA + b.rgb * alphaB * (1.0 - alphaA)) / max(resultAlpha, 0.0001);
    } else if (uniforms.booleanOp == 1) { // Subtract (A - B)
        resultAlpha = alphaA * (1.0 - alphaB);
        resultColor = a.rgb;
    } else if (uniforms.booleanOp == 2) { // Intersect
        resultAlpha = alphaA * alphaB;
        resultColor = mix(a.rgb, b.rgb, 0.5);
    } else if (uniforms.booleanOp == 3) { // Difference (B - A)
        resultAlpha = alphaB * (1.0 - alphaA);
        resultColor = b.rgb;
    } else if (uniforms.booleanOp == 4) { // XOR
        resultAlpha = alphaA + alphaB - 2.0 * alphaA * alphaB;
        resultColor = mix(a.rgb, b.rgb, alphaB / max(alphaA + alphaB, 0.0001));
    }
    
    // Apply feather to result
    if (uniforms.feather > 0.0) {
        // Would need SDF of result for proper feather - simplified here
        resultAlpha = smoothstep(-uniforms.feather, uniforms.feather, resultAlpha - 0.5);
    }
    
    outColor = vec4(resultColor, resultAlpha);
}