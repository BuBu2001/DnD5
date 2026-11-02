// engine/render/renderer.h — интерфейс рендер-бэкенда (E-D04, контракт п. Г.10).
// Заглушка до этапа M2: реализация OpenGL 3.3 Core / OpenGL ES 2.0.
// До flush() — только накопление в пакет (E-D05, запрет draw call на спрайт).
#pragma once

#include <cstdint>

namespace engine::render {

using TextureId = std::uint32_t;

struct Rect { float x = 0, y = 0, w = 0, h = 0; };
struct Color { float r = 1.f, g = 1.f, b = 1.f, a = 1.f; };

class IRenderer {
public:
    virtual ~IRenderer() = default;
    // renderer.draw_sprite(texture, src_rect, dst_rect, tint)  — п. Г.10
    virtual void draw_sprite(TextureId texture, const Rect& src, const Rect& dst,
                             const Color& tint = Color{}) = 0;
    virtual void flush() = 0; // принудительная отправка пакета
};

} // namespace engine::render
