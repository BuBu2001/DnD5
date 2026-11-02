// rules/grid.h — тактическая сетка боя (п. 4.1.8: перемещение по клеткам,
// DOC-01: 1 клетка = 5 футов; формула feet_to_cells — п. Г.13).
// Сетка хранит только проходимость клеток; расчёт пути (A*) — игровой слой.
// ВАЖНО (E-D01): файл не включает заголовки engine/*.
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace rules {

struct Cell {
    int x = 0;
    int y = 0;

    friend bool operator==(const Cell&, const Cell&) = default;
};

inline constexpr int kFeetPerCell = 5; // 1 клетка = 5 футов (DOC-01, п. 2.3)

enum class TerrainType : std::uint8_t {
    Plain,     // обычная местность: 1 фут за 1 фут скорости
    Difficult, // сложная местность: 1 клетка стоит 2 клетки (п. 4.1.8.3)
    Impassable // непроходимо (стена, пропасть)
};

class TacticalGrid {
public:
    TacticalGrid(int width, int height)
        : width_(width), height_(height), terrain_(static_cast<size_t>(width) * height,
                                                    TerrainType::Plain) {}

    int width() const { return width_; }
    int height() const { return height_; }

    bool in_bounds(const Cell& c) const {
        return c.x >= 0 && c.y >= 0 && c.x < width_ && c.y < height_;
    }

    TerrainType terrain(const Cell& c) const {
        if (!in_bounds(c)) return TerrainType::Impassable;
        return terrain_[static_cast<size_t>(c.y) * width_ + c.x];
    }

    void set_terrain(const Cell& c, TerrainType t) {
        if (in_bounds(c)) terrain_[static_cast<size_t>(c.y) * width_ + c.x] = t;
    }

    bool is_passable(const Cell& c) const { return terrain(c) != TerrainType::Impassable; }

    // Стоимость входа в клетку в футах (сложная местность ×2, п. 4.1.8.3)
    int entry_cost_feet(const Cell& c) const {
        return terrain(c) == TerrainType::Difficult ? 2 * kFeetPerCell : kFeetPerCell;
    }

    // Манхэттенское расстояние в клетках (базовая метрика; диагонали — V1)
    static int manhattan(const Cell& a, const Cell& b) {
        return abs(a.x - b.x) + abs(a.y - b.y);
    }

    // Проверка достижимости при прямой траектории без поиска пути:
    // сколько футов движения «стоит» пройти по ортогональному маршруту.
    std::optional<int> path_cost_feet_straight(const Cell& from, const Cell& to) const {
        if (!in_bounds(from) || !in_bounds(to)) return std::nullopt;
        int cost = 0;
        Cell cur = from;
        while (cur.x != to.x) { cur.x += (to.x > cur.x) ? 1 : -1; cost += entry_cost_or_block(cur); }
        while (cur.y != to.y) { cur.y += (to.y > cur.y) ? 1 : -1; cost += entry_cost_or_block(cur); }
        return cost;
    }

private:
    int entry_cost_or_block(const Cell& c) const {
        return is_passable(c) ? entry_cost_feet(c) : INT32_MAX / 2;
    }

    int width_ = 0, height_ = 0;
    std::vector<TerrainType> terrain_;
};

// Перевод скорости в футах в дальность хода в клетках (формула п. Г.13)
inline int movement_range_cells(int speed_feet) { return speed_feet / kFeetPerCell; }

} // namespace rules
