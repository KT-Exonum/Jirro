// TextPath vertex shader - text along bezier path
#version 450

layout(set = 0, binding = 0) uniform Uniforms {
    mat4 uProjection;
    vec2 uResolution;
} uniforms;

layout(set = 1, binding = 0) uniform Params {
    vec4 textColor;
    vec4 strokeColor;
    float strokeWidth;
    float feather;
    float pathOffset; // 0-1 along path
    float pathScale;
    bool pathFlip;
    int alignment; // 0=left, 1=center, 2=right
    float pathLength;
} params;

// Path data passed via push constants or storage buffer
layout(push_constant) uniform PathData {
    vec2 points[64]; // Flattened bezier points
    int pointCount;
    int curveCount;
} pathData;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inGlyphData;
layout(location = 3) in vec4 inInstanceData; // charIndex, advanceX, lineOffset, effects

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec4 vColor;
layout(location = 2) out float vStrokeWidth;
layout(location = 3) out float vFeather;

// Quadratic bezier evaluation
vec2 evalQuadBezier(vec2 p0, vec2 p1, vec2 p2, float t) {
    float u = 1.0 - t;
    return u*u*p0 + 2.0*u*t*p1 + t*t*p2;
}

// Cubic bezier evaluation
vec2 evalCubicBezier(vec2 p0, vec2 p1, vec2 p2, vec2 p3, float t) {
    float u = 1.0 - t;
    return u*u*u*p0 + 3.0*u*u*t*p1 + 3.0*u*t*t*p2 + t*t*t*p3;
}

// Get position and tangent on path at distance
vec2 getPathPosition(float distance, out vec2 tangent) {
    float d = 0.0;
    vec2 pos = vec2(0.0);
    tangent = vec2(1.0, 0.0);
    
    // Simplified: linear interpolation along path segments
    // Real implementation would use arc-length parameterization
    for (int i = 0; i < pathData.curveCount; ++i) {
        int base = i * 3;
        vec2 p0 = pathData.points[base];
        vec2 p1 = pathData.points[base + 1];
        vec2 p2 = pathData.points[base + 2];
        
        // Approximate segment length
        float segLen = length(p1 - p0) + length(p2 - p1);
        
        if (d + segLen >= distance) {
            float t = (distance - d) / segLen;
            pos = evalQuadBezier(p0, p1, p2, t);
            vec2 dp0 = 2.0 * (p1 - p0);
            vec2 dp1 = 2.0 * (p2 - p1);
            tangent = normalize(mix(dp0, dp1, t));
            break;
        }
        d += segLen;
    }
    return pos;
}

void main() {
    float charIndex = inInstanceData.x;
    float advanceX = inInstanceData.y;
    float lineOffset = inInstanceData.z;
    
    // Distance along path for this character
    float charDist = params.pathOffset * params.pathLength + charIndex * advanceX * params.pathScale;
    
    vec2 tangent;
    vec2 pos = getPathPosition(charDist, tangent);
    
    // Normal perpendicular to tangent
    vec2 normal = vec2(-tangent.y, tangent.x);
    if (params.pathFlip) normal = -normal;
    
    // Glyph local position
    vec2 localPos = inPosition * inGlyphData.zw; // glyphWidth, glyphHeight
    localPos *= params.pathScale;
    
    // Offset by line
    localPos.y += lineOffset;
    
    // Position along path
    vec2 worldPos = pos + normal * localPos.y + tangent * localPos.x;
    
    // Convert to NDC
    vec2 ndc = (worldPos / uniforms.uResolution) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    
    gl_Position = uniforms.uProjection * vec4(ndc, 0.0, 1.0);
    
    vTexCoord = inTexCoord;
    vColor = params.textColor;
    vStrokeWidth = params.strokeWidth;
    vFeather = params.feather;
}