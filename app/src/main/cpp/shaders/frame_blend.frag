// Frame Blend fragment shader - optical flow based frame interpolation
// Blends between two frames using motion vectors for smooth slow-motion / frame rate conversion

#version 450
#extension GL_KHR_vulkan_memory_model : enable

layout(set = 0, binding = 0) uniform Uniforms {
    mat4 uProjection;
    vec2 uResolution;
} uniforms;

layout(set = 1, binding = 0) uniform FrameBlendParams {
    float uBlendFactor;       // 0.0 = frame0, 1.0 = frame1, 0.5 = halfway
    float uMotionScale;       // Scale for motion vectors
    int uMode;                // 0=Simple mix, 1=Optical flow, 2=Motion blur
    float uShutterAngle;      // For motion blur mode (degrees)
    int uFrameCount;          // Number of frames to blend (for motion blur)
} blendParams;

layout(set = 2, binding = 0) uniform sampler2D uFrame0;
layout(set = 2, binding = 1) uniform sampler2D uFrame1;
layout(set = 2, binding = 2) uniform sampler2D uMotionVectors; // RG = forward flow, BA = backward flow
layout(set = 2, binding = 3) uniform sampler2D uOcclusion;    // R = forward occlusion, G = backward occlusion

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

// Simple bilinear mix
vec4 simpleMix(vec2 uv, float t) {
    vec4 c0 = texture(uFrame0, uv);
    vec4 c1 = texture(uFrame1, uv);
    return mix(c0, c1, t);
}

// Warp frame by motion vectors
vec4 warpFrame(sampler2D frame, vec2 uv, vec2 flow) {
    vec2 warpedUv = uv + flow * blendParams.uMotionScale;
    warpedUv = clamp(warpedUv, vec2(0.0), vec2(1.0));
    return texture(frame, warpedUv);
}

// Optical flow blend (forward + backward warping with occlusion handling)
vec4 opticalFlowBlend(vec2 uv, float t) {
    // Sample motion vectors
    vec4 mv = texture(uMotionVectors, uv);
    vec2 forwardFlow = mv.rg;   // Frame0 -> Frame1
    vec2 backwardFlow = mv.ba;  // Frame1 -> Frame0
    
    // Sample occlusion
    vec2 occ = texture(uOcclusion, uv).rg;
    float forwardOcc = occ.r;
    float backwardOcc = occ.g;
    
    // Warp both frames to intermediate time
    vec2 tForward = forwardFlow * t;
    vec2 tBackward = backwardFlow * (1.0 - t);
    
    vec4 warped0 = warpFrame(uFrame0, uv, tForward);
    vec4 warped1 = warpFrame(uFrame1, uv, tBackward);
    
    // Blend with occlusion weighting
    float w0 = (1.0 - t) * (1.0 - forwardOcc);
    float w1 = t * (1.0 - backwardOcc);
    float wSum = w0 + w1;
    
    if (wSum < 0.001) {
        // Both occluded - fall back to simple mix
        return simpleMix(uv, t);
    }
    
    return (warped0 * w0 + warped1 * w1) / wSum;
}

// Motion blur: accumulate multiple warped frames
vec4 motionBlur(vec2 uv) {
    vec4 acc = vec4(0.0);
    float totalWeight = 0.0;
    
    vec4 mv = texture(uMotionVectors, uv);
    vec2 forwardFlow = mv.rg;
    vec2 backwardFlow = mv.ba;
    
    int samples = blendParams.uFrameCount;
    float shutter = radians(blendParams.uShutterAngle);
    
    for (int i = 0; i < samples; i++) {
        float t = float(i) / float(max(samples - 1, 1));
        float weight = 1.0;
        
        // Temporal weighting (trapezoidal)
        if (i == 0 || i == samples - 1) weight = 0.5;
        
        vec2 tForward = forwardFlow * t;
        vec2 tBackward = backwardFlow * (1.0 - t);
        
        vec4 warped0 = warpFrame(uFrame0, uv, tForward);
        vec4 warped1 = warpFrame(uFrame1, uv, tBackward);
        
        vec4 blended = mix(warped0, warped1, t);
        acc += blended * weight;
        totalWeight += weight;
    }
    
    return acc / max(totalWeight, 0.001);
}

void main() {
    vec2 uv = vUv;
    float t = blendParams.uBlendFactor;
    
    vec4 result;
    
    if (blendParams.uMode == 0) {
        // Simple cross-dissolve
        result = simpleMix(uv, t);
    } else if (blendParams.uMode == 1) {
        // Optical flow blend
        result = opticalFlowBlend(uv, t);
    } else if (blendParams.uMode == 2) {
        // Motion blur
        result = motionBlur(uv);
    } else {
        result = simpleMix(uv, t);
    }
    
    outColor = result;
}