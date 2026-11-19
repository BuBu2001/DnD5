// rules/rest.cpp — реализация отдыха и восстановления (rules/rest.h).
// Броски кубиков жизни — только через DiceRoller (FR-K102, AC-K22):
// модуль не обращается к IRandomProvider напрямую, источник инжектируется (E-D07).
#include "rules/rest.h"

namespace rules {

ShortRestResult perform_short_rest(HealthState& health, HitDicePool& pool,
                                   DiceRoller& roller, int hit_die_sides,
                                   int con_modifier, int dice_to_spend) {
    ShortRestResult out;

    // FR-S60: потратить можно любое количество оставшихся кубиков, минимум один.
    if (dice_to_spend < 1) {
        out.reason = "короткий отдых: нужно потратить хотя бы один кубик жизни";
        return out;
    }
    if (dice_to_spend > pool.available()) {
        out.reason = "короткий отдых: доступно кубиков жизни " +
                     std::to_string(pool.available()) + ", запрошено " +
                     std::to_string(dice_to_spend);
        return out;
    }
    if (hit_die_sides < 2) {
        out.reason = "короткий отдых: недопустимый тип кости хитов";
        return out; // защита от битых данных (NFR-E14)
    }
    if (health.hp_current < 0) {
        out.reason = "короткий отдых: недопустимое состояние хитов";
        return out;
    }

    if (!pool.spend(dice_to_spend)) { // теоретически недостижимо после проверок
        out.reason = "короткий отдых: не удалось потратить кубики жизни";
        return out;
    }

    const int before = health.hp_current;
    out.dice_spent = dice_to_spend;
    out.rolls.reserve(static_cast<std::size_t>(dice_to_spend));
    for (int i = 0; i < dice_to_spend; ++i) {
        const int die = roller.roll_die(hit_die_sides); // единая точка броска
        int healed = die + con_modifier;
        if (healed < 0) healed = 0; // минимум 0 за кубик (DOC-02 FR-S60)
        out.rolls.push_back(die);
        health.hp_current += healed;
        // Восстановление ограничено максимумом; temp_hp отдых не трогает.
        if (health.hp_current > health.hp_max) health.hp_current = health.hp_max;
        if (health.hp_current < 0) health.hp_current = 0;
    }
    out.hp_restored = health.hp_current - before;
    out.resources_recovered = true; // слоты/классовые ресурсы — по флагу (E-D01)
    out.ok = true;
    return out;
}

LongRestEvaluation evaluate_long_rest(int total_hours, int sleep_hours,
                                      int hours_since_last_long_rest,
                                      int strenuous_hours) {
    LongRestEvaluation out;
    if (total_hours < kLongRestMinHours) {
        out.reason = "длинный отдых: длительность меньше " +
                     std::to_string(kLongRestMinHours) + " часов";
        return out;
    }
    if (sleep_hours < kLongRestMinSleepHours) {
        out.reason = "длинный отдых: сна меньше " +
                     std::to_string(kLongRestMinSleepHours) + " часов";
        return out;
    }
    if (hours_since_last_long_rest < kLongRestPeriodHours) {
        out.reason = "длинный отдых: не чаще одного раза за " +
                     std::to_string(kLongRestPeriodHours) + " часов";
        return out;
    }
    if (strenuous_hours >= kRestInterruptHours) {
        out.reason = "длинный отдых прерван (не менее " +
                     std::to_string(kRestInterruptHours) +
                     " ч напряжённой деятельности): период начинается заново";
        return out;
    }
    out.ok = true;
    return out;
}

LongRestResult apply_long_rest(HealthState& health, HitDicePool& pool) {
    LongRestResult out;
    const int before = health.hp_current;
    if (health.hp_current > health.hp_max) health.hp_current = health.hp_max;
    else health.hp_current = health.hp_max;
    health.temp_hp = 0; // временные хиты снимаются к началу нового дня
    out.hp_restored = health.hp_current - before;

    const int used_before = pool.used;
    pool.restore_all(); // полное восстановление кубиков жизни (DOC-02 FR-S61)
    out.hit_dice_recovered = used_before - pool.used;
    return out;
}

} // namespace rules
