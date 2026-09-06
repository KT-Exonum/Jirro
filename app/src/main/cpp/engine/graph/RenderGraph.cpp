#include "RenderGraph.h"

#include <android/log.h>

#include <algorithm>
#include <array>
#include <deque>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#define LOG_TAG "RenderGraph"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace vfx {

// Forward declare SPIR-V bytecode from VulkanDevice.cpp (generated_shader_bytecode namespace)
extern const uint32_t* kFullscreenVertSpirv;
extern size_t kFullscreenVertSpirvWords;
extern const uint32_t* kBlendNormalFragSpirv;
extern size_t kBlendNormalFragSpirvWords;
extern const uint32_t* kBlendMultiplyFragSpirv;
extern size_t kBlendMultiplyFragSpirvWords;
extern const uint32_t* kBlendScreenFragSpirv;
extern size_t kBlendScreenFragSpirvWords;
extern const uint32_t* kBlendOverlayFragSpirv;
extern size_t kBlendOverlayFragSpirvWords;
extern const uint32_t* kBlendAddFragSpirv;
extern size_t kBlendAddFragSpirvWords;
extern const uint32_t* kBlendSubtractFragSpirv;
extern size_t kBlendSubtractFragSpirvWords;
extern const uint32_t* kColorCorrectionFragSpirv;
extern size_t kColorCorrectionFragSpirvWords;
extern const uint32_t* kBlurFragSpirv;
extern size_t kBlurFragSpirvWords;
extern const uint32_t* kMaskFragSpirv;
extern size_t kMaskFragSpirvWords;
extern const uint32_t* kCompositeFragSpirv;
extern size_t kCompositeFragSpirvWords;
extern const uint32_t* kVectorSourceVertSpirv;
extern size_t kVectorSourceVertSpirvWords;
extern const uint32_t* kVectorSourceFragSpirv;
extern size_t kVectorSourceFragSpirvWords;
extern const uint32_t* kTextSourceVertSpirv;
extern size_t kTextSourceVertSpirvWords;
extern const uint32_t* kTextSourceFragSpirv;
extern size_t kTextSourceFragSpirvWords;
extern const uint32_t* kStrokeSourceVertSpirv;
extern size_t kStrokeSourceVertSpirvWords;
extern const uint32_t* kStrokeSourceFragSpirv;
extern size_t kStrokeSourceFragSpirvWords;
extern const uint32_t* kAdjustmentVertSpirv;
extern size_t kAdjustmentVertSpirvWords;
extern const uint32_t* kAdjustmentFragSpirv;
extern size_t kAdjustmentFragSpirvWords;
extern const uint32_t* kNullLayerVertSpirv;
extern size_t kNullLayerVertSpirvWords;
extern const uint32_t* kNullLayerFragSpirv;
extern size_t kNullLayerFragSpirvWords;
extern const uint32_t* kOutputVertSpirv;
extern size_t kOutputVertSpirvWords;
extern const uint32_t* kOutputFragSpirv;
extern size_t kOutputFragSpirvWords;
extern const uint32_t* kMotionBlurVertSpirv;
extern size_t kMotionBlurVertSpirvWords;
extern const uint32_t* kMotionBlurFragSpirv;
extern size_t kMotionBlurFragSpirvWords;
extern const uint32_t* kDirectionalBlurVertSpirv;
extern size_t kDirectionalBlurVertSpirvWords;
extern const uint32_t* kDirectionalBlurFragSpirv;
extern size_t kDirectionalBlurFragSpirvWords;
extern const uint32_t* kTimeRemapVertSpirv;
extern size_t kTimeRemapVertSpirvWords;
extern const uint32_t* kTimeRemapFragSpirv;
extern size_t kTimeRemapFragSpirvWords;
extern const uint32_t* kBezierMaskVertSpirv;
extern size_t kBezierMaskVertSpirvWords;
extern const uint32_t* kBezierMaskFragSpirv;
extern size_t kBezierMaskFragSpirvWords;
extern const uint32_t* kParticleVertSpirv;
extern size_t kParticleVertSpirvWords;
extern const uint32_t* kParticleFragSpirv;
extern size_t kParticleFragSpirvWords;
extern const uint32_t* kParticleSimCompSpirv;
extern size_t kParticleSimCompSpirvWords;
extern const uint32_t* kShape2DVertSpirv;
extern size_t kShape2DVertSpirvWords;
extern const uint32_t* kShape2DFragSpirv;
extern size_t kShape2DFragSpirvWords;
extern const uint32_t* kShapeMergeFragSpirv;
extern size_t kShapeMergeFragSpirvWords;
extern const uint32_t* kShapeTransformVertSpirv;
extern size_t kShapeTransformVertSpirvWords;
extern const uint32_t* kTransform3DVertSpirv;
extern size_t kTransform3DVertSpirvWords;
extern const uint32_t* kTransform3DFragSpirv;
extern size_t kTransform3DFragSpirvWords;
extern const uint32_t* kCamera3DVertSpirv;
extern size_t kCamera3DVertSpirvWords;
extern const uint32_t* kDepthOfFieldFragSpirv;
extern size_t kDepthOfFieldFragSpirvWords;
extern const uint32_t* kChromaKeyVertSpirv;
extern size_t kChromaKeyVertSpirvWords;
extern const uint32_t* kChromaKeyFragSpirv;
extern size_t kChromaKeyFragSpirvWords;
extern const uint32_t* kMeshPBRVertSpirv;
extern size_t kMeshPBRVertSpirvWords;
extern const uint32_t* kMeshPBRFragSpirv;
extern size_t kMeshPBRFragSpirvWords;

// ---------------------------------------------------------------------------
// Shader registry: replaces the old 200+ line switch(pass.kind) with a
// data-driven lookup. Each NodeKind maps to a vertex/fragment SPIR-V pair.
// Blend nodes have a secondary lookup by BlendMode.
// ---------------------------------------------------------------------------

struct ShaderEntry {
    const uint32_t* vert;
    size_t vertWords;
    const uint32_t* frag;
    size_t fragWords;
};

struct NodeKindHash {
    auto operator()(NodeKind k) const noexcept {
        return static_cast<std::underlying_type_t<NodeKind>>(k);
    }
};

static const std::unordered_map<NodeKind, ShaderEntry, NodeKindHash>& GetShaderRegistry() {
    static const std::unordered_map<NodeKind, ShaderEntry, NodeKindHash> registry = []{
        std::unordered_map<NodeKind, ShaderEntry, NodeKindHash> m;
        #define V(kind, vs, vsW, fs, fsW) m[NodeKind::kind] = {vs, vsW, fs, fsW}
        // Sources
        V(ImageSource, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
        V(AudioSource, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
        V(VectorSource, kVectorSourceVertSpirv, kVectorSourceVertSpirvWords, kVectorSourceFragSpirv, kVectorSourceFragSpirvWords);
        V(TextSource, kTextSourceVertSpirv, kTextSourceVertSpirvWords, kTextSourceFragSpirv, kTextSourceFragSpirvWords);
        V(StrokeSource, kStrokeSourceVertSpirv, kStrokeSourceVertSpirvWords, kStrokeSourceFragSpirv, kStrokeSourceFragSpirvWords);
        // Effects
        V(Shader, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
        V(ColorCorrection, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kColorCorrectionFragSpirv, kColorCorrectionFragSpirvWords);
        V(Blur, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kBlurFragSpirv, kBlurFragSpirvWords);
        V(Mask, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kMaskFragSpirv, kMaskFragSpirvWords);
        V(Composite, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kCompositeFragSpirv, kCompositeFragSpirvWords);
        V(Adjustment, kAdjustmentVertSpirv, kAdjustmentVertSpirvWords, kAdjustmentFragSpirv, kAdjustmentFragSpirvWords);
        // Output
        V(Output, kOutputVertSpirv, kOutputVertSpirvWords, kOutputFragSpirv, kOutputFragSpirvWords);
        // Null
        V(Null, kNullLayerVertSpirv, kNullLayerVertSpirvWords, kNullLayerFragSpirv, kNullLayerFragSpirvWords);
        // Motion blur
        V(MotionBlur, kMotionBlurVertSpirv, kMotionBlurVertSpirvWords, kMotionBlurFragSpirv, kMotionBlurFragSpirvWords);
        V(DirectionalBlur, kDirectionalBlurVertSpirv, kDirectionalBlurVertSpirvWords, kDirectionalBlurFragSpirv, kDirectionalBlurFragSpirvWords);
        V(TransformBlur, kDirectionalBlurVertSpirv, kDirectionalBlurVertSpirvWords, kDirectionalBlurFragSpirv, kDirectionalBlurFragSpirvWords);
        // Time remap
        V(VelocityGraph, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kTimeRemapFragSpirv, kTimeRemapFragSpirvWords);
        V(TimeRemap, kTimeRemapVertSpirv, kTimeRemapVertSpirvWords, kTimeRemapFragSpirv, kTimeRemapFragSpirvWords);
        V(OpticalFlow, kTimeRemapVertSpirv, kTimeRemapVertSpirvWords, kTimeRemapFragSpirv, kTimeRemapFragSpirvWords);
        // Masking
        V(BezierMask, kBezierMaskVertSpirv, kBezierMaskVertSpirvWords, kBezierMaskFragSpirv, kBezierMaskFragSpirvWords);
        V(Rotoscoping, kBezierMaskVertSpirv, kBezierMaskVertSpirvWords, kBezierMaskFragSpirv, kBezierMaskFragSpirvWords);
        V(RotoBrush, kBezierMaskVertSpirv, kBezierMaskVertSpirvWords, kBezierMaskFragSpirv, kBezierMaskFragSpirvWords);
        V(Tracker, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
        // Particles
        V(ParticleEmitter, kParticleVertSpirv, kParticleVertSpirvWords, kParticleFragSpirv, kParticleFragSpirvWords);
        V(ParticleForces, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
        V(ParticleRenderer, kParticleVertSpirv, kParticleVertSpirvWords, kParticleFragSpirv, kParticleFragSpirvWords);
        // Shape2D
        V(ShapeRectangle, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        V(ShapeEllipse, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        V(ShapePolygon, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        V(ShapeStar, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        V(ShapePath, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        V(ShapeRender, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        V(ShapeMerge, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kShapeMergeFragSpirv, kShapeMergeFragSpirvWords);
        V(ShapeTransform, kShapeTransformVertSpirv, kShapeTransformVertSpirvWords, kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
        V(ShapeStroke, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        V(ShapeFill, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        V(ShapeRepeater, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        V(ShapeBoolean, kShape2DVertSpirv, kShape2DVertSpirvWords, kShape2DFragSpirv, kShape2DFragSpirvWords);
        // 2.5D
        V(Transform3D, kTransform3DVertSpirv, kTransform3DVertSpirvWords, kTransform3DFragSpirv, kTransform3DFragSpirvWords);
        V(Camera3D, kCamera3DVertSpirv, kCamera3DVertSpirvWords, kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
        V(DepthOfField, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kDepthOfFieldFragSpirv, kDepthOfFieldFragSpirvWords);
        // Keying
        V(ChromaKey, kChromaKeyVertSpirv, kChromaKeyVertSpirvWords, kChromaKeyFragSpirv, kChromaKeyFragSpirvWords);
        // 3D
        V(MeshSource, kMeshPBRVertSpirv, kMeshPBRVertSpirvWords, kMeshPBRFragSpirv, kMeshPBRFragSpirvWords);
        V(Group, kFullscreenVertSpirv, kFullscreenVertSpirvWords, kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
        #undef V
        return m;
    }();
    return registry;
}

static const std::unordered_map<BlendMode, std::pair<const uint32_t*, size_t>>& GetBlendFragRegistry() {
    static const std::unordered_map<BlendMode, std::pair<const uint32_t*, size_t>> registry = []{
        std::unordered_map<BlendMode, std::pair<const uint32_t*, size_t>> m;
        m[BlendMode::Normal] = {kBlendNormalFragSpirv, kBlendNormalFragSpirvWords};
        m[BlendMode::Multiply] = {kBlendMultiplyFragSpirv, kBlendMultiplyFragSpirvWords};
        m[BlendMode::Screen] = {kBlendScreenFragSpirv, kBlendScreenFragSpirvWords};
        m[BlendMode::Overlay] = {kBlendOverlayFragSpirv, kBlendOverlayFragSpirvWords};
        m[BlendMode::Add] = {kBlendAddFragSpirv, kBlendAddFragSpirvWords};
        m[BlendMode::Subtract] = {kBlendSubtractFragSpirv, kBlendSubtractFragSpirvWords};
        return m;
    }();
    return registry;
}

static std::optional<ShaderEntry> ResolveShaderEntry(NodeKind kind, BlendMode blendMode) {
    if (kind == NodeKind::Blend) {
        const auto& blendRegistry = GetBlendFragRegistry();
        auto it = blendRegistry.find(blendMode);
        if (it != blendRegistry.end()) {
            return ShaderEntry{kFullscreenVertSpirv, kFullscreenVertSpirvWords, it->second.first, it->second.second};
        }
        return std::nullopt;
    }

    const auto& registry = GetShaderRegistry();
    auto it = registry.find(kind);
    if (it != registry.end()) {
        return it->second;
    }
    return std::nullopt;
}

CompileResult RenderGraph::Compile(const NodeGraph& graph, const std::string& outputNodeId) {
    CompileResult result;

    const Node* outputNode = graph.FindNode(outputNodeId);
    if (!outputNode) {
        result.cycleNodeIds = {outputNodeId};
        LOGE("Compile: output node '%s' not found", outputNodeId.c_str());
        return result;
    }

    std::unordered_set<std::string> reachable;
    std::deque<std::string> toVisit{outputNodeId};
    while (!toVisit.empty()) {
        std::string id = toVisit.front();
        toVisit.pop_front();
        if (!reachable.insert(id).second) continue;
        for (const Connection* c : graph.InputsTo(id)) toVisit.push_back(c->fromNodeId);
    }

    // Expand groups: recursively inline group members into the main graph
    ExpandGroups(graph, reachable);

    std::unordered_map<std::string, int> inDegree;
    std::unordered_map<std::string, std::vector<std::string>> dependents;
    for (const auto& id : reachable) inDegree[id] = 0;

    for (const auto& conn : graph.AllConnections()) {
        if (!reachable.count(conn.fromNodeId) || !reachable.count(conn.toNodeId)) continue;
        inDegree[conn.toNodeId]++;
        dependents[conn.fromNodeId].push_back(conn.toNodeId);
    }

    std::deque<std::string> ready;
    for (const auto& [id, deg] : inDegree) if (deg == 0) ready.push_back(id);

    std::vector<std::string> order;
    while (!ready.empty()) {
        std::sort(ready.begin(), ready.end());
        std::string current = ready.front();
        ready.pop_front();
        order.push_back(current);

        for (const auto& dep : dependents[current]) {
            if (--inDegree[dep] == 0) ready.push_back(dep);
        }
    }

    if (order.size() != reachable.size()) {
        for (const auto& [id, deg] : inDegree) if (deg > 0) result.cycleNodeIds.push_back(id);
        LOGE("Compile: cycle detected involving %zu node(s)", result.cycleNodeIds.size());
        return result;
    }

    for (const auto& id : order) {
        const Node* node = graph.FindNode(id);
        CompiledPass pass;
        pass.nodeId = id;
        pass.kind = node->kind;
        pass.isFinalOutput = (id == outputNodeId);
        for (const Connection* c : graph.InputsTo(id)) pass.inputNodeIds.push_back(c->fromNodeId);
        
        // Mark particle nodes
        if (node->kind == NodeKind::ParticleEmitter || 
            node->kind == NodeKind::ParticleForces || 
            node->kind == NodeKind::ParticleRenderer) {
            pass.isParticleNode = true;
            if (node->kind == NodeKind::ParticleEmitter || 
                node->kind == NodeKind::ParticleForces) {
                // These run compute shaders for GPU particles
                pass.isParticleCompute = node->particle.useGpuParticles;
            }
        }
        
        result.passes.push_back(std::move(pass));
    }

    return result;
}

// Expand groups by inlining their member nodes into the main graph
void RenderGraph::ExpandGroups(const NodeGraph& graph, std::unordered_set<std::string>& reachable) {
    // Collect all groups in the reachable subgraph
    std::vector<std::string> groupNodes;
    for (const auto& id : reachable) {
        const Node* node = graph.FindNode(id);
        if (node && node->kind == NodeKind::Group) {
            groupNodes.push_back(id);
        }
    }

    // For each group, inline its members
    for (const auto& groupId : groupNodes) {
        const Node* groupNode = graph.FindNode(groupId);
        if (!groupNode) continue;

        // Get the group info
        const auto* group = graph.FindGroup(groupNode->groupId);
        if (!group) continue;

        // Collect all members of this group (recursively for nested groups)
        std::vector<std::string> groupMembers = graph.GetGroupMembers(groupNode->groupId);
        
        // Build mapping from old member IDs to new IDs (with prefix to avoid conflicts)
        std::unordered_map<std::string, std::string> idMap;
        for (const auto& memberId : groupMembers) {
            std::string newId = groupId + "_" + memberId;
            idMap[memberId] = newId;
        }

        // Note: Full group expansion would require modifying the graph structure
        // which is complex since the graph is passed as const.
        // For now, we mark group nodes to be skipped during execution.
        // A full implementation would require:
        // 1. Creating new node IDs for inlined members
        // 2. Remapping all connections (internal and external)
        // 3. Exposing group inputs/outputs as connections to/from group boundary
        // 3. Removing the group node from the execution plan
        
        // For now, we just remove the group node from reachable so it won't execute
        reachable.erase(groupNode->nodeId);
        
        // In a full implementation, we would:
        // 1. Create new nodes for each member with new IDs
        // 2. Remap internal connections
        // 3. Connect external inputs to group's exposed inputs
        // 4. Connect group's exposed outputs to external outputs
        // 5. Add the new nodes to the reachable set
    }
}

void RenderGraph::Execute(const NodeGraph& graph, const CompileResult& plan, double timelineSeconds,
                           MediaEngine* mediaEngine) {
    if (!plan.Ok()) {
        LOGE("Execute called on a plan that failed to compile — aborting frame");
        return;
    }
    
    // Check if we need to initialize particle system
    for (const auto& pass : plan.passes) {
        if (pass.isParticleNode && !particleState_.initialized) {
            // Find the particle config from the first particle node
            const Node* node = graph.FindNode(pass.nodeId);
            if (node) {
                InitializeParticleSystem(node->particle);
                break;
            }
        }
    }
    
    double deltaTime = timelineSeconds - lastTimelineSeconds_;
    if (deltaTime <= 0.0) deltaTime = 1.0 / 60.0; // fallback for first frame
    lastTimelineSeconds_ = timelineSeconds;
    
    for (const auto& pass : plan.passes) {
        ExecutePass(graph, pass, timelineSeconds, mediaEngine);
    }
    texturePool_.EndFrame();
}

ShaderModuleHandle RenderGraph::GetOrCreateShaderModule(const uint32_t* spirv, size_t wordCount) {
    std::string key(reinterpret_cast<const char*>(spirv), wordCount * sizeof(uint32_t));
    auto it = shaderModuleCache_.find(key);
    if (it != shaderModuleCache_.end()) return it->second;

    auto result = device_.CreateShaderModule(std::span<const uint32_t>(spirv, wordCount));
    if (!result) return ShaderModuleHandle{0, 0};

    ShaderModuleHandle handle = result.value;
    shaderModuleCache_[key] = handle;
    return handle;
}

void RenderGraph::ExecutePass(const NodeGraph& graph, const CompiledPass& pass, double timelineSeconds,
                                MediaEngine* mediaEngine) {
    const Node* node = graph.FindNode(pass.nodeId);
    if (!node) return;

    if (pass.kind == NodeKind::VideoSource) {
        if (mediaEngine) {
            if (auto frame = mediaEngine->TryGetFrame(pass.nodeId, timelineSeconds)) {
                lastVideoFrameByNode_[pass.nodeId] = frame->texture;
            }
        }
        return;
    }

    TextureDesc outputDesc;
    outputDesc.width = 1920;
    outputDesc.height = 1080;
    outputDesc.format = PixelFormat::RGBA8Unorm;
    outputDesc.usage = TextureUsage::ColorAttachmentAndSampled;
    outputDesc.transient = true;
    outputDesc.debugName = "pass_output_" + pass.nodeId;

    auto acquired = texturePool_.Acquire(outputDesc);
    if (!acquired) return;
    TextureHandle outputTexture = acquired.value;

    std::vector<TextureHandle> inputTextures;
    for (const std::string& inputNodeId : pass.inputNodeIds) {
        auto videoIt = lastVideoFrameByNode_.find(inputNodeId);
        if (videoIt != lastVideoFrameByNode_.end()) {
            inputTextures.push_back(videoIt->second);
        } else {
            LOGI("ExecutePass: input '%s' not found in lastVideoFrameByNode_", inputNodeId.c_str());
        }
    }

    ShaderModuleHandle vsHandle{0, 0};
    ShaderModuleHandle fsHandle{0, 0};

    // Special case: custom SPIR-V fragment shader
    if (pass.kind == NodeKind::Shader && !node->spirvFragment.empty()) {
        vsHandle = GetOrCreateShaderModule(kFullscreenVertSpirv, kFullscreenVertSpirvWords);
        fsHandle = GetOrCreateShaderModule(node->spirvFragment.data(), node->spirvFragment.size());
    } else {
        auto entry = ResolveShaderEntry(pass.kind, node->blendMode);
        if (!entry) {
            LOGE("ExecutePass: no shader entry for node '%s' kind=%d", pass.nodeId.c_str(), static_cast<int>(pass.kind));
            texturePool_.Release(outputTexture);
            return;
        }
        vsHandle = GetOrCreateShaderModule(entry->vert, entry->vertWords);
        fsHandle = GetOrCreateShaderModule(entry->frag, entry->fragWords);
    }

    if (!vsHandle.IsValid() || !fsHandle.IsValid()) {
        LOGE("ExecutePass: failed to get/create shader modules for node '%s'", pass.nodeId.c_str());
        texturePool_.Release(outputTexture);
        return;
    }

    auto pipelineResult = device_.GetOrCreatePipeline(vsHandle, fsHandle, outputDesc.usage);
    if (!pipelineResult) {
        LOGE("ExecutePass: failed to get/create pipeline for node '%s': %s", pass.nodeId.c_str(), pipelineResult.error.c_str());
        texturePool_.Release(outputTexture);
        return;
    }
    PipelineHandle pipelineHandle = pipelineResult.value;

    std::unordered_map<std::string, float> animatedUniforms;
    for (const auto& [name, track] : node->animatedUniforms) {
        if (!track.Empty()) {
            animatedUniforms[name] = track.Evaluate(timelineSeconds);
        }
    }
    for (const auto& [name, value] : node->uniformFloats) {
        if (animatedUniforms.find(name) == animatedUniforms.end()) {
            animatedUniforms[name] = value;
        }
    }

    // Handle particle nodes
    if (pass.isParticleNode) {
        if (pass.isParticleCompute && node->particle.useGpuParticles) {
            // Compute pass for particle simulation
            double deltaTime = timelineSeconds - lastTimelineSeconds_;
            if (deltaTime < 0) deltaTime = 1.0 / 60.0; // fallback
            DispatchParticleCompute(pass, deltaTime, node->particle);
            // No output texture for compute pass
            return;
        } else if (node->kind == NodeKind::ParticleRenderer) {
            // Render particles
            RenderParticles(pass, node->particle, outputTexture, inputTextures, animatedUniforms);
            return;
        }
    }

    device_.DrawFullscreenPass(pipelineHandle, inputTextures, outputTexture, animatedUniforms);
    lastVideoFrameByNode_[pass.nodeId] = outputTexture;
}

Result<TextureHandle> TransientTexturePool::Acquire(const TextureDesc& desc) {
    for (auto& entry : pool_) {
        if (entry.idle && entry.desc.width == desc.width && entry.desc.height == desc.height &&
            entry.desc.format == desc.format && entry.desc.usage == desc.usage) {
            entry.idle = false;
            return Result<TextureHandle>::Ok(entry.handle);
        }
    }
    auto created = device_.CreateTexture(desc);
    if (!created) return created;
    pool_.push_back(Entry{created.value, desc, false});
    return created;
}

void TransientTexturePool::Release(TextureHandle handle) {
    for (auto& entry : pool_) {
        if (entry.handle == handle) { entry.idle = true; return; }
    }
}

void TransientTexturePool::EndFrame() {
    for (auto& entry : pool_) entry.idle = true;
}

void TransientTexturePool::EndFrame() {
    for (auto& entry : pool_) entry.idle = true;
}

// Particle system initialization
void RenderGraph::InitializeParticleSystem(const ParticleConfig& config) {
    if (particleState_.initialized) return;
    
    particleState_.maxParticles = config.maxParticles;
    
    // Create particle storage buffer (storage buffer for compute shader read/write)
    BufferDesc particleBufferDesc;
    particleBufferDesc.size = sizeof(vfx::Particle) * particleState_.maxParticles;
    particleBufferDesc.usage = BufferUsage::StorageBuffer;
    particleBufferDesc.hostVisible = false;
    particleBufferDesc.debugName = "particle_buffer";
    
    auto particleBufferResult = device_.CreateBuffer(particleBufferDesc);
    if (particleBufferResult) {
        particleState_.particleBuffer = particleBufferResult.value;
    }
    
    // Create simulation params uniform buffer
    BufferDesc simParamsDesc;
    simParamsDesc.size = 256; // enough for sim params
    simParamsDesc.usage = BufferUsage::UniformBuffer;
    simParamsDesc.hostVisible = true;
    simParamsDesc.debugName = "particle_sim_params";
    
    auto simParamsResult = device_.CreateBuffer(simParamsDesc);
    if (simParamsResult) {
        particleState_.simParamsBuffer = simParamsResult.value;
    }
    
    // Create compute pipeline for particle simulation
    auto csHandle = GetOrCreateShaderModule(kParticleSimCompSpirv, kParticleSimCompSpirvWords);
    if (csHandle.IsValid()) {
        // Create compute pipeline layout and pipeline
        // This would require adding compute pipeline support to VulkanDevice
        // For now, mark as initialized
        particleState_.initialized = true;
    }
}

void RenderGraph::DispatchParticleCompute(const CompiledPass& pass, double deltaTime, const ParticleConfig& config) {
    if (!particleState_.initialized || !config.useGpuParticles) return;
    
    // Update simulation params buffer
    // Dispatch compute shader
    // This would require compute dispatch support in VulkanDevice
    // For now, just increment frame index
    particleState_.frameIndex++;
}

void RenderGraph::RenderParticles(const CompiledPass& pass, const ParticleConfig& config, TextureHandle outputTexture,
                                  const std::vector<TextureHandle>& inputTextures, const std::unordered_map<std::string, float>& uniforms) {
    if (!particleState_.initialized) return;
    
    // Get the particle render pipeline
    // Bind particle buffer as storage buffer
    // Draw instanced (one instance per particle)
    // This would require a custom draw call in VulkanDevice
    
    // For now, fall back to regular draw
    // device_.DrawFullscreenPass(pipelineHandle, inputTextures, outputTexture, animatedUniforms);
}
