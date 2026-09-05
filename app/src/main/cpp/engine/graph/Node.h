#pragma once
// Section 2/10: node/connection data model. This is intentionally decoupled
// from GraphicsDevice and from the Compose node editor — it's the shape
// RenderGraph consumes, produced either by the UI (via JNI commands) or by
// tests directly constructing a graph in C++.

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "engine/core/Types.h"
#include "engine/timeline/Keyframe.h"

namespace vfx {

enum class NodeKind {
    VideoSource,
    ImageSource,
    Shader,
    Blend,
    ColorCorrection,
    Blur,
    Mask,
    Composite,
    Output,
};

enum class BlendMode { Normal, Multiply, Screen, Overlay, Add, Subtract };

// A single input/output socket on a node. `slotName` lets a node expose
// multiple distinct inputs (e.g. Blend needs "base" and "overlay", not just
// "input0"/"input1") which the node editor can label meaningfully.
struct NodeSocket {
    std::string slotName;
};

struct Connection {
    std::string fromNodeId;
    std::string fromSlot;
    std::string toNodeId;
    std::string toSlot;
};

// Section 3's ShaderNode example, generalized: every node (not just Shader)
// can expose animatable float uniforms, since color-correction, blur radius,
// mask feather, etc. are all "just uniforms" to the render graph even if the
// node editor gives them dedicated widgets.
struct Node {
    std::string nodeId;
    NodeKind kind;
    std::string debugName;

    std::vector<NodeSocket> inputs;
    NodeSocket output{"output"};

    // Static (non-animated) parameters.
    std::unordered_map<std::string, float> uniformFloats;

    // Animated parameters — takes precedence over uniformFloats for the same
    // key when a track has at least one keyframe (Section 11: "shader
    // uniforms ... should be animatable").
    std::unordered_map<std::string, KeyframeTrack> animatedUniforms;

    // Only meaningful for kind == Shader.
    std::vector<uint32_t> spirvFragment;

    // Only meaningful for kind == Blend.
    BlendMode blendMode = BlendMode::Normal;

    // Only meaningful for kind == VideoSource (Phase 2). This is the file
    // path MediaEngine's DecoderPool opens a decoder for; nodeId doubles as
    // the clipId key into MediaEngine::TryGetFrame/FrameCache so a single
    // node-graph node maps 1:1 onto one decoded-frame stream even though a
    // Timeline::Clip is a separate, reusable placement of it (see Timeline.h).
    std::string sourceFilePath;

    [[nodiscard]] float EvaluateUniform(const std::string& name, double timelineSeconds) const {
        if (auto it = animatedUniforms.find(name); it != animatedUniforms.end() && !it->second.Empty()) {
            return it->second.Evaluate(timelineSeconds);
        }
        if (auto it = uniformFloats.find(name); it != uniformFloats.end()) return it->second;
        return 0.0f;
    }
};

// The editable graph: nodes + connections, as authored by the node editor.
// RenderGraph (RenderGraph.h) compiles an instance of this into an ordered,
// resource-allocated execution plan for a specific timeline timestamp.
class NodeGraph {
public:
    void AddNode(Node node) { nodes_[node.nodeId] = std::move(node); }
    void RemoveNode(const std::string& id) {
        nodes_.erase(id);
        std::erase_if(connections_, [&](const Connection& c) {
            return c.fromNodeId == id || c.toNodeId == id;
        });
    }
    void Connect(Connection c) { connections_.push_back(std::move(c)); }

    [[nodiscard]] const Node* FindNode(const std::string& id) const {
        auto it = nodes_.find(id);
        return it == nodes_.end() ? nullptr : &it->second;
    }
    Node* FindNodeMutable(const std::string& id) {
        auto it = nodes_.find(id);
        return it == nodes_.end() ? nullptr : &it->second;
    }

    [[nodiscard]] const std::unordered_map<std::string, Node>& AllNodes() const { return nodes_; }
    [[nodiscard]] const std::vector<Connection>& AllConnections() const { return connections_; }

    [[nodiscard]] std::vector<const Connection*> InputsTo(const std::string& nodeId) const {
        std::vector<const Connection*> result;
        for (const auto& c : connections_) if (c.toNodeId == nodeId) result.push_back(&c);
        return result;
    }

private:
    std::unordered_map<std::string, Node> nodes_;
    std::vector<Connection> connections_;
};

} // namespace vfx
