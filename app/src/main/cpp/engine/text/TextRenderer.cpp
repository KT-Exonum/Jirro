#include "TextRenderer.h"

#include <android/log.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>

#define LOG_TAG "TextRenderer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace vfx {

// ---------- FontFace ----------

FontFace::~FontFace() {
    if (hbFont_) hb_font_destroy(hbFont_);
    if (hbFace_) hb_face_destroy(hbFace_);
    if (face_) FT_Done_Face(face_);
}

bool FontFace::LoadFromFile(const std::string& path, float pixelSize) {
    FT_Library ftLib = nullptr;
    if (FT_Init_FreeType(&ftLib) != 0) {
        LOGE("FT_Init_FreeType failed");
        return false;
    }
    
    if (FT_New_Face(ftLib, path.c_str(), 0, &face_) != 0) {
        LOGE("FT_New_Face failed for %s", path.c_str());
        FT_Done_FreeType(ftLib);
        return false;
    }
    
    FT_Done_FreeType(ftLib);
    return InitializeFace(pixelSize);
}

bool FontFace::LoadFromMemory(const void* data, size_t size, float pixelSize) {
    FT_Library ftLib = nullptr;
    if (FT_Init_FreeType(&ftLib) != 0) return false;
    
    if (FT_New_Memory_Face(ftLib, static_cast<const FT_Byte*>(data), static_cast<FT_Long>(size), 0, &face_) != 0) {
        FT_Done_FreeType(ftLib);
        return false;
    }
    
    FT_Done_FreeType(ftLib);
    return InitializeFace(pixelSize);
}

bool FontFace::InitializeFace(float pixelSize) {
    if (!face_) return false;
    
    pixelSize_ = pixelSize;
    FT_Set_Pixel_Sizes(face_, 0, static_cast<FT_UInt>(std::ceil(pixelSize)));
    
    // Create HarfBuzz face and font
    hbFace_ = hb_ft_face_create(face_, nullptr);
    hbFont_ = hb_font_create(hbFace_);
    hb_ft_font_set_funcs(hbFont_);
    
    // Get family name
    familyName_ = face_->family_name ? face_->family_name : "Unknown";
    
    // Get metrics
    metrics_.ascent = face_->size->metrics.ascender / 64.0f;
    metrics_.descent = face_->size->metrics.descender / 64.0f;
    metrics_.lineGap = (face_->size->metrics.height - 
                        (face_->size->metrics.ascender - face_->size->metrics.descender)) / 64.0f;
    metrics_.emSize = face_->units_per_EM;
    metrics_.capHeight = face_->cap_height ? face_->cap_height / 64.0f : metrics_.ascent;
    metrics_.xHeight = face_->x_height ? face_->x_height / 64.0f : metrics_.ascent * 0.5f;
    
    return true;
}

// ---------- GlyphAtlas ----------

GlyphAtlas::GlyphAtlas(GraphicsDevice& device, uint32_t pageSize)
    : device_(device), pageSize_(pageSize) {
    pages_.emplace_back();
    pages_[0].width = pageSize;
    pages_[0].height = pageSize;
    pages_[0].pixels.resize(pageSize * pageSize);
}

GlyphAtlas::~GlyphAtlas() = default;

std::optional<std::pair<int, std::array<float, 4>>> GlyphAtlas::Allocate(
    uint32_t glyphWidth, uint32_t glyphHeight,
    const uint8_t* bitmap, uint32_t pitch) {
    
    if (glyphWidth == 0 || glyphHeight == 0) {
        return std::make_pair(0, std::array<float, 4>{0, 0, 0, 0});
    }
    
    for (int pageIdx = 0; pageIdx < static_cast<int>(pages_.size()); ++pageIdx) {
        auto& page = pages_[pageIdx];
        
        // Try to fit in current row
        if (page.nextX + glyphWidth <= page.width) {
            // Fits in current row
            uint32_t x = page.nextX;
            uint32_t y = page.nextY;
            
            // Copy bitmap to page
            for (uint32_t gy = 0; gy < glyphHeight; ++gy) {
                uint32_t srcY = (pitch > 0) ? gy : glyphHeight - 1 - gy;
                std::memcpy(&page.pixels[(y + gy) * page.width + x],
                           &bitmap[srcY * pitch], glyphWidth);
            }
            
            page.nextX += glyphWidth + 1; // 1px padding
            page.rowHeight = std::max(page.rowHeight, glyphHeight);
            
            float u0 = static_cast<float>(x) / page.width;
            float v0 = static_cast<float>(y) / page.height;
            float u1 = static_cast<float>(x + glyphWidth) / page.width;
            float v1 = static_cast<float>(y + glyphHeight) / page.height;
            
            return std::make_pair(pageIdx, std::array<float, 4>{u0, v0, u1, v1});
        }
        
        // Try new row
        if (page.nextY + page.rowHeight + glyphHeight <= page.height) {
            page.nextX = 0;
            page.nextY += page.rowHeight + 1;
            page.rowHeight = 0;
            
            // Retry allocation on new row
            return Allocate(glyphWidth, glyphHeight, bitmap, pitch);
        }
    }
    
    // Need new page
    pages_.emplace_back();
    auto& newPage = pages_.back();
    newPage.width = pageSize_;
    newPage.height = pageSize_;
    newPage.pixels.resize(pageSize_ * pageSize_);
    
    // Upload previous page to GPU if not already
    // (would be done in EnsureGlyph)
    
    // Retry on new page
    return Allocate(glyphWidth, glyphHeight, bitmap, pitch);
}

bool GlyphAtlas::EnsureGlyph(const GlyphInfo& glyph, const uint8_t* bitmap, uint32_t pitch) {
    if (glyph.atlasPage >= 0 && glyph.atlasPage < static_cast<int>(pages_.size())) {
        return true; // Already in atlas
    }
    
    auto result = Allocate(glyph.width, glyph.height, bitmap, pitch);
    if (!result) return false;
    
    // Note: In a real implementation, we'd upload the page texture here
    // and update glyph.atlasPage, u0, v0, u1, v1
    return true;
}

TextureHandle GlyphAtlas::GetPageTexture(int page) const {
    if (page >= 0 && page < static_cast<int>(pages_.size())) {
        return pages_[page].texture;
    }
    return TextureHandle{};
}

// ---------- TextShaper ----------

TextShaper::TextShaper() {
    buffer_ = hb_buffer_create();
}

TextShaper::~TextShaper() {
    if (buffer_) hb_buffer_destroy(buffer_);
}

TextRun TextShaper::Shape(const FontFace& font, const std::string& text,
                          float fontSize, hb_direction_t direction,
                          hb_script_t script, const char* language) {
    TextRun run;
    run.text = text;
    
    hb_buffer_clear_contents(buffer_);
    hb_buffer_add_utf8(buffer_, text.c_str(), static_cast<int>(text.size()), 0, static_cast<int>(text.size()));
    hb_buffer_set_direction(buffer_, direction);
    hb_buffer_set_script(buffer_, script);
    if (language) hb_buffer_set_language(buffer_, hb_language_from_string(language, -1));
    
    hb_shape(font.GetHarfBuzzFont(), buffer_, nullptr, 0);
    
    unsigned int glyphCount = 0;
    hb_glyph_info_t* glyphInfos = hb_buffer_get_glyph_infos(buffer_, &glyphCount);
    hb_glyph_position_t* glyphPositions = hb_buffer_get_glyph_positions(buffer_, &glyphCount);
    
    run.glyphs.reserve(glyphCount);
    float x = 0.0f, y = 0.0f;
    float maxY = 0.0f, minY = 0.0f;
    
    for (unsigned int i = 0; i < glyphCount; ++i) {
        ShapedGlyph sg;
        sg.info.codepoint = glyphInfos[i].codepoint;
        sg.info.glyphIndex = glyphInfos[i].codepoint; // Simplified
        sg.cluster = glyphInfos[i].cluster;
        
        sg.x = x + glyphPositions[i].x_offset / 64.0f;
        sg.y = y - glyphPositions[i].y_offset / 64.0f;
        
        x += glyphPositions[i].x_advance / 64.0f;
        y += glyphPositions[i].y_advance / 64.0f;
        
        // Get glyph metrics from FreeType
        FT_Face ftFace = const_cast<FontFace&>(*const_cast<FontFace*>(&const_cast<FontFace&>(font))).GetFreeTypeFace();
        if (FT_Load_Glyph(const_cast<FontFace&>(font).GetFreeTypeFace(), 
                          glyphInfos[i].codepoint, FT_LOAD_RENDER) == 0) {
            FT_GlyphSlot slot = const_cast<FontFace&>(font).GetFreeTypeFace()->glyph;
            sg.info.width = slot->bitmap.width;
            sg.info.height = slot->bitmap.rows;
            sg.info.bearingX = slot->bitmap_left;
            sg.info.bearingY = slot->bitmap_top;
            sg.info.advanceX = slot->advance.x / 64.0f;
            sg.info.advanceY = slot->advance.y / 64.0f;
        }
        
        run.glyphs.push_back(sg);
    }
    
    run.width = x;
    run.height = maxY - minY;
    run.baseline = 0.0f;
    
    return run;
}

TextRun TextShaper::ShapeWithFeatures(const FontFace& font, const std::string& text,
                                      float fontSize,
                                      const std::vector<std::pair<std::string, int>>& features) {
    // Would add hb_feature_t for each feature
    return Shape(font, text, fontSize);
}

// ---------- TextRenderer ----------

TextRenderer::TextRenderer(GraphicsDevice& device) : device_(device) {
    atlas_ = std::make_unique<GlyphAtlas>(device);
    shaper_ = std::make_unique<TextShaper>();
    
    FT_Init_FreeType(&ftLibrary_);
}

TextRenderer::~TextRenderer() {
    if (ftLibrary_) FT_Done_FreeType(ftLibrary_);
}

bool TextRenderer::Initialize(const std::string& defaultFontPath) {
    if (!defaultFontPath.empty()) {
        FontFace* font = LoadFont(defaultFontPath, 24.0f);
        if (!font) return false;
    }
    
    // Create text rendering pipeline
    // Would create vertex/fragment shaders for SDF text rendering
    
    return true;
}

FontFace* TextRenderer::LoadFont(const std::string& path, float pixelSize) {
    auto font = std::make_unique<FontFace>();
    if (!font->LoadFromFile(path, pixelSize)) return nullptr;
    
    FontKey key{font->GetFamilyName(), pixelSize, false, false};
    FontFace* raw = font.get();
    fontCache_[key] = std::move(font);
    return raw;
}

FontFace* TextRenderer::GetFont(const std::string& family, float size, bool bold, bool italic) {
    FontKey key{family, size, bold, italic};
    auto it = fontCache_.find(key);
    if (it != fontCache_.end()) return it->second.get();
    
    // Would find system font or load from file
    return nullptr;
}

TextRenderer::TextLayout TextRenderer::LayoutText(const std::string& text, const TextStyle& style,
                                                  float maxWidth, hb_direction_t direction) {
    TextLayout layout;
    
    // Get font
    FontFace* font = GetFont(style.fontFamily, style.fontSize, style.bold, style.italic);
    if (!font) return layout;
    
    // Shape text
    TextRun run = shaper_->Shape(*font, text, style.fontSize, direction);
    
    // Wrap if needed
    if (maxWidth > 0 && run.width > maxWidth) {
        // Would implement word wrapping here
    }
    
    layout.runs.push_back(run);
    layout.totalWidth = run.width;
    layout.totalHeight = run.height;
    
    // Generate vertices for SDF rendering
    // Each glyph = 2 triangles = 6 vertices (x, y, u, v, color)
    for (const auto& glyph : run.glyphs) {
        float x = glyph.x;
        float y = glyph.y;
        float w = glyph.info.width;
        float h = glyph.info.height;
        float u0 = glyph.info.u0, v0 = glyph.info.v0, u1 = glyph.info.u1, v1 = glyph.info.v1;
        
        // Triangle 1
        layout.vertices.push_back(x); layout.vertices.push_back(y); 
        layout.vertices.push_back(u0); layout.vertices.push_back(v0);
        layout.vertices.push_back(style.color & 0xFF); layout.vertices.push_back((style.color >> 8) & 0xFF);
        layout.vertices.push_back((style.color >> 16) & 0xFF); layout.vertices.push_back((style.color >> 24) & 0xFF);
        
        layout.vertices.push_back(x + w); layout.vertices.push_back(y);
        layout.vertices.push_back(u1); layout.vertices.push_back(v0);
        layout.vertices.push_back(style.color & 0xFF); layout.vertices.push_back((style.color >> 8) & 0xFF);
        layout.vertices.push_back((style.color >> 16) & 0xFF); layout.vertices.push_back((style.color >> 24) & 0xFF);
        
        layout.vertices.push_back(x); layout.vertices.push_back(y + h);
        layout.vertices.push_back(u0); layout.vertices.push_back(v1);
        layout.vertices.push_back(style.color & 0xFF); layout.vertices.push_back((style.color >> 8) & 0xFF);
        layout.vertices.push_back((style.color >> 16) & 0xFF); layout.vertices.push_back((style.color >> 24) & 0xFF);
        
        // Triangle 2
        layout.vertices.push_back(x + w); layout.vertices.push_back(y);
        layout.vertices.push_back(u1); layout.vertices.push_back(v0);
        layout.vertices.push_back(style.color & 0xFF); layout.vertices.push_back((style.color >> 8) & 0xFF);
        layout.vertices.push_back((style.color >> 16) & 0xFF); layout.vertices.push_back((style.color >> 24) & 0xFF);
        
        layout.vertices.push_back(x + w); layout.vertices.push_back(y + h);
        layout.vertices.push_back(u1); layout.vertices.push_back(v1);
        layout.vertices.push_back(style.color & 0xFF); layout.vertices.push_back((style.color >> 8) & 0xFF);
        layout.vertices.push_back((style.color >> 16) & 0xFF); layout.vertices.push_back((style.color >> 24) & 0xFF);
        
        layout.vertices.push_back(x); layout.vertices.push_back(y + h);
        layout.vertices.push_back(u0); layout.vertices.push_back(v1);
        layout.vertices.push_back(style.color & 0xFF); layout.vertices.push_back((style.color >> 8) & 0xFF);
        layout.vertices.push_back((style.color >> 16) & 0xFF); layout.vertices.push_back((style.color >> 24) & 0xFF);
        
        layout.indices.push_back(layout.vertices.size() / 8 - 6);
        layout.indices.push_back(layout.vertices.size() / 8 - 5);
        layout.indices.push_back(layout.vertices.size() / 8 - 4);
        layout.indices.push_back(layout.vertices.size() / 8 - 3);
        layout.indices.push_back(layout.vertices.size() / 8 - 2);
        layout.indices.push_back(layout.vertices.size() / 8 - 1);
    }
    
    layout.totalWidth = run.width;
    layout.totalHeight = run.height;
    
    return layout;
}

void TextRenderer::DrawText(const TextLayout& layout, float x, float y,
                            const std::array<float, 16>& projection) {
    // Would bind pipeline, upload vertex buffer, bind atlas textures, draw
    // vkCmdDrawIndexed for each atlas page
}

} // namespace vfx