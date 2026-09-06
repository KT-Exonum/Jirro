#pragma once
// Text rendering with HarfBuzz shaping + GPU glyph atlas
// Renders vector text via signed-distance-field (SDF) glyph atlas

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <optional>

#include <hb.h>
#include <hb-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "engine/core/GraphicsDevice.h"

namespace vfx {

struct GlyphInfo {
    uint32_t codepoint = 0;
    int glyphIndex = 0;
    float advanceX = 0.0f;
    float advanceY = 0.0f;
    float bearingX = 0.0f;
    float bearingY = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    
    // Atlas position (UV coordinates)
    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
    int atlasPage = 0;
};

struct FontMetrics {
    float ascent = 0.0f;
    float descent = 0.0f;
    float lineGap = 0.0f;
    float emSize = 0.0f;
    float capHeight = 0.0f;
    float xHeight = 0.0f;
};

struct ShapedGlyph {
    GlyphInfo info;
    float x = 0.0f;  // position in layout (pixels)
    float y = 0.0f;
    uint32_t cluster = 0;  // original character cluster index
};

struct TextRun {
    std::string text;
    std::vector<ShapedGlyph> glyphs;
    float width = 0.0f;
    float height = 0.0f;
    float baseline = 0.0f;
};

class FontFace {
public:
    FontFace() = default;
    ~FontFace();

    bool LoadFromFile(const std::string& path, float pixelSize);
    bool LoadFromMemory(const void* data, size_t size, float pixelSize);

    [[nodiscard]] bool IsValid() const { return face_ != nullptr; }
    [[nodiscard]] FontMetrics GetMetrics() const { return metrics_; }
    [[nodiscard]] hb_font_t* GetHarfBuzzFont() const { return hbFont_; }
    [[nodiscard]] FT_Face GetFreeTypeFace() const { return face_; }
    [[nodiscard]] float GetPixelSize() const { return pixelSize_; }
    [[nodiscard]] const std::string& GetFamilyName() const { return familyName_; }

private:
    FT_Face face_ = nullptr;
    hb_font_t* hbFont_ = nullptr;
    hb_face_t* hbFace_ = nullptr;
    FontMetrics metrics_;
    float pixelSize_ = 0.0f;
    std::string familyName_;
};

class GlyphAtlas {
public:
    struct Page {
        TextureHandle texture;
        uint32_t width = 0, height = 0;
        uint32_t nextX = 0, nextY = 0, rowHeight = 0;
        std::vector<uint8_t> pixels; // CPU-side for upload
    };

    explicit GlyphAtlas(GraphicsDevice& device, uint32_t pageSize = 1024);
    ~GlyphAtlas();

    // Allocate space for a glyph, returns atlas page and UV coordinates
    std::optional<std::pair<int, std::array<float, 4>>> Allocate(
        uint32_t glyphWidth, uint32_t glyphHeight,
        const uint8_t* bitmap, uint32_t pitch);

    // Ensure glyph is in atlas, upload if needed
    bool EnsureGlyph(const GlyphInfo& glyph, const uint8_t* bitmap, uint32_t pitch);

    TextureHandle GetPageTexture(int page) const;
    int GetPageCount() const { return static_cast<int>(pages_.size()); }

private:
    GraphicsDevice& device_;
    uint32_t pageSize_;
    std::vector<Page> pages_;
};

class TextShaper {
public:
    TextShaper() = default;

    // Shape a text run with HarfBuzz
    TextRun Shape(const FontFace& font, const std::string& text,
                  float fontSize, hb_direction_t direction = HB_DIRECTION_LTR,
                  hb_script_t script = HB_SCRIPT_LATIN,
                  const char* language = nullptr);

    // Shape with font features (ligatures, kerning, etc.)
    TextRun ShapeWithFeatures(const FontFace& font, const std::string& text,
                              float fontSize,
                              const std::vector<std::pair<std::string, int>>& features);

private:
    hb_buffer_t* buffer_ = nullptr;
};

class TextRenderer {
public:
    struct TextStyle {
        std::string fontFamily = "Roboto";
        float fontSize = 24.0f;
        float lineHeight = 1.2f;
        float letterSpacing = 0.0f;
        float wordSpacing = 0.0f;
        uint32_t color = 0xFFFFFFFF;
        float strokeWidth = 0.0f;
        uint32_t strokeColor = 0xFF000000;
        bool bold = false;
        bool italic = false;
        bool underline = false;
        bool strikethrough = false;
    };

    explicit TextRenderer(GraphicsDevice& device);
    ~TextRenderer();

    // Initialize with default font
    bool Initialize(const std::string& defaultFontPath = "");

    // Load a font file
    FontFace* LoadFont(const std::string& path, float pixelSize);

    // Get or load font
    FontFace* GetFont(const std::string& family, float size, bool bold, bool italic);

    // Layout and render text to a vertex buffer
    struct TextLayout {
        std::vector<TextRun> runs;
        float totalWidth = 0.0f;
        float totalHeight = 0.0f;
        std::vector<float> vertices;  // x, y, u, v, color (packed)
        std::vector<uint32_t> indices;
    };

    TextLayout LayoutText(const std::string& text, const TextStyle& style,
                          float maxWidth = 0.0f,  // 0 = no wrapping
                          hb_direction_t direction = HB_DIRECTION_LTR);

    // Draw text layout (call from render pass)
    void DrawText(const TextLayout& layout, float x, float y,
                  const std::array<float, 16>& projection);

private:
    GraphicsDevice& device_;
    std::unique_ptr<GlyphAtlas> atlas_;
    std::unique_ptr<TextShaper> shaper_;
    
    struct FontKey {
        std::string family;
        float size;
        bool bold;
        bool italic;
        bool operator==(const FontKey& other) const {
            return family == other.family && size == other.size && 
                   bold == other.bold && italic == other.italic;
        }
    };
    struct FontKeyHash {
        size_t operator()(const FontKey& k) const {
            size_t h = std::hash<std::string>{}(k.family);
            h ^= std::hash<float>{}(k.size) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<bool>{}(k.bold) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<bool>{}(k.italic) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
    std::unordered_map<FontKey, std::unique_ptr<FontFace>, FontKeyHash> fontCache_;
    
    FT_Library ftLibrary_ = nullptr;
    PipelineHandle textPipeline_ = 0;
};

} // namespace vfx