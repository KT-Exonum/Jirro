#pragma once
// Shape2D vector system: path representation, boolean ops, repeaters, strokes, fills
// GPU-accelerated via compute shaders for tessellation and rendering

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <array>

#include "engine/core/GraphicsDevice.h"

namespace vfx {

// 2D vector math
struct Vec2 {
    float x = 0.0f, y = 0.0f;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2 operator/(float s) const { return {x / s, y / s}; }
    float dot(const Vec2& o) const { return x * o.x + y * o.y; }
    float cross(const Vec2& o) const { return x * o.y - y * o.x; }
    float length() const { return std::sqrt(x*x + y*y); }
    Vec2 normalize() const { float l = length(); return l > 0 ? *this / l : Vec2{0,0}; }
    Vec2 perp() const { return {-y, x}; }
};

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
};

// Path commands
enum class PathCmd {
    MoveTo,
    LineTo,
    QuadTo,
    CubicTo,
    Close,
    ArcTo,
};

// Path segment
struct PathSegment {
    PathCmd cmd = PathCmd::MoveTo;
    std::array<Vec2, 3> points; // Up to 3 control points
};

// Bezier path (sequence of segments)
class BezierPath {
public:
    std::vector<PathSegment> segments;
    
    // Build from SVG path string
    static BezierPath FromSVG(const std::string& svgPath);
    
    // Convert to polyline for rendering (adaptive subdivision)
    std::vector<Vec2> Flatten(float tolerance = 0.25f) const;
    
    // Get bounding box
    std::pair<Vec2, Vec2> Bounds() const;
    
    // Transform
    void Transform(const std::array<float, 9>& matrix);
    
    // Boolean operations
    static BezierPath Union(const BezierPath& a, const BezierPath& b);
    static BezierPath Intersect(const BezierPath& a, const BezierPath& b);
    static BezierPath Difference(const BezierPath& a, const BezierPath& b);
    static BezierPath Xor(const BezierPath& a, const BezierPath& b);
    
    // Offset path (stroke outline)
    BezierPath Offset(float distance, bool closed = true) const;
    
    // Simplify (reduce points)
    void Simplify(float tolerance);
    
    // Reverse winding
    void Reverse();
    
    // Split at parameter t
    std::pair<BezierPath, BezierPath> Split(float t) const;
};

// Shape fill styles
enum class FillRule {
    NonZero,
    EvenOdd,
};

enum class FillType {
    Solid,
    LinearGradient,
    RadialGradient,
    AngularGradient,
    Image,
};

struct GradientStop {
    float offset = 0.0f;
    Vec4 color{1,1,1,1};
};

struct Gradient {
    FillType type = FillType::LinearGradient;
    std::vector<GradientStop> stops;
    Vec2 start{0,0}, end{1,1};
    float radius = 1.0f;
    float angle = 0.0f;
    bool spreadRepeat = false;
};

// Stroke style
struct StrokeStyle {
    float width = 1.0f;
    enum class Cap { Butt, Round, Square } cap = Cap::Butt;
    enum class Join { Miter, Round, Bevel } join = Join::Miter;
    float miterLimit = 4.0f;
    std::vector<float> dashPattern; // on, off, on, off...
    float dashOffset = 0.0f;
    FillType fillType = FillType::Solid;
    Vec4 color{1,1,1,1};
    Gradient gradient;
};

// Shape layer (vector shape with style)
struct ShapeLayer {
    std::string name;
    BezierPath path;
    bool visible = true;
    float opacity = 1.0f;
    
    // Fill
    bool hasFill = true;
    FillType fillType = FillType::Solid;
    Vec4 fillColor{1,1,1,1};
    FillRule fillRule = FillRule::NonZero;
    Gradient fillGradient;
    
    // Stroke
    bool hasStroke = false;
    StrokeStyle stroke;
    
    // Transform
    Vec2 anchor{0.5f, 0.5f};
    Vec2 position{0,0};
    float rotation = 0.0f;
    Vec2 scale{1,1};
    Vec2 skew{0,0};
    
    // Blend mode
    enum class BlendMode { Normal, Multiply, Screen, Overlay, Add, Subtract } blendMode = BlendMode::Normal;
    
    // Effects
    float blurRadius = 0.0f;
    float feather = 0.0f;
};

// Repeater - duplicates shape with transform offsets
struct Repeater {
    int copies = 3;
    Vec2 position{0,0};
    float rotation = 0.0f;
    Vec2 scale{1,1};
    Vec2 anchor{0.5f, 0.5f};
    float startOpacity = 1.0f;
    float endOpacity = 1.0f;
    float startScale = 1.0f;
    float endScale = 1.0f;
    float rotationOffset = 0.0f;
    bool compositeEach = false; // If true, each copy composites separately
};

// Trim paths (animate path drawing)
struct TrimPaths {
    float start = 0.0f;    // 0-1
    float end = 1.0f;      // 0-1
    float offset = 0.0f;   // 0-1
    bool simultaneously = true;
};

// Shape group (contains layers, repeaters, trims)
struct ShapeGroup {
    std::string name;
    std::vector<ShapeLayer> layers;
    std::vector<Repeater> repeaters;
    std::vector<TrimPaths> trims;
    
    // Group transform
    Vec2 anchor{0.5f, 0.5f};
    Vec2 position{0,0};
    float rotation = 0.0f;
    Vec2 scale{1,1};
    Vec2 skew{0,0};
    float opacity = 1.0f;
    enum class BlendMode { Normal, Multiply, Screen, Overlay, Add } blendMode = BlendMode::Normal;
    
    // Flatten all layers with repeaters/trims applied
    std::vector<ShapeLayer> Flatten() const;
};

// Shape2D node configuration
struct Shape2DConfig {
    std::vector<ShapeGroup> groups;
    uint32_t width = 1920;
    uint32_t height = 1080;
    Vec4 backgroundColor{0,0,0,0};
    
    // Viewport
    Vec2 viewCenter{0,0};
    float viewZoom = 1.0f;
};

// GPU buffers for shape rendering
struct ShapeBuffers {
    // Vertex buffer: position, uv, color, layer_id
    struct Vertex {
        Vec2 pos;
        Vec2 uv;
        Vec4 color;
        int layerId;
        int pathId;
    };
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    // Uniform buffer
    struct Uniforms {
        std::array<float, 9> transform;
        Vec2 resolution;
        float time;
        int layerCount;
    } uniforms;
    
    // GPU handles
    BufferHandle vertexBuffer;
    BufferHandle indexBuffer;
    BufferHandle uniformBuffer;
    PipelineHandle pipeline;
};

// Shape2D renderer (tessellates and renders vector shapes)
class Shape2DRenderer {
public:
    explicit Shape2DRenderer(GraphicsDevice& device);
    ~Shape2DRenderer();
    
    // Build GPU buffers from shape config
    bool BuildBuffers(const Shape2DConfig& config, float time);
    
    // Render to target texture
    void Render(TextureHandle output, const std::array<float, 16>& projection, float time);
    
    // Update uniform buffer for frame
    void UpdateUniforms(float time, const std::array<float, 9>& transform);
    
    // Get current bounds
    std::pair<Vec2, Vec2> GetBounds() const;

private:
    GraphicsDevice& device_;
    std::unique_ptr<ShapeBuffers> buffers_;
    
    // Tessellate shape layers to triangles
    void Tessellate(const Shape2DConfig& config, float time);
    
    // Triangulate a path (ear clipping for polygons, curve tessellation for curves)
    void TriangulatePath(const ShapeLayer& layer, float time, 
                         std::vector<ShapeBuffers::Vertex>& vertices,
                         std::vector<uint32_t>& indices, int baseVertex, int layerId);
    
    // Apply repeater to layer
    std::vector<ShapeLayer> ApplyRepeater(const ShapeLayer& layer, const Repeater& repeater, float time) const;
    
    // Apply trim paths to layer
    ShapeLayer ApplyTrim(const ShapeLayer& layer, const TrimPaths& trim) const;
    
    // Apply group transform
    ShapeLayer ApplyTransform(const ShapeLayer& layer, const ShapeGroup& group) const;
    
    // Flatten groups recursively
    void FlattenGroups(const ShapeGroup& group, float time, std::vector<ShapeLayer>& output) const;
};

} // namespace vfx