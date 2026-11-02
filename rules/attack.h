// rules/attack.h — разрешение броска атаки (п. 4.1.11.2, п. Г.5; DOC-01 формулы атаки).
// d20 + модификатор характеристики + бонус мастерства (если владеете) >= AC → попадание.
// Натуральные 20/1: автоматическое попадание / промах независимо от модификаторов.
#pragma once

#include <string>
#include <vector>

#include "rules/dice_roller.h"

namespace rules {

struct AttackInput {
    int ability_modifier = 0;      // модификатор SIL/DEX для оружия ближнего/дальнего боя
    int proficiency_bonus = 0;     // 0, если нет владения
    bool has_advantage = false;    // фланкирование и т.п. применяются на уровне расчёта флагов
    bool has_disadvantage = false; // реакция Shield Wall не входит в бросок (см. п. Г.5)
    int extra_modifiers = 0;       // прочие слоистые модификаторы (агрегируются вне функции)
};

struct AttackOutcome {
    RollResult roll;               // полный breakdown для UI (UI-E03)
    int ac = 0;
    bool hit = false;
    bool critical = false;         // natural 20
    bool fumble = false;           // natural 1
    std::string description;       // человекочитаемое пояснение для журнала (FR-E08)
};

inline AttackOutcome resolve_attack(DiceRoller& roller, const AttackInput& in, int target_ac) {
    AttackOutcome out;
    out.ac = target_ac;
    const int mod = in.ability_modifier + in.proficiency_bonus + in.extra_modifiers;
    out.roll = roller.roll(target_ac, mod, in.has_advantage, in.has_disadvantage);

    const int natural = out.roll.taken;
    out.critical = (natural == 20);
    out.fumble = (natural == 1);
    out.hit = out.critical || (!out.fumble && out.roll.total >= target_ac);

    out.description = "d20=" + std::to_string(natural) +
                      " mod=" + std::to_string(mod) +
                      " total=" + std::to_string(out.roll.total) +
                      " vs AC=" + std::to_string(target_ac) +
                      (out.critical ? " CRIT" : out.hit ? " HIT" : " MISS");
    return out;
}

} // namespace rules
