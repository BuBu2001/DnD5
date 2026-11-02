#include "rules/dice_roller.h"

#include <cstdint>

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

int DiceRoller::roll_die(int sides) {
    if (sides <= 1) return 1;
    return static_cast<int>(rng_.next_u32() % static_cast<std::uint32_t>(sides)) + 1;
}

DamageRollResult DiceRoller::roll_damage(int dice_count, int dice_sides, int modifier,
                                        bool critical, RollComponentType extra_type) {
    DamageRollResult out;
    out.critical = critical;
    out.modifier = modifier;
    out.notation = std::to_string(dice_count) + "d" + std::to_string(dice_sides);
    if (modifier != 0) {
        out.notation += (modifier > 0 ? "+" : "") + std::to_string(modifier);
    }

    // Критический успех удваивает КОСТИ урона, но не модификатор (DOC-01, 5e RAW)
    const int rolls = critical ? dice_count * 2 : dice_count;
    for (int i = 0; i < rolls; ++i) {
        const int value = roll_die(dice_sides);
        out.dice.push_back(value);
        out.dice_total += value;
        out.breakdown.push_back(RollComponent{value, "die.d" + std::to_string(dice_sides),
                                              "d" + std::to_string(dice_sides),
                                              RollComponentType::Die});
    }
    if (modifier != 0) {
        out.breakdown.push_back(
            RollComponent{modifier, "damage.modifier", "Модификатор урона", extra_type});
    }
    out.total = out.dice_total + modifier;
    if (out.total < 0) out.total = 0; // урон не может быть отрицательным (DOC-01 FR-K42)
    return out;
}

} // namespace rules
