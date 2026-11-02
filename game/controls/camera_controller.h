// game/controls/camera_controller.h — управление камерой боя: WASD-панорама,
// зум колесом, ограничение по карте (docs/controls.md; зум 0.5×…4× — Camera2D).
#pragma once

#include "controls/control_scheme.h"
#include "platform/input.h"
#include "platform/input_actions.h"
#include "render/camera.h"

namespace game {

class CameraController {
public:
    // world_w/world_h — размер карты в пикселях (клетки × размер клетки)
    CameraController(engine::render::Camera2D& camera, float world_w, float world_h,
                     float viewport_w, float viewport_h)
        : camera_(camera), world_w_(world_w), world_h_(world_h),
          viewport_w_(viewport_w), viewport_h_(viewport_h) {}

    void set_viewport(float w, float h) { viewport_w_ = w; viewport_h_ = h; }
    void set_pan_speed(float pixels_per_second) { pan_speed_ = pixels_per_second; }
    float pan_speed() const { return pan_speed_; }

    // Один шаг логики: непрерывное панорамирование (held) + событийный зум (wheel)
    void update(const engine::platform::InputActions& actions, float dt_seconds) {
        const float dx = actions.axis(actions::kCameraLeft, actions::kCameraRight);
        // Ось Y инвертирована: W (kCameraUp) даёт −1 — движение вверх по карте
        const float dy = actions.axis(actions::kCameraUp, actions::kCameraDown);
        auto pos = camera_.position();
        pos.x += dx * pan_speed_ * dt_seconds;
        pos.y += dy * pan_speed_ * dt_seconds; // W → dy=−1 → y уменьшается (вверх)
        camera_.set_position(pos.x, pos.y);

        if (actions.wheel(actions::kZoomIn)) camera_.set_zoom(camera_.zoom() * kZoomStep);
        if (actions.wheel(actions::kZoomOut)) camera_.set_zoom(camera_.zoom() / kZoomStep);

        clamp_to_map();
    }

    // Ограничение камеры границами карты: видимая область не выходит за карту,
    // если карта больше вьюпорта; иначе карта центрируется.
    void clamp_to_map() {
        const float view_w = viewport_w_ / camera_.zoom();
        const float view_h = viewport_h_ / camera_.zoom();
        auto pos = camera_.position();
        if (view_w >= world_w_) {
            pos.x = (world_w_ - view_w) * 0.5f;
        } else {
            if (pos.x < 0.f) pos.x = 0.f;
            if (pos.x + view_w > world_w_) pos.x = world_w_ - view_w;
        }
        if (view_h >= world_h_) {
            pos.y = (world_h_ - view_h) * 0.5f;
        } else {
            if (pos.y < 0.f) pos.y = 0.f;
            if (pos.y + view_h > world_h_) pos.y = world_h_ - view_h;
        }
        camera_.set_position(pos.x, pos.y);
    }

private:
    static constexpr float kZoomStep = 1.25f;

    engine::render::Camera2D& camera_;
    float world_w_;
    float world_h_;
    float viewport_w_;
    float viewport_h_;
    float pan_speed_ = 480.f; // логических пикселей в секунду
};

} // namespace game
