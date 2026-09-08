#pragma once
// Section 15: Vulkan is the primary backend and gets the majority of
// development effort, but the engine (RenderGraph, Node, MediaEngine) never
// talks to Vulkan directly — everything goes through this interface, so
// OpenGLDevice can exist as a real compatibility path rather than an
// afterthought bolted on later.
//
// Design notes for implementers:
//  - All methods are expected to be called from the engine thread only
//    (see Engine.h) except where noted. No internal locking.
//  - Texture/Buffer creation should go through GraphicsDevice so pooling
//    (Section 5) has a single choke point to instrument and reuse from.
//  - BeginFrame/EndFrame bracket exactly one presented frame; Submit() may
//    be called multiple times within that bracket for off-screen graph
//    passes before the final composite is presented.

#include <memory>
#include <span>
#include <vector>

#include "Types.h"

struct ANativeWindow; // from android/native_window.h, fwd-declared to avoid
                       // pulling platform headers into engine-agnostic code

namespace vfx {

struct BufferDesc {
    size_t size = 0;
    BufferUsage usage = BufferUsage::Storage;
    bool hostVisible = false;
    std::string debugName;
};

struct DrawIndirectCommand {
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t firstVertex;
    uint32_t firstInstance;
};

struct DeviceCapabilities {
    bool supportsVulkan = false;
    uint32_t vulkanApiVersion = 0;
    bool supportsYcbcrConversion = false;   // VK_KHR_sampler_ycbcr_conversion
    bool supportsHardwareBufferImport = false; // VK_ANDROID_external_memory_android_hardware_buffer
    uint32_t maxTextureDimension = 4096;
    bool sustainedPerformanceMode = false;
    bool supportsComputeShaders = false;
    bool supportsGeometryShaders = false;
    bool supportsTessellation = false;
    uint64_t dedicatedVideoMemory = 0;
};

struct FrameStats { // Section 16/6: profiling hooks, populated per frame
    double gpuTimeMs = 0.0;
    double cpuSubmitTimeMs = 0.0;
    uint32_t drawCalls = 0;
    uint32_t pipelineSwitches = 0;
    size_t pooledTextureBytes = 0;
};

class GraphicsDevice {
public:
    virtual ~GraphicsDevice() = default;

    virtual bool Initialize(ANativeWindow* window) = 0;
    virtual void Shutdown() = 0;
    virtual void OnSurfaceResized(uint32_t width, uint32_t height) = 0;

    [[nodiscard]] virtual const DeviceCapabilities& GetCapabilities() const = 0;

    // Resource creation. Implementations should satisfy these from an
    // internal pool when `desc.transient` is true and a compatible resource
    // is idle (Section 5) rather than allocating device memory each call.
    virtual Result<TextureHandle> CreateTexture(const TextureDesc& desc) = 0;
    virtual void ReleaseTexture(TextureHandle handle) = 0; // returns to pool, does not necessarily free
    virtual Result<BufferHandle> CreateBuffer(size_t sizeBytes, bool hostVisible) = 0;
    virtual void ReleaseBuffer(BufferHandle handle) = 0;
    virtual void FillBuffer(BufferHandle handle, uint32_t data) = 0;

    // Access underlying resources (for advanced use like text rendering)
    virtual void* GetBufferMapped(BufferHandle handle) = 0;
    virtual void* GetTextureMapped(TextureHandle handle) = 0;

    // Compiled shader upload. `spirv` for VulkanDevice, ignored (or cross
    // compiled) by OpenGLDevice — see OpenGLDevice.h for that seam.
    virtual Result<ShaderModuleHandle> CreateShaderModule(std::span<const uint32_t> spirv) = 0;

    // Pipeline objects are cached internally keyed by (shaders, blend mode,
    // vertex layout) so RenderGraph can call this every frame cheaply once
    // warm (Section 16 "pipeline caching").
    virtual Result<PipelineHandle> GetOrCreatePipeline(
        ShaderModuleHandle vertexShader,
        ShaderModuleHandle fragmentShader,
        TextureUsage targetUsage) = 0;

    // Compute pipeline support for GPU particle simulation and other GPGPU work
    virtual Result<PipelineHandle> CreateComputePipeline(ShaderModuleHandle computeShader) = 0;

    // Dispatch compute shader (workgroup count)
    virtual void DispatchCompute(PipelineHandle pipeline, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;

    // Frame bracket. BeginFrame acquires the swapchain image (or, for an
    // off-screen export pass — Section 6 export pipeline — a headless
    // target); EndFrame presents (or, headless, is a no-op besides sync).
    virtual bool BeginFrame() = 0;
    virtual void EndFrame() = 0;

    // Command recording is intentionally not exposed as raw VkCommandBuffer
    // here — RenderGraph issues higher-level draw/blit/dispatch calls and
    // the backend decides how to batch them into command buffers
    // (Section 5 "command-buffer pools", Section 16 "command-buffer reuse").
    virtual void DrawFullscreenPass(
        PipelineHandle pipeline,
        std::span<const TextureHandle> inputs,
        TextureHandle output,
        const std::unordered_map<std::string, float>& uniformValues = {}) = 0;

    // Particle system: render from compute shader's storage buffer
    struct ParticleDrawParams {
        BufferHandle particleBuffer;      // storage buffer with Particle[]
        BufferHandle simParamsBuffer;     // uniform buffer with sim params
        BufferHandle indirectBuffer;      // buffer with VkDrawIndirectCommand for alive particle count
        uint32_t maxParticles;
        float pointSizeScale = 1.0f;
        bool additiveBlending = true;
        TextureHandle texture = {};       // optional sprite sheet
        float feather = 0.0f;
    };
    virtual void DrawParticlePass(
        PipelineHandle pipeline,
        const ParticleDrawParams& params,
        TextureHandle output,
        const std::unordered_map<std::string, float>& uniformValues = {}) = 0;

    virtual void Submit() = 0;

    // Import a decoded video frame without a CPU copy. Concrete meaning is
    // backend-specific: VulkanDevice imports an AHardwareBuffer via
    // VK_ANDROID_external_memory_android_hardware_buffer + ycbcr conversion;
    // OpenGLDevice binds it as a GL_TEXTURE_EXTERNAL_OES via EGLImage. See
    // engine/media/MediaEngine.h for the producer side of this handle.
    struct HardwareBufferHandle { void* nativeHandle = nullptr; };
    virtual Result<TextureHandle> ImportHardwareBuffer(HardwareBufferHandle buffer,
                                                        uint32_t width, uint32_t height) = 0;

    [[nodiscard]] virtual FrameStats GetLastFrameStats() const = 0;
};

enum class BackendKind { Vulkan, OpenGLES };

// Chooses Vulkan when DeviceCapabilities allow it, otherwise GLES 3.2.
// Implemented in Engine.cpp (needs both concrete headers), not here, to
// keep this file backend-header-free.
std::unique_ptr<GraphicsDevice> CreateGraphicsDevice(BackendKind preferred);

} // namespace vfx
