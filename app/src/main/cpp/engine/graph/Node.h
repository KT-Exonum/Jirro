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
    AudioSource,
    Shader,
    Blend,
    ColorCorrection,
    Blur,
    Mask,
    Composite,
    Output,
    Adjustment,
    Null,
    Group,
    VectorSource,
    TextSource,
    StrokeSource,
};

enum class BlendMode { Normal, Multiply, Screen, Overlay, Add, Subtract };

// Parenting/rigging: transform hierarchy
struct Transform {
    float positionX = 0.0f;
    float positionY = 0.0f;
    float rotation = 0.0f; // radians
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float anchorX = 0.5f;
    float anchorY = 0.5f;

    [[nodiscard]] float EvaluatePositionX(double timelineSeconds) const { return positionX; }
    [[nodiscard]] float EvaluatePositionY(double timelineSeconds) const { return positionY; }
    [[nodiscard]] float EvaluateRotation(double timelineSeconds) const { return rotation; }
    [[nodiscard]] float EvaluateScaleX(double timelineSeconds) const { return scaleX; }
    [[nodiscard]] float EvaluateScaleY(double timelineSeconds) const { return scaleY; }
};

// Expression/scripting support
struct Expression {
    std::string script; // JavaScript/Lua expression
    std::vector<std::string> dependencies; // variable names this expression reads
    
    [[nodiscard]] bool IsValid() const { return !script.empty(); }
};

// Node socket with type info for validation
struct NodeSocket {
    std::string slotName;
    std::string valueType = "float4"; // "float", "float2", "float4", "audio", "image", "video", "transform"
    bool isArray = false; // for multi-inputs like Blend with multiple layers
};

struct Connection {
    std::string fromNodeId;
    std::string fromSlot;
    std::string toNodeId;
    std::string toSlot;
};

struct NodeGroup {
    std::string groupId;
    std::string name;
    std::vector<std::string> memberNodeIds;
    std::vector<NodeSocket> exposedInputs;  // inputs from outside the group
    std::vector<NodeSocket> exposedOutputs; // outputs to outside the group
    bool isCollapsed = true; // UI state
};

// Onion skinning configuration
struct OnionSkinConfig {
    bool enabled = false;
    int framesBefore = 2;
    int framesAfter = 2;
    float opacityBefore = 0.3f;
    float opacityAfter = 0.3f;
    float colorBeforeR = 1.0f, colorBeforeG = 0.0f, colorBeforeB = 0.0f; // red for past
    float colorAfterR = 0.0f, colorAfterG = 0.0f, colorAfterB = 1.0f; // blue for future
};

struct Node {
    std::string nodeId;
    NodeKind kind;
    std::string debugName;

    std::vector<NodeSocket> inputs;
    NodeSocket output{"output"};

    // Static (non-animated) parameters.
    std::unordered_map<std::string, float> uniformFloats;

    // Animated parameters
    std::unordered_map<std::string, KeyframeTrack> animatedUniforms;

    // Expressions for procedural animation
    std::unordered_map<std::string, Expression> expressions;

    // Transform for parenting/rigging
    Transform transform;
    std::string parentNodeId; // empty = no parent (world space)

    // Node grouping
    std::string groupId; // empty = not in a group
    bool isGroupRoot = false; // true if this node represents a collapsed group

    // Only meaningful for kind == Shader.
    std::vector<uint32_t> spirvFragment;

    // Only meaningful for kind == Blend.
    BlendMode blendMode = BlendMode::Normal;

    // Only meaningful for kind == VideoSource / AudioSource.
    std::string sourceFilePath;

    // Only meaningful for kind == VectorSource.
    std::string svgPathData; // SVG path data for vector shapes

    // Only meaningful for kind == TextSource.
    std::string textContent = "";
    std::string fontPath = "";
    float fontSize = 48.0f;
    float lineHeight = 1.2f;
    int alignment = 0; // 0=left, 1=center, 2=right

    // Only meaningful for kind == StrokeSource.
    std::vector<float> strokePoints; // flattened x,y,x,y... points
    float strokeWidth = 2.0f;
    uint32_t strokeColor = 0xFFFFFFFF;

    // Only meaningful for kind == Output.
    OnionSkinConfig onionSkin;

    [[nodiscard]] float EvaluateUniform(const std::string& name, double timelineSeconds) const {
        // Check animated uniforms first
        if (auto it = animatedUniforms.find(name); it != animatedUniforms.end() && !it->second.Empty()) {
            return it->second.Evaluate(timelineSeconds);
        }
        // Check expressions
        if (auto it = expressions.find(name); it != expressions.end() && it->second.IsValid()) {
            // Expression evaluation would happen here (via script engine)
            // For now fall through to static value
        }
        if (auto it = uniformFloats.find(name); it != uniformFloats.end()) return it->second;
        return 0.0f;
    }

    [[nodiscard]] Transform EvaluateTransform(double timelineSeconds) const {
        Transform t = transform;
        t.positionX = EvaluateUniform("positionX", timelineSeconds);
        t.positionY = EvaluateUniform("positionY", timelineSeconds);
        t.rotation = EvaluateUniform("rotation", timelineSeconds);
        t.scaleX = EvaluateUniform("scaleX", timelineSeconds);
        t.scaleY = EvaluateUniform("scaleY", timelineSeconds);
        return t;
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
        // Also remove from any groups
        for (auto& [gid, group] : groups_) {
            std::erase(group.memberNodeIds, id);
        }
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

    // Node grouping
    void CreateGroup(const std::string& groupId, const std::string& name, const std::vector<std::string>& memberIds) {
        NodeGroup group;
        group.groupId = groupId;
        group.name = name;
        group.memberNodeIds = memberIds;
        // Auto-expose inputs/outputs from boundary nodes
        for (const auto& memberId : memberIds) {
            if (auto* node = FindNodeMutable(memberId)) {
                node->groupId = groupId;
            }
        }
        groups_[groupId] = std::move(group);
    }

    void RemoveGroup(const std::string& groupId) {
        auto it = groups_.find(groupId);
        if (it != groups_.end()) {
            for (const auto& memberId : it->second.memberNodeIds) {
                if (auto* node = FindNodeMutable(memberId)) {
                    node->groupId.clear();
                }
            }
            groups_.erase(it);
        }
    }

    [[nodiscard]] const std::unordered_map<std::string, NodeGroup>& AllGroups() const { return groups_; }
    [[nodiscard]] const NodeGroup* FindGroup(const std::string& groupId) const {
        auto it = groups_.find(groupId);
        return it == groups_.end() ? nullptr : &it->second;
    }

    // Get all nodes in a group (including nested)
    [[nodiscard]] std::vector<std::string> GetGroupMembers(const std::string& groupId) const {
        auto* group = FindGroup(groupId);
        if (!group) return {};
        return group->memberNodeIds;
    }

    // Multi-select support
    std::unordered_set<std::string> selectedNodeIds;
    std::unordered_set<std::string> selectedGroupIds;

    // Undo/Redo support
    struct HistoryEntry {
        enum class Type { NodeAdded, NodeRemoved, NodeModified, ConnectionAdded, ConnectionRemoved, GroupCreated, GroupRemoved, PropertyChanged };
        Type type;
        std::string targetId;
        std::string propertyName;
        std::string oldValue; // serialized
        std::string newValue; // serialized
    };
    
    std::vector<HistoryEntry> history_;
    size_t historyIndex_ = 0;
    const size_t maxHistorySize_ = 100;

    void RecordChange(const HistoryEntry& entry) {
        // Truncate future history if we're not at the end
        if (historyIndex_ < history_.size()) {
            history_.resize(historyIndex_);
        }
        history_.push_back(entry);
        if (history_.size() > maxHistorySize_) {
            history_.erase(history_.begin());
        } else {
            historyIndex_ = history_.size();
        }
    }

    bool CanUndo() const { return historyIndex_ > 0; }
    bool CanRedo() const { return historyIndex_ < history_.size(); }

private:
    std::unordered_map<std::string, Node> nodes_;
    std::vector<Connection> connections_;
    std::unordered_map<std::string, NodeGroup> groups_;
};

} // namespace vfx
