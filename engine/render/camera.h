// engine/render/camera.h — ортографическая 2D-камера (FR-E30…FR-E39).
// Преобразование мировых координат в экранные: логическое разрешение 1280x720,
// масштаб S по формуле (1) (п. 4.1.2.2) применяется поверх камеры.
// Движок не знает правил D&D (E-D01): камера оперирует «мировыми единицами»,
// связь «1 клетка = 5 футов» устанавливает игровой слой (rules::feet_to_cells).
#pragma once

#include "render/renderer.h"

namespace engine::render {

class Camera2D {
public:
    void set_position(float x, float y) { position_ = {x, y}; }
    Vec2f position() const { return position_; }

    void set_zoom(float z) {
        if (z < kMinZoom) z = kMinZoom;
        if (z > kMaxZoom) z = kMaxZoom;
        zoom_ = z;
    }
    float zoom() const { return zoom_; }

    // Центр видимой области в мировых координатах
    void look_at_world(Vec2f world_point, float viewport_w, float viewport_h) {
        position_.x = world_point.x - viewport_w * 0.5f / zoom_;
        position_.y = world_point.y - viewport_h * 0.5f / zoom_;
    }

    // Мировые координаты -> экранные (логическое пространство 1280x720)
    Vec2f world_to_screen(Vec2f world) const {
        return {(world.x - position_.x) * zoom_, (world.y - position_.y) * zoom_};
    }

    // Экранные -> мировые (для попадания курсора по клетке, FR-E33)
    Vec2f screen_to_world(Vec2f screen) const {
        return {screen.x / zoom_ + position_.x, screen.y / zoom_ + position_.y};
    }

    // Прямоугольник видимости в мировых координатах (для отсечения спрайтов, NFR-E02)
    Rect visible_rect(float viewport_w, float viewport_h) const {
        return {position_.x, position_.y, viewport_w / zoom_, viewport_h / zoom_};
    }

    bool is_visible(const Rect& world_rect, float viewport_w, float viewport_h) const {
        const Rect v = visible_rect(viewport_w, viewport_h);
        return !(world_rect.x + world_rect.w < v.x || world_rect.x > v.x + v.w ||
                 world_rect.y + world_rect.h < v.y || world_rect.y > v.y + v.h);
    }

private:
    static constexpr float kMinZoom = 0.5f;  // ограничение зума (FR-E35)
    static constexpr float kMaxZoom = 4.0f;
    Vec2f position_{};
    float zoom_ = 1.0f;
};

} // namespace engine::render
