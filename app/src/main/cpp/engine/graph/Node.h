#pragma once
// Section 2/10: node/connection data model. This is intentionally decoupled
// from GraphicsDevice and from the Compose node editor — it's the shape
// RenderGraph consumes, produced either by the UI (via JNI commands) or by
// tests directly constructing a graph in C++.

#include <memory>
#include <string>
#include <unordered_map>
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
    // Motion blur
    MotionBlur,
    DirectionalBlur,
    TransformBlur,    // Transform with integrated motion blur
    // Velocity/Time remapping
    VelocityGraph,
    TimeRemap,
    OpticalFlow,      // For frame interpolation
    // Masking/Rotoscoping
    BezierMask,
    Rotoscoping,
    RotoBrush,
    Tracker,          // Planar/point tracker
    // Particle system
    ParticleEmitter,
    ParticleForces,
    ParticleRenderer,
    // Shape2D System (DaVinci Resolve style sNodes)
    ShapeRectangle,
    ShapeEllipse,
    ShapePolygon,
    ShapeStar,
    ShapePath,        // Custom bezier path
    ShapeRender,      // Renders vector shapes to pixels
    ShapeMerge,       // Boolean operations (union, subtract, intersect)
    ShapeTransform,   // Vector transform (pre-render, infinite scale)
    ShapeStroke,      // Stroke on shapes
    ShapeFill,        // Fill (solid, gradient, texture)
    ShapeRepeater,    // Duplicate/array shapes
    ShapeBoolean,     // Boolean operations between shapes
    // 2.5D System (Z-axis for 2D planes)
    Transform3D,      // 3D transform on 2D layer
    Camera3D,         // 3D camera with DOF
    DepthOfField,     // Depth of field post-process
    // Keying/Compositing
    ChromaKey,        // Green/blue screen keying
    // 3D Models
    MeshSource,       // GLTF/OBJ mesh with PBR materials
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

// Motion blur configuration
struct MotionBlurConfig {
    bool enabled = true;
    float shutterAngle = 180.0f;        // degrees (180 = 50% shutter)
    float shutterPhase = -90.0f;        // degrees (-90 = centered on frame)
    int samples = 16;                   // number of temporal samples
    float sampleDistribution = 0.0f;    // 0=uniform, 1=gaussian
    bool useVelocityBuffer = true;      // use velocity buffer for object motion blur
    float maxBlurRadius = 64.0f;        // clamp extreme blur
    // For directional/transform blur
    float blurLength = 1.0f;            // multiplier for transform-based blur
    bool useTransformDerivatives = true; // compute velocity from transform animation
};

// Velocity graph / time remap configuration
struct VelocityConfig {
    // Velocity curve (position vs time derivative)
    std::vector<Keyframe> velocityKeyframes; // time, velocity value
    InterpolationType velocityInterp = InterpolationType::Bezier;
    // Time remap curve
    std::vector<Keyframe> timeRemapKeyframes; // input time -> output time
    InterpolationType timeRemapInterp = InterpolationType::Bezier;
    // Optical flow
    bool enableOpticalFlow = false;
    float flowQuality = 0.5f;           // 0=fast, 1=quality
    int flowIterations = 5;
    // Frame blending fallback
    bool enableFrameBlending = true;
    BlendMode blendMode = BlendMode::Normal;
};

// Bezier mask / Rotoscoping configuration
struct MaskConfig {
    // Bezier spline points (flattened: x,y, inTangentX,inTangentY, outTangentX,outTangentY, ...)
    std::vector<float> splinePoints;
    bool isClosed = true;
    float feather = 0.0f;               // edge feather in pixels
    float expansion = 0.0f;             // positive=expand, negative=contract
    float opacity = 1.0f;
    int featherFalloff = 0;             // 0=linear, 1=smooth, 2=gaussian
    // Rotoscoping specific
    bool autoKeyframe = false;          // auto-create keyframes on shape change
    int keyframeInterval = 1;           // keyframe every N frames
    std::vector<std::string> trackedPointIds; // point IDs linked to tracker
    // Roto brush
    bool useRotoBrush = false;
    std::vector<float> brushStrokes;    // foreground/background strokes
    float brushSize = 20.0f;
    float brushHardness = 0.5f;
};

// Particle system configuration
struct ParticleConfig {
    // Emitter
    enum class EmitterShape { Point, Line, Disc, Sphere, Box, Mesh };
    EmitterShape emitterShape = EmitterShape::Point;
    float emitRate = 100.0f;            // particles per second
    float emitRateVariation = 0.0f;
    float initialLife = 2.0f;           // seconds
    float lifeVariation = 0.5f;
    // Initial velocity
    float initialSpeed = 100.0f;        // pixels per second
    float speedVariation = 0.0f;
    float emitAngle = 0.0f;             // degrees
    float angleVariation = 360.0f;
    // Forces (applied per frame)
    float gravity = 0.0f;
    float windX = 0.0f;
    float windY = 0.0f;
    float turbulence = 0.0f;
    float drag = 0.0f;
    // Appearance
    float startSize = 10.0f;
    float endSize = 1.0f;
    uint32_t startColor = 0xFFFFFFFF;
    uint32_t endColor = 0xFFFFFF00;
    float startRotation = 0.0f;
    float rotationSpeed = 0.0f;
    // Rendering
    bool additiveBlending = true;
    bool sortByDepth = false;
    int maxParticles = 10000;
    // GPU simulation toggle
    bool useGpuParticles = true;
    // Sub-emitters (spawn on death)
    bool enableSubEmitters = false;
    float subEmitProbability = 0.1f;
};

// Shape2D configuration (DaVinci Resolve style)
struct Shape2DConfig {
    // Shape type and parameters
    enum class ShapeType { Rectangle, Ellipse, Polygon, Star, CustomPath };
    ShapeType shapeType = ShapeType::Rectangle;
    
    // Rectangle
    float rectWidth = 100.0f;
    float rectHeight = 100.0f;
    float rectCornerRadius = 0.0f;
    
    // Ellipse
    float ellipseWidth = 100.0f;
    float ellipseHeight = 100.0f;
    
    // Polygon
    int polygonSides = 6;
    float polygonRadius = 50.0f;
    float polygonRotation = 0.0f;
    float polygonRoundness = 0.0f;
    
    // Star
    int starPoints = 5;
    float starOuterRadius = 50.0f;
    float starInnerRadius = 20.0f;
    float starRotation = 0.0f;
    
    // Custom path
    std::vector<float> pathPoints; // flattened x,y for bezier path
    bool pathClosed = true;
    
    // Transform (vector space, pre-render)
    float positionX = 0.0f;
    float positionY = 0.0f;
    float rotation = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float anchorX = 0.5f;
    float anchorY = 0.5f;
    float skewX = 0.0f;
    float skewY = 0.0f;
    
    // Stroke
    bool strokeEnabled = true;
    float strokeWidth = 2.0f;
    uint32_t strokeColor = 0xFFFFFFFF;
    enum class StrokeStyle { Solid, Dash, Dot, DashDot };
    StrokeStyle strokeStyle = StrokeStyle::Solid;
    std::vector<float> strokeDashPattern;
    
    // Fill
    bool fillEnabled = true;
    enum class FillType { Solid, LinearGradient, RadialGradient, Texture };
    FillType fillType = FillType::Solid;
    uint32_t fillColor = 0xFFFFFFFF;
    // Gradient
    float gradStartX = 0.0f, gradStartY = 0.0f;
    float gradEndX = 100.0f, gradEndY = 0.0f;
    uint32_t gradColor1 = 0xFFFFFFFF;
    uint32_t gradColor2 = 0xFF0000FF;
    // Texture
    std::string fillTexturePath;
    
    // Boolean operations (for ShapeMerge/ShapeBoolean)
    enum class BooleanOp { Union, Subtract, Intersect, Difference, XOR };
    BooleanOp booleanOp = BooleanOp::Union;
    bool invertMask = false;
    
    // Repeater (ShapeRepeater)
    int repeatCount = 1;
    float repeatOffsetX = 0.0f;
    float repeatOffsetY = 0.0f;
    float repeatRotation = 0.0f;
    float repeatScale = 1.0f;
    float repeatOpacity = 1.0f;
    
    // Render settings
    float renderQuality = 1.0f;       // 0.5=fast, 1=quality, 2=supersampled
    bool antialias = true;
    float feather = 0.0f;             // edge feather
};

// 2.5D Transform (extends 2D Transform with Z-axis)
struct Transform3D {
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float rotationX = 0.0f;  // radians
    float rotationY = 0.0f;
    float rotationZ = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float scaleZ = 1.0f;
    float anchorX = 0.5f;
    float anchorY = 0.5f;
    float anchorZ = 0.0f;

    [[nodiscard]] float EvaluatePositionX(double timelineSeconds) const { return positionX; }
    [[nodiscard]] float EvaluatePositionY(double timelineSeconds) const { return positionY; }
    [[nodiscard]] float EvaluatePositionZ(double timelineSeconds) const { return positionZ; }
    [[nodiscard]] float EvaluateRotationX(double timelineSeconds) const { return rotationX; }
    [[nodiscard]] float EvaluateRotationY(double timelineSeconds) const { return rotationY; }
    [[nodiscard]] float EvaluateRotationZ(double timelineSeconds) const { return rotationZ; }
    [[nodiscard]] float EvaluateScaleX(double timelineSeconds) const { return scaleX; }
    [[nodiscard]] float EvaluateScaleY(double timelineSeconds) const { return scaleY; }
    [[nodiscard]] float EvaluateScaleZ(double timelineSeconds) const { return scaleZ; }
};

// 3D Camera for 2.5D compositing
struct Camera3D {
    float focalLength = 50.0f;      // mm
    float aperture = 2.8f;          // f-stop (for DOF)
    float focusDistance = 1000.0f;  // mm
    float nearPlane = 1.0f;
    float farPlane = 10000.0f;
    Transform3D transform;          // world transform

    [[nodiscard]] float EvaluateFocalLength(double timelineSeconds) const { return focalLength; }
    [[nodiscard]] float EvaluateAperture(double timelineSeconds) const { return aperture; }
    [[nodiscard]] float EvaluateFocusDistance(double timelineSeconds) const { return focusDistance; }
};

// Depth of Field configuration
struct DepthOfFieldConfig {
    bool enabled = true;
    float focalLength = 50.0f;      // mm (from camera)
    float aperture = 2.8f;          // f-stop
    float focusDistance = 1000.0f;  // mm
    int samples = 8;                // bokeh sample count
    float maxCoC = 0.02f;           // max circle of confusion (normalized)
    bool useBokehShape = false;     // polygonal bokeh
    int bokehSides = 6;             // polygon sides for bokeh
    float bokehRotation = 0.0f;     // rotation of bokeh shape
};

// Chroma Key configuration
struct ChromaKeyConfig {
    // Key color (in HSV for picker, converted to RGB for shader)
    float keyHue = 120.0f;          // 120 = green, 240 = blue
    float keySaturation = 1.0f;
    float keyValue = 1.0f;
    
    // Tolerance/threshold
    float similarity = 0.3f;        // How close to key color (0-1)
    float smoothness = 0.1f;        // Edge softness (0-1)
    
    // Spill suppression
    float spillReduction = 0.5f;    // Desaturate spill color
    bool advancedSpill = false;     // Use color difference method
    float spillThreshold = 0.5f;    // Threshold for spill detection
    
    // Edge refinement
    float edgeFeather = 0.0f;       // Feather edges
    float edgeExpand = 0.0f;        // Expand/contract matte
    float edgeBlur = 0.0f;          // Blur matte edges
    
    // Light wrap
    float lightWrap = 0.0f;         // Wrap background light onto foreground
    float lightWrapSize = 0.1f;
    
    // Color correction on result
    float foregroundGain = 1.0f;
    float foregroundGamma = 1.0f;
    float foregroundSaturation = 1.0f;
    
    // View mode
    enum class ViewMode { Composite, Matte, Foreground, Background, Spill };
    ViewMode viewMode = ViewMode::Composite;
    
    // Key method
    enum class KeyMethod { ColorDifference, HSV, Luminance };
    KeyMethod keyMethod = KeyMethod::ColorDifference;
};

// Minimal vector type used by MeshConfig (no GLM dependency)
struct vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    vec3() = default;
    vec3(float vx, float vy, float vz) : x(vx), y(vy), z(vz) {}
};

// 3D Mesh configuration
struct MeshConfig {
    std::string filePath;           // Path to GLTF/OBJ file
    std::string meshName;           // Specific mesh name (if multiple in file)
    
    // Material override
    struct MaterialOverride {
        vec3 baseColor = {1.0f, 1.0f, 1.0f};
        float metallic = 0.0f;
        float roughness = 0.5f;
        float emissive = 0.0f;
        float alpha = 1.0f;
        bool useVertexColors = false;
    };
    std::unordered_map<std::string, MaterialOverride> materialOverrides; // by material name
    
    // Animation
    bool playAnimation = true;
    float animationSpeed = 1.0f;
    int currentAnimation = 0;       // Animation index
    float animationTime = 0.0f;     // Manual time override
    
    // Render settings
    bool castShadows = true;
    bool receiveShadows = true;
    bool doubleSided = false;
    int renderLayer = 0;
    
    // LOD
    bool useLOD = false;
    float lodDistance = 100.0f;
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

    // Only meaningful for kind == MotionBlur, DirectionalBlur, TransformBlur.
    MotionBlurConfig motionBlur;

    // Only meaningful for kind == VelocityGraph, TimeRemap, OpticalFlow.
    VelocityConfig velocity;

    // Only meaningful for kind == BezierMask, Rotoscoping, RotoBrush, Tracker.
    MaskConfig mask;

    // Only meaningful for kind == ParticleEmitter, ParticleForces, ParticleRenderer.
    ParticleConfig particle;

    // Only meaningful for kind == ShapeRectangle, ShapeEllipse, ShapePolygon, ShapeStar, ShapePath, ShapeRender, ShapeMerge, ShapeTransform, ShapeStroke, ShapeFill, ShapeRepeater, ShapeBoolean.
    Shape2DConfig shape2D;

    // Only meaningful for kind == Transform3D.
    Transform3D transform3D;

    // Only meaningful for kind == Camera3D.
    Camera3D camera3D;

    // Only meaningful for kind == DepthOfField.
    DepthOfFieldConfig depthOfField;

    // Only meaningful for kind == ChromaKey.
    ChromaKeyConfig chromaKey;

    // Only meaningful for kind == MeshSource.
    MeshConfig mesh;

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

    [[nodiscard]] Transform3D EvaluateTransform3D(double timelineSeconds) const {
        Transform3D t = transform3D;
        t.positionX = EvaluateUniform("positionX", timelineSeconds);
        t.positionY = EvaluateUniform("positionY", timelineSeconds);
        t.positionZ = EvaluateUniform("positionZ", timelineSeconds);
        t.rotationX = EvaluateUniform("rotationX", timelineSeconds);
        t.rotationY = EvaluateUniform("rotationY", timelineSeconds);
        t.rotationZ = EvaluateUniform("rotationZ", timelineSeconds);
        t.scaleX = EvaluateUniform("scaleX", timelineSeconds);
        t.scaleY = EvaluateUniform("scaleY", timelineSeconds);
        t.scaleZ = EvaluateUniform("scaleZ", timelineSeconds);
        return t;
    }

    [[nodiscard]] Camera3D EvaluateCamera3D(double timelineSeconds) const {
        Camera3D c = camera3D;
        c.focalLength = EvaluateUniform("focalLength", timelineSeconds);
        c.aperture = EvaluateUniform("aperture", timelineSeconds);
        c.focusDistance = EvaluateUniform("focusDistance", timelineSeconds);
        c.transform = EvaluateTransform3D(timelineSeconds);
        return c;
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
