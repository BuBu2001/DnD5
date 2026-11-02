// engine/ui/ui_root.h — корень UI-дерева и масштабирование под разрешение окна
// (FR-E02, FR-E84, формула (1) п. 4.1.2.2).
//
// Логическое разрешение (базовое 1280×720) не равно физическому размеру окна:
// разметка выполняется в логических единицах, коэффициент масштабирования
//   S = min(W_window / W_logical, H_window / H_logical)
// свободные поля окна заполняются фоном (letterbox/pillarbox).
#pragma once

#include <algorithm>
#include <cstdint>

#include "render/renderer.h"
#include "ui/control.h"

namespace engine::ui {

struct ViewportInfo {
    std::uint32_t window_w = 1280;
    std::uint32_t window_h = 720;
    float scale = 1.f;        // коэффициент S из формулы (1)
    float offset_x = 0.f;     // поле pillarbox, физические пиксели
    float offset_y = 0.f;     // поле letterbox, физические пиксели
};

class UiRoot {
public:
    UiRoot() : root_(std::make_shared<Control>()) {
        root_->set_name("UiRoot");
        root_->set_size(Size{static_cast<float>(kLogicalWidth), static_cast<float>(kLogicalHeight)});
    }

    static constexpr std::uint32_t kLogicalWidth = 1280;   // FR-E01
    static constexpr std::uint32_t kLogicalHeight = 720;

    Control& root() { return *root_; }
    const Control& root() const { return *root_; }

    void add(ControlPtr child) { root_->add_child(std::move(child)); }

    // Пересчёт масштабирования под физический размер окна (FR-E02, FR-E84)
    ViewportInfo resize(std::uint32_t window_w, std::uint32_t window_h) {
        viewport_.window_w = window_w;
        viewport_.window_h = window_h;
        const float sx = static_cast<float>(window_w) / static_cast<float>(kLogicalWidth);
        const float sy = static_cast<float>(window_h) / static_cast<float>(kLogicalHeight);
        viewport_.scale = std::min(sx, sy);
        viewport_.offset_x = (static_cast<float>(window_w) - kLogicalWidth * viewport_.scale) * 0.5f;
        viewport_.offset_y = (static_cast<float>(window_h) - kLogicalHeight * viewport_.scale) * 0.5f;
        return viewport_;
    }

    const ViewportInfo& viewport() const { return viewport_; }
    float scale() const { return viewport_.scale; }

    // Физические координаты окна -> логические координаты разметки (для ввода, FR-E03)
    Vec2 window_to_logical(float window_x, float window_y) const {
        const float s = viewport_.scale > 0.f ? viewport_.scale : 1.f;
        return Vec2{(window_x - viewport_.offset_x) / s, (window_y - viewport_.offset_y) / s};
    }

    // Логические -> физические (для отладочной панели и снимков экрана)
    Vec2 logical_to_window(float logical_x, float logical_y) const {
        return Vec2{logical_x * viewport_.scale + viewport_.offset_x,
                    logical_y * viewport_.scale + viewport_.offset_y};
    }

    bool inside_logical_area(Vec2 logical) const {
        return logical.x >= 0.f && logical.y >= 0.f &&
               logical.x <= static_cast<float>(kLogicalWidth) &&
               logical.y <= static_cast<float>(kLogicalHeight);
    }

    void update(double dt_logic) { root_->update_all(dt_logic); }

    // Отрисовка дерева: поля letterbox/pillarbox заполняются фоном (FR-E02)
    void draw(render::IRenderer& renderer) {
        renderer.draw_rect(
            render::Rect{0.f, 0.f, static_cast<float>(viewport_.window_w),
                         static_cast<float>(viewport_.window_h)},
            render::Color{0.f, 0.f, 0.f, 1.f});
        root_->draw_all(renderer);
    }

    bool dispatch_event(UiEvent& event) { return root_->dispatch_event(event); }

    Control* find_by_name(const std::string& name) { return root_->find_by_name(name); }
    std::size_t widget_count() const { return root_->subtree_size(); }

private:
    ControlPtr root_;
    ViewportInfo viewport_{};
};

} // namespace engine::ui
