#include "OpenGLDevice.h"

#include <android/log.h>
#include <cstring>

#define LOG_TAG "OpenGLDevice"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace vfx {

bool OpenGLDevice::Initialize(ANativeWindow* window) {
    LOGE("OpenGLDevice::Initialize not implemented");
    return false;
}

void OpenGLDevice::Shutdown() {
    LOGE("OpenGLDevice::Shutdown not implemented");
}

void OpenGLDevice::OnSurfaceResized(uint32_t width, uint32_t height) {
    LOGE("OpenGLDevice::OnSurfaceResized not implemented");
}

Result<TextureHandle> OpenGLDevice::CreateTexture(const TextureDesc& desc) {
    LOGE("OpenGLDevice::CreateTexture not implemented");
    return Result<TextureHandle>::Fail("Not implemented");
}

void OpenGLDevice::ReleaseTexture(TextureHandle handle) {
    LOGE("OpenGLDevice::ReleaseTexture not implemented");
}

Result<BufferHandle> OpenGLDevice::CreateBuffer(size_t sizeBytes, bool hostVisible) {
    LOGE("OpenGLDevice::CreateBuffer not implemented");
    return Result<BufferHandle>::Fail("Not implemented");
}

void OpenGLDevice::ReleaseBuffer(BufferHandle handle) {
    LOGE("OpenGLDevice::ReleaseBuffer not implemented");
}

void OpenGLDevice::FillBuffer(BufferHandle handle, uint32_t data) {
    LOGE("OpenGLDevice::FillBuffer not implemented");
}

Result<ShaderModuleHandle> OpenGLDevice::CreateShaderModule(std::span<const uint32_t> spirv) {
    LOGE("OpenGLDevice::CreateShaderModule not implemented");
    return Result<ShaderModuleHandle>::Fail("Not implemented");
}

Result<PipelineHandle> OpenGLDevice::GetOrCreatePipeline(ShaderModuleHandle vs, ShaderModuleHandle fs,
                                                          TextureUsage targetUsage) {
    LOGE("OpenGLDevice::GetOrCreatePipeline not implemented");
    return Result<PipelineHandle>::Fail("Not implemented");
}

Result<PipelineHandle> OpenGLDevice::CreateComputePipeline(ShaderModuleHandle computeShader) {
    LOGE("OpenGLDevice::CreateComputePipeline not implemented");
    return Result<PipelineHandle>::Fail("Not implemented");
}

void OpenGLDevice::DispatchCompute(PipelineHandle pipeline, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    LOGE("OpenGLDevice::DispatchCompute not implemented");
}

void* OpenGLDevice::GetBufferMapped(BufferHandle handle) {
    LOGE("OpenGLDevice::GetBufferMapped not implemented");
    return nullptr;
}

void* OpenGLDevice::GetTextureMapped(TextureHandle handle) {
    LOGE("OpenGLDevice::GetTextureMapped not implemented");
    return nullptr;
}

bool OpenGLDevice::BeginFrame() {
    LOGE("OpenGLDevice::BeginFrame not implemented");
    return false;
}

void OpenGLDevice::EndFrame() {
    LOGE("OpenGLDevice::EndFrame not implemented");
}

void OpenGLDevice::DrawFullscreenPass(PipelineHandle pipeline, std::span<const TextureHandle> inputs,
                                       TextureHandle output,
                                       const std::unordered_map<std::string, float>& uniformValues) {
    LOGE("OpenGLDevice::DrawFullscreenPass not implemented");
}

void OpenGLDevice::DrawParticlePass(
    PipelineHandle pipeline,
    const ParticleDrawParams& params,
    TextureHandle output,
    const std::unordered_map<std::string, float>& uniformValues) {
    LOGE("OpenGLDevice::DrawParticlePass not implemented");
}

void OpenGLDevice::Submit() {
    LOGE("OpenGLDevice::Submit not implemented");
}

Result<TextureHandle> OpenGLDevice::ImportHardwareBuffer(HardwareBufferHandle buffer, uint32_t width,
                                                          uint32_t height) {
    LOGE("OpenGLDevice::ImportHardwareBuffer not implemented");
    return Result<TextureHandle>::Fail("Not implemented");
}

} // namespace vfx