// engine/render/null_renderer.h — headless-рендерер для тестов и демо-прогона.
// Реализует IRenderer «в пустоту», считая метрики (спрайты, примитивы, текст,
// draw calls) — позволяет гонять главный цикл, UI и SpriteBatch без окна и GPU
// (этапы M0/M1, интеграционные тесты раздела 8.3 ТЗ DOC-04).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "render/renderer.h"

namespace engine::render {

class NullRenderer final : public IRenderer {
public:
    void draw_sprite(TextureId /*texture*/, const Rect& /*src*/, const Rect& /*dst*/,
                     const Color& /*tint*/) override {
        ++sprites_drawn_;
    }

    void draw_rect(const Rect& /*rect*/, const Color& /*color*/) override {
        ++primitives_drawn_;
        ++draw_calls_; // примитивы в MVP не пакетируются: каждый — отдельный вызов
    }

    void draw_rect_outline(const Rect& /*rect*/, const Color& /*color*/,
                           float /*thickness*/) override {
        ++primitives_drawn_;
        ++draw_calls_;
    }

    void draw_line(Vec2f /*from*/, Vec2f /*to*/, const Color& /*color*/,
                   float /*thickness*/) override {
        ++primitives_drawn_;
        ++draw_calls_;
    }

    void draw_circle(Vec2f /*center*/, float /*radius*/, const Color& /*color*/) override {
        ++primitives_drawn_;
        ++draw_calls_;
    }

    void draw_text(float /*x*/, float /*y*/, const std::string& text, float /*size*/,
                   const Color& /*color*/) override {
        ++text_calls_;
        last_text_ = text;
    }

    // flush() имитирует отправку накопленного пакета спрайтов одним вызовом (E-D05)
    void flush() override {
        if (sprites_drawn_ > flushed_sprites_) {
            ++draw_calls_;
            flushed_sprites_ = sprites_drawn_;
        }
        ++flushes_;
    }

    std::uint32_t draw_call_count() const override { return draw_calls_; }

    std::size_t sprites_drawn() const { return sprites_drawn_; }
    std::size_t flushes() const { return flushes_; }
    std::size_t primitives_drawn() const { return primitives_drawn_; }
    std::size_t text_calls() const { return text_calls_; }
    const std::string& last_text() const { return last_text_; }

    void reset_metrics() {
        sprites_drawn_ = 0;
        flushes_ = 0;
        primitives_drawn_ = 0;
        text_calls_ = 0;
        draw_calls_ = 0;
        flushed_sprites_ = 0;
        last_text_.clear();
    }

private:
    std::size_t sprites_drawn_ = 0;
    std::size_t flushes_ = 0;
    std::size_t primitives_drawn_ = 0;
    std::size_t text_calls_ = 0;
    std::uint32_t draw_calls_ = 0;
    std::size_t flushed_sprites_ = 0;
    std::string last_text_;
};

} // namespace engine::render
