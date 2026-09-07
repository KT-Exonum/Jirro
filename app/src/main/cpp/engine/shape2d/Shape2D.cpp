#include "Shape2D.h"

#include <android/log.h>
#include <algorithm>
#include <cmath>
#include <stack>
#include <queue>

#define LOG_TAG "Shape2D"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vfx {

// ---------- Vec2 helpers ----------
inline float Cross(const Vec2& a, const Vec2& b) { return a.x * b.y - a.y * b.x; }
inline float Dot(const Vec2& a, const Vec2& b) { return a.x * b.x + a.y * b.y; }

// ---------- BezierPath: SVG parsing ----------
BezierPath BezierPath::FromSVG(const std::string& svgPath) {
    BezierPath path;
    if (svgPath.empty()) return path;
    
    size_t i = 0;
    char lastCmd = 'M';
    Vec2 current{0,0}, start{0,0};
    
    auto skipSpaces = [&]() {
        while (i < svgPath.size() && (svgPath[i] == ' ' || svgPath[i] == '\t' || svgPath[i] == '\n' || svgPath[i] == ',' || svgPath[i] == '\r')) ++i;
    };
    
    auto parseNumber = [&]() -> float {
        skipSpaces();
        bool neg = false;
        if (i < svgPath.size() && svgPath[i] == '-') { neg = true; ++i; }
        else if (i < svgPath.size() && svgPath[i] == '+') ++i;
        
        float val = 0.0f;
        bool hasDigits = false;
        while (i < svgPath.size() && std::isdigit(svgPath[i])) {
            val = val * 10.0f + (svgPath[i] - '0');
            hasDigits = true;
            ++i;
        }
        if (i < svgPath.size() && svgPath[i] == '.') {
            ++i;
            float frac = 0.1f;
            while (i < svgPath.size() && std::isdigit(svgPath[i])) {
                val += (svgPath[i] - '0') * frac;
                frac *= 0.1f;
                ++i;
            }
        }
        if (i < svgPath.size() && (svgPath[i] == 'e' || svgPath[i] == 'E')) {
            ++i;
            bool expNeg = false;
            if (svgPath[i] == '-') { expNeg = true; ++i; }
            else if (svgPath[i] == '+') ++i;
            int exp = 0;
            while (i < svgPath.size() && std::isdigit(svgPath[i])) {
                exp = exp * 10 + (svgPath[i] - '0');
                ++i;
            }
            float mult = std::pow(10.0f, expNeg ? -exp : exp);
            val *= mult;
        }
        return neg ? -val : val;
    };
    
    while (i < svgPath.size()) {
        skipSpaces();
        if (i >= svgPath.size()) break;
        
        char cmd = svgPath[i];
        bool isRelative = std::islower(cmd);
        char upperCmd = std::toupper(cmd);
        ++i;
        
        PathSegment seg;
        
        switch (upperCmd) {
            case 'M': case 'L': {
                float x = parseNumber();
                float y = parseNumber();
                Vec2 pt = isRelative ? Vec2{current.x + x, current.y + y} : Vec2{x, y};
                seg.cmd = (upperCmd == 'M') ? PathCmd::MoveTo : PathCmd::LineTo;
                seg.points[0] = pt;
                current = pt;
                if (upperCmd == 'M') start = current;
                segments.push_back(seg);
                
                // Implicit lineto for remaining coords
                while (i < svgPath.size()) {
                    skipSpaces();
                    if (i >= svgPath.size() || std::isalpha(svgPath[i])) break;
                    x = parseNumber();
                    y = parseNumber();
                    pt = isRelative ? Vec2{current.x + x, current.y + y} : Vec2{x, y};
                    seg.cmd = PathCmd::LineTo;
                    seg.points[0] = pt;
                    current = pt;
                    segments.push_back(seg);
                }
                break;
            }
            case 'H': case 'V': {
                float val = parseNumber();
                Vec2 pt = current;
                if (upperCmd == 'H') pt.x = isRelative ? current.x + val : val;
                else pt.y = isRelative ? current.y + val : val;
                seg.cmd = PathCmd::LineTo;
                seg.points[0] = pt;
                current = pt;
                segments.push_back(seg);
                
                while (i < svgPath.size()) {
                    skipSpaces();
                    if (i >= svgPath.size() || std::isalpha(svgPath[i])) break;
                    val = parseNumber();
                    pt = current;
                    if (upperCmd == 'H') pt.x = isRelative ? current.x + val : val;
                    else pt.y = isRelative ? current.y + val : val;
                    seg.cmd = PathCmd::LineTo;
                    seg.points[0] = pt;
                    current = pt;
                    segments.push_back(seg);
                }
                break;
            }
            case 'Q': {
                float x1 = parseNumber();
                float y1 = parseNumber();
                float x2 = parseNumber();
                float y2 = parseNumber();
                Vec2 cp = isRelative ? Vec2{current.x + x1, current.y + y1} : Vec2{x1, y1};
                Vec2 end = isRelative ? Vec2{current.x + x2, current.y + y2} : Vec2{x2, y2};
                seg.cmd = PathCmd::QuadTo;
                seg.points[0] = cp;
                seg.points[1] = end;
                current = end;
                segments.push_back(seg);
                
                while (i < svgPath.size()) {
                    skipSpaces();
                    if (i >= svgPath.size() || std::isalpha(svgPath[i])) break;
                    x1 = parseNumber(); y1 = parseNumber();
                    x2 = parseNumber(); y2 = parseNumber();
                    cp = isRelative ? Vec2{current.x + x1, current.y + y1} : Vec2{x1, y1};
                    end = isRelative ? Vec2{current.x + x2, current.y + y2} : Vec2{x2, y2};
                    seg.cmd = PathCmd::QuadTo;
                    seg.points[0] = cp; seg.points[1] = end;
                    current = end;
                    segments.push_back(seg);
                }
                break;
            }
            case 'C': {
                float x1 = parseNumber(); float y1 = parseNumber();
                float x2 = parseNumber(); float y2 = parseNumber();
                float x3 = parseNumber(); float y3 = parseNumber();
                Vec2 cp1 = isRelative ? Vec2{current.x + x1, current.y + y1} : Vec2{x1, y1};
                Vec2 cp2 = isRelative ? Vec2{current.x + x2, current.y + y2} : Vec2{x2, y2};
                Vec2 end = isRelative ? Vec2{current.x + x3, current.y + y3} : Vec2{x3, y3};
                seg.cmd = PathCmd::CubicTo;
                seg.points[0] = cp1; seg.points[1] = cp2; seg.points[2] = end;
                current = end;
                segments.push_back(seg);
                
                while (i < svgPath.size()) {
                    skipSpaces();
                    if (i >= svgPath.size() || std::isalpha(svgPath[i])) break;
                    float x1=parseNumber(), y1=parseNumber();
                    float x2=parseNumber(), y2=parseNumber();
                    float x3=parseNumber(), y3=parseNumber();
                    Vec2 cp1 = isRelative ? Vec2{current.x + x1, current.y + y1} : Vec2{x1, y1};
                    Vec2 cp2 = isRelative ? Vec2{current.x + x2, current.y + y2} : Vec2{x2, y2};
                    Vec2 end = isRelative ? Vec2{current.x + x3, current.y + y3} : Vec2{x3, y3};
                    seg.cmd = PathCmd::CubicTo;
                    seg.points[0] = cp1; seg.points[1] = cp2; seg.points[2] = end;
                    current = end;
                    segments.push_back(seg);
                }
                break;
            }
            case 'S': case 'T': {
                // Smooth cubic/quad - simplified
                float x1 = parseNumber(); float y1 = parseNumber();
                float x2 = parseNumber(); float y2 = parseNumber();
                Vec2 cp = isRelative ? Vec2{current.x + x1, current.y + y1} : Vec2{x1, y1};
                Vec2 end = isRelative ? Vec2{current.x + x2, current.y + y2} : Vec2{x2, y2};
                seg.cmd = (upperCmd == 'S') ? PathCmd::CubicTo : PathCmd::QuadTo;
                if (upperCmd == 'S') {
                    seg.points[0] = cp; seg.points[1] = end; seg.points[2] = end;
                } else {
                    seg.points[0] = cp; seg.points[1] = end;
                }
                current = end;
                segments.push_back(seg);
                break;
            }
            case 'A': {
                // Arc - simplified to line
                float rx=parseNumber(), ry=parseNumber(), rot=parseNumber();
                int large=parseNumber(), sweep=parseNumber();
                float x2=parseNumber(), y2=parseNumber();
                Vec2 end = isRelative ? Vec2{current.x + x2, current.y + y2} : Vec2{x2, y2};
                seg.cmd = PathCmd::LineTo; // Simplified
                seg.points[0] = end;
                current = end;
                segments.push_back(seg);
                break;
            }
            case 'Z': case 'z': {
                seg.cmd = PathCmd::Close;
                segments.push_back(seg);
                current = start;
                break;
            }
            default:
                break;
        }
        
        if (upperCmd != 'M' && upperCmd != 'm') lastCmd = upperCmd;
    }
    
    return path;
}

// ---------- Flatten path to polyline ----------
std::vector<Vec2> BezierPath::Flatten(float tolerance) const {
    std::vector<Vec2> result;
    if (segments.empty()) return result;
    
    Vec2 current{0,0};
    bool first = true;
    
    for (const auto& seg : segments) {
        switch (seg.cmd) {
            case PathCmd::MoveTo:
                if (!first && !(current.x == result.back().x && current.y == result.back().y)) {
                    result.push_back(current);
                }
                current = seg.points[0];
                result.push_back(current);
                first = false;
                break;
            case PathCmd::LineTo:
                current = seg.points[0];
                result.push_back(current);
                break;
            case PathCmd::QuadTo: {
                Vec2 p0 = current;
                Vec2 p1 = seg.points[0];
                Vec2 p2 = seg.points[1];
                // Adaptive subdivision
                int steps = std::max(2, (int)std::ceil((p0 - p1).length() / tolerance + (p1 - seg.points[1]).length() / tolerance));
                for (int i = 1; i <= steps; ++i) {
                    float t = (float)i / steps;
                    float u = 1 - t;
                    Vec2 pt = p0 * u * u + p1 * 2 * u * t + seg.points[1] * t * t;
                    result.push_back(pt);
                }
                current = seg.points[1];
                break;
            }
            case PathCmd::CubicTo: {
                Vec2 p0 = current;
                Vec2 p1 = seg.points[0];
                Vec2 p2 = seg.points[1];
                Vec2 p3 = seg.points[1];
                int steps = std::max(4, (int)std::ceil((p0 - p1).length() / tolerance + (p1 - p2).length() / tolerance + (p2 - p3).length() / tolerance));
                for (int i = 1; i <= steps; ++i) {
                    float t = (float)i / steps;
                    float u = 1 - t;
                    float uu = u * u, uuu = uu * u;
                    float tt = t * t, ttt = tt * t;
                    Vec2 pt = p0 * uuu + p1 * 3 * uu * t + p2 * 3 * u * tt + seg.points[1] * ttt;
                    result.push_back(pt);
                }
                current = seg.points[1];
                break;
            }
            case PathCmd::Close:
                // Connect to first point
                if (!result.empty() && !(current.x == result.front().x && current.y == result.front().y)) {
                    result.push_back(result.front());
                }
                break;
        }
    }
    return result;
}

// ---------- Bounds ----------
std::pair<Vec2, Vec2> BezierPath::Bounds() const {
    if (segments.empty()) return {{0,0}, {0,0}};
    float minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
    Vec2 current{0,0};
    
    for (const auto& seg : segments) {
        switch (seg.cmd) {
            case PathCmd::MoveTo:
            case PathCmd::LineTo:
                minX = std::min(minX, seg.points[0].x);
                minY = std::min(minY, seg.points[0].y);
                maxX = std::max(maxX, seg.points[0].x);
                maxY = std::max(maxY, seg.points[0].y);
                current = seg.points[0];
                break;
            case PathCmd::QuadTo: {
                // Include control point in bounds
                minX = std::min({minX, current.x, seg.points[0].x, seg.points[1].x});
                minY = std::min({minY, current.y, seg.points[0].y, seg.points[1].y});
                maxX = std::max({maxX, current.x, seg.points[0].x, seg.points[1].x});
                maxY = std::max({maxY, current.y, seg.points[0].y, seg.points[1].y});
                // Add extremas
                Vec2 d0 = seg.points[0] - current;
                Vec2 d1 = seg.points[1] - seg.points[0] * 2 + current;
                for (int dim = 0; dim < 2; ++dim) {
                    float a = d1[dim] * 2;
                    float b = d0[dim] * 2;
                    if (a != 0) {
                        float t = -b / (2 * a);
                        if (t > 0 && t < 1) {
                            float val = current[dim] * (1-t)*(1-t) + seg.points[0][dim] * 2*t*(1-t) + seg.points[1][dim] * t*t;
                            if (dim == 0) { minX = std::min(minX, val); maxX = std::max(maxX, val); }
                            else { minY = std::min(minY, val); maxY = std::max(maxY, val); }
                        }
                    }
                }
                current = seg.points[1];
                break;
            }
            case PathCmd::CubicTo: {
                minX = std::min({minX, current.x, seg.points[0].x, seg.points[1].x, seg.points[1].x});
                minY = std::min({minY, current.y, seg.points[0].y, seg.points[1].y, seg.points[1].y});
                maxX = std::max({maxX, current.x, seg.points[0].x, seg.points[1].x, seg.points[1].x});
                maxY = std::max({maxY, current.y, seg.points[0].y, seg.points[1].y, seg.points[1].y});
                current = seg.points[1];
                break;
            }
            case PathCmd::Close: break;
        }
    }
    return {{minX, minY}, {maxX, maxY}};
}

// ---------- Transform ----------
void BezierPath::Transform(const std::array<float, 9>& m) {
    for (auto& seg : segments) {
        for (auto& pt : seg.points) {
            float x = pt.x, y = pt.y;
            pt.x = m[0] * x + m[3] * y + m[6];
            pt.y = m[1] * x + m[4] * y + m[7];
        }
    }
}

// ---------- Boolean ops (placeholder - would use clipper/boost geometry) ----------
BezierPath BezierPath::Union(const BezierPath& a, const BezierPath& b) {
    // Would use Clipper library or custom polygon boolean
    return a; // Placeholder
}
BezierPath BezierPath::Intersect(const BezierPath& a, const BezierPath& b) { return a; }
BezierPath BezierPath::Difference(const BezierPath& a, const BezierPath& b) { return a; }
BezierPath BezierPath::Xor(const BezierPath& a, const BezierPath& b) { return a; }

// ---------- Offset ----------
BezierPath BezierPath::Offset(float distance, bool closed) const {
    // Offset curve - simplified
    return *this;
}

void BezierPath::Simplify(float tolerance) {
    // Ramer-Douglas-Peucker on flattened
}

void BezierPath::Reverse() {
    std::reverse(segments.begin(), segments.end());
    for (auto& seg : segments) {
        if (seg.cmd == PathCmd::MoveTo || seg.cmd == PathCmd::LineTo) {
            // Single point - nothing to reverse
        } else if (seg.cmd == PathCmd::QuadTo) {
            std::swap(seg.points[0], seg.points[1]);
        } else if (seg.cmd == PathCmd::CubicTo) {
            std::swap(seg.points[0], seg.points[2]);
        }
    }
    // Reverse order and flip commands
    std::vector<PathSegment> reversed;
    Vec2 last = segments.back().points[seg.cmd == PathCmd::MoveTo ? 0 : (seg.cmd == PathCmd::QuadTo ? 1 : 1)];
    for (auto it = segments.rbegin(); it != segments.rend(); ++it) {
        // Complex - placeholder
    }
}

// ---------- Split ----------
std::pair<BezierPath, BezierPath> BezierPath::Split(float t) const {
    return {*this, *this};
}

// ---------- ShapeGroup::Flatten ----------
std::vector<ShapeLayer> ShapeGroup::Flatten() const {
    std::vector<ShapeLayer> result;
    
    for (const auto& layer : layers) {
        ShapeLayer l = layer;
        // Apply group transform
        // Apply trims
        // Apply repeaters
        result.push_back(l);
    }
    
    return result;
}

// ---------- Shape2DRenderer ----------
Shape2DRenderer::Shape2DRenderer(GraphicsDevice& device) : device_(device) {
    buffers_ = std::make_unique<ShapeBuffers>();
}

Shape2DRenderer::~Shape2DRenderer() {
    if (buffers_->vertexBuffer) device_.DestroyBuffer(buffers_->vertexBuffer);
    if (buffers_->indexBuffer) device_.DestroyBuffer(buffers_->indexBuffer);
    if (buffers_->uniformBuffer) device_.DestroyBuffer(buffers_->uniformBuffer);
    if (buffers_->pipeline) device_.DestroyPipeline(buffers_->pipeline);
}

bool Shape2DRenderer::BuildBuffers(const Shape2DConfig& config, float time) {
    Tessellate(config, time);
    
    if (buffers_->vertices.empty()) return false;
    
    // Create vertex buffer
    size_t vertexSize = buffers_->vertices.size() * sizeof(ShapeBuffers::Vertex);
    auto vbResult = device_.CreateBuffer(vertexSize, false, BufferUsage::Vertex);
    if (!vbResult) return false;
    buffers_->vertexBuffer = vbResult.value;
    
    // Upload vertex data
    auto vbRes = device_.GetBuffer(buffers_->vertexBuffer);
    if (vbRes && vbRes->mapped) {
        std::memcpy(vbRes->mapped, buffers_->vertices.data(), vertexSize);
    }
    
    // Create index buffer
    size_t indexSize = buffers_->indices.size() * sizeof(uint32_t);
    auto ibResult = device_.CreateBuffer(indexSize, false, BufferUsage::Index);
    if (!ibResult) return false;
    buffers_->indexBuffer = ibResult.value;
    
    auto ibRes = device_.GetBuffer(buffers_->indexBuffer);
    if (ibRes && ibRes->mapped) {
        std::memcpy(ibRes->mapped, buffers_->indices.data(), indexSize);
    }
    
    // Create uniform buffer
    auto ubResult = device_.CreateBuffer(sizeof(ShapeBuffers::Uniforms), true, BufferUsage::Uniform);
    if (!ubResult) return false;
    buffers_->uniformBuffer = ubResult.value;
    
    // Create pipeline (would compile shaders)
    // buffers_->pipeline = device_.CreateGraphicsPipeline(...);
    
    return true;
}

void Shape2DRenderer::Tessellate(const Shape2DConfig& config, float time) {
    buffers_->vertices.clear();
    buffers_->indices.clear();
    
    int vertexOffset = 0;
    int layerId = 0;
    
    for (const auto& group : config.groups) {
        // Flatten groups with repeaters/trims
        std::vector<ShapeLayer> flatLayers;
        FlattenGroups(group, time, flatLayers);
        
        for (const auto& layer : flatLayers) {
            if (!layer.visible) continue;
            
            TriangulatePath(layer, time, buffers_->vertices, buffers_->indices, vertexOffset, layerId);
            vertexOffset = buffers_->vertices.size();
            ++layerId;
        }
    }
    
    // Update uniform buffer
    buffers_->uniforms.transform = {1,0,0, 0,1,0, 0,0,1};
    buffers_->uniforms.resolution = {config.width, config.height};
    buffers_->uniforms.time = time;
    buffers_->uniforms.layerCount = layerId;
}

void Shape2DRenderer::FlattenGroups(const ShapeGroup& group, float time, std::vector<ShapeLayer>& output) const {
    for (const auto& layer : group.layers) {
        ShapeLayer l = layer;
        
        // Apply group transform
        l = ApplyTransform(l, group);
        
        // Apply trims
        for (const auto& trim : group.trims) {
            l = ApplyTrim(l, trim);
        }
        
        // Apply repeaters
        std::vector<ShapeLayer> repeated = {l};
        for (const auto& repeater : group.repeaters) {
            std::vector<ShapeLayer> newRepeated;
            for (const auto& rl : repeated) {
                auto r = ApplyRepeater(rl, repeater, time);
                newRepeated.insert(newRepeated.end(), r.begin(), r.end());
            }
            repeated = std::move(newRepeated);
        }
        
        for (auto& rl : repeated) {
            output.push_back(rl);
        }
    }
    
    // Recurse into nested groups (if any)
}

ShapeLayer Shape2DRenderer::ApplyTransform(const ShapeLayer& layer, const ShapeGroup& group) const {
    ShapeLayer result = layer;
    
    // Build transform matrix
    float c = std::cos(group.rotation), s = std::sin(group.rotation);
    float sx = group.scale.x, sy = group.scale.y;
    float ax = group.anchor.x, ay = group.anchor.y;
    
    // Translate to anchor, rotate/scale/skew, translate back, then position
    // Simplified - would build full 3x3 matrix
    
    result.position = layer.position + group.position;
    result.rotation = layer.rotation + group.rotation;
    result.scale = {layer.scale.x * group.scale.x, layer.scale.y * group.scale.y};
    result.opacity = layer.opacity * group.opacity;
    
    return result;
}

ShapeLayer Shape2DRenderer::ApplyTrim(const ShapeLayer& layer, const TrimPaths& trim) const {
    // Would modify path segments based on start/end/offset
    // For now, return as-is
    return layer;
}

std::vector<ShapeLayer> Shape2DRenderer::ApplyRepeater(const ShapeLayer& layer, const Repeater& repeater, float time) const {
    std::vector<ShapeLayer> result;
    
    for (int i = 0; i < repeater.copies; ++i) {
        float t = repeater.copies > 1 ? (float)i / (repeater.copies - 1) : 0.5f;
        ShapeLayer copy = layer;
        
        // Interpolate transform
        float rot = repeater.rotation + t * repeater.rotationOffset;
        Vec2 pos = {t * repeater.position.x, t * repeater.position.y};
        Vec2 scale = {1 + t * (repeater.scale.x - 1), 1 + t * (repeater.scale.y - 1)};
        float opacity = repeater.startOpacity + t * (repeater.endOpacity - repeater.startOpacity);
        
        copy.rotation += rot;
        copy.position = copy.position + pos;
        copy.scale = {copy.scale.x * scale.x, copy.scale.y * scale.y};
        copy.opacity *= opacity;
        
        result.push_back(copy);
    }
    
    return result;
}

ShapeLayer Shape2DRenderer::ApplyRepeater(const ShapeLayer& layer, const Repeater& repeater) const {
    return ApplyRepeater(layer, repeater, 0.0f)[0]; // Simplified
}

// ---------- Triangulation ----------
void Shape2DRenderer::TriangulatePath(const ShapeLayer& layer, float time,
                                       std::vector<ShapeBuffers::Vertex>& vertices,
                                       std::vector<uint32_t>& indices, int baseVertex, int layerId) {
    // Flatten path to polyline
    auto polyline = layer.path.Flatten(0.25f);
    if (polyline.size() < 3) return;
    
    // Simple triangulation for convex polygons (ear clipping)
    // For concave/complex paths, would use proper triangulation library
    
    // For now, create a simple fan from center
    Vec2 center{0,0};
    for (const auto& pt : polyline) center = center + pt;
    center = center / (float)polyline.size();
    
    int cv = vertices.size();
    for (const auto& pt : polyline) {
        ShapeBuffers::Vertex v;
        v.pos = pt;
        v.uv = {(pt.x + 1) * 0.5f, (pt.y + 1) * 0.5f};
        v.color = layer.hasFill ? layer.fillColor : Vec4{0,0,0,0};
        v.layerId = layerId;
        v.pathId = 0;
        vertices.push_back(v);
    }
    
    // Fan triangulation
    for (size_t i = 1; i + 1 < polyline.size(); ++i) {
        indices.push_back(cv);
        indices.push_back(cv + i);
        indices.push_back(cv + i + 1);
    }
    
    // Stroke would add additional vertices around the path
}

void Shape2DRenderer::Render(TextureHandle output, const std::array<float, 16>& projection, float time) {
    // Bind pipeline, vertex/index buffers, uniform buffer
    // vkCmdDrawIndexed
    UpdateUniforms(time, {1,0,0, 0,1,0, 0,0,1});
}

void Shape2DRenderer::UpdateUniforms(float time, const std::array<float, 9>& transform) {
    if (!buffers_->uniformBuffer) return;
    
    buffers_->uniforms.transform = transform;
    buffers_->uniforms.time = time;
    
    auto ubRes = device_.GetBuffer(buffers_->uniformBuffer);
    if (ubRes && ubRes->mapped) {
        std::memcpy(ubRes->mapped, &buffers_->uniforms, sizeof(ShapeBuffers::Uniforms));
    }
}

std::pair<Vec2, Vec2> Shape2DRenderer::GetBounds() const {
    if (buffers_->vertices.empty()) return {{0,0}, {0,0}};
    
    float minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
    for (const auto& v : buffers_->vertices) {
        minX = std::min(minX, v.pos.x);
        minY = std::min(minY, v.pos.y);
        maxX = std::max(maxX, v.pos.x);
        maxY = std::max(maxY, v.pos.y);
    }
    return {{minX, minY}, {maxX, maxY}};
}

} // namespace vfx