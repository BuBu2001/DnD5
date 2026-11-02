// engine/ui/layout.h — контейнеры макета (FR-E81): вертикальный, горизонтальный, сетка.
// Макет выполняется в логических единицах базового разрешения 1280×720 (FR-E84);
// пересчёт в физические пиксели — задача слоя отрисовки, не макета.
#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "ui/control.h"

namespace engine::ui {

enum class Orientation : std::uint8_t { Horizontal, Vertical };

// Линейный контейнер: располагает потомков вдоль оси с шагом spacing
class BoxLayout : public Control {
public:
    explicit BoxLayout(Orientation orientation = Orientation::Vertical)
        : orientation_(orientation) {}

    Orientation orientation() const { return orientation_; }
    void set_spacing(float spacing) { spacing_ = spacing; }
    float spacing() const { return spacing_; }

    // Пересчёт позиций и размера контейнера по содержимому
    void layout();

protected:
    void on_update(double dt_logic) override;

private:
    Orientation orientation_;
    float spacing_ = 4.f;
};

class VerticalBox : public BoxLayout {
public:
    VerticalBox() : BoxLayout(Orientation::Vertical) {}
};

class HorizontalBox : public BoxLayout {
public:
    HorizontalBox() : BoxLayout(Orientation::Horizontal) {}
};

// Сетка: fixed_size — фиксированный размер ячейки; если 0 — ячейки равной доли
class GridLayout : public Control {
public:
    GridLayout(std::size_t columns, std::size_t rows) : columns_(columns), rows_(rows) {}

    void set_cell_size(Size s) { cell_size_ = s; }
    void set_spacing(float s) { spacing_ = s; }
    std::size_t columns() const { return columns_; }
    std::size_t rows() const { return rows_; }

    void layout();

protected:
    void on_update(double dt_logic) override;

private:
    std::size_t columns_ = 1;
    std::size_t rows_ = 1;
    Size cell_size_{};
    float spacing_ = 4.f;
};

} // namespace engine::ui
