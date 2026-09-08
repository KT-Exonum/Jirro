#pragma once
// Section 10: compiles a NodeGraph into an ordered execution plan for a
// specific timeline timestamp. Dependency resolution and topological
// ordering are pure graph algorithms with no GPU dependency, so they're
// fully implemented here (see RenderGraph.cpp). Pass *execution*
// (DrawFullscreenPass calls, actual texture acquisition) depends on
// GraphicsDevice being feature-complete for the node kinds involved: Phase 2
// wires up VideoSource (see ExecutePass), Phase 3 wires up the rest.

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Node.h"
#include "engine/core/GraphicsDevice.h"
#ifdef ENGINE_ENABLE_EXPRESSION_ENGINE
#include "engine/expression/ExpressionEngine.h"
#else
namespace vfx { class ExpressionEngine; }
#endif
#include "engine/media/MediaEngine.h"
#include "engine/graph/ParticleSystem.h"

namespace vfx {

struct CompiledPass {
    std::string nodeId;
    NodeKind kind;
    std::vector<std::string> inputNodeIds; // already topo-resolved
    TextureHandle output; // assigned during Compile() from the pool
    bool isFinalOutput = false;
    
    // Particle system fields
    bool isParticleNode = false;
    bool isParticleCompute = false; // true for compute pass (ParticleEmitter/Forces)
    uint32_t particleBufferHandle = 0; // storage buffer handle for particles
};

struct CompileResult {
    std::vector<CompiledPass> passes; // execution order: index 0 runs first
    std::vector<std::string> cycleNodeIds; // non-empty on failure
    [[nodiscard]] bool Ok() const { return cycleNodeIds.empty(); }
};

// Section 5/6: transient textures live here, not in GraphicsDevice, because
// lifetime decisions ("can pass 4's output reuse pass 1's now-dead buffer?")
// need graph-level visibility that GraphicsDevice intentionally doesn't have.
class TransientTexturePool {
public:
    explicit TransientTexturePool(GraphicsDevice& device) : device_(device) {}

    // Returns a texture matching `desc` that is not currently referenced by
    // any pass still pending in this compile, reusing a pooled one whenever
    // possible (Section 6: "do not create a new framebuffer/image for every
    // node unless the resource lifetime actually requires it").
    Result<TextureHandle> Acquire(const TextureDesc& desc);
    void Release(TextureHandle handle); // returns to the idle set, not to GraphicsDevice
    void EndFrame(); // moves this frame's "in use" set back to idle for next compile

private:
    GraphicsDevice& device_;
    struct Entry { TextureHandle handle; TextureDesc desc; bool idle = true; };
    std::vector<Entry> pool_;
};

class RenderGraph {
public:
    explicit RenderGraph(GraphicsDevice& device) : device_(device), texturePool_(device) {}

    // Builds execution order via Kahn's algorithm; returns cycleNodeIds
    // populated (and Ok()==false) if the graph isn't a DAG rather than
    // hanging or crashing on a malformed user-authored graph.
    CompileResult Compile(const NodeGraph& graph, const std::string& outputNodeId) const;

    // Executes a previously-compiled plan at a given timeline timestamp
    // (uniform evaluation is time-dependent; topology is not, so Compile()
    // and Execute() are split to avoid re-resolving dependencies every
    // frame when only keyframed values changed). `mediaEngine` and
    // `audioEngine` are optional (nullable) so unit tests and early-phase
    // callers don't need a full pipeline. VideoSource passes produce no
    // output when mediaEngine is null; audio visualization nodes render
    // with fallback values when audioEngine is null.
    void Execute(const NodeGraph& graph, const CompileResult& plan, double timelineSeconds,
                 const MediaEngine* mediaEngine = nullptr,
                 ExpressionEngine* expressionEngine = nullptr,
                 class AudioEngine* audioEngine = nullptr);

private:
    void ExecutePass(const NodeGraph& graph, const CompiledPass& pass, double timelineSeconds,
                      const MediaEngine* mediaEngine,
                      ExpressionEngine* expressionEngine,
                      AudioEngine* audioEngine);
    
    // Particle system
    void InitializeParticleSystem(const ParticleConfig& config);
    void DispatchParticleCompute(const CompiledPass& pass, double deltaTime, const ParticleConfig& config);
    void RenderParticles(const CompiledPass& pass, const ParticleConfig& config, TextureHandle outputTexture,
                         const std::vector<TextureHandle>& inputTextures, const std::unordered_map<std::string, float>& uniforms);
    ParticleSimParams PackSimParams(const ParticleConfig& config, double deltaTime, uint32_t frameIndex, uint32_t seed);

    // Load or create shader module from SPIR-V bytecode (cached by bytecode content)
    ShaderModuleHandle GetOrCreateShaderModule(const uint32_t* spirv, size_t wordCount);

    // Expand groups by inlining member nodes
    void ExpandGroups(const NodeGraph& graph, std::unordered_set<std::string>& reachable) const;

    GraphicsDevice& device_;
    TransientTexturePool texturePool_;

    // Pipeline/shader-module handles keyed by node id, so repeated Execute()
    // calls at different timestamps don't recompile shaders every frame
    // (Section 16 "pipeline caching" / "descriptor reuse").
    std::unordered_map<std::string, PipelineHandle> pipelineCache_;

    // Shader module cache keyed by SPIR-V bytecode content hash
    std::unordered_map<std::string, ShaderModuleHandle> shaderModuleCache_;

    // Phase 2: the most recent frame successfully pulled for each
    // VideoSource node, so a pass holds its last good frame rather than
    // rendering nothing on ticks where MediaEngine::TryGetFrame misses
    // (decode is asynchronous — see MediaEngine.h's threading note).
    std::unordered_map<std::string, TextureHandle> lastVideoFrameByNode_;

    // Particle system state
    struct ParticleSystemState {
        BufferHandle particleBuffer = 0;          // storage buffer for particle data
        BufferHandle simParamsBuffer = 0;         // uniform buffer for simulation params
        BufferHandle indirectBuffer = 0;          // indirect draw buffer (VkDrawIndirectCommand)
        PipelineHandle computePipeline = 0;       // particle simulation compute pipeline
        uint32_t maxParticles = 10000;
        uint32_t frameIndex = 0;
        uint32_t seed = 12345;
        bool initialized = false;
    } particleState_;

    double lastTimelineSeconds_ = 0.0;
};

} // namespace vfx
