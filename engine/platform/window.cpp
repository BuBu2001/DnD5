// engine/platform/window.cpp — расчёт вьюпорта окна (формула (1), п. 4.1.2.2).
// Функция не зависит от SDL2 и доступна в headless-сборке: именно она определяет
// масштаб S и поля letterbox/pillarbox, которые затем используют UI (FR-E84) и
// рендер-бэкенд (FR-E02). Создание окна — platform/sdl_window.cpp.
#include "platform/window.h"

namespace engine::platform {

// Формула (1): S = min(W_window / W_logical, H_window / H_logical);
// свободные поля делятся пополам (центрирование изображения).
ui::ViewportInfo Window::compute_viewport(std::uint32_t window_w, std::uint32_t window_h,
                                          std::uint32_t logical_w, std::uint32_t logical_h) {
    ui::ViewportInfo vp;
    vp.window_w = window_w;
    vp.window_h = window_h;
    if (logical_w == 0 || logical_h == 0) {
        vp.scale = 1.f;
        return vp;
    }
    const float sx = static_cast<float>(window_w) / static_cast<float>(logical_w);
    const float sy = static_cast<float>(window_h) / static_cast<float>(logical_h);
    vp.scale = sx < sy ? sx : sy;
    vp.offset_x = (static_cast<float>(window_w) - logical_w * vp.scale) * 0.5f;
    vp.offset_y = (static_cast<float>(window_h) - logical_h * vp.scale) * 0.5f;
    return vp;
}

} // namespace engine::platform
