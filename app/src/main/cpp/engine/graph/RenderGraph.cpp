#include "RenderGraph.h"

#include <android/log.h>

#include <algorithm>
#include <array>
#include <deque>
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
        // Phase 2 wiring: MediaEngine::TryGetFrame is non-blocking (Section
        // 14 — the engine thread must never stall on decode latency), so a
        // miss this tick is expected and not an error; we hold the last
        // frame this node produced rather than flashing to black while the
        // media thread catches up.
        if (mediaEngine) {
            if (auto frame = mediaEngine->TryGetFrame(pass.nodeId, timelineSeconds)) {
                lastVideoFrameByNode_[pass.nodeId] = frame->texture;
            }
        }
        return;
    }

    // For all other node kinds, we need to render into an output texture
    // Acquire output texture from pool (unless this is the final output)
    TextureDesc outputDesc;
    outputDesc.width = 1920;  // TODO: get from swapchain/surface size
    outputDesc.height = 1080;
    outputDesc.format = PixelFormat::RGBA8Unorm;
    outputDesc.usage = pass.isFinalOutput ? TextureUsage::ColorAttachmentAndSampled : TextureUsage::ColorAttachmentAndSampled;
    outputDesc.transient = true;
    outputDesc.debugName = "pass_output_" + pass.nodeId;

    TextureHandle outputTexture;
    if (pass.isFinalOutput) {
        // For final output, we need the swapchain image - but we don't have direct access
        // In a real implementation, this would be the swapchain texture
        // For now, acquire from pool
        auto acquired = texturePool_.Acquire(outputDesc);
        if (!acquired) return;
        outputTexture = acquired.value;
    } else {
        auto acquired = texturePool_.Acquire(outputDesc);
        if (!acquired) return;
        outputTexture = acquired.value;
    }

    // Gather input textures
    std::vector<TextureHandle> inputTextures;
    for (const std::string& inputNodeId : pass.inputNodeIds) {
        // Check if input is a VideoSource (has decoded frame)
        auto videoIt = lastVideoFrameByNode_.find(inputNodeId);
        if (videoIt != lastVideoFrameByNode_.end()) {
            inputTextures.push_back(videoIt->second);
        } else {
            // For other node types, the output should have been stored
            // We'd need to track intermediate outputs - for now skip
            LOGI("ExecutePass: input '%s' not found in lastVideoFrameByNode_", inputNodeId.c_str());
        }
    }

    // Get or create shader modules and pipeline based on node kind
    ShaderModuleHandle vsHandle = GetOrCreateShaderModule(kFullscreenVertSpirv, kFullscreenVertSpirvWords);
    ShaderModuleHandle fsHandle{0, 0};

    switch (pass.kind) {
        case NodeKind::ImageSource: {
            // Simple passthrough - use normal blend shader
            fsHandle = GetOrCreateShaderModule(kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
            break;
        }
        case NodeKind::Shader: {
            // Custom shader node - use the node's SPIR-V
            if (!node->spirvFragment.empty()) {
                fsHandle = GetOrCreateShaderModule(node->spirvFragment.data(), node->spirvFragment.size());
            } else {
                fsHandle = GetOrCreateShaderModule(kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
            }
            break;
        }
        case NodeKind::Blend: {
            switch (node->blendMode) {
                case BlendMode::Normal:     fsHandle = GetOrCreateShaderModule(kBlendNormalFragSpirv, kBlendNormalFragSpirvWords); break;
                case BlendMode::Multiply:   fsHandle = GetOrCreateShaderModule(kBlendMultiplyFragSpirv, kBlendMultiplyFragSpirvWords); break;
                case BlendMode::Screen:     fsHandle = GetOrCreateShaderModule(kBlendScreenFragSpirv, kBlendScreenFragSpirvWords); break;
                case BlendMode::Overlay:    fsHandle = GetOrCreateShaderModule(kBlendOverlayFragSpirv, kBlendOverlayFragSpirvWords); break;
                case BlendMode::Add:        fsHandle = GetOrCreateShaderModule(kBlendAddFragSpirv, kBlendAddFragSpirvWords); break;
                case BlendMode::Subtract:   fsHandle = GetOrCreateShaderModule(kBlendSubtractFragSpirv, kBlendSubtractFragSpirvWords); break;
            }
            break;
        }
        case NodeKind::ColorCorrection: {
            fsHandle = GetOrCreateShaderModule(kColorCorrectionFragSpirv, kColorCorrectionFragSpirvWords);
            break;
        }
        case NodeKind::Blur: {
            fsHandle = GetOrCreateShaderModule(kBlurFragSpirv, kBlurFragSpirvWords);
            break;
        }
        case NodeKind::Mask: {
            fsHandle = GetOrCreateShaderModule(kMaskFragSpirv, kMaskFragSpirvWords);
            break;
        }
        case NodeKind::Composite: {
            fsHandle = GetOrCreateShaderModule(kCompositeFragSpirv, kCompositeFragSpirvWords);
            break;
        }
        case NodeKind::Output: {
            // Output just passes through the input
            fsHandle = GetOrCreateShaderModule(kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
            break;
        }
        default: {
            fsHandle = GetOrCreateShaderModule(kBlendNormalFragSpirv, kBlendNormalFragSpirvWords);
            break;
        }
    }

    if (!vsHandle.IsValid() || !fsHandle.IsValid()) {
        LOGE("ExecutePass: failed to get/create shader modules for node '%s'", pass.nodeId.c_str());
        if (!pass.isFinalOutput) texturePool_.Release(outputTexture);
        return;
    }

    // Get or create pipeline
    auto pipelineResult = device_.GetOrCreatePipeline(vsHandle, fsHandle, outputDesc.usage);
    if (!pipelineResult) {
        LOGE("ExecutePass: failed to get/create pipeline for node '%s': %s", pass.nodeId.c_str(), pipelineResult.error.c_str());
        if (!pass.isFinalOutput) texturePool_.Release(outputTexture);
        return;
    }
    PipelineHandle pipelineHandle = pipelineResult.value;

    // Draw the pass
    device_.DrawFullscreenPass(pipelineHandle, inputTextures, outputTexture);

    // Store output texture for downstream passes
    lastVideoFrameByNode_[pass.nodeId] = outputTexture;

    // Release output texture back to pool if not final (but keep reference for downstream)
    // The TransientTexturePool::EndFrame() will mark all as idle
    // We keep the handle in lastVideoFrameByNode_ so downstream passes can use it
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
