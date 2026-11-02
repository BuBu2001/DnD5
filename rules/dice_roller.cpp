#include "rules/dice_roller.h"

namespace rules {

RollResult DiceRoller::roll(int target_dc, int modifier, bool has_advantage,
                            bool has_disadvantage) {
    RollResult r;
    r.target_dc = target_dc;
    r.modifier = modifier;

    if (has_advantage && has_disadvantage) {
        // Правило: одновременные преимущество и помеха → один бросок, cancelled = true.
        r.cancelled = true;
        has_advantage = false;
        has_disadvantage = false;
    }

    const int rolls = (has_advantage || has_disadvantage) ? 2 : 1;
    for (int i = 0; i < rolls; ++i) {
        r.dice.push_back(rng_.roll_d20());
    }

    if (has_advantage) {
        r.used_advantage = true;
        r.taken = r.dice[0] > r.dice[1] ? r.dice[0] : r.dice[1];
    } else if (has_disadvantage) {
        r.used_disadvantage = true;
        r.taken = r.dice[0] < r.dice[1] ? r.dice[0] : r.dice[1];
    } else {
        r.taken = r.dice[0];
    }

    r.total = r.taken + modifier;
    r.success = r.total >= target_dc;
    // Крит 20/1 обрабатывает резолвер атаки/проверки, не DiceRoller (п. 4.1.11.2)

    r.breakdown.push_back(RollComponent{r.taken, "die.d20", "d20", RollComponentType::Die});
    if (modifier != 0) {
        r.breakdown.push_back(
            RollComponent{modifier, "mod", "Модификатор", RollComponentType::AbilityMod});
    }
    return r;
}

} // namespace rules
