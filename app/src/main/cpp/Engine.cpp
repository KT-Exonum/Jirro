#include "Engine.h"

#include <android/log.h>

#include "engine/opengl/OpenGLDevice.h"
#include "engine/vulkan/VulkanDevice.h"

#define LOG_TAG "Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

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
    engineThread_ = std::thread(&Engine::ThreadMain, this);
}

void Engine::Stop() {
    if (!running_.exchange(false)) return;
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

void Engine::ThreadMain() {
    // Section 14: this thread owns the Render Graph, GraphicsDevice, and
    // Timeline stepping. Media decode work is intentionally NOT pumped here
    // — MediaEngine (Phase 2) runs its own thread, started once the device
    // exists (see Tick()); this thread only reads decoded frames back out
    // via RenderGraph::Execute(..., mediaEngine_.get()), never blocking on
    // decode latency.
    lastTickTime_ = std::chrono::steady_clock::now();

    while (running_.load(std::memory_order_acquire)) {
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

            exportPipeline_ = std::make_unique<ExportPipeline>(*device_, *renderGraph_, graph_, *timeline_);

            projectManager_ = std::make_unique<ProjectManager>();
            projectManager_->SetOnProjectChanged([this](const std::string& path) {
                LOGI("Project changed: %s", path.c_str());
            });
            projectManager_->SetOnError([this](const std::string& err) {
                LOGE("Project error: %s", err.c_str());
            });
            projectManager_->EnableAutosave(true, 60);
        } else if (!pendingWindow_ && device_) {
            if (mediaEngine_) { mediaEngine_->Stop(); mediaEngine_.reset(); }
            if (exportPipeline_) { exportPipeline_->Cancel(); exportPipeline_.reset(); }
            if (profiler_) { profiler_->Shutdown(); profiler_.reset(); }
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
            renderGraph_->Execute(graph_, plan, timeline_->CurrentTime().seconds, mediaEngine_.get());
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