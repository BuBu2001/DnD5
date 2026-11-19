// rules/contest.h — состязания проверок (DOC-02 FR-S24, статус M).
// Противопоставление двух проверок способностей (классический пример:
// Скрытность против Восприятия). Побеждает участник с более высоким итогом;
// при ничьей ни одна сторона не достигает цели (FR-S24, бинарность результата
// FR-S09 сохраняется: только «достиг цели» / «не достиг»).
// Оба броска идут через DiceRoller (FR-K102, AC-K22) с поддержкой
// преимущества/помехи у каждой стороны независимо (FR-S07).
// Слои модификаторов (FR-K25) соблюдает вызывающий код: передаётся итоговый
// модификатор стороны, собранный через modifier_stack.
// ВАЖНО (E-D01): файл не включает заголовки engine/*.
#pragma once

#include <string>

#include "rules/dice_roller.h"

namespace rules {

struct ContestResult {
    RollResult active;        // бросок активной стороны (кто инициирует состязание)
    RollResult opposing;      // бросок противодействующей стороны
    bool active_won = false;  // победа активной стороны
    bool tie = false;         // ничья: ни одна сторона не достигла цели (FR-S24)
    std::string description;  // строка для журнала боя (без правил в движке)
};

// Активная сторона активирует состязание; порог DC отсутствует — сравниваются
// итоги (кубик + модификатор) двух проверок. При ничьей active_won = false и
// tie = true одновременно. Отдельные advantage/disadvantage у каждой стороны.
inline ContestResult resolve_contest(DiceRoller& roller,
                                     const std::string& active_name,
                                     int active_modifier,
                                     const std::string& opposing_name,
                                     int opposing_modifier,
                                     bool active_advantage = false,
                                     bool active_disadvantage = false,
                                     bool opposing_advantage = false,
                                     bool opposing_disadvantage = false) {
    ContestResult out;
    // Порог 0 только для вызова единой точки броска: итог сравнения — в total.
    out.active = roller.roll(0, active_modifier, active_advantage, active_disadvantage);
    out.opposing = roller.roll(0, opposing_modifier, opposing_advantage, opposing_disadvantage);
    out.active.target_dc = 0;
    out.opposing.target_dc = 0;
    out.active.success = false;   // у состязания нет «успеха против DC»
    out.opposing.success = false;

    if (out.active.total == out.opposing.total) {
        out.tie = true;
        out.active_won = false;   // при ничьей ни одна сторона не достигла цели
    } else {
        out.active_won = out.active.total > out.opposing.total;
    }

    out.description = active_name + " " + std::to_string(out.active.total) +
                      " против " + opposing_name + " " +
                      std::to_string(out.opposing.total) +
                      (out.tie      ? " — ничья (цель не достигнута)"
                       : out.active_won ? " — успех активной стороны"
                                        : " — цель не достигнута");
    return out;
}

} // namespace rules
