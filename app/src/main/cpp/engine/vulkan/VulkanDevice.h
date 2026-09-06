#pragma once
// Phase 1 deliverable: instance/device/swapchain/pipeline capable of
// rendering a triangle, structured so Phase 3 (render graph passes) and
// Phase 2 (hardware-buffer import) extend it rather than replace it.
//
// Phase 2 addition: ImportHardwareBuffer wraps a decoder-produced
// AHardwareBuffer as a real VkImage + VkImageView with a
// VkSamplerYcbcrConversion attached, so YUV->RGB happens during texture
// sampling on the GPU (VK_KHR_sampler_ycbcr_conversion /
// VK_ANDROID_external_memory_android_hardware_buffer). See MediaEngine.h
// for the producer side.
//
// Phase 3 (first half) addition: GetOrCreatePipeline/DrawFullscreenPass now
// have a real implementation for the generic (non-video) fullscreen-pass
// case — Shader/ColorCorrection/Blur/Mask/Blend/Composite nodes sampling
// ordinary RGBA16Float textures. Sampling a VideoSource's YCbCr-backed
// texture through this path is deliberately rejected (see
// VulkanDevice.cpp's DrawFullscreenPass): that needs a pipeline variant
// with an immutable ycbcr sampler baked into its descriptor set layout
// (see GetOrCreateYcbcrSampler's doc comment below), which is a separate,
// not-yet-built pipeline family, not a generalization of this one.

#include <android/hardware_buffer.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>

#include <array>
#include <optional>
#include <unordered_map>
#include <vector>

#include "engine/core/GraphicsDevice.h"

namespace vfx {

// Simple slab-backed pool: fixed capacity, generation-checked handles, O(1)
// acquire/release. Concrete textures/buffers/pipelines each get one of
// these rather than hand-rolled bookkeeping per type (Section 5).
template <typename T, typename Tag>
class ResourcePool {
public:
    Handle<Tag> Insert(T resource) {
        uint32_t index;
        if (!freeList_.empty()) {
            index = freeList_.back();
            freeList_.pop_back();
            slots_[index] = std::move(resource);
        } else {
            index = static_cast<uint32_t>(slots_.size());
            slots_.push_back(std::move(resource));
            generations_.push_back(1);
        }
        return Handle<Tag>{index, generations_[index]};
    }

    T* Get(Handle<Tag> h) {
        if (h.index >= slots_.size() || generations_[h.index] != h.generation) return nullptr;
        return &slots_[h.index];
    }

    void Release(Handle<Tag> h) {
        if (h.index >= slots_.size() || generations_[h.index] != h.generation) return;
        generations_[h.index]++;
        freeList_.push_back(h.index);
    }

private:
    std::vector<T> slots_;
    std::vector<uint32_t> generations_;
    std::vector<uint32_t> freeList_;
};

struct VkTextureResource {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkSamplerYcbcrConversion ycbcrConversion = VK_NULL_HANDLE; // Phase 2: set for imported video frames
    VkSampler ycbcrSampler = VK_NULL_HANDLE; // a ycbcr-conversion image needs its own combined sampler
    // Phase 3: only set when this texture was created with ColorAttachment
    // or ColorAttachmentAndSampled usage — the framebuffer a fullscreen
    // pass renders into. Built once at CreateTexture time (not lazily in
    // DrawFullscreenPass) so a bad format/usage combination fails loudly at
    // creation instead of at the first draw. See GetOrCreateEffectRenderPass.
    VkFramebuffer effectFramebuffer = VK_NULL_HANDLE;
    uint32_t width = 0, height = 0;
    PixelFormat format = PixelFormat::RGBA8Unorm;
    bool externallyOwned = false; // true for imported hardware buffers: we
                                   // don't own/free the underlying memory
                                   // (AImage/AHardwareBuffer lifetime is
                                   // owned by MediaEngine's FrameCache)
    bool inUseByGpu = false;      // cleared by fence check before pool reuse
    AHardwareBuffer* sourceHardwareBuffer = nullptr; // borrowed pointer, Phase 2 bookkeeping only
};

struct VkBufferResource {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    size_t size = 0;
    void* mapped = nullptr;
};

struct VkPipelineResource {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
};

// Key for caching VkSamplerYcbcrConversion + VkSampler objects. Distinct
// AHardwareBuffer formats (e.g. two decoders emitting different YUV
// subsampling) need distinct conversion objects, but the same format should
// reuse one (Section 16 "pipeline caching" / "descriptor reuse" extends to
// these too — conversions are not cheap to create per frame).
struct YcbcrConversionKey {
    uint64_t externalFormat = 0; // VkExternalFormatANDROID.externalFormat
    friend bool operator==(const YcbcrConversionKey&, const YcbcrConversionKey&) = default;
};
struct YcbcrConversionKeyHash {
    size_t operator()(const YcbcrConversionKey& k) const noexcept {
        return std::hash<uint64_t>{}(k.externalFormat);
    }
};

// Key for the fullscreen-pipeline cache (Phase 3). Format is deliberately
// NOT part of this key — see GraphicsDevice.h's design note: every
// fullscreen-pass target is standardized on RGBA16Float, so one render
// pass/pipeline per (shader pair, blend mode) is sufficient. There is also
// no vertex-layout component (unlike the general Vulkan notion of a
// pipeline key) because every fullscreen pass shares the same
// vertex-buffer-free vertex shader.
struct FullscreenPipelineKey {
    ShaderModuleHandle vertexShader;
    ShaderModuleHandle fragmentShader;
    BlendMode blendMode;
    friend bool operator==(const FullscreenPipelineKey&, const FullscreenPipelineKey&) noexcept = default;
};
struct FullscreenPipelineKeyHash {
    size_t operator()(const FullscreenPipelineKey& k) const noexcept {
        auto hashHandle = [](const auto& h) noexcept {
            return std::hash<uint32_t>{}(h.index) ^ (std::hash<uint32_t>{}(h.generation) << 1);
        };
        size_t seed = hashHandle(k.vertexShader);
        seed ^= hashHandle(k.fragmentShader) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint8_t>{}(static_cast<uint8_t>(k.blendMode)) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        return seed;
    }
};

constexpr int kMaxFramesInFlight = 2; // double-buffered CPU/GPU sync (Section 5/16)

class VulkanDevice final : public GraphicsDevice {
public:
    VulkanDevice() = default;
    ~VulkanDevice() override;

    bool Initialize(ANativeWindow* window) override;
    void Shutdown() override;
    void OnSurfaceResized(uint32_t width, uint32_t height) override;

    [[nodiscard]] const DeviceCapabilities& GetCapabilities() const override { return caps_; }

    Result<TextureHandle> CreateTexture(const TextureDesc& desc) override;
    void ReleaseTexture(TextureHandle handle) override;
    Result<BufferHandle> CreateBuffer(size_t sizeBytes, bool hostVisible) override;
    void ReleaseBuffer(BufferHandle handle) override;

    Result<ShaderModuleHandle> CreateShaderModule(std::span<const uint32_t> spirv) override;
    Result<PipelineHandle> GetOrCreatePipeline(ShaderModuleHandle vs, ShaderModuleHandle fs,
                                                TextureUsage targetUsage,
                                                BlendMode blendMode) override;

    bool BeginFrame() override;
    void EndFrame() override;
    void DrawFullscreenPass(PipelineHandle pipeline, std::span<const TextureHandle> inputs,
                             TextureHandle output, std::span<const float> uniformFloats) override;
    void Submit() override;

    Result<TextureHandle> ImportHardwareBuffer(HardwareBufferHandle buffer, uint32_t width,
                                                uint32_t height) override;

    [[nodiscard]] FrameStats GetLastFrameStats() const override { return lastFrameStats_; }

    // Phase-1-specific: draws the bring-up triangle directly, bypassing the
    // render graph. Kept as an explicit, separate entry point so it's easy
    // to delete once Phase 3's graph can produce the same image, without
    // it silently becoming load-bearing production code.
    void RenderBringUpTriangle();

    // Exposes the raw VkDevice/queue for MediaEngine-adjacent code that
    // needs it (e.g. a future explicit ownership-transfer barrier from the
    // media thread's implicit queue to the graphics queue). Not used by
    // Phase 2's synchronous import path but kept narrow and explicit rather
    // than widening GraphicsDevice's public interface for a Phase-6 need.
    [[nodiscard]] VkDevice RawDevice() const { return device_; }

private:
    bool CreateInstance();
    bool PickPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapchain(uint32_t width, uint32_t height);
    void DestroySwapchain();
    bool CreateRenderPass();
    bool CreateFramebuffers();
    bool CreateCommandPoolAndBuffers();
    bool CreateSyncObjects();
    bool CreateBringUpPipeline();
    uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const;
    void QueryCapabilities();

    // Phase 2 helpers for ImportHardwareBuffer.
    VkSamplerYcbcrConversion GetOrCreateYcbcrConversion(const VkExternalFormatANDROID& externalFormat,
                                                         const VkAndroidHardwareBufferFormatPropertiesANDROID& formatProps);
    VkSampler GetOrCreateYcbcrSampler(VkSamplerYcbcrConversion conversion);

    // Phase 3 helpers for GetOrCreatePipeline/DrawFullscreenPass.
    // One-time setup (descriptor set layout, pipeline layout, default
    // sampler, descriptor pool) — idempotent, safe to call from every
    // GetOrCreatePipeline invocation; only does real work on the first call.
    bool EnsureFullscreenPipelineInfrastructure();
    // Render passes are cached per VkFormat (not per pipeline) so
    // CreateTexture (building a target's framebuffer) and GetOrCreatePipeline
    // (building the matching pipeline) always agree on the same render pass
    // object for a given format.
    VkRenderPass GetOrCreateEffectRenderPass(VkFormat format);
    // Hands out one of a bounded, pre-allocated set of descriptor sets for
    // this frame-in-flight slot (Section 16: no per-pass descriptor pool
    // allocation). The cursor resets at the top of BeginFrame once this
    // slot's prior fence wait proves the GPU is done reading them.
    VkDescriptorSet AcquireFullscreenDescriptorSet();

    ANativeWindow* window_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily_ = 0;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> framebuffers_;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_; // one per frame-in-flight

    std::array<VkSemaphore, kMaxFramesInFlight> imageAvailable_{};
    std::array<VkSemaphore, kMaxFramesInFlight> renderFinished_{};
    std::array<VkFence, kMaxFramesInFlight> inFlightFences_{};
    uint32_t currentFrame_ = 0;
    uint32_t currentImageIndex_ = 0;

    // Bring-up pipeline (Phase 1 triangle). Real graph pipelines live in
    // pipelines_ (below) once Phase 3 ports blend-mode fragment shaders here.
    VkPipelineLayout bringUpLayout_ = VK_NULL_HANDLE;
    VkPipeline bringUpPipeline_ = VK_NULL_HANDLE;
    VkShaderModule bringUpVert_ = VK_NULL_HANDLE;
    VkShaderModule bringUpFrag_ = VK_NULL_HANDLE;

    VkPipelineCache pipelineCache_ = VK_NULL_HANDLE; // Section 16: persisted to disk between runs

    ResourcePool<VkTextureResource, TextureTag> textures_;
    ResourcePool<VkBufferResource, BufferTag> buffers_;
    ResourcePool<VkPipelineResource, PipelineTag> pipelines_;
    ResourcePool<VkShaderModule, ShaderModuleTag> shaderModules_;

    // Phase 2: caches so repeated frames from the same decoder (same
    // AHardwareBuffer format every time) don't recreate a
    // VkSamplerYcbcrConversion + VkSampler per frame — these are
    // comparatively expensive Vulkan objects, not something to churn at
    // 30-60fps (Section 16 "descriptor reuse" extends to these).
    std::unordered_map<YcbcrConversionKey, VkSamplerYcbcrConversion, YcbcrConversionKeyHash> ycbcrConversions_;
    std::unordered_map<VkSamplerYcbcrConversion, VkSampler> ycbcrSamplers_;

    // Phase 3: fullscreen-pass pipeline infrastructure. See
    // EnsureFullscreenPipelineInfrastructure in VulkanDevice.cpp for what
    // each of these is and why it's shaped this way.
    VkDescriptorSetLayout fullscreenSetLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout fullscreenPipelineLayout_ = VK_NULL_HANDLE;
    VkSampler defaultSampler_ = VK_NULL_HANDLE; // clamp-to-edge trilinear; non-ycbcr inputs only
    VkDescriptorPool fullscreenDescriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> fullscreenDescriptorSets_; // pre-allocated, round-robin per pass
    uint32_t fullscreenDescriptorCursor_ = 0;
    std::unordered_map<VkFormat, VkRenderPass> effectRenderPasses_;
    std::unordered_map<FullscreenPipelineKey, PipelineHandle, FullscreenPipelineKeyHash> fullscreenPipelineCache_;

    DeviceCapabilities caps_{};
    FrameStats lastFrameStats_{};
};

} // namespace vfx
