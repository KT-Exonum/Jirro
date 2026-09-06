// Time remap / velocity graph fragment shader
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D currentFrame;
layout(set = 0, binding = 1) uniform sampler2D prevFrame;
layout(set = 0, binding = 2) uniform sampler2D nextFrame;

layout(set = 0, binding = 3) uniform TimeRemapUniforms {
    // Time remap curve: maps output time -> input time
    // Stored as piecewise cubic bezier segments
    int curvePointCount;
    // Packed: [x0,y0, x1,y1, x2,y2, x3,y3, ...] where x=inputTime, y=outputTime (normalized 0-1)
} uniforms;

layout(set = 0, binding = 4) buffer CurveBuffer {
    vec2 curvePoints[]; // x=inputTime, y=outputTime (both 0-1)
};

layout(set = 0, binding = 5) uniform FrameInfo {
    float currentTime;      // current output time (normalized 0-1)
    float frameDuration;    // duration of one frame (1/fps)
    float timelineDuration; // total timeline duration
    int frameIndex;         // current frame index
    float playbackSpeed;    // can be negative for reverse
    bool enableOpticalFlow;
    bool enableFrameBlending;
} frameInfo;

// Evaluate cubic bezier curve at parameter t (0-1)
float evaluateCurve(float t) {
    if (uniforms.curvePointCount < 4) return t; // identity
    
    // Find segment
    int segmentCount = (uniforms.curvePointCount - 1) / 3;
    if (segmentCount <= 0) return t;
    
    float segmentT = t * segmentCount;
    int segment = int(min(segmentT, float(segmentCount - 1)));
    float localT = segmentT - float(segment);
    
    int base = segment * 3;
    vec2 p0 = curvePoints[base];
    vec2 p1 = curvePoints[base + 1];
    vec2 p2 = curvePoints[base + 2];
    vec2 p3 = curvePoints[base + 3];
    
    // Cubic bezier for y(outputTime) given x(inputTime) = localT
    // We need to solve for t where x(t) = localT, then return y(t)
    // Use Newton-Raphson or binary search for x(t) = localT
    
    float u = localT;
    for (int iter = 0; iter < 5; iter++) {
        float omu = 1.0 - u;
        float omu2 = omu * omu;
        float u2 = u * u;
        
        float x = omu2 * omu * p0.x + 3.0 * omu2 * u * p1.x + 3.0 * omu * u2 * p2.x + u2 * u * p3.x;
        float dx = 3.0 * (omu2 * (p1.x - p0.x) + 2.0 * omu * u * (p2.x - p1.x) + u2 * (p3.x - p2.x));
        
        if (abs(dx) < 0.0001) break;
        u -= (x - localT) / dx;
        u = clamp(u, 0.0, 1.0);
    }
    
    float omu = 1.0 - u;
    float omu2 = omu * omu;
    float u2 = u * u;
    
    return omu2 * omu * p0.y + 3.0 * omu2 * u * p1.y + 3.0 * omu * u2 * p2.y + u2 * u * p3.y;
}

// Optical flow frame interpolation (simplified - would use compute shader in practice)
vec3 opticalFlowInterpolate(vec2 uv, float t) {
    // This is a placeholder - real implementation would use a compute shader
    // to compute dense optical flow between prevFrame and nextFrame
    // For now, fall back to frame blending
    return mix(texture(prevFrame, uv).rgb, texture(nextFrame, uv).rgb, t);
}

void main() {
    // Get remapped time for current output frame
    float remappedTime = evaluateCurve(frameInfo.currentTime);
    
    // Calculate which input frames we need
    float inputFrameFloat = remappedTime * (frameInfo.timelineDuration / frameInfo.frameDuration);
    int frame0 = int(floor(inputFrameFloat));
    int frame1 = frame0 + 1;
    float frameBlend = fract(inputFrameFloat);
    
    // Apply playback speed
    if (frameInfo.playbackSpeed < 0.0) {
        frameBlend = 1.0 - frameBlend;
    }
    
    vec3 color;
    if (frameInfo.enableOpticalFlow && frameBlend > 0.001 && frameBlend < 0.999) {
        color = opticalFlowInterpolate(vTexCoord, frameBlend);
    } else if (frameInfo.enableFrameBlending && frameBlend > 0.001 && frameBlend < 0.999) {
        // Simple frame blending
        color = mix(texture(prevFrame, vTexCoord).rgb, texture(nextFrame, vTexCoord).rgb, frameBlend);
    } else {
        // Nearest frame
        color = texture(currentFrame, vTexCoord).rgb;
    }
    
    outColor = vec4(color, 1.0);
}