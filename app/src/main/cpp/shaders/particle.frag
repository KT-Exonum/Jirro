// Particle fragment shader - life-based fading with sprite sheet, motion vectors, soft particles
#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec4 vColor;
layout(location = 2) in float vLife;
layout(location = 3) in float vFrame;  // frame index for sprite sheet animation
layout(location = 4) in vec2 vVelocity; // velocity for motion vectors
layout(location = 5) in float vDepth;   // depth for soft particles

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outMotionVector; // motion vector for temporal AA / motion blur

layout(set = 0, binding = 0) uniform sampler2D particleTexture; // optional sprite sheet
layout(set = 0, binding = 1) uniform sampler2D depthTexture;    // scene depth for soft particles
layout(set = 0, binding = 1) uniform ParticleUniforms {
    bool useTexture;          // use particleTexture instead of procedural
    bool softParticles;       // feather edges based on depth
    float softnessFactor;     // soft particle falloff distance
    // Sprite sheet animation
    bool useSpriteSheet;
    vec2 spriteSheetSize;     // number of frames (x, y)
    float frameRate;          // frames per second
    bool loopAnimation;
    // Motion vectors
    bool outputMotionVectors;
    float shutterAngle;       // shutter angle for motion blur
    // Soft particles
    float softParticleDistance;
} uniforms;

void main() {
    vec2 uv = vTexCoord;
    vec4 color = vColor;
    
    if (uniforms.useTexture) {
        if (uniforms.useSpriteSheet) {
            // Sprite sheet animation
            float totalFrames = uniforms.spriteSheetSize.x * uniforms.spriteSheetSize.y;
            float frame = vFrame;
            if (uniforms.loopAnimation) {
                frame = mod(frame, totalFrames);
            } else {
                frame = min(frame, totalFrames - 1.0);
            }
            
            // Calculate UV for the current frame
            float frameX = mod(frame, uniforms.spriteSheetSize.x);
            float frameY = floor(frame / uniforms.spriteSheetSize.x);
            
            vec2 frameSize = vec2(1.0 / uniforms.spriteSheetSize.x, 1.0 / uniforms.spriteSheetSize.y);
            vec2 frameUV = vec2(frameX, frameY) * frameSize + uv * frameSize;
            
            vec4 texColor = texture(particleTexture, frameUV);
            color *= texColor;
        } else {
            // Single texture
            vec4 texColor = texture(particleTexture, uv);
            color *= texColor;
        }
    } else {
        // Procedural circular particle with soft edge
        float dist = length(uv - 0.5) * 2.0;
        float alpha = 1.0 - smoothstep(0.7, 1.0, dist);
        color.a *= alpha;
    }
    
    // Soft particles - fade based on scene depth
    if (uniforms.softParticles && uniforms.softParticleDistance > 0.0) {
        float sceneDepth = texture(depthTexture, gl_FragCoord.xy / vec2(textureSize(depthTexture, 0))).r;
        float particleDepth = vDepth;
        float depthDiff = sceneDepth - particleDepth;
        float softAlpha = smoothstep(0.0, uniforms.softParticleDistance, depthDiff);
        color.a *= softAlpha;
    }
    
    // Fade out based on life
    color.a *= smoothstep(0.0, 0.1, vLife) * smoothstep(0.0, 0.1, vLife);
    
    // Premultiply alpha for additive blending
    if (color.a > 0.0) {
        color.rgb *= color.a;
    }
    
    // Output motion vector for temporal AA / motion blur
    if (uniforms.outputMotionVectors) {
        outMotionVector = vVelocity * uniforms.shutterAngle / 360.0;
    }
    
    outColor = color;
}