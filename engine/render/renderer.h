// engine/render/renderer.h — интерфейс рендер-бэкенда (FR-E20…FR-E32, E-D04, контракт п. Г.10).
// Реализации: OpenGL 3.3 Core (ПК, M2), OpenGL ES 2.0/3.0 (мобильные, M11),
// NullRenderer (headless-тесты). Остальной код движка работает ТОЛЬКО через
// этот интерфейс (E-D04): смена бэкенда не затрагивает слои выше.
// До flush() — только накопление в пакет (E-D05, запрет draw call на спрайт).
#pragma once

#include <cstdint>
#include <string>

namespace engine::render {

using TextureId = std::uint32_t;
inline constexpr TextureId kInvalidTexture = 0xFFFFFFFFu;

struct Rect { float x = 0, y = 0, w = 0, h = 0; };
struct Color { float r = 1.f, g = 1.f, b = 1.f, a = 1.f; };
struct Vec2f { float x = 0.f, y = 0.f; };

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // --- спрайты (FR-E22, FR-E23, FR-E30, FR-E31) ----------------------------
    // renderer.draw_sprite(texture, src_rect, dst_rect, tint) — п. Г.10
    virtual void draw_sprite(TextureId texture, const Rect& src, const Rect& dst,
                             const Color& tint = Color{}) = 0;

    // Зеркальный разворот по осям X/Y (FR-E31)
    virtual void draw_sprite_flipped(TextureId texture, const Rect& src, const Rect& dst,
                                     const Color& tint, bool flip_x, bool flip_y) {
        (void)flip_x; (void)flip_y; // по умолчанию разворот не поддерживается бэкендом
        draw_sprite(texture, src, dst, tint);
    }

    // --- примитивы (FR-E28) ---------------------------------------------------
    virtual void draw_rect(const Rect& rect, const Color& color) = 0;
    virtual void draw_rect_outline(const Rect& rect, const Color& color, float thickness) = 0;
    virtual void draw_line(Vec2f from, Vec2f to, const Color& color, float thickness) = 0;
    virtual void draw_circle(Vec2f center, float radius, const Color& color) = 0;

    // --- текст (FR-E29): шрифтовой атлас stb_truetype + stb_rect_pack ----------
    // x, y — позиция базовой линии в логических единицах; size — кегль
    virtual void draw_text(float x, float y, const std::string& text, float size,
                           const Color& color) = 0;

    // --- кадр -----------------------------------------------------------------
    virtual void begin_frame() {}
    virtual void flush() = 0; // принудительная отправка накопленного пакета
    virtual void end_frame() { flush(); }

    // --- метрики для отладочной панели (FR-E100, NFR-E01: не более 300 draw calls)
    virtual std::uint32_t draw_call_count() const { return 0; }
};

} // namespace engine::render
