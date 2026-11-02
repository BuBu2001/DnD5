// rules/ac.h — класс доспеха (п. Г.3).
#pragma once

namespace rules {

enum class ArmorCategory : std::uint8_t { None, Light, Medium, Heavy };

struct ACInput {
    int base = 10;
    int dex_modifier = 0;
    bool has_shield = false;
    ArmorCategory category = ArmorCategory::None;
};

inline int calculate_ac(const ACInput& input) {
    int ac = input.base + input.dex_modifier;
    if (input.has_shield) ac += 2;
    return ac;
}

} // namespace rules
