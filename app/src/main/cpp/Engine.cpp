#include "Engine.h"

#include <android/log.h>
#include <cstdlib>

#include "engine/opengl/OpenGLDevice.h"
#include "engine/vulkan/VulkanDevice.h"

#define LOG_TAG "Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

namespace vfx {

// Declared in GraphicsDevice.h, defined here because this is the one
// translation unit allowed to know about both concrete backends — keeps
// VulkanDevice.h/OpenGLDevice.h from needing to know about each other.
std::unique_ptr<GraphicsDevice> CreateGraphicsDevice(BackendKind preferred) {
    if (preferred == BackendKind::Vulkan) return std::make_unique<VulkanDevice>();
    return std::make_unique<OpenGLDevice>();
}

Engine::Engine() : timeline_(std::make_unique<Timeline>(/*frameRate=*/30.0)) {}

Engine::~Engine() { Stop(); }

void Engine::AttachSurface(ANativeWindow* window) {
    std::lock_guard<std::mutex> lock(windowMutex_);
    if (pendingWindow_) ANativeWindow_release(pendingWindow_);
    pendingWindow_ = window;
    if (window) ANativeWindow_acquire(window);
    surfaceDirty_.store(true, std::memory_order_release);
}

void Engine::DetachSurface() { AttachSurface(nullptr); }

void Engine::Start() {
    if (running_.exchange(true)) return; // already running
    engineThread_ = std::jthread(&Engine::ThreadMain, this, engineStopSource_.get_token());
}

void Engine::Stop() {
    if (!running_.exchange(false)) return;
    engineStopSource_.request_stop();
    if (engineThread_.joinable()) engineThread_.join();
    if (mediaEngine_) mediaEngine_->Stop(); // join the media thread before tearing down the device it imports into
    if (exportPipeline_) exportPipeline_->Cancel();
    if (device_) device_->Shutdown();
    std::lock_guard<std::mutex> lock(windowMutex_);
    if (pendingWindow_) {
        ANativeWindow_release(pendingWindow_);
        pendingWindow_ = nullptr;
    }
}

void Engine::ThreadMain(std::stop_token stopToken) {
    // Section 14: this thread owns the Render Graph, GraphicsDevice, and
    // Timeline stepping. Media decode work is intentionally NOT pumped here
    // — MediaEngine (Phase 2) runs its own thread, started once the device
    // exists (see Tick()); this thread only reads decoded frames back out
    // via RenderGraph::Execute(..., mediaEngine_.get()), never blocking on
    // decode latency.
    lastTickTime_ = std::chrono::steady_clock::now();

    while (running_.load(std::memory_order_acquire) && !stopToken.stop_requested()) {
        Tick();

        // Section 16: this loop is intentionally not a tight spin. A real
        // implementation should pace this to the display's vsync via
        // choreographer callbacks routed through JNI, or to
        // swapchain-present timing once BeginFrame/EndFrame are hooked up;
        // sleeping here is a placeholder to keep bring-up CPU-cheap.
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

void Engine::RefreshActiveClips(double timelineSeconds) {
    if (!mediaEngine_) return;

    std::vector<ActiveClipRequest> requests;
    for (const Clip* clip : timeline_->ActiveClipsAt(timelineSeconds)) {
        const Node* node = graph_.FindNode(clip->sourceNodeId);
        if (!node || node->kind != NodeKind::VideoSource || node->sourceFilePath.empty()) continue;

        ActiveClipRequest req;
        req.clipId = node->nodeId; // RenderGraph::ExecutePass keys TryGetFrame by node id too
        req.sourceFilePath = node->sourceFilePath;
        // Clip::ToSourceTime applies trim/speed/direction — MediaEngine and
        // RenderGraph both operate in *source* time, never raw timeline
        // time, per Timeline.h's design note on this exact seam.
        req.sourceTimeSeconds = clip->ToSourceTime(timelineSeconds);
        requests.push_back(std::move(req));
    }
    mediaEngine_->SetActiveClips(std::move(requests), timelineSeconds);
}

void Engine::Tick() {
    // 1. Drain commands at a well-defined sync point (Section 12) — never
    // mid-render, so a node's uniform can't change halfway through the pass
    // that reads it.
    commandQueue_.DrainAll([this](EngineCommand& cmd) { cmd(*this); });

    // 2. Handle surface attach/detach.
    if (surfaceDirty_.exchange(false, std::memory_order_acquire)) {
        std::lock_guard<std::mutex> lock(windowMutex_);
        if (pendingWindow_ && !device_) {
            device_ = CreateGraphicsDevice(BackendKind::Vulkan);
            if (!device_->Initialize(pendingWindow_)) {
                LOGI("Vulkan init failed, falling back to OpenGL ES");
                device_ = CreateGraphicsDevice(BackendKind::OpenGLES);
                device_->Initialize(pendingWindow_);
            }
            renderGraph_ = std::make_unique<RenderGraph>(*device_);

            // MediaEngine needs a live GraphicsDevice to import hardware
            // buffers into (VulkanDevice::ImportHardwareBuffer), so it's
            // constructed here rather than in Engine's constructor.
            mediaEngine_ = std::make_unique<MediaEngine>(*device_, DecoderPoolConfig{}, FrameCacheConfig{});
            mediaEngine_->Start();

            // Phase 6: Initialize profiler, export pipeline, project manager
            profiler_ = std::make_unique<Profiler>(ProfilerConfig{
                .enableGpuTimestamps = true,
                .enableCpuTiming = true,
                .enableMemoryTracking = true,
                .targetFrameTimeMs = 16.67
            });
            profiler_->Initialize(dynamic_cast<VulkanDevice*>(device_.get()));

            exportPipeline_ = std::make_unique<ExportPipeline>(*device_, *renderGraph_, graph_, *timeline_, *mediaEngine_, audioEngine_.get());

            projectManager_ = std::make_unique<ProjectManager>();
            projectManager_->SetOnProjectChanged([this](const std::string& path) {
                LOGI("Project changed: %s", path.c_str());
            });
            projectManager_->SetOnError([this](const std::string& err) {
                LOGE("Project error: %s", err.c_str());
            });
            projectManager_->EnableAutosave(true, 60);

            // Phase 7+: Initialize expression engine
            expressionEngine_ = std::make_unique<ExpressionEngine>();
            ExpressionEngine::RegisterBuiltins(*expressionEngine_);

            // Text rendering
            textRenderer_ = std::make_unique<TextRenderer>(*device_);
            textRenderer_->Initialize("/system/fonts/Roboto-Regular.ttf");

            // Phase 7+: Audio engine
            audioEngine_ = std::make_unique<AudioEngine>();
            audioEngine_->Initialize(device_.get());
        } else if (!pendingWindow_ && device_) {
            if (mediaEngine_) { mediaEngine_->Stop(); mediaEngine_.reset(); }
            if (exportPipeline_) { exportPipeline_->Cancel(); exportPipeline_.reset(); }
            if (profiler_) { profiler_->Shutdown(); profiler_.reset(); }
            expressionEngine_.reset();
            if (audioEngine_) { audioEngine_->Shutdown(); audioEngine_.reset(); }
            textRenderer_.reset();
            projectManager_.reset();
            device_->Shutdown();
            device_.reset();
            renderGraph_.reset();
        }
    }

    if (!device_) return; // no surface yet

    // 3. Advance timeline clock (Section 7).
    const auto now = std::chrono::steady_clock::now();
    const double dt = std::chrono::duration<double>(now - lastTickTime_).count();
    lastTickTime_ = now;
    timeline_->Advance(dt, masterSpeed_);

    // 3b. Phase 2: tell MediaEngine which clips are near the playhead right
    // now so its media thread can keep the right decoders warm and the
    // FrameCache populated ahead of RenderGraph actually needing a frame.
    RefreshActiveClips(timeline_->CurrentTime().seconds);

    // 3c. Phase 6: Update thermal adaptation and profiler
    UpdateThermalAdaptation();
    if (profiler_) {
        profiler_->BeginFrame(timeline_->CurrentTime().frameIndex);
        // Note: GPU timestamps are recorded in VulkanDevice::DrawFullscreenPass
        profiler_->EndFrame();
    }

    // 4. Render using RenderGraph (Phase 3).
    // Find the output node (first node of kind Output, or create a default)
    static const std::string kOutputNodeId = "output";
    const Node* outputNode = graph_.FindNode(kOutputNodeId);
    if (!outputNode) {
        // No output node yet - fall back to bring-up triangle for now
        if (device_->BeginFrame()) {
            if (auto* vulkan = dynamic_cast<VulkanDevice*>(device_.get())) {
                vulkan->RenderBringUpTriangle();
            }
            device_->EndFrame();
        }
        return;
    }

    if (device_->BeginFrame()) {
        // Phase 6: Profile render
        auto cpuScope = profiler_ ? profiler_->CpuScope("RenderGraph_Execute") : nullptr;
        
        auto plan = renderGraph_->Compile(graph_, kOutputNodeId);
        if (plan.Ok()) {
            renderGraph_->Execute(graph_, plan, timeline_->CurrentTime().seconds, mediaEngine_.get(), expressionEngine_.get(), audioEngine_.get());
        }
        device_->EndFrame();
    }
    
    // Trigger autosave if needed
    if (projectManager_) {
        projectManager_->TriggerAutosave();
    }
}

void Engine::UpdateThermalAdaptation() {
    if (!profiler_) return;
    
    // Check thermal status and adapt quality
    if (profiler_->IsThrottling()) {
        // Could reduce resolution, lower frame rate, simplify shaders
        // For now, just log
        static bool logged = false;
        if (!logged) {
            LOGI("Thermal throttling detected - consider reducing quality");
            logged = true;
        }
    } else {
        // Reset when thermal status improves
    }
}

void Engine::ReloadShadersFromAssets() {
    if (!assetManager_ || !device_) {
        LOGW("ReloadShadersFromAssets: skipped (no asset manager or device)");
        return;
    }

    auto* vulkan = dynamic_cast<VulkanDevice*>(device_.get());
    if (!vulkan) {
        LOGW("ReloadShadersFromAssets: skipped (not Vulkan backend)");
        return;
    }

    struct ShaderEntry {
        const char* name;
        const char* assetPath;
    };
    static constexpr ShaderEntry kShaders[] = {
        {"fullscreen_vert", "shaders/fullscreen.vert.spv"},
        {"blend_normal_frag", "shaders/blend_normal.frag.spv"},
        {"blend_multiply_frag", "shaders/blend_multiply.frag.spv"},
        {"blend_screen_frag", "shaders/blend_screen.frag.spv"},
        {"blend_overlay_frag", "shaders/blend_overlay.frag.spv"},
        {"blend_add_frag", "shaders/blend_add.frag.spv"},
        {"blend_subtract_frag", "shaders/blend_subtract.frag.spv"},
        {"color_correction_frag", "shaders/color_correction.frag.spv"},
        {"blur_frag", "shaders/blur.frag.spv"},
        {"mask_frag", "shaders/mask.frag.spv"},
        {"composite_frag", "shaders/composite.frag.spv"},
        {"vector_source_vert", "shaders/vector_source.vert.spv"},
        {"vector_source_frag", "shaders/vector_source.frag.spv"},
        {"text_source_vert", "shaders/text_source.vert.spv"},
        {"text_source_frag", "shaders/text_source.frag.spv"},
        {"stroke_source_vert", "shaders/stroke_source.vert.spv"},
        {"stroke_source_frag", "shaders/stroke_source.frag.spv"},
        {"adjustment_vert", "shaders/adjustment.vert.spv"},
        {"adjustment_frag", "shaders/adjustment.frag.spv"},
        {"null_layer_vert", "shaders/null_layer.vert.spv"},
        {"null_layer_frag", "shaders/null_layer.frag.spv"},
        {"output_vert", "shaders/output.vert.spv"},
        {"output_frag", "shaders/output.frag.spv"},
        {"motion_blur_vert", "shaders/motion_blur.vert.spv"},
        {"motion_blur_frag", "shaders/motion_blur.frag.spv"},
        {"directional_blur_vert", "shaders/directional_blur.vert.spv"},
        {"directional_blur_frag", "shaders/directional_blur.frag.spv"},
        {"time_remap_vert", "shaders/time_remap.vert.spv"},
        {"time_remap_frag", "shaders/time_remap.frag.spv"},
        {"bezier_mask_vert", "shaders/bezier_mask.vert.spv"},
        {"bezier_mask_frag", "shaders/bezier_mask.frag.spv"},
        {"particle_vert", "shaders/particle.vert.spv"},
        {"particle_frag", "shaders/particle.frag.spv"},
        {"shape2d_vert", "shaders/shape2d.vert.spv"},
        {"shape2d_frag", "shaders/shape2d.frag.spv"},
        {"shape_merge_frag", "shaders/shape_merge.frag.spv"},
        {"shape_transform_vert", "shaders/shape_transform.vert.spv"},
        {"transform3d_vert", "shaders/transform3d.vert.spv"},
        {"transform3d_frag", "shaders/transform3d.frag.spv"},
        {"camera3d_vert", "shaders/camera3d.vert.spv"},
        {"camera3d_frag", "shaders/camera3d.frag.spv"},
        {"depth_of_field_vert", "shaders/depth_of_field.vert.spv"},
        {"depth_of_field_frag", "shaders/depth_of_field.frag.spv"},
        {"chroma_key_vert", "shaders/chroma_key.vert.spv"},
        {"chroma_key_frag", "shaders/chroma_key.frag.spv"},
        {"mesh_pbr_vert", "shaders/mesh_pbr.vert.spv"},
        {"mesh_pbr_frag", "shaders/mesh_pbr.frag.spv"},
    };

    for (const auto& entry : kShaders) {
        auto result = vulkan->LoadShaderFromAssets(assetManager_, entry.assetPath);
        if (result) {
            LOGI("Reloaded shader: %s -> %s", entry.name, entry.assetPath);
        } else {
            LOGW("Failed to reload shader %s from %s: %s",
                 entry.name, entry.assetPath, result.error.c_str());
        }
    }
}

void Engine::StartExport(ExportCommand&& cmd) {
    if (!exportPipeline_) {
        if (cmd.onComplete) {
            ExportResult result;
            result.success = false;
            result.errorMessage = "Export pipeline not initialized";
            cmd.onComplete(result);
        }
        return;
    }

    ExportConfig config;
    config.outputPath = cmd.outputPath;
    config.width = cmd.width;
    config.height = cmd.height;
    config.frameRate = cmd.frameRate;
    config.startTime = cmd.startTime;
    config.endTime = cmd.endTime;
    config.bitrateMbps = cmd.bitrateMbps;
    config.codec = cmd.codec;
    config.useHardwareEncoder = true;

    exportPipeline_->StartExport(config,
        [](double progress, const std::string& status) {
            LOGI("Export progress: %.1f%% - %s", progress * 100, status.c_str());
        },
        [callback = std::move(cmd.onComplete)](ExportResult result) {
            if (callback) callback(result);
        });
}

void Engine::SaveProject(SaveProjectCommand&& cmd) {
    if (!projectManager_) {
        if (cmd.onComplete) cmd.onComplete(false);
        return;
    }

    // Serialize current state
    auto projectData = ProjectSerializer::Serialize(graph_, *timeline_);
    
    // Update metadata
    projectData.metadata.name = projectManager_->GetProjectName();
    projectData.metadata.modifiedDate = ProjectSerializer::GetCurrentTimestamp();
    
    bool success = false;
    if (cmd.filePath.empty()) {
        success = projectManager_->SaveProject();
    } else {
        success = projectManager_->SaveProjectAs(cmd.filePath);
    }

    if (cmd.onComplete) cmd.onComplete(success);
}

void Engine::LoadProject(LoadProjectCommand&& cmd) {
    if (!projectManager_) {
        if (cmd.onComplete) cmd.onComplete(false);
        return;
    }

    bool success = projectManager_->OpenProject(cmd.filePath);
    if (success) {
        // Deserialize into engine state
        const auto& data = projectManager_->GetData();
        ProjectSerializer::Deserialize(data, graph_, *timeline_);
    }

    if (cmd.onComplete) cmd.onComplete(success);
}

} // namespace vfx

// Phase 7+: Node graph operations implementation
namespace vfx {

void Engine::AddNode(AddNodeCommand&& cmd) {
    Node node;
    node.nodeId = cmd.nodeId;
    node.kind = cmd.kind;
    node.debugName = cmd.name;
    node.x = cmd.x;
    node.y = cmd.y;
    
    // Set default ports based on kind
    switch (cmd.kind) {
        case NodeKind::VideoSource:
        case NodeKind::ImageSource:
        case NodeKind::AudioSource:
            node.outputs.push_back(NodeSocket{"output"});
            break;
        case NodeKind::Shader:
        case NodeKind::ColorCorrection:
        case NodeKind::Blur:
        case NodeKind::Mask:
            node.inputs.push_back(NodeSocket{"input"});
            node.outputs.push_back(NodeSocket{"output"});
            break;
        case NodeKind::Blend:
        case NodeKind::Composite:
            node.inputs.push_back(NodeSocket{"base"});
            node.inputs.push_back(NodeSocket{"overlay"});
            node.outputs.push_back(NodeSocket{"output"});
            break;
        case NodeKind::Output:
            node.inputs.push_back(NodeSocket{"input"});
            break;
        case NodeKind::Adjustment:
            node.inputs.push_back(NodeSocket{"input"});
            node.outputs.push_back(NodeSocket{"output"});
            break;
        case NodeKind::Null:
            node.outputs.push_back(NodeSocket{"output"});
            break;
        case NodeKind::VectorSource:
        case NodeKind::TextSource:
        case NodeKind::StrokeSource:
            node.outputs.push_back(NodeSocket{"output"});
            break;
        case NodeKind::Group:
            // Group ports are dynamic
            break;
        // Motion Effects - all single input, single output
        case NodeKind::Oscillate:
        case NodeKind::Shake:
        case NodeKind::RandomDisplacement:
        case NodeKind::Pulse:
        case NodeKind::Swing:
        case NodeKind::Bounce:
        case NodeKind::Elastic:
        case NodeKind::CameraShake:
        case NodeKind::ZoomBlur:
        case NodeKind::RadialBlur:
        case NodeKind::MotionBlur:
        case NodeKind::DirectionalBlur:
        case NodeKind::Ripple:
        case NodeKind::Wave:
        case NodeKind::Twist:
        case NodeKind::Bulge:
        case NodeKind::Vortex:
        case NodeKind::Glitch:
        case NodeKind::VHS:
        case NodeKind::Scanlines:
        case NodeKind::CRT:
        case NodeKind::ChromaticAberration:
        case NodeKind::RGBShift:
        case NodeKind::TimeStretch:
        case NodeKind::FrameBlend:
        case NodeKind::StopMotion:
        case NodeKind::PosterizeTime:
        case NodeKind::Wiggle:
        case NodeKind::Jitter:
        case NodeKind::Drift:
        case NodeKind::Orbit:
        case NodeKind::CameraShakePro:
        case NodeKind::DynamicZoom:
        case NodeKind::FilmDamage:
        case NodeKind::FilmGrain:
        case NodeKind::Vignette:
        case NodeKind::Letterbox:
        case NodeKind::BezierWarp:
        case NodeKind::MeshWarp:
        case NodeKind::PolarCoordinates:
        case NodeKind::DisplacementMap:
            node.inputs.push_back(NodeSocket{"input"});
            node.outputs.push_back(NodeSocket{"output"});
            break;
    }

    // Set default uniform values for motion effects
    switch (cmd.kind) {
        case NodeKind::Oscillate:
            node.uniformFloats["mFrequency"] = 2.0f;
            node.uniformFloats["mMagnitude"] = 25.0f;
            node.uniformFloats["mAngle"] = 45.0f;
            node.uniformFloats["mPhase"] = 0.0f;
            node.uniformFloats["mWaveType"] = 0; // 0 = Sine
            break;
        case NodeKind::Shake:
            node.uniformFloats["mFrequency"] = 2.0f;
            node.uniformFloats["mMagnitude"] = 15.0f;
            node.uniformFloats["mDecay"] = 0.5f;
            node.uniformFloats["mRotation"] = 0.1f;
            node.uniformFloats["mSeed"] = static_cast<float>((std::rand() % 10000));
            break;
        case NodeKind::RandomDisplacement:
            node.uniformFloats["mAmount"] = 0.05f;
            node.uniformFloats["mSpeed"] = 1.0f;
            node.uniformFloats["mScale"] = 1.0f;
            node.uniformFloats["mOctaves"] = 3;
            node.uniformFloats["mSeed"] = static_cast<float>((std::rand() % 10000));
            break;
        case NodeKind::Pulse:
            node.uniformFloats["mFrequency"] = 1.0f;
            node.uniformFloats["mMagnitude"] = 0.5f;
            node.uniformFloats["mPhase"] = 0.0f;
            node.uniformFloats["mAmount"] = 0.5f; // anchorX
            node.uniformFloats["mSpeed"] = 0.5f;  // anchorY
            break;
        case NodeKind::Swing:
            node.uniformFloats["mFrequency"] = 0.5f;
            node.uniformFloats["mMagnitude"] = 30.0f;
            node.uniformFloats["mAngle"] = 0.0f;
            node.uniformFloats["mPhase"] = 0.0f;
            break;
        case NodeKind::Bounce:
            node.uniformFloats["mFrequency"] = 1.5f;
            node.uniformFloats["mMagnitude"] = 50.0f;
            node.uniformFloats["mDecay"] = 0.3f;
            node.uniformFloats["mPhase"] = 0.0f;
            break;
        case NodeKind::Elastic:
            node.uniformFloats["mFrequency"] = 2.0f;
            node.uniformFloats["mMagnitude"] = 20.0f;
            node.uniformFloats["mDecay"] = 0.4f;
            node.uniformFloats["mPhase"] = 0.0f;
            break;
        case NodeKind::CameraShake:
            node.uniformFloats["mFrequency"] = 2.0f;
            node.uniformFloats["mAmount"] = 25.0f;
            node.uniformFloats["mRotation"] = 0.15f;
            node.uniformFloats["mScale"] = 0.05f;
            node.uniformFloats["mDecay"] = 0.8f;
            node.uniformFloats["mSeed"] = static_cast<float>((std::rand() % 10000));
            node.uniformFloats["mPhase"] = 0.0f;
            break;
        case NodeKind::ZoomBlur:
            node.uniformFloats["mAmount"] = 0.3f;
            node.uniformFloats["mSpeed"] = 1.0f;
            node.uniformFloats["mScale"] = 1.0f;
            node.uniformFloats["mOctaves"] = 1;
            break;
        case NodeKind::RadialBlur:
            node.uniformFloats["mAmount"] = 0.2f;
            node.uniformFloats["mIntensity"] = 1.0f;
            node.uniformFloats["mOctaves"] = 1;
            break;
        case NodeKind::DirectionalBlur:
            node.uniformFloats["mAmount"] = 0.5f;
            node.uniformFloats["mAngle"] = 0.0f;
            break;
        case NodeKind::Ripple:
            node.uniformFloats["mFrequency"] = 10.0f;
            node.uniformFloats["mMagnitude"] = 0.02f;
            node.uniformFloats["mSpeed"] = 2.0f;
            node.uniformFloats["mAmount"] = 1.0f;
            node.uniformFloats["mOctaves"] = 1;
            break;
        case NodeKind::Wave:
            node.uniformFloats["mFrequency"] = 5.0f;
            node.uniformFloats["mMagnitude"] = 0.05f;
            node.uniformFloats["mAngle"] = 0.0f;
            node.uniformFloats["mSpeed"] = 1.0f;
            node.uniformFloats["mAmount"] = 1.0f;
            break;
        case NodeKind::Twist:
            node.uniformFloats["mFrequency"] = 1.0f;
            node.uniformFloats["mMagnitude"] = 2.0f;
            node.uniformFloats["mAmount"] = 1.0f;
            node.uniformFloats["mSpeed"] = 1.0f;
            break;
        case NodeKind::Bulge:
            node.uniformFloats["mAmount"] = 0.5f;
            node.uniformFloats["mScale"] = 1.0f;
            node.uniformFloats["mIntensity"] = 1.0f;
            break;
        case NodeKind::Vortex:
            node.uniformFloats["mFrequency"] = 1.0f;
            node.uniformFloats["mMagnitude"] = 1.5f;
            node.uniformFloats["mAmount"] = 1.0f;
            node.uniformFloats["mSpeed"] = 1.0f;
            break;
        case NodeKind::Glitch:
            node.uniformFloats["mIntensity"] = 0.5f;
            node.uniformFloats["mBlockSize"] = 32.0f;
            node.uniformFloats["mFrequency"] = 10.0f;
            node.uniformFloats["mSeed"] = static_cast<float>((std::rand() % 10000));
            node.uniformFloats["mChromatic"] = 1.0f;
            break;
        case NodeKind::VHS:
            node.uniformFloats["mNoiseAmount"] = 0.3f;
            node.uniformFloats["mScanlineAmount"] = 0.5f;
            node.uniformFloats["mDistortion"] = 0.2f;
            node.uniformFloats["mColorBleed"] = 0.3f;
            node.uniformFloats["mJitter"] = 0.2f;
            node.uniformFloats["mSeed"] = static_cast<float>((std::rand() % 10000));
            break;
        case NodeKind::Scanlines:
            node.uniformFloats["mScanlineAmount"] = 0.5f;
            node.uniformFloats["mIntensity"] = 1.0f;
            break;
        case NodeKind::CRT:
            node.uniformFloats["mScanlineAmount"] = 0.7f;
            node.uniformFloats["mDistortion"] = 0.1f;
            node.uniformFloats["mColorBleed"] = 0.2f;
            node.uniformFloats["mNoiseAmount"] = 0.1f;
            break;
        case NodeKind::ChromaticAberration:
            node.uniformFloats["mChromatic"] = 2.0f;
            node.uniformFloats["mIntensity"] = 1.0f;
            break;
        case NodeKind::RGBShift:
            node.uniformFloats["mChromatic"] = 3.0f;
            node.uniformFloats["mIntensity"] = 1.0f;
            break;
        case NodeKind::TimeStretch:
            node.uniformFloats["mSpeed"] = 0.5f;
            node.uniformFloats["mAmount"] = 1.0f;
            break;
        case NodeKind::FrameBlend:
            node.uniformFloats["mAmount"] = 0.5f;
            node.uniformFloats["mSpeed"] = 1.0f;
            break;
        case NodeKind::StopMotion:
            node.uniformFloats["mSpeed"] = 0.25f;
            node.uniformFloats["mAmount"] = 1.0f;
            break;
        case NodeKind::PosterizeTime:
            node.uniformFloats["mSpeed"] = 0.1f;
            node.uniformFloats["mAmount"] = 1.0f;
            break;
        case NodeKind::Wiggle:
            node.uniformFloats["mFrequency"] = 1.0f;
            node.uniformFloats["mMagnitude"] = 10.0f;
            node.uniformFloats["mOctaves"] = 1;
            node.uniformFloats["mAmount"] = 1.0f;
            node.uniformFloats["mSeed"] = static_cast<float>((std::rand() % 10000));
            break;
        case NodeKind::Jitter:
            node.uniformFloats["mFrequency"] = 30.0f;
            node.uniformFloats["mMagnitude"] = 2.0f;
            node.uniformFloats["mAmount"] = 1.0f;
            node.uniformFloats["mSeed"] = static_cast<float>((std::rand() % 10000));
            break;
        case NodeKind::Drift:
            node.uniformFloats["mFrequency"] = 0.1f;
            node.uniformFloats["mMagnitude"] = 5.0f;
            node.uniformFloats["mOctaves"] = 2;
            node.uniformFloats["mAmount"] = 1.0f;
            node.uniformFloats["mSeed"] = static_cast<float>((std::rand() % 10000));
            break;
        case NodeKind::Orbit:
            node.uniformFloats["mFrequency"] = 0.25f;
            node.uniformFloats["mMagnitude"] = 50.0f;
            node.uniformFloats["mAmount"] = 1.0f;
            node.uniformFloats["mPhase"] = 0.0f;
            break;
        case NodeKind::CameraShakePro:
            node.uniformFloats["mFrequency"] = 2.0f;
            node.uniformFloats["mAmount"] = 30.0f;
            node.uniformFloats["mRotation"] = 0.2f;
            node.uniformFloats["mScale"] = 0.1f;
            node.uniformFloats["mDecay"] = 1.0f;
            node.uniformFloats["mSeed"] = static_cast<float>((std::rand() % 10000));
            break;
        case NodeKind::DynamicZoom:
            node.uniformFloats["mFrequency"] = 0.1f;
            node.uniformFloats["mMagnitude"] = 0.3f;
            node.uniformFloats["mAmount"] = 1.0f;
            node.uniformFloats["mPhase"] = 0.0f;
            break;
        case NodeKind::FilmDamage:
            node.uniformFloats["mIntensity"] = 0.4f;
            node.uniformFloats["mNoiseAmount"] = 0.3f;
            node.uniformFloats["mScanlineAmount"] = 0.2f;
            node.uniformFloats["mSeed"] = static_cast<float>((std::rand() % 10000));
            break;
        case NodeKind::FilmGrain:
            node.uniformFloats["mIntensity"] = 0.3f;
            node.uniformFloats["mNoiseAmount"] = 0.5f;
            node.uniformFloats["mScanlineAmount"] = 0.0f;
            node.uniformFloats["mSeed"] = static_cast<float>((std::rand() % 10000));
            break;
        case NodeKind::Vignette:
            node.uniformFloats["mIntensity"] = 0.5f;
            node.uniformFloats["mAmount"] = 1.0f;
            break;
        case NodeKind::Letterbox:
            node.uniformFloats["mAmount"] = 0.2f;
            node.uniformFloats["mIntensity"] = 1.0f;
            break;
        case NodeKind::BezierWarp:
            node.uniformFloats["mAmount"] = 1.0f;
            node.uniformFloats["mIntensity"] = 1.0f;
            break;
        case NodeKind::MeshWarp:
            node.uniformFloats["mAmount"] = 1.0f;
            node.uniformFloats["mIntensity"] = 1.0f;
            break;
        case NodeKind::PolarCoordinates:
            node.uniformFloats["mAmount"] = 1.0f;
            node.uniformFloats["mIntensity"] = 1.0f;
            break;
        case NodeKind::DisplacementMap:
            node.uniformFloats["mAmount"] = 0.5f;
            node.uniformFloats["mIntensity"] = 1.0f;
            node.uniformFloats["mScale"] = 1.0f;
            break;
        default:
            break;
    }

    graph_.AddNode(std::move(node));
    
    // Add to group if specified
    if (!cmd.groupId.empty()) {
        auto* group = graph_.FindGroup(cmd.groupId);
        if (group) {
            // Note: group is const, need mutable access
            // In real implementation, would use FindGroupMutable
        }
    }
}

void Engine::RemoveNode(RemoveNodeCommand&& cmd) {
    graph_.RemoveNode(cmd.nodeId);
}

void Engine::ConnectNodes(ConnectNodesCommand&& cmd) {
    Connection conn;
    conn.fromNodeId = cmd.fromNodeId;
    conn.fromSlot = cmd.fromSlot;
    conn.toNodeId = cmd.toNodeId;
    conn.toSlot = cmd.toSlot;
    graph_.Connect(std::move(conn));
}

void Engine::SetNodeParent(SetNodeParentCommand&& cmd) {
    if (auto* node = graph_.FindNodeMutable(cmd.nodeId)) {
        node->parentNodeId = cmd.parentNodeId;
    }
}

void Engine::CreateGroup(CreateGroupCommand&& cmd) {
    graph_.CreateGroup(cmd.groupId, cmd.name, cmd.memberNodeIds);
}

void Engine::RemoveGroup(RemoveGroupCommand&& cmd) {
    graph_.RemoveGroup(cmd.groupId);
}

void Engine::AddClip(AddClipCommand&& cmd) {
    Timeline::Clip clip;
    clip.clipId = cmd.clipId;
    clip.sourceNodeId = cmd.sourceNodeId;
    clip.type = cmd.type;
    clip.timelineStart = cmd.timelineStart;
    clip.sourceInPoint = cmd.sourceInPoint;
    clip.sourceOutPoint = cmd.sourceOutPoint;
    clip.playbackSpeed = cmd.playbackSpeed;
    clip.layer = cmd.layer;
    timeline_->AddClip(std::move(clip));
}

void Engine::RemoveClip(RemoveClipCommand&& cmd) {
    timeline_->RemoveClip(cmd.clipId);
}

void Engine::UpdateClip(UpdateClipCommand&& cmd) {
    if (auto* clip = timeline_->FindClipMutable(cmd.clipId)) {
        if (cmd.timelineStart) clip->timelineStart = *cmd.timelineStart;
        if (cmd.sourceInPoint) clip->sourceInPoint = *cmd.sourceInPoint;
        if (cmd.sourceOutPoint) clip->sourceOutPoint = *cmd.sourceOutPoint;
        if (cmd.playbackSpeed) clip->playbackSpeed = *cmd.playbackSpeed;
        if (cmd.layer) clip->layer = *cmd.layer;
        if (cmd.enabled) clip->enabled = *cmd.enabled;
        if (cmd.locked) clip->locked = *cmd.locked;
    }
}

void Engine::Undo() {
    // Undo logic would be implemented here
    // For now, just log
    LOGI("Undo requested");
}

void Engine::Redo() {
    // Redo logic would be implemented here
    // For now, just log
    LOGI("Redo requested");
}

} // namespace vfx