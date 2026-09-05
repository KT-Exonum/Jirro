#pragma once
// Section 15: compatibility backend for devices with inadequate Vulkan
// support. Deliberately thin — this exists to prove GraphicsDevice isn't
// secretly Vulkan-shaped, not to get equal development investment (per the
// spec: "OpenGL ES should exist primarily as a compatibility backend").
//
// Implementation notes for whoever picks this up:
//  - EGL context on a dedicated GL thread (cannot share the Vulkan device's
//    threading assumptions if both are compiled in for capability-testing).
//  - Video frames: import AHardwareBuffer via EGL_ANDROID_get_native_client_buffer
//    + eglCreateImageKHR, bind as GL_TEXTURE_EXTERNAL_OES, sample with
//    `#extension GL_OES_EGL_image_external_essl3` in fragment shaders
//    (this is the GLES equivalent of VK_KHR_sampler_ycbcr_conversion — the
//    Phase 2 video path documented in engine/media/MediaEngine.h and
//    VulkanDevice::ImportHardwareBuffer has no GLES port yet; this is the
//    seam to add it at, should a device fail Vulkan bring-up).
//  - SPIR-V input: either ship parallel GLSL ES sources for shader nodes, or
//    cross-compile SPIR-V -> GLSL ES via SPIRV-Cross at asset-build time.
//    Prefer parallel GLSL ES sources for the small, fixed set of blend-mode
//    shaders (Section 10); use SPIRV-Cross for user-authored shader nodes.

#include "engine/core/GraphicsDevice.h"

namespace vfx {

class OpenGLDevice final : public GraphicsDevice {
public:
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
                                                TextureUsage targetUsage) override;

    bool BeginFrame() override;
    void EndFrame() override;
    void DrawFullscreenPass(PipelineHandle pipeline, std::span<const TextureHandle> inputs,
                             TextureHandle output) override;
    void Submit() override;

    Result<TextureHandle> ImportHardwareBuffer(HardwareBufferHandle buffer, uint32_t width,
                                                uint32_t height) override;

    [[nodiscard]] FrameStats GetLastFrameStats() const override { return lastFrameStats_; }

private:
    DeviceCapabilities caps_{};
    FrameStats lastFrameStats_{};
    // EGLDisplay/EGLContext/EGLSurface members intentionally omitted from
    // this header to avoid pulling <EGL/egl.h> into every translation unit
    // that includes GraphicsDevice.h transitively; define in OpenGLDevice.cpp
    // with a private impl struct (pimpl) when implementing.
};

} // namespace vfx
