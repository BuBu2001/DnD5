// rules/rest.h — отдых и восстановление (DOC-02: FR-S60 короткий отдых,
// FR-S61 длинный отдых — статус M; связь с AC-K18 «настройка занимает короткий
// отдых» и FR-S62 «рост хит-поинтов»). Отсутствие XP и вехи — FR-K70: отдых не
// начисляет опыта и не повышает уровень.
// Жёсткие правила (FR-S60):
//   1) кубиков жизни доступно ровно level штук (HitDicePool.total = уровень);
//   2) потратить можно любое количество оставшихся, минимум — один;
//   3) за каждый кубик: кость типа класса + Modifier(CON), минимум 0 (DOC-02);
//   4) восстановление HP ограничено hp_max; temp_hp отдых не продлевает.
// Жёсткие правила (FR-S61):
//   1) длительность — не менее 8 часов, из них не менее 6 часов сна;
//   2) не чаще одного раза за 24-часовой период;
//   3) прерывание (≥ 1 часа напряжённой деятельности) — период начинается заново.
// Восстановление кубиков жизни при длинном отдыхе — ПОЛНОЕ (формулировка DOC-02;
// расхождение с RAW «половина, минимум один» зафиксировано в DR-11, docs/decisions.md).
// ВАЖНО (E-D01): файл не включает заголовки engine/*.
#pragma once

#include <string>
#include <vector>

#include "rules/dice_roller.h"
#include "rules/health.h"

namespace rules {

// --- Пороги длинного отдыха (FR-S61) -----------------------------------------
inline constexpr int kLongRestMinHours = 8;         // всего часов отдыха
inline constexpr int kLongRestMinSleepHours = 6;    // из них часов сна
inline constexpr int kLongRestPeriodHours = 24;     // не чаще одного раза за период
inline constexpr int kRestInterruptHours = 1;       // порог прерывания, часов

// --- Кубики жизни (FR-S60, FR-S62) --------------------------------------------
// total задаётся уровнем персонажа (расходуется при коротком отдыхе, see
// perform_short_rest); тип кости берётся из данных класса (assets:/data/classes,
// поле "hit_die": Воин d10, Плут d8, Варвар d12 — DOC-01 OQ-01, SRD 5.1).
struct HitDicePool {
    int total = 1;  // уровень персонажа (FR-S62: растёт при повышении уровня)
    int used = 0;   // израсходовано за сутки

    int available() const { return total - used; }

    // Потратить n кубиков: минимум один (FR-S60), не больше оставшихся.
    bool spend(int n) {
        if (n < 1 || n > available()) return false;
        used += n;
        return true;
    }

    // Полное восстановление кубиков при длинном отдыхе (FR-S60, DOC-02).
    void restore_all() { used = 0; }

    // Частичное восстановление (запас на будущее; RAW-вариант «половина»).
    void restore(int n) {
        used -= n;
        if (used < 0) used = 0;
    }
};

// --- Короткий отдых (FR-S60) --------------------------------------------------
struct ShortRestResult {
    bool ok = false;
    std::string reason;                // причина отказа — для журнала боя (FR-E48)
    int dice_spent = 0;                // потрачено кубиков жизни
    std::vector<int> rolls;            // значения костей (для разбора в UI, UI-E03)
    int hp_restored = 0;               // фактически восстановлено (с учётом hp_max)
    bool resources_recovered = false;  // слоты заклинаний и ресурсы класса (FR-S60)
};

// Короткий отдых длительностью не менее 1 часа: потратить dice_to_spend кубиков
// жизни (минимум один), восстановить HP. Броски идут ТОЛЬКО через DiceRoller
// (FR-K102, AC-K22). Восстановление: кость + Mod(CON), минимум 0 (DOC-02).
// Ресурсы, привязанные к короткому отдыху (слоты заклинаний, Всплеск действий,
// Ki), восстанавливаются: rules не хранит пул ресурсов класса — игровые пулы
// восстанавливает игровой слой по флагу resources_recovered (E-D01).
// Перенастройка предметов (AttunementSet::rest()) выполняется игровым слоем в
// момент завершения короткого отдыха (AC-K18, rules/items.h).
ShortRestResult perform_short_rest(HealthState& health, HitDicePool& pool,
                                   DiceRoller& roller, int hit_die_sides,
                                   int con_modifier, int dice_to_spend);

// --- Длинный отдых (FR-S61) ---------------------------------------------------
struct LongRestEvaluation {
    bool ok = false;
    std::string reason;  // причина отказа — для журнала; при ok пустая
};

// Проверка условий завершения длинного отдыха (без применения эффектов):
//   total_hours               — суммарная длительность отдыха (≥ 8);
//   sleep_hours               — из них часов сна (≥ 6);
//   hours_since_last_long_rest— часов с момента окончания предыдущего (≥ 24);
//   strenuous_hours           — часов напряжённой деятельности (> 0 прерывает
//                               отдых: восьмичасовой период начинается заново).
LongRestEvaluation evaluate_long_rest(int total_hours, int sleep_hours,
                                      int hours_since_last_long_rest,
                                      int strenuous_hours = 0);

struct LongRestResult {
    int hp_restored = 0;        // фактически восстановлено (hp_current = hp_max)
    int hit_dice_recovered = 0; // возвращено кубиков жизни (полное, DOC-02)
};

// Применение эффектов завершённого длинного отдыха: полное восстановление HP,
// temp_hp обнуляются, кубики жизни возвращаются полностью (DOC-02 FR-S61).
// Слоты заклинаний и уникальные ресурсы класса восстанавливает игровой слой.
LongRestResult apply_long_rest(HealthState& health, HitDicePool& pool);

} // namespace rules
