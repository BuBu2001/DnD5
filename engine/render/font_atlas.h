// engine/render/font_atlas.h — растровый атлас встроенного шрифта (FR-E29, этап M2).
// В V0.4 текст рисуется векторно-штриховым шрифтом движка, который запекается
// в RGBA-атлас при инициализации рендерера: это не требует внешних .ttf и работает
// в headless-сборке. Переход на stb_truetype + стилизованный шрифт — V1 (п. 4.1.3.3,
// таблица 4.16); интерфейс FontAtlas при этом не меняется (E-D04).
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine::render {

struct GlyphRect {
    float u0 = 0.f, v0 = 0.f, u1 = 0.f, v1 = 0.f; // координаты в атласе (0..1)
    float advance = 0.f;                           // шаг в долях кегля (× size = лог. px)
};

class FontAtlas {
public:
    // cell_w/cell_h — размер ячейки глифа в пикселях атласа
    static constexpr int kCellWidth = 16;
    static constexpr int kCellHeight = 32;
    static constexpr int kColumns = 16;
    // Высота прописных знаков в пикселях ячейки: базовый кегль для масштабирования.
    // Дизайн-сетка: прописные занимают Y 0..8 из диапазона -3..10, то есть
    // 8/13 высоты ячейки = 32 × 8/13 ≈ 19,69 px.
    static constexpr float kBaseCapHeight = 19.6923f;

    FontAtlas() = default;

    // Запекание атласа в памяти (без обращения к GPU). Возвращает false при ошибке.
    bool bake();

    const std::vector<std::uint8_t>& pixels() const { return pixels_; } // RGBA8
    int width() const { return width_; }
    int height() const { return height_; }
    bool ready() const { return ready_; }

    // Поиск глифа; для неизвестного кода возвращается глиф «?» (подстановка, п. 4.2.1)
    const GlyphRect* glyph(std::uint32_t codepoint) const;
    const GlyphRect& fallback_glyph() const { return fallback_; }

    // Декодирование UTF-8 (текст в UI и данных — UTF-8, DATA-E01).
    // Возвращает число прочитанных байт; 0 — при ошибке кодировки.
    static std::size_t decode_utf8(const char* text, std::size_t length, std::uint32_t& out_cp);

    // Ширина строки в логических единицах при данном кегле (для разметки UI, FR-E82)
    float text_width(const std::string& text, float size) const;

private:
    std::unordered_map<std::uint32_t, GlyphRect> glyphs_;
    std::vector<std::uint8_t> pixels_;
    GlyphRect fallback_{};
    int width_ = 0;
    int height_ = 0;
    int next_cell_ = 0;
    bool ready_ = false;
};

} // namespace engine::render
