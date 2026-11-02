// rules/items.cpp — разбор записи костей для данных предметов (E-D09, NFR-E13).
#include "rules/items.h"

namespace rules {

DiceNotation parse_dice_notation(const std::string& text) {
    DiceNotation out;
    const auto sep = text.find('d');
    if (sep == std::string::npos || sep == 0 || sep + 1 >= text.size()) return out;

    const std::string count_part = text.substr(0, sep);
    const std::string sides_part = text.substr(sep + 1);
    for (const char ch : count_part) if (ch < '0' || ch > '9') return out;
    for (const char ch : sides_part) if (ch < '0' || ch > '9') return out;

    out.count = std::stoi(count_part);
    out.sides = std::stoi(sides_part);
    out.valid = out.count > 0 && out.sides > 0;
    return out;
}

} // namespace rules
