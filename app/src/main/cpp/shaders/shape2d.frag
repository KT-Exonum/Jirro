// Shape2D fragment shader - DaVinci Resolve style vector shapes
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform Shape2DUniforms {
    // Shape type
    int shapeType;           // 0=Rectangle, 1=Ellipse, 2=Polygon, 3=Star, 4=CustomPath
    
    // Rectangle
    float rectWidth;
    float rectHeight;
    float rectCornerRadius;
    
    // Ellipse
    float ellipseWidth;
    float ellipseHeight;
    
    // Polygon
    int polygonSides;
    float polygonRadius;
    float polygonRotation;
    float polygonRoundness;
    
    // Star
    int starPoints;
    float starOuterRadius;
    float starInnerRadius;
    float starRotation;
    
    // Custom path
    int pathPointCount;
    bool pathClosed;
    
    // Transform (vector space, pre-render)
    mat3 transform;
    vec2 position;
    float rotation;
    vec2 scale;
    vec2 anchor;
    vec2 skew;
    
    // Stroke
    bool strokeEnabled;
    float strokeWidth;
    vec4 strokeColor;
    int strokeStyle;         // 0=Solid, 1=Dash, 2=Dot, 3=DashDot
    float strokeDashOffset;
    
    // Fill
    bool fillEnabled;
    int fillType;            // 0=Solid, 1=LinearGradient, 2=RadialGradient, 3=Texture
    vec4 fillColor;
    vec2 gradStart;
    vec2 gradEnd;
    vec4 gradColor1;
    vec4 gradColor2;
    
    // Boolean ops
    int booleanOp;           // 0=Union, 1=Subtract, 2=Intersect, 3=Difference, 4=XOR
    bool invertMask;
    
    // Repeater
    int repeatCount;
    vec2 repeatOffset;
    float repeatRotation;
    float repeatScale;
    float repeatOpacity;
    
    // Render
    float renderQuality;
    bool antialias;
    float feather;
    vec2 resolution;
} uniforms;

layout(set = 0, binding = 1) buffer PathBuffer {
    vec2 pathPoints[]; // custom path points
};

layout(set = 0, binding = 2) uniform sampler2D fillTexture;

// SDF functions for shapes
float sdRect(vec2 p, vec2 size, float r) {
    vec2 d = abs(p) - size + vec2(r);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - r;
}

float sdEllipse(vec2 p, vec2 r) {
    vec2 k = sqrt(r * r - r.r * r.r * p * p);
    return length(p * r - k) * sign(p.x * k.x + p.y * k.y - r.x * r.y);
}

float sdPolygon(vec2 p, int n, float r, float roundness) {
    float angle = 6.28318530718 / float(n);
    vec2 p1 = vec2(cos(uniforms.polygonRotation), sin(uniforms.polygonRotation));
    p = mat2(p1.x, -p1.y, p1.y, p1.x) * p; // rotate
    
    float a = atan(p.y, p.x) + 3.14159265359;
    float sector = floor(a / angle + 0.5);
    float a0 = sector * angle - 3.14159265359;
    vec2 pRot = mat2(cos(a0), -sin(a0), sin(a0), cos(a0)) * p;
    
    float d = length(pRot - vec2(r, 0.0));
    if (roundness > 0.0) d -= roundness;
    return d;
}

float sdStar(vec2 p, int n, float r1, float r2, float rot) {
    float angle = 3.14159265359 / float(n);
    vec2 p1 = vec2(cos(rot), sin(rot));
    p = mat2(p1.x, -p1.y, p1.y, p1.x) * p;
    
    float a = atan(p.y, p.x);
    float sector = floor(a / angle + 0.5);
    float a0 = sector * angle;
    vec2 pRot = mat2(cos(a0), -sin(a0), sin(a0), cos(a0)) * p;
    
    float d = length(pRot - vec2(r1, 0.0));
    float d2 = length(pRot - vec2(r2 * cos(angle), r2 * sin(angle)));
    return min(d, d2);
}

float sdPath(vec2 p, vec2* points, int count, bool closed) {
    float d = 1e10;
    int segCount = closed ? count : count - 1;
    for (int i = 0; i < segCount; i++) {
        vec2 a = points[i];
        vec2 b = points[(i + 1) % count];
        vec2 pa = p - a;
        vec2 ba = b - a;
        float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
        d = min(d, length(pa - ba * h));
    }
    return d;
}

// Apply transform to UV coordinates (inverse for sampling)
vec2 applyInverseTransform(vec2 uv) {
    // Convert to centered coordinates
    vec2 p = (uv - 0.5) * 2.0;
    
    // Apply skew
    p.x -= p.y * uniforms.skew.x;
    p.y -= p.x * uniforms.skew.y;
    
    // Apply inverse scale
    p /= uniforms.scale;
    
    // Apply inverse rotation
    float ca = cos(-uniforms.rotation);
    float sa = sin(-uniforms.rotation);
    p = vec2(p.x * ca - p.y * sa, p.x * sa + p.y * ca);
    
    // Apply inverse position
    p -= uniforms.position * 2.0;
    
    // Apply inverse anchor
    p += uniforms.anchor * 2.0 - 1.0;
    
    // Convert back to UV
    return p * 0.5 + 0.5;
}

// Stroke pattern
float strokePattern(float t) {
    if (uniforms.strokeStyle == 0) return 1.0; // solid
    if (uniforms.strokeStyle == 1) { // dash
        return step(0.5, fract(t * 10.0 + uniforms.strokeDashOffset));
    }
    if (uniforms.strokeStyle == 2) { // dot
        return step(0.8, fract(t * 20.0 + uniforms.strokeDashOffset));
    }
    if (uniforms.strokeStyle == 3) { // dash-dot
        float f = fract(t * 15.0 + uniforms.strokeDashOffset);
        return step(0.3, f) * step(f, 0.7) + step(0.85, f);
    }
    return 1.0;
}

// Fill color evaluation
vec4 evalFill(vec2 uv) {
    if (!uniforms.fillEnabled) return vec4(0.0);
    
    if (uniforms.fillType == 0) { // Solid
        return uniforms.fillColor;
    } else if (uniforms.fillType == 1) { // Linear gradient
        float t = dot(uv - uniforms.gradStart, uniforms.gradEnd - uniforms.gradStart) 
                / dot(uniforms.gradEnd - uniforms.gradStart, uniforms.gradEnd - uniforms.gradStart);
        t = clamp(t, 0.0, 1.0);
        return mix(uniforms.gradColor1, uniforms.gradColor2, t);
    } else if (uniforms.fillType == 2) { // Radial gradient
        float t = length(uv - uniforms.gradStart) / length(uniforms.gradEnd - uniforms.gradStart);
        t = clamp(t, 0.0, 1.0);
        return mix(uniforms.gradColor1, uniforms.gradColor2, t);
    } else if (uniforms.fillType == 3) { // Texture
        return texture(fillTexture, uv);
    }
    return vec4(0.0);
}

// Compute distance to shape (negative = inside)
float shapeSDF(vec2 uv) {
    vec2 p = applyInverseTransform(uv);
    p = p * uniforms.resolution; // convert to pixels
    
    float d = 1e10;
    
    if (uniforms.shapeType == 0) { // Rectangle
        d = sdRect(p, vec2(uniforms.rectWidth, uniforms.rectHeight) * 0.5, uniforms.rectCornerRadius);
    } else if (uniforms.shapeType == 1) { // Ellipse
        d = sdEllipse(p, vec2(uniforms.ellipseWidth, uniforms.ellipseHeight) * 0.5);
    } else if (uniforms.shapeType == 2) { // Polygon
        d = sdPolygon(p, uniforms.polygonSides, uniforms.polygonRadius, uniforms.polygonRoundness);
    } else if (uniforms.shapeType == 3) { // Star
        d = sdStar(p, uniforms.starPoints, uniforms.starOuterRadius, uniforms.starInnerRadius, uniforms.starRotation);
    } else if (uniforms.shapeType == 4 && uniforms.pathPointCount > 0) { // Custom path
        d = sdPath(p, pathPoints, uniforms.pathPointCount, uniforms.pathClosed);
    }
    
    return d;
}

// Repeater: evaluate shape at multiple transformed positions
float repeaterSDF(vec2 uv) {
    if (uniforms.repeatCount <= 1) return shapeSDF(uv);
    
    float minD = 1e10;
    for (int i = 0; i < uniforms.repeatCount; i++) {
        float t = float(i) / float(max(uniforms.repeatCount - 1, 1));
        
        // Apply repeater transform
        vec2 repUV = uv;
        repUV -= uniforms.repeatOffset * t;
        float ca = cos(uniforms.repeatRotation * t);
        float sa = sin(uniforms.repeatRotation * t);
        repUV = vec2(repUV.x * ca - repUV.y * sa, repUV.x * sa + repUV.y * ca) * uniforms.repeatScale;
        
        float d = shapeSDF(repUV);
        minD = min(minD, d);
    }
    return minD;
}

void main() {
    vec2 uv = vTexCoord;
    
    // Boolean operations need special handling - for now use single shape
    float d = repeaterSDF(uv);
    
    // Apply boolean op (simplified - would need multiple shapes for real boolean)
    // For now just use the single shape distance
    
    // Fill
    vec4 fill = vec4(0.0);
    if (uniforms.fillEnabled) {
        float fillAlpha = 1.0;
        if (uniforms.feather > 0.0) {
            fillAlpha = 1.0 - smoothstep(-uniforms.feather, uniforms.feather, d);
        } else {
            fillAlpha = d <= 0.0 ? 1.0 : 0.0;
        }
        fill = evalFill(uv) * fillAlpha;
    }
    
    // Stroke
    vec4 stroke = vec4(0.0);
    if (uniforms.strokeEnabled && uniforms.strokeWidth > 0.0) {
        float strokeDist = abs(d) - uniforms.strokeWidth * 0.5;
        float strokeAlpha = 1.0 - smoothstep(-uniforms.feather, uniforms.feather, strokeDist);
        
        // Apply stroke pattern
        float perimeter = 0.0; // Would need actual perimeter calc
        strokeAlpha *= strokePattern(perimeter * 0.01);
        
        stroke = uniforms.strokeColor * strokeAlpha;
    }
    
    // Combine fill and stroke
    vec3 color = fill.rgb + stroke.rgb;
    float alpha = max(fill.a, stroke.a);
    
    // Apply repeater opacity falloff
    if (uniforms.repeatCount > 1) {
        // This is simplified - real implementation would blend each instance
    }
    
    if (uniforms.invertMask) {
        alpha = 1.0 - alpha;
    }
    
    outColor = vec4(color, alpha);
}