#pragma once
// Shared, backend-agnostic types used across GraphicsDevice, RenderGraph,
// and Media subsystems. Kept dependency-free (no Vulkan/GLES headers) so
// higher-level code (RenderGraph, Node) never needs to know which backend
// is active.

#include <cstdint>
#include <string>
#include <string_view>

namespace vfx {

// Opaque, generation-checked handles. Never raw pointers across subsystem
// boundaries — this is what lets GraphicsDevice implementations swap freely
// and lets pools detect stale handles (use-after-return-to-pool) cheaply.
template <typename Tag>
struct Handle {
    uint32_t index = 0;
    uint32_t generation = 0;

    constexpr Handle(uint32_t i = 0, uint32_t g = 0) : index(i), generation(g) {}

    [[nodiscard]] constexpr bool IsValid() const noexcept { return generation != 0; }
    friend constexpr bool operator==(const Handle&, const Handle&) noexcept = default;
};

struct TextureTag {};
struct BufferTag {};
struct PipelineTag {};
struct ShaderModuleTag {};
struct SamplerTag {};

using TextureHandle      = Handle<TextureTag>;
using BufferHandle       = Handle<BufferTag>;
using PipelineHandle     = Handle<PipelineTag>;
using ShaderModuleHandle = Handle<ShaderModuleTag>;
using SamplerHandle      = Handle<SamplerTag>;

enum class PixelFormat : uint8_t {
    RGBA8Unorm,
    RGBA16Float,
    BGRA8Unorm,
    R8Unorm,
    // External/YUV formats produced by the hardware decoder. These map to
    // VK_FORMAT_G8_B8R8_2PLANE_420_UNORM (or vendor-specific external format)
    // behind VulkanDevice; see engine/media/MediaEngine.h.
    YCbCr420_SP,
    ExternalOES, // GLES fallback only (samplerExternalOES equivalent)
};

enum class TextureUsage : uint8_t {
    SampledOnly,
    ColorAttachment,
    ColorAttachmentAndSampled,
    Storage,
};

enum class BufferUsage : uint8_t {
    Uniform,
    Vertex,
    Index,
    Storage,
    Indirect,
};

inline BufferUsage operator|(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

struct TextureDesc {
    uint32_t width = 0;
    uint32_t height = 0;
    PixelFormat format = PixelFormat::RGBA8Unorm;
    TextureUsage usage = TextureUsage::SampledOnly;
    bool transient = true; // true => eligible for graph-lifetime pooling (Phase 3/§5)
    std::string debugName;
};

// Result/error handling: engine code avoids exceptions on the render path
// (allocation churn, and exceptions are a poor fit for a per-frame hot loop
// on mobile). Use this instead.
template <typename T>
struct Result {
    bool ok = false;
    T value{};
    std::string error;

    static Result Ok(T v) { return Result{true, std::move(v), {}}; }
    static Result Fail(std::string e) { return Result{false, T{}, std::move(e)}; }
    explicit operator bool() const noexcept { return ok; }
};

} // namespace vfx
