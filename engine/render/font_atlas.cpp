// engine/render/font_atlas.cpp — запекание встроенного шрифта в RGBA-атлас (FR-E29).
// Штриховые контуры (font_table.inc) растеризуются в ячейки атласа; атлас затем
// загружается рендер-бэкендом как обычная текстура. Внешние .ttf не требуются,
// поэтому текст работает и в headless-сборке (E-D11). Переход на stb_truetype — V1.
#include "render/font_atlas.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "render/font_table.inc"

namespace engine::render {

namespace {

// Геометрия ячейки: дизайн-сетка X 0..6, Y -3..10 → ячейка 16×32 px.
constexpr float kCellW = static_cast<float>(FontAtlas::kCellWidth);
constexpr float kCellH = static_cast<float>(FontAtlas::kCellHeight);
constexpr float kScaleX = kCellW / kDesignWidth;                 // 16 / 6
constexpr float kScaleY = kCellH / (kDesignMaxY - kDesignMinY);  // 32 / 13

// Толщина штриха в пикселях ячейки (подобрана под кегль 13–20 px)
constexpr float kStrokeRadius = 0.72f;
// Суперсэмплинг 2×2 для сглаживания краёв
constexpr int kSamples = 2;

float design_to_px_x(float x) { return x * kScaleX; }
float design_to_px_y(float y) { return (y - kDesignMinY) * kScaleY; }

// Расстояние от точки до отрезка
float segment_distance(float px, float py, float x0, float y0, float x1, float y1) {
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float len2 = dx * dx + dy * dy;
    float t = 0.f;
    if (len2 > 1e-6f) {
        t = ((px - x0) * dx + (py - y0) * dy) / len2;
        t = std::clamp(t, 0.f, 1.f);
    }
    const float cx = x0 + dx * t;
    const float cy = y0 + dy * t;
    return std::sqrt((px - cx) * (px - cx) + (py - cy) * (py - cy));
}

// Покрытие пикселя штрихом: 0..1 (суперсэмплинг)
float coverage(float px, float py, const GlyphDef& def) {
    if (def.segments == 0) return 0.f;
    int covered = 0;
    const float step = 1.f / static_cast<float>(kSamples);
    for (int sy = 0; sy < kSamples; ++sy) {
        for (int sx = 0; sx < kSamples; ++sx) {
            const float x = px + (static_cast<float>(sx) + 0.5f) * step;
            const float y = py + (static_cast<float>(sy) + 0.5f) * step;
            float min_dist = 1e9f;
            for (unsigned char s = 0; s < def.segments; ++s) {
                const float* p = def.pts + static_cast<std::size_t>(s) * 4;
                const float d = segment_distance(x, y, design_to_px_x(p[0]), design_to_px_y(p[1]),
                                                 design_to_px_x(p[2]), design_to_px_y(p[3]));
                min_dist = std::min(min_dist, d);
            }
            if (min_dist <= kStrokeRadius) ++covered;
        }
    }
    return static_cast<float>(covered) / static_cast<float>(kSamples * kSamples);
}

} // namespace

bool FontAtlas::bake() {
    if (ready_) return true;

    const int rows = static_cast<int>((kGlyphDefCount + kColumns - 1) / kColumns);
    width_ = kColumns * kCellWidth;
    height_ = rows * kCellHeight;
    pixels_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_) * 4, 0);

    int cell = 0;
    for (std::size_t i = 0; i < kGlyphDefCount; ++i) {
        const GlyphDef& def = kGlyphDefs[i];
        const int cx = (cell % kColumns) * kCellWidth;
        const int cy = (cell / kColumns) * kCellHeight;
        ++cell;

        for (int y = 0; y < kCellHeight; ++y) {
            for (int x = 0; x < kCellWidth; ++x) {
                const float cov = coverage(static_cast<float>(x), static_cast<float>(y), def);
                if (cov <= 0.f) continue;
                const std::size_t offset =
                    (static_cast<std::size_t>(cy + y) * static_cast<std::size_t>(width_) +
                     static_cast<std::size_t>(cx + x)) * 4;
                const auto alpha = static_cast<std::uint8_t>(cov * 255.f);
                pixels_[offset + 0] = 255; // белый: цвет задаётся тонированием (FR-E30)
                pixels_[offset + 1] = 255;
                pixels_[offset + 2] = 255;
                pixels_[offset + 3] = alpha;
            }
        }

        GlyphRect rect;
        const float inv_w = 1.f / static_cast<float>(width_);
        const float inv_h = 1.f / static_cast<float>(height_);
        rect.u0 = static_cast<float>(cx) * inv_w;
        rect.v0 = static_cast<float>(cy) * inv_h;
        rect.u1 = static_cast<float>(cx + kCellWidth) * inv_w;
        rect.v1 = static_cast<float>(cy + kCellHeight) * inv_h;
        // Шаг: ширина ячейки в долях базовой высоты прописных знаков (кегль = size)
        rect.advance = static_cast<float>(kCellWidth) / kBaseCapHeight;
        glyphs_.emplace(def.code, rect);
    }

    const auto it = glyphs_.find(static_cast<std::uint32_t>('?'));
    if (it != glyphs_.end()) fallback_ = it->second;
    next_cell_ = cell;
    ready_ = true;
    return width_ > 0 && height_ > 0 && !glyphs_.empty();
}

const GlyphRect* FontAtlas::glyph(std::uint32_t codepoint) const {
    const auto it = glyphs_.find(codepoint);
    return it != glyphs_.end() ? &it->second : nullptr;
}

std::size_t FontAtlas::decode_utf8(const char* text, std::size_t length, std::uint32_t& out_cp) {
    if (length == 0) return 0;
    const auto b0 = static_cast<unsigned char>(text[0]);
    if (b0 < 0x80) {
        out_cp = b0;
        return 1;
    }
    if ((b0 & 0xE0) == 0xC0 && length >= 2) {
        const auto b1 = static_cast<unsigned char>(text[1]);
        if ((b1 & 0xC0) != 0x80) return 0;
        out_cp = (static_cast<std::uint32_t>(b0 & 0x1F) << 6) | (b1 & 0x3F);
        return 2;
    }
    if ((b0 & 0xF0) == 0xE0 && length >= 3) {
        const auto b1 = static_cast<unsigned char>(text[1]);
        const auto b2 = static_cast<unsigned char>(text[2]);
        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) return 0;
        out_cp = (static_cast<std::uint32_t>(b0 & 0x0F) << 12) |
                 (static_cast<std::uint32_t>(b1 & 0x3F) << 6) | (b2 & 0x3F);
        return 3;
    }
    if ((b0 & 0xF8) == 0xF0 && length >= 4) {
        const auto b1 = static_cast<unsigned char>(text[1]);
        const auto b2 = static_cast<unsigned char>(text[2]);
        const auto b3 = static_cast<unsigned char>(text[3]);
        if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) return 0;
        out_cp = (static_cast<std::uint32_t>(b0 & 0x07) << 18) |
                 (static_cast<std::uint32_t>(b1 & 0x3F) << 12) |
                 (static_cast<std::uint32_t>(b2 & 0x3F) << 6) | (b3 & 0x3F);
        return 4;
    }
    return 0; // недопустимый UTF-8: вызывающий код пропускает байт (п. 4.2.1)
}

float FontAtlas::text_width(const std::string& text, float size) const {
    float width = 0.f;
    const char* p = text.data();
    std::size_t remaining = text.size();
    while (remaining > 0) {
        std::uint32_t cp = 0;
        const std::size_t n = decode_utf8(p, remaining, cp);
        if (n == 0) {
            ++p;
            --remaining;
            continue;
        }
        const GlyphRect* g = glyph(cp);
        width += (g != nullptr ? g->advance : fallback_.advance) * size;
        p += n;
        remaining -= n;
    }
    return width;
}

} // namespace engine::render
