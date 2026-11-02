// game/controls/cell_picking.h — выбор клетки тактической сетки курсором
// (FR-E33): логические координаты мыши → мировые (камера) → клетка.
#pragma once

#include <cmath>
#include <optional>

#include "render/camera.h"
#include "rules/grid.h"

namespace game {

struct CellPick {
    rules::Cell cell;
    float world_x = 0.f; // мировые координаты курсора (для подсветки)
    float world_y = 0.f;
};

// cell_size — размер клетки в мировых пикселях (kTileSize = 32).
inline std::optional<CellPick> pick_cell(const engine::render::Camera2D& camera,
                                         float logical_x, float logical_y,
                                         const rules::TacticalGrid& grid, float cell_size) {
    const auto world = camera.screen_to_world({logical_x, logical_y});
    const int cx = static_cast<int>(std::floor(world.x / cell_size));
    const int cy = static_cast<int>(std::floor(world.y / cell_size));
    const rules::Cell cell{cx, cy};
    if (!grid.in_bounds(cell)) return std::nullopt;
    return CellPick{cell, world.x, world.y};
}

} // namespace game
