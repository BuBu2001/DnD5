// rules/abilities.h — характеристики и модификаторы (п. Г.2).
#pragma once

#include <cstdint>

namespace rules {

enum class Ability : std::uint8_t { Str, Dex, Con, Int, Wis, Cha };

struct AbilityScores {
    std::int8_t score[6] = {10, 10, 10, 10, 10, 10};
    std::int8_t modifier_cache[6] = {0, 0, 0, 0, 0, 0};

    int modifier(Ability a) const { return modifier_cache[static_cast<int>(a)]; }
    void set_score(Ability a, int value);

    static int compute_modifier(int score) { return (score - 10) / 2; } // floor при целых
};

} // namespace rules
