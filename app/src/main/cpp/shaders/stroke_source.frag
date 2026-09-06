// Stroke source fragment shader - freehand stroke rendering
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform Uniforms {
    vec4 strokeColor;
    float strokeWidth;
    float feather;
    float taperStart; // 0-1, where taper begins
    float taperEnd;   // 0-1, where taper ends
    int pointCount;
    vec2 resolution;
} uniforms;

layout(set = 0, binding = 1) buffer StrokePoints {
    vec2 points[]; // Flattened: x0, y0, x1, y1, ...
} strokeBuffer;

// Distance from point to line segment
float distToSegment(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a;
    vec2 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

// Get parameter t along the stroke (0 at start, 1 at end)
float getStrokeParameter(vec2 p, int index, int count) {
    if (count <= 1) return 0.0;
    return float(index) / float(count - 1);
}

void main() {
    vec2 uv = vTexCoord * uniforms.resolution; // Convert to pixel coordinates
    
    if (uniforms.pointCount < 2) {
        outColor = vec4(0.0);
        return;
    }
    
    float minDist = 1e10;
    float strokeParam = 0.0;
    
    // Find closest point on stroke
    for (int i = 0; i < uniforms.pointCount - 1; i++) {
        vec2 a = strokeBuffer.points[i * 2];
        vec2 b = strokeBuffer.points[i * 2 + 1];
        float d = distToSegment(uv, a, b);
        if (d < minDist) {
            minDist = d;
            strokeParam = getStrokeParameter(uv, i, uniforms.pointCount);
        }
    }
    
    // Apply taper (thinner at start/end)
    float width = uniforms.strokeWidth;
    if (strokeParam < uniforms.taperStart) {
        width *= smoothstep(0.0, uniforms.taperStart, strokeParam);
    } else if (strokeParam > uniforms.taperEnd) {
        width *= smoothstep(1.0, uniforms.taperEnd, strokeParam);
    }
    
    // Distance with feathering
    float dist = minDist - width * 0.5;
    float alpha = 1.0 - smoothstep(-uniforms.feather, uniforms.feather, dist);
    
    outColor = vec4(uniforms.strokeColor.rgb, alpha * uniforms.strokeColor.a);
}