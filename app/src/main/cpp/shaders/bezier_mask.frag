// Bezier mask / Rotoscoping fragment shader
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D inputTexture;

layout(set = 0, binding = 1) uniform MaskUniforms {
    int splineCount;
    int totalPoints;
    float feather;           // edge feather in pixels
    float expansion;         // positive=expand, negative=contract
    float opacity;           // mask opacity
    int featherFalloff;      // 0=linear, 1=smooth, 2=gaussian
    bool invert;             // invert mask
    vec2 resolution;         // frame resolution for pixel-accurate feather
} uniforms;

layout(set = 0, binding = 2) buffer SplineBuffer {
    // Packed: [x0,y0, inTanX0,inTanY0, outTanX0,outTanY0, x1,y1, inTanX1,inTanY1, outTanX1,outTanY1, ...]
    // For each spline: first point has inTangent=(0,0), last point has outTangent=(0,0) if open
    vec4 splineData[]; // 3 vec4 per point: pos, inTan, outTan
};

// Distance from point to cubic bezier segment
float distToBezier(vec2 p, vec2 p0, vec2 p1, vec2 p2, vec2 p3) {
    // Use iterative closest point approach
    float t = 0.5;
    for (int i = 0; i < 8; i++) {
        float omt = 1.0 - t;
        float omt2 = omt * omt;
        float t2 = t * t;
        
        vec2 B = omt2 * omt * p0 + 3.0 * omt2 * t * p1 + 3.0 * omt * t2 * p2 + t2 * t * p3;
        vec2 dB = 3.0 * (omt2 * (p1 - p0) + 2.0 * omt * t * (p2 - p1) + t2 * (p3 - p2));
        
        vec2 diff = B - p;
        float denom = dot(dB, dB) + dot(diff, 
            6.0 * (omt * (p2 - 2.0 * p1 + p0) + t * (p3 - 3.0 * p2 + 3.0 * p1 - p0)));
        
        if (abs(denom) < 0.0001) break;
        t -= dot(diff, dB) / denom;
        t = clamp(t, 0.0, 1.0);
    }
    
    float omt = 1.0 - t;
    float omt2 = omt * omt;
    float t2 = t * t;
    vec2 closest = omt2 * omt * p0 + 3.0 * omt2 * t * p1 + 3.0 * omt * t2 * p2 + t2 * t * p3;
    return length(closest - p);
}

// Winding number test for point in polygon (handles self-intersecting)
bool pointInSpline(vec2 p) {
    int winding = 0;
    int pointIdx = 0;
    
    for (int s = 0; s < uniforms.splineCount; s++) {
        // Count points in this spline (simplified - assumes equal points per spline)
        int pointsPerSpline = uniforms.totalPoints / max(uniforms.splineCount, 1);
        int startIdx = s * pointsPerSpline * 3; // 3 vec4 per point
        
        for (int i = 0; i < pointsPerSpline - 1; i++) {
            int idx0 = startIdx + i * 3;
            int idx1 = startIdx + (i + 1) * 3;
            
            vec2 p0 = splineData[idx0].xy;
            vec2 p1 = splineData[idx1].xy;
            vec2 outTan0 = splineData[idx0].zw;
            vec2 inTan1 = splineData[idx1].zw;
            
            // Sample curve at multiple points for crossing test
            const int SUBDIV = 8;
            vec2 prev = p0;
            for (int j = 1; j <= SUBDIV; j++) {
                float t = float(j) / float(SUBDIV);
                float omt = 1.0 - t;
                float omt2 = omt * omt;
                float t2 = t * t;
                
                vec2 curr = omt2 * omt * p0 
                          + 3.0 * omt2 * t * (p0 + outTan0)
                          + 3.0 * omt * t2 * (p1 + inTan1)
                          + t2 * t * p1;
                
                // Crossing test
                if ((prev.y > p.y) != (curr.y > p.y)) {
                    float xIntersect = prev.x + (curr.x - prev.x) * (p.y - prev.y) / (curr.y - prev.y);
                    if (xIntersect > p.x) winding++;
                }
                prev = curr;
            }
        }
        
        // Close spline if needed
        if (splineData[startIdx].xy != splineData[startIdx + (pointsPerSpline - 1) * 3].xy) {
            int idx0 = startIdx + (pointsPerSpline - 1) * 3;
            int idx1 = startIdx;
            vec2 p0 = splineData[idx0].xy;
            vec2 p1 = splineData[idx1].xy;
            vec2 outTan0 = splineData[idx0].zw;
            vec2 inTan1 = splineData[idx1].zw;
            
            const int SUBDIV = 8;
            vec2 prev = p0;
            for (int j = 1; j <= SUBDIV; j++) {
                float t = float(j) / float(SUBDIV);
                float omt = 1.0 - t;
                float omt2 = omt * omt;
                float t2 = t * t;
                
                vec2 curr = omt2 * omt * p0 
                          + 3.0 * omt2 * t * (p0 + outTan0)
                          + 3.0 * omt * t2 * (p1 + inTan1)
                          + t2 * t * p1;
                
                if ((prev.y > p.y) != (curr.y > p.y)) {
                    float xIntersect = prev.x + (curr.x - prev.x) * (p.y - prev.y) / (curr.y - prev.y);
                    if (xIntersect > p.x) winding++;
                }
                prev = curr;
            }
        }
    }
    
    return winding != 0;
}

// Feather falloff functions
float featherFalloff(float d, float feather, int falloffType) {
    if (feather <= 0.0) return d <= 0.0 ? 1.0 : 0.0;
    
    float t = (d + feather * 0.5) / feather; // 0 at inner edge, 1 at outer edge
    t = clamp(t, 0.0, 1.0);
    
    if (falloffType == 0) return 1.0 - t; // linear
    if (falloffType == 1) return 1.0 - smoothstep(0.0, 1.0, t); // smooth
    // gaussian
    return exp(-4.0 * t * t);
}

void main() {
    vec2 uv = vTexCoord;
    vec2 pixelUV = uv * uniforms.resolution;
    
    // Check if point is inside any spline
    bool inside = pointInSpline(pixelUV);
    
    if (uniforms.invert) inside = !inside;
    
    float alpha = 0.0;
    if (inside) {
        // Compute distance to nearest edge for feathering
        float minDist = 1e10;
        int pointIdx = 0;
        
        for (int s = 0; s < uniforms.splineCount; s++) {
            int pointsPerSpline = uniforms.totalPoints / max(uniforms.splineCount, 1);
            int startIdx = s * pointsPerSpline * 3;
            
            for (int i = 0; i < pointsPerSpline - 1; i++) {
                int idx0 = startIdx + i * 3;
                int idx1 = startIdx + (i + 1) * 3;
                
                vec2 p0 = splineData[idx0].xy;
                vec2 p1 = splineData[idx1].xy;
                vec2 outTan0 = splineData[idx0].zw;
                vec2 inTan1 = splineData[idx1].zw;
                
                float d = distToBezier(pixelUV, p0, p0 + outTan0, p1 + inTan1, p1);
                minDist = min(minDist, d);
            }
            
            // Closing segment
            int idx0 = startIdx + (pointsPerSpline - 1) * 3;
            int idx1 = startIdx;
            vec2 p0 = splineData[idx0].xy;
            vec2 p1 = splineData[idx1].xy;
            vec2 outTan0 = splineData[idx0].zw;
            vec2 inTan1 = splineData[idx1].zw;
            
            float d = distToBezier(pixelUV, p0, p0 + outTan0, p1 + inTan1, p1);
            minDist = min(minDist, d);
        }
        
        // Apply expansion
        minDist -= uniforms.expansion;
        
        // Feather falloff
        alpha = featherFalloff(minDist, uniforms.feather, uniforms.featherFalloff);
    }
    
    alpha *= uniforms.opacity;
    
    // Apply mask to input
    vec4 src = texture(inputTexture, uv);
    outColor = vec4(src.rgb, src.a * alpha);
}