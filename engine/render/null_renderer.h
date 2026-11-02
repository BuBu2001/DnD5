// engine/render/null_renderer.h — headless-рендерер для тестов и демо-прогона.
// Реализует IRenderer «в пустоту», считая метрики (спрайты, draw calls) —
// позволяет гонять главный цикл и SpriteBatch без окна и GPU (этап M0/M1).
#pragma once

#include <cstddef>

#include "render/renderer.h"

namespace engine::render {

class NullRenderer final : public IRenderer {
public:
    void draw_sprite(TextureId /*texture*/, const Rect& /*src*/, const Rect& /*dst*/,
                     const Color& /*tint*/) override {
        ++sprites_drawn_;
    }

    void flush() override { ++flushes_; }

    std::size_t sprites_drawn() const { return sprites_drawn_; }
    std::size_t flushes() const { return flushes_; }

    void reset_metrics() {
        sprites_drawn_ = 0;
        flushes_ = 0;
    }

private:
    std::size_t sprites_drawn_ = 0;
    std::size_t flushes_ = 0;
};

} // namespace engine::render
