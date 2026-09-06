// Particle vertex shader - reads from compute shader's storage buffer
#version 450
#extension GL_KHR_vulkan_memory_model : enable

layout(location = 0) in vec2 inPosition;      // quad corner (-1,-1 to 1,1)
layout(location = 1) in vec2 inTexCoord;      // texture coordinate

// Instance data from compute shader
struct Particle {
    vec2 position;
    vec2 velocity;
    vec4 color;
    float size;
    float rotation;
    float life;
    float maxLife;
    float age;
    uint active;
    float rotationSpeed;
    float sizeStart;
    float sizeEnd;
    vec4 colorStart;
    vec4 colorEnd;
    float drag;
    
    // Sub-emitter / event data
    uint emitterType;       // 0=normal, 1=sub-emitter on death, 2=sub-emitter on collision
    uint parentEmitterId;   // ID of parent emitter for sub-emitters
    uint spawnCount;        // number of particles to spawn
    float spawnProbability; // probability of spawning
};

layout(set = 0, binding = 0) buffer ParticleBuffer {
    Particle particles[];
} particleData;

layout(set = 0, binding = 1) uniform ParticleUniforms {
    vec2 resolution;
    float pointSizeScale;
    bool useAdditiveBlend;
    // Sprite sheet
    bool useSpriteSheet;
    vec2 spriteSheetSize;
    float frameRate;
    bool loopAnimation;
} uniforms;

layout(location = 0) out vec2 vTexCoord;
layout(location = 1) out vec4 vColor;
layout(location = 2) out float vLife;
layout(location = 3) out float vFrame;   // frame index for sprite sheet
layout(location = 4) out vec2 vVelocity; // velocity for motion vectors
layout(location = 5) out float vDepth;   // depth for soft particles

void main() {
    // gl_InstanceIndex comes from DrawIndexedIndirect or baseInstance
    uint idx = gl_InstanceIndex;
    Particle p = particleData.particles[idx];
    
    if (p.active == 0) {
        // Dead particle - don't render
        gl_Position = vec4(0.0);
        return;
    }
    
    vTexCoord = inTexCoord;
    
    // Interpolate color and size based on life
    float t = 1.0 - p.life; // 0 at birth, 1 at death
    vColor = mix(p.colorStart, p.colorEnd, t);
    float size = mix(p.sizeStart, p.sizeEnd, t) * uniforms.pointSizeScale;
    
    // Premultiply alpha for additive blending
    if (vColor.a > 0.0) {
        vColor.rgb *= vColor.a;
    }
    vLife = p.life;
    
    // Sprite sheet frame index
    if (uniforms.useSpriteSheet) {
        float totalFrames = uniforms.spriteSheetSize.x * uniforms.spriteSheetSize.y;
        float frame = p.age * uniforms.frameRate;
        if (uniforms.loopAnimation) {
            frame = mod(frame, uniforms.spriteSheetSize.x * uniforms.spriteSheetSize.y);
        } else {
            frame = min(frame, uniforms.spriteSheetSize.x * uniforms.spriteSheetSize.y - 1.0);
        }
        vFrame = frame;
    } else {
        vFrame = 0.0;
    }
    
    // Velocity for motion vectors
    vVelocity = p.velocity;
    
    // Depth for soft particles (linearized)
    vDepth = p.position.y / uniforms.resolution.y; // simplified depth
    
    // Compute screen-space position
    vec2 ndcPos = p.position / uniforms.resolution * 2.0 - 1.0;
    
    // Rotate quad corners
    float ca = cos(p.rotation);
    float sa = sin(p.rotation);
    vec2 corner = inPosition * size;
    vec2 rotated = vec2(
        corner.x * ca - corner.y * sa,
        corner.x * sa + corner.y * ca
    );
    
    gl_Position = vec4(ndcPos + rotated / uniforms.resolution * 2.0, 0.0, 1.0);
    gl_PointSize = size;
}