#pragma once
// Section 12/14: the single owner of the engine thread and all engine-side
// state. The JNI bridge only ever calls QueueCommand (or, for surface
// lifecycle, the explicit Attach/DetachSurface below, which are inherently
// synchronous handshakes with the Android lifecycle and are the one
// deliberate exception to "commands only").
//
// Phase 2: also owns the MediaEngine (Section 14's "Media Thread" box).
// Engine::Tick refreshes MediaEngine's active-clip set from Timeline every
// frame and hands MediaEngine to RenderGraph::Execute so VideoSource passes
// can pull decoded frames — but MediaEngine's actual decode work happens on
// its own thread, never here.
//
// Phase 6: owns Profiler, ExportPipeline, ProjectManager for optimization,
// export, and project persistence.
//
// Phase 7+: owns AudioMixer, ExpressionEngine, UndoRedoManager for advanced features.

#include <android/asset_manager.h>
#include <android/native_window.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>

#include "engine/core/CommandQueue.h"
#include "engine/core/GraphicsDevice.h"
#include "engine/core/Profiler.h"
#include "engine/export/ExportPipeline.h"
#include "engine/export/ProjectSerializer.h"
#include "engine/expression/ExpressionEngine.h"
#include "engine/graph/Node.h"
#include "engine/graph/RenderGraph.h"
#include "engine/media/MediaEngine.h"
#include "engine/text/TextRenderer.h"
#include "engine/timeline/Timeline.h"

namespace vfx {

// Example command types referenced in Section 12. Kept as simple structs
// converted to EngineCommand closures at the call site (see jni_bridge.cpp)
// rather than a polymorphic Command class hierarchy — less indirection, and
// std::function already gives us the type erasure we need.
struct UpdateUniformCommand {
    std::string nodeId;
    std::string uniformName;
    float value;
};

// Phase 6: Export command
struct ExportCommand {
    std::string outputPath;
    uint32_t width = 1920;
    uint32_t height = 1080;
    double frameRate = 30.0;
    double startTime = 0.0;
    double endTime = 10.0;
    int bitrateMbps = 20;
    std::string codec = "video/avc";
    std::function<void(ExportResult)> onComplete;
};

// Phase 6: Project commands
struct SaveProjectCommand {
    std::string filePath;
    std::function<void(bool)> onComplete;
};

struct LoadProjectCommand {
    std::string filePath;
    std::function<void(bool)> onComplete;
};

// Phase 7+: Node commands
struct AddNodeCommand {
    std::string nodeId;
    NodeKind kind;
    float x = 0.0f;
    float y = 0.0f;
    std::string name;
    std::string groupId; // optional group to add to
};

struct RemoveNodeCommand {
    std::string nodeId;
};

struct ConnectNodesCommand {
    std::string fromNodeId;
    std::string fromSlot;
    std::string toNodeId;
    std::string toSlot;
};

struct SetNodeParentCommand {
    std::string nodeId;
    std::string parentNodeId; // empty = unparent
};

struct CreateGroupCommand {
    std::string groupId;
    std::string name;
    std::vector<std::string> memberNodeIds;
};

struct RemoveGroupCommand {
    std::string groupId;
};

struct AddClipCommand {
    std::string clipId;
    std::string sourceNodeId;
    Timeline::ClipType type = Timeline::ClipType::Video;
    double timelineStart = 0.0;
    double sourceInPoint = 0.0;
    double sourceOutPoint = 0.0;
    double playbackSpeed = 1.0;
    int layer = 0;
};

struct RemoveClipCommand {
    std::string clipId;
};

struct UpdateClipCommand {
    std::string clipId;
    std::optional<double> timelineStart;
    std::optional<double> sourceInPoint;
    std::optional<double> sourceOutPoint;
    std::optional<double> playbackSpeed;
    std::optional<int> layer;
    std::optional<bool> enabled;
    std::optional<bool> locked;
};

// Undo/Redo
struct UndoCommand {};
struct RedoCommand {};

class Engine {
public:
    Engine();
    ~Engine();

    // Called from the JNI bridge on Activity/SurfaceView lifecycle events.
    // Synchronous by necessity (ANativeWindow ownership is tied to the
    // Android lifecycle), but cheap — no GPU work happens on this call path
    // beyond what GraphicsDevice::Initialize itself needs.
    void AttachSurface(ANativeWindow* window);
    void DetachSurface();

    void Start(); // spawns the engine thread (and the media thread)
    void Stop();  // joins the engine thread (and the media thread)

    // The only cross-thread entry point besides Attach/DetachSurface. Safe
    // to call from the UI thread at any time; never blocks (see
    // CommandQueue::Push).
    void QueueCommand(EngineCommand cmd) { commandQueue_.Push(std::move(cmd)); }

    // Convenience wrapper matching Section 12's example call shape:
    //   engine->QueueCommand(UpdateUniform{node_id, uniform_name, value});
    void QueueUniformUpdate(UpdateUniformCommand cmd) {
        QueueCommand([cmd = std::move(cmd)](Engine& engine) {
            if (Node* node = engine.graph_.FindNodeMutable(cmd.nodeId)) {
                node->uniformFloats[cmd.uniformName] = cmd.value;
            }
        });
    }

    void QueueSeek(double seconds) {
        QueueCommand([seconds](Engine& engine) { engine.timeline_->Seek(seconds); });
    }

    void QueueSetPlaying(bool playing) {
        QueueCommand([playing](Engine& engine) {
            engine.timeline_->SetPlaybackState(playing ? PlaybackState::Playing
                                                        : PlaybackState::Stopped);
        });
    }

    // Phase 4: Timeline control for variable speed, reverse, scrubbing
    void QueueSetPlaybackSpeed(double speed) {
        QueueCommand([speed](Engine& engine) {
            engine.timeline_->SetPlaybackState(speed > 0 ? PlaybackState::Playing
                                                         : PlaybackState::Stopped);
            engine.masterSpeed_ = speed;
        });
    }

    void QueueSetScrubbing(bool scrubbing) {
        QueueCommand([scrubbing](Engine& engine) {
            engine.timeline_->SetPlaybackState(scrubbing ? PlaybackState::Scrubbing
                                                         : PlaybackState::Stopped);
        });
    }

    void QueueSetMasterSpeed(double speed) {
        QueueCommand([speed](Engine& engine) {
            engine.masterSpeed_ = speed;
        });
    }

    // Phase 6: Export command
    void QueueExport(ExportCommand cmd) {
        QueueCommand([cmd = std::move(cmd)](Engine& engine) {
            engine.StartExport(std::move(cmd));
        });
    }

    // Phase 6: Project commands
    void QueueSaveProject(SaveProjectCommand cmd) {
        QueueCommand([cmd = std::move(cmd)](Engine& engine) {
            engine.SaveProject(std::move(cmd));
        });
    }

    void QueueLoadProject(LoadProjectCommand cmd) {
        QueueCommand([cmd = std::move(cmd)](Engine& engine) {
            engine.LoadProject(std::move(cmd));
        });
    }

    // Phase 6: Dev hot-reload — set the AAssetManager so VulkanDevice can
    // load .spv shaders from the APK's assets/ folder at runtime.
    void SetAssetManager(AAssetManager* mgr) { assetManager_ = mgr; }

    // Reload all shaders from assets (dev builds only, guarded by
    // ENGINE_DEV_SHADER_HOTLOAD). Falls back to embedded bytecode when
    // asset loading fails for any individual shader.
    void ReloadShadersFromAssets();

    // Phase 7+: Node graph commands
    void QueueAddNode(AddNodeCommand cmd) {
        QueueCommand([cmd = std::move(cmd)](Engine& engine) {
            engine.AddNode(std::move(cmd));
        });
    }

    void QueueRemoveNode(RemoveNodeCommand cmd) {
        QueueCommand([cmd = std::move(cmd)](Engine& engine) {
            engine.RemoveNode(std::move(cmd));
        });
    }

    void QueueConnectNodes(ConnectNodesCommand cmd) {
        QueueCommand([cmd = std::move(cmd)](Engine& engine) {
            engine.ConnectNodes(std::move(cmd));
        });
    }

    void QueueSetNodeParent(SetNodeParentCommand cmd) {
        QueueCommand([cmd = std::move(cmd)](Engine& engine) {
            engine.SetNodeParent(std::move(cmd));
        });
    }

    void QueueCreateGroup(CreateGroupCommand cmd) {
        QueueCommand([cmd = std::move(cmd)](Engine& engine) {
            engine.CreateGroup(std::move(cmd));
        });
    }

    void QueueRemoveGroup(RemoveGroupCommand cmd) {
        QueueCommand([cmd = std::move(cmd)](Engine& engine) {
            engine.RemoveGroup(std::move(cmd));
        });
    }

    // Timeline clip commands
    void QueueAddClip(AddClipCommand cmd) {
        QueueCommand([cmd = std::move(cmd)](Engine& engine) {
            engine.AddClip(std::move(cmd));
        });
    }

    void QueueRemoveClip(RemoveClipCommand cmd) {
        QueueCommand([cmd = std::move(cmd)](Engine& engine) {
            engine.RemoveClip(std::move(cmd));
        });
    }

    void QueueUpdateClip(UpdateClipCommand cmd) {
        QueueCommand([cmd = std::move(cmd)](Engine& engine) {
            engine.UpdateClip(std::move(cmd));
        });
    }

    // Undo/Redo
    void QueueUndo(UndoCommand) {
        QueueCommand([](Engine& engine) { engine.Undo(); });
    }

    void QueueRedo(RedoCommand) {
        QueueCommand([](Engine& engine) { engine.Redo(); });
    }

    // Profiling access
    [[nodiscard]] Profiler* GetProfiler() { return profiler_.get(); }
    [[nodiscard]] const Profiler* GetProfiler() const { return profiler_.get(); }
    
    // Project manager access
    [[nodiscard]] ProjectManager* GetProjectManager() { return projectManager_.get(); }
    [[nodiscard]] const ProjectManager* GetProjectManager() const { return projectManager_.get(); }

    // Phase 7+: Expression engine access
    [[nodiscard]] ExpressionEngine* GetExpressionEngine() { return expressionEngine_.get(); }
    [[nodiscard]] const ExpressionEngine* GetExpressionEngine() const { return expressionEngine_.get(); }

    NodeGraph& Graph() { return graph_; }
    Timeline* GetTimeline() { return timeline_.get(); }

private:
    void ThreadMain(); // engine thread entry point
    void Tick();       // one frame: drain commands, advance timeline, refresh media requests, render
    void RefreshActiveClips(double timelineSeconds); // Phase 2: Timeline -> MediaEngine::SetActiveClips
    
    // Phase 6: Export and project operations
    void StartExport(ExportCommand&& cmd);
    void SaveProject(SaveProjectCommand&& cmd);
    void LoadProject(LoadProjectCommand&& cmd);
    void UpdateThermalAdaptation();

    // Phase 7+: Node graph operations
    void AddNode(AddNodeCommand&& cmd);
    void RemoveNode(RemoveNodeCommand&& cmd);
    void ConnectNodes(ConnectNodesCommand&& cmd);
    void SetNodeParent(SetNodeParentCommand&& cmd);
    void CreateGroup(CreateGroupCommand&& cmd);
    void RemoveGroup(RemoveGroupCommand&& cmd);
    void AddClip(AddClipCommand&& cmd);
    void RemoveClip(RemoveClipCommand&& cmd);
    void UpdateClip(UpdateClipCommand&& cmd);
    void Undo();
    void Redo();

    std::atomic<bool> running_{false};
    std::thread engineThread_;

    CommandQueue commandQueue_;

    std::unique_ptr<GraphicsDevice> device_;
    std::unique_ptr<RenderGraph> renderGraph_;
    std::unique_ptr<Timeline> timeline_;
    std::unique_ptr<MediaEngine> mediaEngine_;
    NodeGraph graph_;

    // Phase 6: Profiling, export, project management
    std::unique_ptr<Profiler> profiler_;
    std::unique_ptr<ExportPipeline> exportPipeline_;
    std::unique_ptr<ProjectManager> projectManager_;

    // Phase 7+: Expression engine for procedural animation
    std::unique_ptr<ExpressionEngine> expressionEngine_;

    // Text rendering
    std::unique_ptr<TextRenderer> textRenderer_;

    // Dev hot-reload: non-owning pointer to the APK's AAssetManager.
    AAssetManager* assetManager_ = nullptr;

    std::mutex windowMutex_;
    ANativeWindow* pendingWindow_ = nullptr;
    std::atomic<bool> surfaceDirty_{false};

    std::chrono::steady_clock::time_point lastTickTime_{};
    double masterSpeed_ = 1.0; // Phase 4: master timeline speed (negative = reverse)
};

} // namespace vfx