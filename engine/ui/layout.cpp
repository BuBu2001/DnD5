// engine/ui/layout.cpp — реализация контейнеров макета (FR-E81).
#include "ui/layout.h"

namespace engine::ui {

void BoxLayout::layout() {
    float offset = padding_.left;
    float cross = 0.f;
    float main_extent = 0.f;

    if (orientation_ == Orientation::Vertical) offset = padding_.top;

    for (const auto& child : children()) {
        if (!child->visible()) continue;
        if (orientation_ == Orientation::Vertical) {
            child->set_position(Vec2{padding_.left, offset});
            offset += child->size().h + spacing_;
            cross = cross > child->size().w ? cross : child->size().w;
        } else {
            child->set_position(Vec2{offset, padding_.top});
            offset += child->size().w + spacing_;
            cross = cross > child->size().h ? cross : child->size().h;
        }
    }

    main_extent = offset > spacing_ ? offset - spacing_ : 0.f;
    if (orientation_ == Orientation::Vertical) {
        set_size(Size{cross + padding_.left + padding_.right,
                      main_extent + padding_.top + padding_.bottom});
    } else {
        set_size(Size{main_extent + padding_.left + padding_.right,
                      cross + padding_.top + padding_.bottom});
    }
}

void BoxLayout::on_update(double dt_logic) {
    Control::on_update(dt_logic);
    layout();
}

void GridLayout::layout() {
    if (columns_ == 0 || rows_ == 0) return;

    Size cell = cell_size_;
    if (cell.w <= 0.f || cell.h <= 0.f) {
        // равные доли доступного размера контейнера
        const float usable_w = size_.w - padding_.left - padding_.right;
        const float usable_h = size_.h - padding_.top - padding_.bottom;
        cell.w = usable_w / static_cast<float>(columns_);
        cell.h = usable_h / static_cast<float>(rows_);
    }

    const auto& items = children();
    for (std::size_t i = 0; i < items.size(); ++i) {
        const std::size_t col = i % columns_;
        const std::size_t row = i / columns_;
        if (row >= rows_) break; // элементы сверх сетки не размещаются
        items[i]->set_position(Vec2{padding_.left + static_cast<float>(col) * (cell.w + spacing_),
                                    padding_.top + static_cast<float>(row) * (cell.h + spacing_)});
        items[i]->set_size(cell);
    }

    if (cell_size_.w <= 0.f || cell_size_.h <= 0.f) return;
    set_size(Size{padding_.left + padding_.right +
                      static_cast<float>(columns_) * cell.w +
                      static_cast<float>(columns_ > 0 ? columns_ - 1 : 0) * spacing_,
                  padding_.top + padding_.bottom +
                      static_cast<float>(rows_) * cell.h +
                      static_cast<float>(rows_ > 0 ? rows_ - 1 : 0) * spacing_});
}

void GridLayout::on_update(double dt_logic) {
    Control::on_update(dt_logic);
    layout();
}

} // namespace engine::ui
