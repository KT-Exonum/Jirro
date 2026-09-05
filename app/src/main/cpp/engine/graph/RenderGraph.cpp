#include "RenderGraph.h"

#include <android/log.h>

#include <algorithm>
#include <deque>
#include <unordered_set>

#define LOG_TAG "RenderGraph"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vfx {

CompileResult RenderGraph::Compile(const NodeGraph& graph, const std::string& outputNodeId) {
    CompileResult result;

    const Node* outputNode = graph.FindNode(outputNodeId);
    if (!outputNode) {
        result.cycleNodeIds = {outputNodeId}; // reuse the failure slot: "not found" is also fatal
        LOGE("Compile: output node '%s' not found", outputNodeId.c_str());
        return result;
    }

    // Walk backwards from the output to find the subgraph actually feeding
    // it — nodes disconnected from the output are compiled out rather than
    // wasting a pass (and a pooled texture) on dead branches.
    std::unordered_set<std::string> reachable;
    std::deque<std::string> toVisit{outputNodeId};
    while (!toVisit.empty()) {
        std::string id = toVisit.front();
        toVisit.pop_front();
        if (!reachable.insert(id).second) continue;
        for (const Connection* c : graph.InputsTo(id)) toVisit.push_back(c->fromNodeId);
    }

    // Kahn's algorithm over the reachable subgraph. Using in-degree counting
    // (rather than DFS-with-recursion-stack) so a pathological user graph
    // can't blow the native stack, and so cycle nodes are easy to report
    // back to the node editor (whatever's left with nonzero in-degree at the
    // end is part of a cycle).
    std::unordered_map<std::string, int> inDegree;
    std::unordered_map<std::string, std::vector<std::string>> dependents; // nodeId -> nodes that depend on it
    for (const auto& id : reachable) inDegree[id] = 0;

    for (const auto& conn : graph.AllConnections()) {
        if (!reachable.count(conn.fromNodeId) || !reachable.count(conn.toNodeId)) continue;
        inDegree[conn.toNodeId]++;
        dependents[conn.fromNodeId].push_back(conn.toNodeId);
    }

    std::deque<std::string> ready;
    for (const auto& [id, deg] : inDegree) if (deg == 0) ready.push_back(id);

    // Deterministic ordering among independent nodes (e.g. Mask/Overlay in
    // the spec's example diagram) matters for reproducible frame output, so
    // sort the ready set rather than relying on unordered_map iteration order.
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
        // pass.output texture is assigned lazily in Execute() from
        // texturePool_ (or, for VideoSource, straight from MediaEngine — see
        // ExecutePass), not here — Compile() only fixes topology so it can
        // be cached across frames independent of texture residency.
        result.passes.push_back(std::move(pass));
    }

    return result;
}

void RenderGraph::Execute(const NodeGraph& graph, const CompileResult& plan, double timelineSeconds,
                           MediaEngine* mediaEngine) {
    if (!plan.Ok()) {
        LOGE("Execute called on a plan that failed to compile — aborting frame");
        return;
    }
    for (const auto& pass : plan.passes) {
        ExecutePass(graph, pass, timelineSeconds, mediaEngine);
    }
    texturePool_.EndFrame();
}

void RenderGraph::ExecutePass(const NodeGraph& graph, const CompiledPass& pass, double timelineSeconds,
                               MediaEngine* mediaEngine) {
    const Node* node = graph.FindNode(pass.nodeId);
    if (!node) return;

    if (pass.kind == NodeKind::VideoSource) {
        // Phase 2 wiring: MediaEngine::TryGetFrame is non-blocking (Section
        // 14 — the engine thread must never stall on decode latency), so a
        // miss this tick is expected and not an error; we hold the last
        // frame this node produced rather than flashing to black while the
        // media thread catches up.
        //
        // Note: `timelineSeconds` here is the *timeline* clock, but
        // TryGetFrame is keyed by *source* time within the clip (post
        // trim/speed mapping, see Clip::ToSourceTime in Timeline.h). Wiring
        // that mapping through requires RenderGraph to know which Clip (not
        // just which Node) is active — that plumbing is Engine::Tick's job
        // (it calls Timeline::ActiveClipsAt() then MediaEngine::SetActiveClips()
        // with each clip's ToSourceTime already applied), so by the time we
        // get here `timelineSeconds` has already been resolved to source
        // time for VideoSource passes specifically. See Engine.cpp.
        if (mediaEngine) {
            if (auto frame = mediaEngine->TryGetFrame(pass.nodeId, timelineSeconds)) {
                lastVideoFrameByNode_[pass.nodeId] = frame->texture;
            }
        }
        // pass.output is const in CompiledPass as stored in the plan; the
        // texture actually sampled by downstream passes is looked up by
        // node id from lastVideoFrameByNode_ rather than mutated here, so
        // Compile()'s cached plan never needs to change when frames arrive
        // asynchronously at different times than compilation.
        return;
    }

    // GPU-dependent execution seam for everything else. Wiring this up needs:
    //  - ImageSource: static-image decode + upload (not yet implemented;
    //    Phase 2 only covers video per the spec's phase breakdown)
    //  - Shader/ColorCorrection/Blur/Mask: GetOrCreatePipeline + uniform
    //    upload from node->EvaluateUniform(name, timelineSeconds), then
    //    device_.DrawFullscreenPass, sampling lastVideoFrameByNode_[inputId]
    //    for any input that traces back to a VideoSource node (Phase 3)
    //  - Blend/Composite: same, with two bound inputs and node->blendMode
    //    selecting the pipeline's blend-state variant
    //  - Output: DrawFullscreenPass into the swapchain-backed texture
    //    instead of a pooled one, then let VulkanDevice::EndFrame() present
    //
    // This function intentionally does not fabricate a rendering result —
    // see README.md's status table for why.
    (void)node;
    (void)texturePool_;
    (void)pipelineCache_;
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

} // namespace vfx
