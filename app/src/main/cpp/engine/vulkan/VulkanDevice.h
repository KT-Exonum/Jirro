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

#include <android/asset_manager.h>
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
    void FillBuffer(BufferHandle handle, uint32_t data) override;
    void* GetBufferMapped(BufferHandle handle) override;
    void* GetTextureMapped(TextureHandle handle) override;

    Result<ShaderModuleHandle> CreateShaderModule(std::span<const uint32_t> spirv) override;
    Result<PipelineHandle> GetOrCreatePipeline(ShaderModuleHandle vs, ShaderModuleHandle fs,
                                                TextureUsage targetUsage) override;
    Result<PipelineHandle> CreateComputePipeline(ShaderModuleHandle cs) override;
    void DispatchCompute(PipelineHandle pipeline, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;

    // Dev-only: load a pre-compiled .spv from the APK's assets/ folder and
    // create/replace a shader module. Returns null handle on failure.
    Result<ShaderModuleHandle> LoadShaderFromAssets(AAssetManager* assetManager,
                                                     const std::string& assetPath);

    bool BeginFrame() override;
    void EndFrame() override;
    void DrawFullscreenPass(PipelineHandle pipeline, std::span<const TextureHandle> inputs,
                             TextureHandle output,
                             const std::unordered_map<std::string, float>& uniformValues = {}) override;
    void DrawParticlePass(
        PipelineHandle pipeline,
        const ParticleDrawParams& params,
        TextureHandle output,
        const std::unordered_map<std::string, float>& uniformValues = {}) override;
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
    [[nodiscard]] VkPhysicalDevice PhysicalDevice() const { return physicalDevice_; }
    [[nodiscard]] VkCommandBuffer CurrentCommandBuffer() const { return commandBuffers_[currentFrame_]; }

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
    bool CreateDescriptorPoolAndLayouts();
    bool CreateFrameUniformBuffers();
    uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const;
    void QueryCapabilities();

    // Phase 2 helpers for ImportHardwareBuffer.
    VkSamplerYcbcrConversion GetOrCreateYcbcrConversion(const VkExternalFormatANDROID& externalFormat,
                                                         const VkAndroidHardwareBufferFormatPropertiesANDROID& formatProps);
    VkSampler GetOrCreateYcbcrSampler(VkSamplerYcbcrConversion conversion);

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

    // Dynamically loaded Vulkan 1.3 / VK_KHR_dynamic_rendering commands.
    PFN_vkCmdBeginRendering vkCmdBeginRendering_ = nullptr;
    PFN_vkCmdEndRendering vkCmdEndRendering_ = nullptr;

    // Bring-up pipeline (Phase 1 triangle). Real graph pipelines live in
    // pipelinePool_ once Phase 3 ports blend-mode fragment shaders here.
    VkPipelineLayout bringUpLayout_ = VK_NULL_HANDLE;
    VkPipeline bringUpPipeline_ = VK_NULL_HANDLE;
    VkShaderModule bringUpVert_ = VK_NULL_HANDLE;
    VkShaderModule bringUpFrag_ = VK_NULL_HANDLE;

    VkPipelineCache pipelineCache_ = VK_NULL_HANDLE; // Section 16: persisted to disk between runs

    ResourcePool<VkTextureResource, TextureTag> textures_;
    ResourcePool<VkBufferResource, BufferTag> buffers_;
    ResourcePool<VkPipelineResource, PipelineTag> pipelines_;
    ResourcePool<VkShaderModule, ShaderModuleTag> shaderModules_;

    // Phase 3: descriptor pool and set layouts for render graph passes
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout uniformSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout paramSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout textureSetLayout1_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout textureSetLayout2_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout textureSetLayoutYcbcr_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout textureSetLayoutYcbcr1_ = VK_NULL_HANDLE;
    VkPipelineLayout graphPipelineLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout computePipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout computeDescriptorSetLayout_ = VK_NULL_HANDLE;

    // Compute pipeline cache (key = compute shader module index)
    std::unordered_map<uint32_t, PipelineHandle> computePipelineCache_;

    // Per-frame uniform buffers for render graph
    struct FrameUniformBuffers {
        BufferHandle uniformBuffer;  // Set 0: projection + resolution
        BufferHandle paramBuffer;    // Set 1: effect params
        VkDescriptorSet uniformSet;
        VkDescriptorSet paramSet;
        VkDescriptorSet textureSet;
    };
    std::array<FrameUniformBuffers, kMaxFramesInFlight> frameUniformBuffers_{};

    // Phase 2: caches so repeated frames from the same decoder (same
    // AHardwareBuffer format every time) don't recreate a
    // VkSamplerYcbcrConversion + VkSampler per frame — these are
    // comparatively expensive Vulkan objects, not something to churn at
    // 30-60fps (Section 16 "descriptor reuse" extends to these).
    std::unordered_map<YcbcrConversionKey, VkSamplerYcbcrConversion, YcbcrConversionKeyHash> ycbcrConversions_;
    std::unordered_map<VkSamplerYcbcrConversion, VkSampler> ycbcrSamplers_;

    DeviceCapabilities caps_{};
    FrameStats lastFrameStats_{};
};

} // namespace vfx
