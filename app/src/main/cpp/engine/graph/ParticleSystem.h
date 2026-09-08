#pragma once
// Particle system data structures shared between CPU and GPU

#include <cstdint>
#include <vector>

namespace vfx {

// Particle structure for GPU storage buffer (matches compute shader)
struct Particle {
    float position[2];      // 8 bytes
    float velocity[2];      // 8 bytes
    float color[4];         // 16 bytes
    float size;             // 4 bytes
    float rotation;         // 4 bytes
    float life;             // 4 bytes
    float maxLife;          // 4 bytes
    float age;              // 4 bytes
    uint32_t active;        // 4 bytes
    float rotationSpeed;    // 4 bytes
    float sizeStart;        // 4 bytes
    float sizeEnd;          // 4 bytes
    float colorStart[4];    // 16 bytes
    float colorEnd[4];      // 16 bytes
    float drag;             // 4 bytes
    float pad[2];           // 8 bytes padding to 112
    // Total: 112 bytes per particle
};

static_assert(sizeof(Particle) == 112, "Particle struct must be 112 bytes for GPU alignment");

// Simulation parameters for compute shader (matches compute shader uniform)
struct ParticleSimParams {
    float resolution[2];         // 8 bytes
    float deltaTime;             // 4
    float emitRate;              // 4
    float emitRateVariation;     // 4
    uint32_t emitterShape;       // 4
    float emitPosition[2];       // 8
    float emitRadius;            // 4
    float emitLineStart[2];      // 8
    float emitLineEnd[2];        // 8
    float initialLife;           // 4
    float lifeVariation;         // 4
    float initialSpeed;          // 4
    float speedVariation;        // 4
    float emitAngle;             // 4
    float angleVariation;        // 4
    float gravity;               // 4
    float windX;                 // 4
    float windY;                 // 4
    float turbulence;            // 4
    float drag;                  // 4
    float deltaTimeInv;          // 4
    uint32_t maxParticles;       // 4
    uint32_t frameIndex;         // 4
    uint32_t seed;               // 4
    // Total: 120 bytes, padded to 128 for alignment
};

struct ParticleRenderParams {
    float resolution[2];
    float pointSizeScale;
    int32_t useAdditiveBlend;
    int32_t pad;
};

// Particle system state for CPU-side management
class ParticleSystem {
public:
    struct Config {
        uint32_t maxParticles;
        uint32_t seed;
        constexpr Config() : maxParticles(10000), seed(12345) {}
    };

    explicit ParticleSystem(const Config& config = Config());
    ~ParticleSystem();

    // Initialize Vulkan resources
    bool Initialize(class VulkanDevice* device, uint32_t maxParticles);
    void Shutdown();

    // Update simulation (GPU compute)
    void Simulate(double deltaTime, const struct ParticleConfig& config);

    // Render particles
    void Render(class VulkanDevice* device, uint32_t currentFrame, uint32_t imageIndex);

    // Getters
    uint32_t GetMaxParticles() const { return maxParticles_; }
    bool IsInitialized() const { return initialized_; }

private:
    struct BufferHandles {
        uint64_t particleBuffer = 0;
        uint64_t simParamsBuffer = 0;
        uint64_t renderParamsBuffer = 0;
        uint64_t instanceBuffer = 0;  // for indirect draw
    } buffers_;

    struct PipelineHandles {
        uint64_t computePipeline = 0;
        uint64_t renderPipeline = 0;
    } pipelines_;

    struct DescriptorSets {
        uint64_t computeSet = 0;
        uint64_t renderSet = 0;
    } descriptorSets_;

    uint32_t maxParticles_ = 0;
    bool initialized_ = false;
    uint32_t frameIndex_ = 0;
    uint32_t seed_ = 12345;
};

} // namespace vfx