// engine/render/sprite_batch.h — пакетирование спрайтов (E-D05, FR-E20…FR-E29).
// Запрещено выполнять draw call на каждый спрайт: все quad'ы накапливаются в
// буфере и отправляются одним flush(). Реализация бэкенда — этап M2 (OpenGL 3.3 Core).
#pragma once

#include <vector>

#include "render/renderer.h"

namespace engine::render {

struct SpriteQuad {
    TextureId texture = 0;
    Rect src{};   // прямоугольник источника в атласе
    Rect dst{};   // прямоугольник назначения в логических пикселях
    Color tint{};
    float rotation_deg = 0.f;
    Vec2f origin{}; // центр вращения (дополнит V1)
};

// Накопитель пакетов поверх IRenderer: принимает запросы draw_sprite,
// группирует их по текстуре и выдает минимальное число вызовов.
class SpriteBatch {
public:
    explicit SpriteBatch(IRenderer& renderer) : renderer_(renderer) {}

    void begin() {
        quads_.clear();
        batches_.clear();
    }

    void draw(TextureId texture, const Rect& src, const Rect& dst,
              const Color& tint = Color{}, float rotation_deg = 0.f) {
        quads_.push_back(SpriteQuad{texture, src, dst, tint, rotation_deg, {}});
    }

    // Пакетирование по ключу «текстура → слой» (п. 4.1.3.2, E-D05): соседние
    // quad'ы одной текстуры объединяются в один пакет, смена текстуры закрывает
    // пакет. Пустых пакетов не создаётся (дефект первой версии V0.3).
    void end() {
        batches_.clear();
        if (quads_.empty()) return;
        TextureId current = quads_.front().texture;
        std::size_t start = 0;
        for (std::size_t i = 1; i <= quads_.size(); ++i) {
            const TextureId tex = (i < quads_.size()) ? quads_[i].texture : kInvalidTexture;
            if (tex != current) {
                batches_.push_back({current, start, i});
                current = tex;
                start = i;
            }
        }
    }

    // Отправка накопленного в рендерер (один проход на текстуру, E-D05)
    std::size_t flush() {
        std::size_t draw_calls = 0;
        for (const auto& b : batches_) {
            for (std::size_t i = b.begin; i < b.end; ++i) {
                const auto& q = quads_[i];
                renderer_.draw_sprite(q.texture, q.src, q.dst, q.tint);
            }
            renderer_.flush();
            ++draw_calls;
        }
        batches_.clear();
        quads_.clear();
        return draw_calls; // метрика для отладочной панели (FR-E100)
    }

    std::size_t queued_sprites() const { return quads_.size(); }
    std::size_t pending_batches() const { return batches_.size(); }

    static inline constexpr TextureId kInvalidTexture = 0xFFFFFFFFu;

private:
    struct BatchRange {
        TextureId texture;
        std::size_t begin;
        std::size_t end;
    };

    IRenderer& renderer_;
    std::vector<SpriteQuad> quads_;
    std::vector<BatchRange> batches_;
};

} // namespace engine::render
