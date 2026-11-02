#include "rules/abilities.h"

namespace rules {

void AbilityScores::set_score(Ability a, int value) {
    const int i = static_cast<int>(a);
    score[i] = static_cast<std::int8_t>(value);
    // floor((score - 10) / 2) для отрицательных: арифметическое деление C++
    // округляет к нулю, поэтому вычисляем через явный floor
    int m = value - 10;
    if (m >= 0) {
        modifier_cache[i] = static_cast<std::int8_t>(m / 2);
    } else {
        modifier_cache[i] = static_cast<std::int8_t>(-((-m + 1) / 2));
    }
}

} // namespace rules
