// Vector source fragment shader - SDF-based vector rendering
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

// Uniforms for vector shape
layout(set = 0, binding = 0) uniform Uniforms {
    vec4 fillColor;
    vec4 strokeColor;
    float strokeWidth;
    float feather;
    int shapeType; // 0=rect, 1=circle, 2=rounded_rect, 3=custom_path
    vec4 rectParams; // x, y, width, height (normalized 0-1)
    float cornerRadius;
    int pathPointCount;
} uniforms;

// Custom path points (for custom shapes)
layout(set = 0, binding = 1) buffer PathPoints {
    vec2 points[];
} pathBuffer;

// SDF for rectangle
float sdRect(vec2 p, vec2 size) {
    vec2 d = abs(p) - size;
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

// SDF for circle
float sdCircle(vec2 p, float r) {
    return length(p) - r;
}

// SDF for rounded rectangle
float sdRoundedRect(vec2 p, vec2 size, float r) {
    vec2 d = abs(p) - size + vec2(r);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - r;
}

// SDF for custom path (simplified - uses distance to line segments)
float sdPath(vec2 p, vec2* points, int count) {
    float d = 1e10;
    for (int i = 0; i < count; i++) {
        vec2 a = points[i];
        vec2 b = points[(i + 1) % count];
        vec2 pa = p - a;
        vec2 ba = b - a;
        float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
        d = min(d, length(pa - ba * h));
    }
    return d;
}

void main() {
    // Normalize coordinates to -1 to 1
    vec2 uv = vTexCoord * 2.0 - 1.0;
    
    float dist = 0.0;
    
    if (uniforms.shapeType == 0) { // Rectangle
        vec2 size = uniforms.rectParams.zw * 0.5;
        vec2 center = uniforms.rectParams.xy * 2.0 - 1.0;
        dist = sdRect(uv - center, size);
    } else if (uniforms.shapeType == 1) { // Circle
        vec2 center = uniforms.rectParams.xy * 2.0 - 1.0;
        float radius = uniforms.rectParams.z;
        dist = sdCircle(uv - center, radius);
    } else if (uniforms.shapeType == 2) { // Rounded rectangle
        vec2 size = uniforms.rectParams.zw * 0.5;
        vec2 center = uniforms.rectParams.xy * 2.0 - 1.0;
        dist = sdRoundedRect(uv - center, size, uniforms.cornerRadius);
    } else if (uniforms.shapeType == 3 && uniforms.pathPointCount > 0) { // Custom path
        dist = sdPath(uv, pathBuffer.points, uniforms.pathPointCount);
    }
    
    // Fill
    float fillAlpha = 1.0 - smoothstep(-uniforms.feather, uniforms.feather, dist);
    
    // Stroke
    float strokeDist = abs(dist) - uniforms.strokeWidth * 0.5;
    float strokeAlpha = 1.0 - smoothstep(-uniforms.feather, uniforms.feather, strokeDist);
    
    vec3 color = uniforms.fillColor.rgb * fillAlpha + uniforms.strokeColor.rgb * strokeAlpha;
    float alpha = max(fillAlpha * uniforms.fillColor.a, strokeAlpha * uniforms.strokeColor.a);
    
    outColor = vec4(color, alpha);
}