// rules/ac.h — класс доспеха (FR-E112, DOC-01 FR-K22, таблица 4.4; п. Г.3 ТЗ DOC-04).
// Формулы:
//   без брони   AC = 10 + Mod(DEX)
//   лёгкая      AC = Base + Mod(DEX)
//   средняя     AC = Base + min(Mod(DEX), 2)      — ограничение вклада DEX (AC-K03)
//   тяжёлая     AC = Base                          — Mod(DEX) игнорируется полностью,
//                                                    в том числе отрицательный
//   щит         +2 к итоговому AC поверх любого варианта
// ВАЖНО (E-D01): файл не включает заголовки engine/*, SDL.h, GL/gl.h.
#pragma once

#include <cstdint>

namespace rules {

enum class ArmorCategory : std::uint8_t { None, Light, Medium, Heavy };

struct ACInput {
    int base = 10;              // базовое значение брони (для None — 10)
    int dex_modifier = 0;       // модификатор ловкости
    bool has_shield = false;    // щит: +2 (таблица 4.4 DOC-01)
    ArmorCategory category = ArmorCategory::None;
};

// Ограничение вклада DEX для средней брони (AC-K03: DEX 18 даёт +2, а не +4)
inline constexpr int kMediumArmorDexCap = 2;
inline constexpr int kShieldBonus = 2;

inline int calculate_ac(const ACInput& input) {
    int ac = 0;
    switch (input.category) {
        case ArmorCategory::None:   ac = 10 + input.dex_modifier; break;
        case ArmorCategory::Light:  ac = input.base + input.dex_modifier; break;
        case ArmorCategory::Medium: {
            const int dex = input.dex_modifier < kMediumArmorDexCap ? input.dex_modifier
                                                                    : kMediumArmorDexCap;
            ac = input.base + dex;
            break;
        }
        case ArmorCategory::Heavy:  ac = input.base; break; // DEX игнорируется
    }
    if (input.has_shield) ac += kShieldBonus;
    return ac;
}

} // namespace rules
