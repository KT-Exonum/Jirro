// Motion blur fragment shader - temporal accumulation with velocity buffer
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec2 vTexCoordPrev;
layout(location = 2) in vec2 vTexCoordNext;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D currentFrame;
layout(set = 0, binding = 1) uniform sampler2D prevFrame;
layout(set = 0, binding = 2) uniform sampler2D nextFrame;
layout(set = 0, binding = 3) uniform sampler2D velocityBuffer; // RG16F: velocity.x, velocity.y

layout(set = 0, binding = 4) uniform MotionBlurUniforms {
    float shutterAngle;       // degrees (180 = 50% shutter)
    float shutterPhase;       // degrees (-90 = centered)
    int sampleCount;          // number of temporal samples
    float sampleDistribution; // 0=uniform, 1=gaussian
    bool useVelocityBuffer;   // use velocity buffer for object motion blur
    float maxBlurRadius;      // clamp extreme blur
    float blurLength;         // multiplier for transform-based blur
    vec2 resolution;          // frame resolution
} uniforms;

vec3 sampleFrame(sampler2D frame, vec2 uv) {
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return vec3(0.0);
    return texture(frame, uv).rgb;
}

// Gaussian distribution for sample weights
float gaussianWeight(float x, float sigma) {
    return exp(-0.5 * x * x / (sigma * sigma)) / (sigma * sqrt(6.28318530718));
}

void main() {
    vec3 color = texture(currentFrame, vTexCoord).rgb;
    float alpha = texture(currentFrame, vTexCoord).a;
    
    if (!uniforms.useVelocityBuffer || uniforms.sampleCount <= 1) {
        // Simple transform-based motion blur using prev/next frames
        float shutter = uniforms.shutterAngle / 360.0; // 0.5 for 180 degrees
        float phase = uniforms.shutterPhase / 360.0;   // -0.25 for -90 degrees
        
        int samples = max(uniforms.sampleCount, 1);
        vec3 accum = color;
        float totalWeight = 1.0;
        
        // Sample between prev and next frames
        for (int i = 1; i < samples; i++) {
            float t = float(i) / float(samples);
            float weight = 1.0;
            
            if (uniforms.sampleDistribution > 0.0) {
                // Gaussian distribution centered on current frame
                float center = 0.5 + phase;
                weight = gaussianWeight(t - center, 0.25);
            }
            
            vec2 uv = mix(vTexCoordPrev, vTexCoordNext, t);
            vec3 sampleColor = mix(
                texture(prevFrame, uv).rgb,
                texture(nextFrame, uv).rgb,
                t
            );
            accum += sampleColor * weight;
            totalWeight += weight;
        }
        
        color = accum / totalWeight;
    } else {
        // Velocity buffer based motion blur (per-pixel)
        vec2 velocity = texture(velocityBuffer, vTexCoord).rg * uniforms.resolution;
        float velLen = length(velocity);
        
        if (velLen > 0.5) {
            int samples = min(uniforms.sampleCount, 32);
            vec3 accum = color;
            float totalWeight = 1.0;
            
            // Sample along velocity vector (both directions for centered blur)
            vec2 velDir = normalize(velocity);
            float maxDist = min(velLen * uniforms.blurLength, uniforms.maxBlurRadius);
            
            for (int i = 1; i <= samples; i++) {
                // Sample both forward and backward
                float tForward = float(i) / float(samples + 1);
                float tBackward = -tForward;
                
                float weight = 1.0;
                if (uniforms.sampleDistribution > 0.0) {
                    weight = gaussianWeight(tForward, 0.3);
                }
                
                // Forward sample
                vec2 uvF = vTexCoord + velDir * maxDist * tForward;
                vec3 sampleF = sampleFrame(currentFrame, uvF);
                accum += sampleF * weight;
                totalWeight += weight;
                
                // Backward sample
                vec2 uvB = vTexCoord + velDir * maxDist * tBackward;
                vec3 sampleB = sampleFrame(currentFrame, uvB);
                accum += sampleB * weight;
                totalWeight += weight;
            }
            
            color = accum / totalWeight;
        }
    }
    
    outColor = vec4(color, alpha);
}