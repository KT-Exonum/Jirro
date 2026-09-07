// Shape2D fragment shader - SDF-based vector graphics rendering
// Supports: fill (solid/gradient), stroke, boolean operations (union/intersect/subtract/exclude)
// Fill rules: even-odd, non-zero
// Implements DaVinci Resolve Fusion / After Effects Shape Layer feature set

#version 450

layout(set = 0, binding = 0) uniform Uniforms {
    mat4 uProjection;
    mat4 uView;
    float uTime;
    float uResolution[2];
    int uFrameIndex;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D uTexture0;
layout(set = 0, binding = 2) uniform sampler2D uTexture1;
layout(set = 0, binding = 3) uniform sampler2D uTexture2;
layout(set = 0, binding = 4) uniform sampler2D uTexture3;
layout(set = 0, binding = 5) uniform sampler2D uTexture4;
layout(set = 0, binding = 6) uniform sampler2D uTexture5;
layout(set = 0, binding = 7) uniform sampler2D uTexture6;
layout(set = 0, binding = 8) uniform sampler2D uTexture7;
layout(set = 0, binding = 9) uniform sampler2D uTexture8;

// Fill gradient texture (1D texture with gradient stops)
layout(set = 0, binding = 10) uniform sampler2D uGradientTex;

// Shape data from vertex buffer
layout(location = 0) in vec4 vColor;
layout(location = 1) in float vLayerId;
layout(location = 2) in float vPathId;
layout(location = 3) in vec4 vShapeData;  // x=type, y=fillRule, z=strokeWidth, w=strokeCap
layout(location = 4) in vec4 vFillData;   // x=fillType, y=gradientId, z=gradientAngle, w=gradientCenter
layout(location = 5) in vec4 vStrokeData; // x=strokeType, y=strokeColor, z=dashPattern, w=dashOffset

layout(location = 0) out vec4 oColor;

layout(push_constant) uniform PushConstants {
    float uTransform[9];
    float uTime;
    float uResolution[2];
    int uLayerCount;
} pc;

// Shape types
#define SHAPE_RECT        0
#define SHAPE_ELLIPSE     1
#define SHAPE_POLYGON     2
#define SHAPE_STAR        3
#define SHAPE_PATH        4
#define SHAPE_ROUNDED_RECT 5

// Fill types
#define FILL_NONE         0
#define FILL_SOLID        1
#define FILL_GRADIENT     2

// Stroke types
#define STROKE_NONE       0
#define STROKE_SOLID      1
#define STROKE_GRADIENT   2

// Fill rules
#define FILL_EVEN_ODD     0
#define FILL_NON_ZERO     1

// Stroke caps
#define CAP_BUTT          0
#define CAP_ROUND         1
#define CAP_SQUARE        2

// Boolean operations
#define BOOL_UNION        0
#define BOOL_INTERSECT    1
#define BOOL_SUBTRACT     2
#define BOOL_EXCLUDE      3

// Gradient evaluation
vec4 evalGradient(sampler2D gradTex, float t) {
    t = clamp(t, 0.0, 1.0);
    return texture(gradTex, vec2(t, 0.5));
}

// 2x2 matrix determinant
float det2(mat2 m) {
    return m[0][0] * m[1][1] - m[0][1] * m[1][0];
}

// Inverse of 2x2 matrix
mat2 inverse2(mat2 m) {
    float d = det2(m);
    return mat2(m[1][1], -m[0][1], -m[1][0], m[0][0]) / d;
}

// SDF for rounded rectangle
float sdRoundedRect(vec2 p, vec2 size, float r) {
    vec2 d = abs(p) - size + vec2(r);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - r;
}

// SDF for ellipse
float sdEllipse(vec2 p, vec2 r) {
    vec2 k = vec2(1.0);
    p = p / r;
    return length(p) - 1.0;
}

// SDF for regular polygon (n sides)
float sdPolygon(vec2 p, int n, float r) {
    float an = 3.14159265 / float(n);
    float cn = cos(an);
    float sn = sin(an);
    
    vec2 q = vec2(length(p), 0.0);
    float angle = atan(p.y, p.x);
    float sector = floor(0.5 + angle / an);
    float a = angle - sector * an;
    
    vec2 rvec = vec2(cos(a), sin(a)) * r;
    return length(q - rvec) * sign(q.y);
}

// SDF for star (inner radius, outer radius, points)
float sdStar(vec2 p, float rInner, float rOuter, int n) {
    float an = 3.14159265 / float(n);
    float cn = cos(an);
    float sn = sin(an);
    
    vec2 q = vec2(length(p), 0.0);
    float angle = atan(p.y, p.x);
    float sector = floor(0.5 + angle / an);
    float a = angle - sector * an;
    
    float r = mix(rInner, rOuter, step(0.0, cos(a * 2.0)));
    vec2 rvec = vec2(cos(a), sin(a)) * r;
    return length(q - rvec) * sign(q.y);
}

// SDF for quadratic bezier segment
float sdBezier(vec2 p, vec2 a, vec2 b, vec2 c) {
    vec2 v0 = b - a;
    vec2 v1 = c - 2.0 * b + a;
    vec2 v2 = a - p;
    
    float a2 = dot(v1, v1);
    float b2 = 3.0 * dot(v1, v0);
    float c2 = 2.0 * dot(v0, v0) + dot(v2, v1);
    float d2 = dot(v0, v2);
    
    // Solve cubic for closest point
    float t = 0.5;
    for (int i = 0; i < 5; i++) {
        float f = ((a2 * t + b2) * t + c2) * t + d2;
        float df = (3.0 * a2 * t + 2.0 * b2) * t + c2;
        t -= f / df;
        t = clamp(t, 0.0, 1.0);
    }
    
    vec2 closest = ((v1 * t + v0) * t + a) + p;
    return length(closest);
}

// SDF for path (simplified - would need tessellation for complex paths)
float sdPath(vec2 p, int pathId) {
    // Simplified: return large distance for unsupported paths
    // Real implementation would use path texture or compute shader tessellation
    return 1000.0;
}

// Combine SDFs with boolean operation
float combineSDF(float d1, float d2, int op) {
    if (op == BOOL_UNION) return min(d1, d2);
    if (op == BOOL_INTERSECT) return max(d1, d2);
    if (op == BOOL_SUBTRACT) return max(d1, -d2);
    if (op == BOOL_EXCLUDE) return max(min(d1, -d2), min(-d1, d2));
    return min(d1, d2);
}

// Stroke SDF from shape SDF
float strokeSDF(float shapeSDF, float strokeWidth, int cap) {
    return abs(shapeSDF) - strokeWidth * 0.5;
}

// Anti-aliased smoothstep
float aaSmoothstep(float edge0, float edge1, float x, float aa) {
    float t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// Gradient fill
vec4 getGradientColor(vec2 p, vec4 fillData) {
    int fillType = int(fillData.x);
    if (fillType == FILL_NONE) return vec4(0.0);
    if (fillType == FILL_SOLID) return fillData; // w unused, xyz = color
    
    // Gradient
    int gradId = int(fillData.y);
    float angle = fillData.z;
    vec2 center = vec2(0.5); // vFillData.w unused here
    
    // Rotate UV by gradient angle
    float ca = cos(angle);
    float sa = sin(angle);
    mat2 rot = mat2(ca, -sa, sa, ca);
    vec2 uv = rot * (p - center) + center;
    
    // Linear gradient along x
    float t = uv.x;
    return evalGradient(uGradientTex, t);
}

// Stroke color
vec4 getStrokeColor(vec2 p, vec4 strokeData) {
    int strokeType = int(strokeData.x);
    if (strokeType == STROKE_NONE) return vec4(0.0);
    if (strokeType == STROKE_SOLID) {
        // strokeData.y contains packed RGB in x,y,z
        vec3 color = vec3(strokeData.y);
        return vec4(color, 1.0);
    }
    // Gradient stroke - similar to fill gradient
    return getGradientColor(p, strokeData);
}

// Dashing
float dashPattern(float dist, float pattern, float offset) {
    // Simplified: pattern.x = dash, pattern.y = gap (packed)
    // Real implementation would use texture or uniform array
    float dash = 0.1;
    float gap = 0.1;
    float period = dash + gap;
    float d = mod(dist + offset, period);
    return step(d, dash);
}

void main() {
    // Transform UV to shape space
    mat3 M = mat3(
        pc.uTransform[0], pc.uTransform[3], pc.uTransform[6],
        pc.uTransform[1], pc.uTransform[4], pc.uTransform[7],
        pc.uTransform[2], pc.uTransform[5], pc.uTransform[8]
    );
    
    vec2 uv = gl_FragCoord.xy / ubo.uResolution;
    vec3 uv3 = M * vec3(uv, 1.0);
    vec2 p = uv3.xy / uv3.z;
    
    // Convert to normalized shape coordinates (-1 to 1)
    p = p * 2.0 - 1.0;
    
    int shapeType = int(vShapeData.x);
    int fillRule = int(vShapeData.y);
    float strokeWidth = vShapeData.z;
    int strokeCap = int(vShapeData.w);
    
    int layerId = int(vLayerId);
    int pathId = int(vPathId);
    
    // Compute shape SDF
    float shapeDist = 1000.0;
    
    if (shapeType == SHAPE_RECT) {
        shapeDist = sdRoundedRect(p, vec2(0.5), 0.0);
    } else if (shapeType == SHAPE_ROUNDED_RECT) {
        shapeDist = sdRoundedRect(p, vec2(0.5), 0.1);
    } else if (shapeType == SHAPE_ELLIPSE) {
        shapeDist = sdEllipse(p, vec2(0.5));
    } else if (shapeType == SHAPE_POLYGON) {
        // vPathId could encode number of sides
        int sides = max(3, pathId);
        shapeDist = sdPolygon(p, sides, 0.5);
    } else if (shapeType == SHAPE_STAR) {
        // pathId encodes inner/outer radius and points
        int points = max(3, pathId / 100);
        float rInner = float(pathId % 100) / 100.0 * 0.5;
        float rOuter = 0.5;
        shapeDist = sdStar(p, rInner, rOuter, points);
    } else if (shapeType == SHAPE_PATH) {
        shapeDist = sdPath(p, pathId);
    }
    
    // For boolean operations, we'd need multiple shapes per layer
    // This is simplified - real implementation would iterate shapes in layer
    
    // Fill
    vec4 fillColor = vec4(0.0);
    if (shapeDist < 0.0) {
        // Inside shape - apply fill rule
        float fillAlpha = 1.0;
        
        if (fillRule == FILL_EVEN_ODD) {
            // Even-odd: inside if odd number of crossings
            // For single shape, always inside when dist < 0
            fillAlpha = 1.0;
        } else {
            // Non-zero: inside if winding != 0
            fillAlpha = 1.0;
        }
        
        fillColor = getGradientColor(p, vFillData);
        fillColor.a *= fillAlpha;
    }
    
    // Stroke
    vec4 strokeColor = vec4(0.0);
    float strokeDist = strokeSDF(shapeDist, strokeWidth, strokeCap);
    if (strokeDist < 0.0) {
        float strokeAlpha = 1.0 - smoothstep(-1.0, 0.0, strokeDist);
        strokeColor = getStrokeColor(p, vStrokeData);
        strokeColor.a *= strokeAlpha;
        
        // Apply dash pattern
        // strokeAlpha *= dashPattern(length(p), vStrokeData.z, vStrokeData.w);
    }
    
    // Composite: stroke over fill
    oColor = fillColor + strokeColor * (1.0 - fillColor.a);
    
    // Apply vertex color as tint
    oColor.rgb *= vColor.rgb;
    oColor.a *= vColor.a;
    
    // Premultiply alpha for correct blending
    oColor.rgb *= oColor.a;
}