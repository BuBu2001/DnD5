// tests/rules_tests/rules_tests.cpp — модульные испытания модуля правил.
// Состав: таблица 8.2 ТЗ DOC-04 (13 обязательных тестов) + контрольные примеры
// раздела 8.8 + критерии AC-K01…AC-K20 DOC-01. Покрытие rules — не менее 90 % (NFR-E08).
#include <string>
#include <vector>

#include "rules/abilities.h"
#include "rules/ac.h"
#include "rules/action_economy.h"
#include "rules/attack.h"
#include "rules/conditions.h"
#include "rules/damage.h"
#include "rules/dice_roller.h"
#include "rules/dto_convert.h"
#include "rules/features.h"
#include "rules/formulas.h"
#include "rules/grid.h"
#include "rules/health.h"
#include "rules/initiative.h"
#include "rules/items.h"
#include "rules/modifier_stack.h"
#include "rules/proficiency.h"
#include "rules/random_provider.h"
#include "rules/turn_state_machine.h"
#include "test_harness.h"

namespace {

// Тестовый двойник источника случайности (E-D07: RNG инжектируется).
// next_u32() возвращает «сырое» 32-битное слово: DiceRoller::roll_die вычисляет
// 1 + next_u32() % sides, поэтому для предсказуемых костей задаём слово так,
// чтобы остаток от деления давал нужное значение (kRawWord — базовое слово).
class FixedRng final : public rules::IRandomProvider {
public:
    explicit FixedRng(int d20) : d20_(d20) {}
    void set_d20(int v) { d20_ = v; }
    void set_sequence(std::vector<int> seq) { sequence_ = std::move(seq); index_ = 0; }
    std::uint32_t next_u32() override {
        // Слово, дающее нужное значение кости при любом числе граней: (v-1) — остаток
        return static_cast<std::uint32_t>(next_value() - 1);
    }
    int roll_d20() override { return next_value(); }

private:
    int next_value() {
        if (!sequence_.empty()) {
            const int v = sequence_[index_ % sequence_.size()];
            ++index_;
            return v;
        }
        return d20_;
    }
    int d20_ = 10;
    std::vector<int> sequence_;
    std::size_t index_ = 0;
};

} // namespace

// --- AC-K01, FR-K11, FR-E110: таблица модификаторов характеристик -------------
TEST_CASE("ability_modifier_bounds", "[rules][AC-K01]") {
    REQUIRE(rules::ability_modifier(8) == -1);
    REQUIRE(rules::ability_modifier(9) == -1);   // floor, а не округление к нулю
    REQUIRE(rules::ability_modifier(10) == 0);
    REQUIRE(rules::ability_modifier(11) == 0);
    REQUIRE(rules::ability_modifier(16) == 3);
    REQUIRE(rules::ability_modifier(17) == 3);
    REQUIRE(rules::ability_modifier(20) == 5);
    // Значения вне игрового диапазона считаются той же формулой (FR-K11)
    REQUIRE(rules::ability_modifier(6) == -2);
    REQUIRE(rules::ability_modifier(3) == -4);
    REQUIRE(rules::ability_modifier(1) == -5);

    // DATA-K11: модификатор кэшируется и пересчитывается только при изменении Score
    rules::AbilityScores scores;
    scores.set_score(rules::Ability::Str, 16);
    REQUIRE(scores.modifier(rules::Ability::Str) == 3);
    scores.set_score(rules::Ability::Str, 9);
    REQUIRE(scores.modifier(rules::Ability::Str) == -1);
}

// --- AC-K02, FR-K24, FR-E111: бонус мастерства и мультикласс ------------------
TEST_CASE("proficiency_multiclass", "[rules][AC-K02]") {
    REQUIRE(rules::proficiency_bonus_for_level(1) == 2);
    REQUIRE(rules::proficiency_bonus_for_level(4) == 2);
    REQUIRE(rules::proficiency_bonus_for_level(5) == 3);
    REQUIRE(rules::proficiency_bonus_for_level(8) == 3);
    REQUIRE(rules::proficiency_bonus_for_level(9) == 4);
    REQUIRE(rules::proficiency_bonus_for_level(13) == 5);
    REQUIRE(rules::proficiency_bonus_for_level(17) == 6);
    REQUIRE(rules::proficiency_bonus_for_level(20) == 6);

    // Мультикласс Воин 3 / Плут 2 считается как 5-й уровень (контрольный пример 5)
    REQUIRE(rules::total_character_level({3, 2}) == 5);
    REQUIRE(rules::proficiency_bonus_for_level(rules::total_character_level({3, 2})) == 3);

    // Флаги владения: None/Full в MVP, Half/Double заложены (п. Г.2)
    rules::ProficiencySet set;
    set.grant(rules::ProficiencyType::Full, "Skill_Stealth");
    REQUIRE(set.has("Skill_Stealth"));
    REQUIRE_FALSE(set.has("Skill_History"));
    REQUIRE(set.bonus_for("Skill_Stealth", 3) == 3);
    REQUIRE(set.bonus_for("Skill_History", 3) == 0);
    REQUIRE(rules::proficiency_contribution(4, rules::ProficiencyType::Half) == 2);
    REQUIRE(rules::proficiency_contribution(4, rules::ProficiencyType::Double) == 8);
    // Повторная выдача того же владения не удваивает бонус (FR-K35)
    set.grant(rules::ProficiencyType::Full, "Skill_Stealth");
    REQUIRE(set.size() == 1);
}

// --- AC-K03, FR-K22, FR-E112: класс доспеха -----------------------------------
TEST_CASE("ac_medium_armor_dex_cap", "[rules][AC-K03]") {
    // Средняя броня, DEX 18 (mod +4) → вклад ограничен +2 (контрольный пример 6)
    rules::ACInput medium;
    medium.base = 13;
    medium.dex_modifier = 4;
    medium.category = rules::ArmorCategory::Medium;
    REQUIRE(rules::calculate_ac(medium) == 15);

    // Отрицательный модификатор DEX в средней броне учитывается полностью
    medium.dex_modifier = -2;
    REQUIRE(rules::calculate_ac(medium) == 11);

    // Без брони: 10 + Mod(DEX) (формула 5 п. Г.13)
    rules::ACInput none;
    none.dex_modifier = 3;
    none.category = rules::ArmorCategory::None;
    REQUIRE(rules::calculate_ac(none) == 13);

    // Лёгкая броня: Base + Mod(DEX) без ограничения
    rules::ACInput light;
    light.base = 11;
    light.dex_modifier = 4;
    light.category = rules::ArmorCategory::Light;
    REQUIRE(rules::calculate_ac(light) == 15);

    // Тяжёлая броня: Mod(DEX) игнорируется, в том числе отрицательный
    rules::ACInput heavy;
    heavy.base = 16;
    heavy.dex_modifier = -3;
    heavy.category = rules::ArmorCategory::Heavy;
    REQUIRE(rules::calculate_ac(heavy) == 16);

    // Щит: +2 поверх любого варианта (формула 9 п. Г.13)
    heavy.has_shield = true;
    REQUIRE(rules::calculate_ac(heavy) == 18);
}

// --- AC-K04, FR-K20/FR-K21, FR-E113: хиты и временные хиты --------------------
TEST_CASE("temp_hp_damage_order", "[rules][AC-K04]") {
    // Контрольный пример 7: временные HP 8, урон 6 → временные 2, текущие не изменены
    rules::HealthState h = rules::make_health(20);
    rules::grant_temp_hp(h, 8);
    REQUIRE(h.temp_hp == 8);
    rules::apply_damage(h, 6);
    REQUIRE(h.temp_hp == 2);
    REQUIRE(h.hp_current == 20);

    // Урон сверх временных хитов переходит на текущие (п. 4.1.11.4)
    rules::apply_damage(h, 8);
    REQUIRE(h.temp_hp == 0);
    REQUIRE(h.hp_current == 14);

    // Временные хиты не складываются: берётся максимум источника (AC-K04)
    rules::HealthState h2 = rules::make_health(10);
    rules::grant_temp_hp(h2, 8);
    rules::grant_temp_hp(h2, 5);
    REQUIRE(h2.temp_hp == 8);
    rules::grant_temp_hp(h2, 12);
    REQUIRE(h2.temp_hp == 12);

    // Лечение временные хиты не восстанавливает (FR-K21)
    h2.hp_current = 5;
    const int temp_before = h2.temp_hp;
    rules::heal(h2, 100);
    REQUIRE(h2.temp_hp == temp_before);
    REQUIRE(h2.hp_current == 10); // не выше hp_max

    // Хиты 1-го уровня: max(HitDie) + Mod(CON) (формула 3 п. Г.13)
    REQUIRE(rules::hp_at_first_level(10, 2) == 12);   // Воин d10, CON 14
    REQUIRE(rules::hp_at_first_level(8, 1) == 9);      // Плут d8, CON 12
    REQUIRE(rules::hp_at_first_level(12, -1) == 11);   // Варвар d12, CON 8
    REQUIRE(rules::hp_at_first_level(6, -5) == 1);     // минимум 1 хит
    REQUIRE(rules::hp_on_level_up(6, 2) == 8);         // среднее d10 = 6, +2

    // Контрольный пример 13: спасброски от смерти 3/3
    rules::HealthState dying = rules::make_health(10);
    rules::apply_damage(dying, 10);
    REQUIRE(rules::is_unconscious(dying));
    REQUIRE(rules::death_save_outcome(dying) == rules::DeathSaveOutcome::Unconscious);
    rules::record_death_save(dying, true);
    rules::record_death_save(dying, true);
    REQUIRE(rules::death_save_outcome(dying) == rules::DeathSaveOutcome::Unconscious);
    rules::record_death_save(dying, true);
    REQUIRE(rules::death_save_outcome(dying) == rules::DeathSaveOutcome::Stabilized);

    rules::HealthState doomed = rules::make_health(10);
    rules::apply_damage(doomed, 10);
    rules::record_death_save(doomed, false);
    rules::record_death_save(doomed, false);
    rules::record_death_save(doomed, false);
    REQUIRE(rules::death_save_outcome(doomed) == rules::DeathSaveOutcome::Dead);

    // Натуральный 20 в спасброске от смерти: 1 хит, счётчики сброшены
    rules::HealthState revived = rules::make_health(10);
    rules::apply_damage(revived, 10);
    rules::record_death_save(revived, false);
    rules::record_death_save(revived, true, /*natural_20=*/true);
    REQUIRE(revived.hp_current == 1);
    REQUIRE(revived.death_saves_failure == 0);
    REQUIRE(rules::death_save_outcome(revived) == rules::DeathSaveOutcome::Alive);
}

// --- AC-K05…AC-K09, FR-K30…FR-K35, FR-E114: DiceRoller ------------------------
TEST_CASE("dice_advantage_disadvantage_cancel", "[rules][AC-K06][AC-K09]") {
    FixedRng rng(10);
    rng.set_sequence({7, 18}); // преимущество: выбран больший
    rules::DiceRoller roller(rng);

    auto adv = roller.roll(/*dc=*/15, /*modifier=*/5, /*advantage=*/true);
    REQUIRE(adv.dice.size() == 2);
    REQUIRE(adv.taken == 18);
    REQUIRE(adv.used_advantage);
    REQUIRE_FALSE(adv.used_disadvantage);
    REQUIRE_FALSE(adv.cancelled);
    REQUIRE(adv.total == 23);
    REQUIRE(adv.success); // 23 >= 15

    rng.set_sequence({7, 18});
    auto dis = roller.roll(15, 0, false, /*disadvantage=*/true);
    REQUIRE(dis.dice.size() == 2);
    REQUIRE(dis.taken == 7); // помеха: выбран меньший
    REQUIRE(dis.used_disadvantage);

    // AC-K06: одновременные преимущество и помеха → ровно один d20, cancelled = true
    rng.set_sequence({7, 18});
    auto both = roller.roll(15, 0, true, true);
    REQUIRE(both.dice.size() == 1);
    REQUIRE(both.cancelled);
    REQUIRE_FALSE(both.used_advantage);
    REQUIRE_FALSE(both.used_disadvantage);
}

TEST_CASE("dice_no_flat_plus_five", "[rules][AC-K07]") {
    // Преимущество не заменяется плоским бонусом +5: при равенстве исходов
    // результат отличается от «одинарного броска с +5» (FR-K31, AC-K07).
    FixedRng rng(10);
    rng.set_sequence({10, 20});
    rules::DiceRoller roller(rng);
    const auto adv = roller.roll(15, 0, true);
    REQUIRE(adv.taken == 20);            // взят больший кубик, а не 10 + 5
    REQUIRE(adv.modifier == 0);          // плоский бонус не добавлялся
    REQUIRE(adv.total == 20);

    // AC-K09: результат бинарный — success/failure, промежуточных степеней нет
    REQUIRE(adv.success == (adv.total >= adv.target_dc));
    const auto fail = roller.roll(30, 0, false);
    REQUIRE_FALSE(fail.success);
}

TEST_CASE("roll_result_breakdown", "[rules][AC-K08][UI-E03]") {
    FixedRng rng(12);
    rules::DiceRoller roller(rng);
    const auto r = roller.roll(/*dc=*/14, /*modifier=*/5, false);
    REQUIRE(r.dice.size() == 1);
    REQUIRE(r.total == r.taken + r.modifier);
    // Breakdown содержит все слагаемые: кость и модификатор (UI-E03)
    REQUIRE(r.breakdown.size() >= 2);
    bool has_die = false;
    bool has_modifier = false;
    for (const auto& c : r.breakdown) {
        if (c.type == rules::RollComponentType::Die) has_die = true;
        if (c.type == rules::RollComponentType::AbilityMod) has_modifier = true;
        REQUIRE_FALSE(c.source_id.empty()); // источник обязателен для UI
    }
    REQUIRE(has_die);
    REQUIRE(has_modifier);

    // AC-K08: DiceRoller возвращает объект со всеми полями, UI строится только из него
    const auto dto = rules::to_dto(r, "Проверка Силы");
    REQUIRE(dto.title == "Проверка Силы");
    REQUIRE(dto.total == r.total);
    REQUIRE(dto.dc == r.target_dc);
    REQUIRE(dto.success == r.success);
    REQUIRE(dto.dice == r.dice);
    REQUIRE(dto.modifiers.size() == r.breakdown.size());
}

TEST_CASE("dice_damage_roll_single_point", "[rules][FR-K102][AC-K22]") {
    // Единая точка броска: урон также идёт через DiceRoller, без прямых вызовов RNG
    FixedRng rng(1);
    rng.set_sequence({1, 2, 3, 4}); // значения костей 1, 2, 3, 4
    rules::DiceRoller roller(rng);
    const auto dmg = roller.roll_damage(/*count=*/2, /*sides=*/6, /*modifier=*/3);
    REQUIRE(dmg.dice.size() == 2);
    REQUIRE(dmg.dice[0] == 1);
    REQUIRE(dmg.dice[1] == 2);
    REQUIRE(dmg.dice_total == 3);
    REQUIRE(dmg.total == 6);
    REQUIRE(dmg.notation == "2d6+3");
    REQUIRE_FALSE(dmg.critical);

    // Критический успех удваивает КОСТИ, модификатор не удваивается (5e RAW)
    rng.set_sequence({6, 6, 5, 4});
    const auto crit = roller.roll_damage(2, 6, 3, /*critical=*/true);
    REQUIRE(crit.dice.size() == 4);   // 2 кости → 4 при крите
    REQUIRE(crit.critical);
    REQUIRE(crit.dice_total == 21);   // 6+6+5+4
    REQUIRE(crit.total == 24);        // + 3 модификатор (не удваивается)
    REQUIRE(crit.breakdown.size() == 5); // 4 кости + модификатор — всё видно в UI
}

// --- AC-K10, AC-K11, FR-K41/FR-K42, FR-E115: урон и сопротивления --------------
TEST_CASE("damage_resistance_vulnerability_conflict", "[rules][AC-K10][AC-K11]") {
    rules::DamageResistance res;
    // Контрольный пример 8: урон 9, сопротивление → 4 (floor(9/2))
    res.resistant.set(static_cast<std::size_t>(rules::DamageType::Fire));
    REQUIRE(rules::apply_resistance(9, rules::DamageType::Fire, res) == 4);
    REQUIRE(rules::apply_resistance(7, rules::DamageType::Fire, res) == 3);

    // Контрольный пример 9: урон 9, уязвимость → 18
    res.resistant.reset(static_cast<std::size_t>(rules::DamageType::Fire));
    res.vulnerable.set(static_cast<std::size_t>(rules::DamageType::Cold));
    REQUIRE(rules::apply_resistance(9, rules::DamageType::Cold, res) == 18);

    // AC-K11: сопротивление и уязвимость одновременно → множитель ×1
    res.resistant.set(static_cast<std::size_t>(rules::DamageType::Cold));
    REQUIRE(rules::apply_resistance(9, rules::DamageType::Cold, res) == 9);

    // Иммунитет → 0, и перекрывает уязвимость
    res.immune.set(static_cast<std::size_t>(rules::DamageType::Poison));
    res.vulnerable.set(static_cast<std::size_t>(rules::DamageType::Poison));
    REQUIRE(rules::apply_resistance(9, rules::DamageType::Poison, res) == 0);

    // Без модификаторов урон проходит как есть
    rules::DamageResistance clean;
    REQUIRE(rules::apply_resistance(9, rules::DamageType::Slashing, clean) == 9);
    REQUIRE(rules::DamageTypeCount == 13); // 13 типов урона (FR-K40)
}

// --- AC-K12, FR-K50, FR-E116: состояния как данные (E-D08) ---------------------
TEST_CASE("condition_flags_apply", "[rules][AC-K12][E-D08]") {
    const auto table = rules::make_standard_conditions();
    REQUIRE(table.size() == 6); // шесть обязательных состояний DOC-01
    for (const auto sc : {rules::StandardCondition::Restrained, rules::StandardCondition::Blinded,
                          rules::StandardCondition::Prone, rules::StandardCondition::Grappled,
                          rules::StandardCondition::Stunned, rules::StandardCondition::Invisible}) {
        REQUIRE(table.contains(rules::standard_condition_id(sc)));
    }

    // Опутан: скорость 0, помеха на свои атаки, преимущество атакующим
    std::vector<rules::ConditionInstance> active;
    active.push_back(rules::ConditionInstance{"Condition_Restrained", "Effect_Web", -1, false});
    const auto restrained = rules::collect(table, active);
    REQUIRE(rules::has_flag(restrained.flags, rules::ConditionFlags::SpeedZero));
    REQUIRE(rules::effective_speed_feet(30, restrained) == 0);

    rules::AttackContext melee_ctx;
    melee_ctx.melee = true;
    melee_ctx.requires_sight = true;
    const auto against_restrained = rules::resolve_target_attack_modifiers(restrained, melee_ctx);
    REQUIRE(against_restrained.advantage);

    // Ошеломлён: автопровал спасбросков Силы/Ловкости, не может действовать,
    // попадание в ближнем бою — крит (авторское усиление, OQ-08)
    active.clear();
    active.push_back(rules::ConditionInstance{"Condition_Stunned", "Effect_Stun", 1, false});
    const auto stunned = rules::collect(table, active);
    REQUIRE(stunned.incapacitated);
    REQUIRE(stunned.cannot_take_reactions);
    REQUIRE(rules::auto_fails_save(stunned, /*str_or_dex=*/true));
    REQUIRE_FALSE(rules::auto_fails_save(stunned, /*str_or_dex=*/false));
    REQUIRE(rules::has_flag(stunned.flags, rules::ConditionFlags::CritOnMeleeHit));
    REQUIRE(rules::effective_speed_feet(30, stunned) == 0);

    // Невидим: преимущество на свои атаки, помеха атакующим, требуется указать клетку
    active.clear();
    active.push_back(rules::ConditionInstance{"Condition_Invisible", "Effect_Spell", 10, true});
    const auto invisible = rules::collect(table, active);
    const auto attacker_side = rules::resolve_attacker_attack_modifiers(invisible, melee_ctx);
    const auto target_side = rules::resolve_target_attack_modifiers(invisible, melee_ctx);
    REQUIRE(attacker_side.advantage);
    REQUIRE(target_side.disadvantage);
    REQUIRE(rules::has_flag(invisible.flags, rules::ConditionFlags::RequiresGuessedCell));

    // Преимущество и помеха из разных источников: оба флага сохраняются,
    // аннулирование выполняет DiceRoller (п. 4.1.11.2)
    const auto combined = rules::combine(attacker_side, target_side);
    REQUIRE(combined.advantage);
    REQUIRE(combined.disadvantage);
    REQUIRE_FALSE(combined.reasons.empty()); // причины видны в UI (UI-E04)

    // Ослеплён: автопровал проверок, требующих зрения
    active.clear();
    active.push_back(rules::ConditionInstance{"Condition_Blinded", "Effect_Darkness", -1, false});
    const auto blinded = rules::collect(table, active);
    REQUIRE(rules::effective_speed_feet(30, blinded) == 30); // скорость не меняется
    const auto blinded_attack = rules::resolve_attacker_attack_modifiers(blinded, melee_ctx);
    REQUIRE(blinded_attack.disadvantage);

    // Ничком: ближний бой по существу — преимущество, дальний — помеха
    active.clear();
    active.push_back(rules::ConditionInstance{"Condition_Prone", "Effect_Shove", -1, false});
    const auto prone = rules::collect(table, active);
    rules::AttackContext ranged_ctx;
    ranged_ctx.melee = false;
    REQUIRE(rules::resolve_target_attack_modifiers(prone, melee_ctx).advantage);
    REQUIRE(rules::resolve_target_attack_modifiers(prone, ranged_ctx).disadvantage);
    REQUIRE(rules::stand_up_cost_feet(30) == 15); // половина скорости

    // Неизвестное состояние игнорируется, а не роняет программу (NFR-E14)
    active.clear();
    active.push_back(rules::ConditionInstance{"Condition_Unknown", "Effect_Bug", -1, false});
    const auto unknown = rules::collect(table, active);
    REQUIRE(unknown.flags == rules::ConditionFlags::None);
    REQUIRE(rules::effective_speed_feet(30, unknown) == 30);
}

// --- AC-K13, FR-K61, FR-E117: инициатива --------------------------------------
TEST_CASE("initiative_fixed_once", "[rules][AC-K13]") {
    // Равные счёта инициативы: проверяется детерминированный tie-breaker
    FixedRng rng(10);
    rng.set_sequence({10, 10, 10}); // все три участника выбросили 10
    rules::DiceRoller roller(rng);

    std::vector<rules::InitiativeEntry> entries;
    const int dex_mods[] = {3, 5, 5};
    for (rules::EntityId id = 1; id <= 3; ++id) {
        const auto roll = roller.roll(0, dex_mods[id - 1], false);
        REQUIRE(roll.total == 10 + dex_mods[id - 1]);
        entries.push_back(rules::InitiativeEntry{id, roll.total, dex_mods[id - 1], 0});
    }

    rules::InitiativeQueue queue;
    queue.build(entries);
    REQUIRE(queue.size() == 3);

    // Порядок: больший счёт выше; при равенстве — больший Mod(DEX);
    // при равенстве и его — меньший EntityId (п. 4.1.11.3)
    const auto first = queue.next();
    REQUIRE(first.entity == 2);              // счёт 15, DEX +5, меньший id
    REQUIRE(queue.next().entity == 3);       // счёт 15, DEX +5, больший id
    REQUIRE(queue.next().entity == 1);       // счёт 13
    REQUIRE(queue.contains(1));

    // Порядок фиксируется до конца боя: повторный обход даёт ту же последовательность,
    // пересчёт инициативы в следующих раундах запрещён (FR-K61)
    const auto snapshot = queue.entries();
    REQUIRE(snapshot.size() == 3);
    REQUIRE(snapshot[0].entity == 2);
    REQUIRE(queue.next().entity == 2); // круг замкнулся, порядок прежний
    REQUIRE(queue.entries() == snapshot);
}

// --- AC-K14, FR-K62, FR-E118: экономика действий -------------------------------
TEST_CASE("action_economy_limits", "[rules][AC-K14]") {
    rules::TurnResources r;
    rules::reset_turn(r, /*speed_feet=*/30);
    REQUIRE(r.movement_remaining_feet == 30);
    REQUIRE_FALSE(r.action_used);

    // Одно действие за ход
    rules::CostCandidate attack{true, false, 0, false};
    REQUIRE(rules::spend(r, attack));
    REQUIRE_FALSE(rules::spend(r, attack)); // второе действие потратить нельзя (AC-E10)

    // Одно бонусное действие за ход
    rules::CostCandidate bonus{false, true, 0, false};
    REQUIRE(rules::spend(r, bonus));
    REQUIRE_FALSE(rules::spend(r, bonus));

    // Перемещение в пределах скорости, можно разбивать на части
    rules::CostCandidate move10{false, false, 10, false};
    REQUIRE(rules::spend(r, move10));
    REQUIRE(r.movement_remaining_feet == 20);
    REQUIRE(rules::spend(r, move10));
    REQUIRE(r.movement_remaining_feet == 10);
    rules::CostCandidate move20{false, false, 20, false};
    REQUIRE_FALSE(rules::spend(r, move20)); // не хватает скорости
    REQUIRE(r.movement_remaining_feet == 10); // ничего не списано (атомарность)

    // Одна реакция на раунд
    rules::CostCandidate reaction{false, false, 0, true};
    REQUIRE(rules::spend(r, reaction));
    REQUIRE_FALSE(rules::spend(r, reaction));

    // Одно свободное взаимодействие
    REQUIRE_FALSE(r.free_interaction_used);
    r.free_interaction_used = true;
    REQUIRE(r.free_interaction_used);

    // Реакция восстанавливается в начале своего следующего хода
    rules::reset_turn(r, 30);
    REQUIRE(r.reaction_used == false);
    REQUIRE_FALSE(r.action_used);
}

// --- AC-K15, FR-K64, FR-E119: автомат хода -------------------------------------
TEST_CASE("turn_fsm_cancel_before_spend", "[rules][AC-K15]") {
    using S = rules::TurnState;
    // Отмена возможна только из Targeting и Confirming (п. 4.1.7.2)
    REQUIRE_FALSE(rules::can_cancel(S::WaitingForAction));
    REQUIRE(rules::can_cancel(S::Targeting));
    REQUIRE(rules::can_cancel(S::Confirming));
    REQUIRE_FALSE(rules::can_cancel(S::SpendingResources)); // после траты отмена запрещена
    REQUIRE_FALSE(rules::can_cancel(S::Resolving));
    REQUIRE_FALSE(rules::can_cancel(S::EndTurn));

    // Атомарность траты: либо все списания, либо ни одного (п. 4.1.7.3)
    rules::TurnResources r;
    rules::reset_turn(r, 30);
    rules::CostCandidate combo{/*action=*/true, /*bonus=*/true, /*move=*/15, /*reaction=*/false};
    REQUIRE(rules::can_afford(r, combo));
    REQUIRE(rules::spend(r, combo));
    REQUIRE(r.action_used);
    REQUIRE(r.bonus_action_used);
    REQUIRE(r.movement_remaining_feet == 15);

    // Недоступная комбинация не списывает ничего
    rules::CostCandidate too_much{true, false, 100, false};
    REQUIRE_FALSE(rules::can_afford(r, too_much));
    const auto before = r;
    REQUIRE_FALSE(rules::spend(r, too_much));
    REQUIRE(r.action_used == before.action_used);
    REQUIRE(r.movement_remaining_feet == before.movement_remaining_feet);
}

// --- AC-K18, FR-K81, FR-E121: настройка предметов не более трёх ----------------
TEST_CASE("attunement_limit_3", "[rules][AC-K18]") {
    rules::AttunementSet set;
    REQUIRE(set.request_attune("Item_Ring1"));
    REQUIRE(set.request_attune("Item_Ring2"));
    REQUIRE(set.request_attune("Item_Ring3"));
    // Настройка занимает короткий отдых: до отдыха предметы не настроены (AC-K18)
    REQUIRE(set.count() == 0);
    REQUIRE_FALSE(set.is_attuned("Item_Ring1"));
    REQUIRE(set.rest() == 3);
    REQUIRE(set.count() == 3);
    REQUIRE(set.is_attuned("Item_Ring1"));

    // Невозможно настроить четвёртый предмет
    REQUIRE(set.is_full());
    REQUIRE_FALSE(set.request_attune("Item_Ring4"));
    REQUIRE(set.rest() == 0);
    REQUIRE(set.count() == 3);

    // Повторный запрос того же предмета отклоняется (идемпотентность)
    REQUIRE_FALSE(set.request_attune("Item_Ring1"));

    // Снятие освобождает слот
    REQUIRE(set.unattune("Item_Ring2"));
    REQUIRE_FALSE(set.is_full());
    REQUIRE(set.request_attune("Item_Ring4"));
    REQUIRE(set.rest() == 1);
    REQUIRE(set.is_attuned("Item_Ring4"));
}

// --- AC-K19, AC-K20, FR-K80/FR-K83: инвентарь и симметрия экипировки -----------
TEST_CASE("item_components_and_equipment_symmetry", "[rules][AC-K19][AC-K20][E-D09]") {
    // Предмет — набор компонентов данных (E-D09), хардкод запрещён (NFR-E13)
    rules::ItemDef sword;
    sword.id = "Item_Sword_of_Flames";
    sword.name = "Пламенный меч";
    rules::ItemComponent weapon;
    weapon.type = rules::ItemComponentType::Weapon;
    weapon.base_damage = "1d8";
    weapon.damage_type = rules::DamageType::Slashing;
    rules::ItemComponent on_hit;
    on_hit.type = rules::ItemComponentType::OnHitEffect;
    on_hit.effect = "AddDamage";
    on_hit.damage = "1d4";
    on_hit.damage_type = rules::DamageType::Fire;
    rules::ItemComponent attune;
    attune.type = rules::ItemComponentType::AttunementRequired;
    sword.components = {weapon, on_hit, attune};

    REQUIRE(sword.requires_attunement());
    REQUIRE(sword.find(rules::ItemComponentType::Weapon) != nullptr);
    REQUIRE(sword.find(rules::ItemComponentType::Weapon)->base_damage == "1d8");
    REQUIRE(sword.find(rules::ItemComponentType::Armor) == nullptr);

    // Разбор записи костей из данных
    const auto notation = rules::parse_dice_notation("2d6");
    REQUIRE(notation.valid);
    REQUIRE(notation.count == 2);
    REQUIRE(notation.sides == 6);
    REQUIRE_FALSE(rules::parse_dice_notation("d6").valid);
    REQUIRE_FALSE(rules::parse_dice_notation("2d").valid);

    // Инвентарь: лимит слотами, вес и перегрузка отсутствуют (AC-K19)
    rules::Inventory inv(3);
    REQUIRE(inv.add("Item_Longsword"));
    REQUIRE(inv.add("Item_Shield"));
    REQUIRE(inv.add("Item_Potion"));
    REQUIRE_FALSE(inv.add("Item_Rope")); // слоты закончились
    REQUIRE(inv.remove("Item_Shield"));
    REQUIRE(inv.add("Item_Rope"));
    REQUIRE(inv.contains("Item_Rope"));
    REQUIRE_FALSE(inv.contains("Item_Shield"));

    // AC-K20: два цикла «надеть/снять» не меняют модификаторы (симметричность)
    rules::ItemDef cloak;
    cloak.id = "Item_Cloak_of_Protection";
    rules::ItemComponent mod;
    mod.type = rules::ItemComponentType::Modifier;
    rules::Modifier ac_bonus;
    ac_bonus.value = 1;
    ac_bonus.source = rules::ModifierSource::Item;
    ac_bonus.source_id = "Item_Cloak_of_Protection";
    ac_bonus.stacks = false;
    mod.modifiers = {ac_bonus};
    cloak.components = {mod};

    rules::Equipment equipment;
    const std::size_t mods_before = equipment.modifiers().size();
    REQUIRE(equipment.equip(cloak));
    REQUIRE(equipment.modifiers().size() == mods_before + 1);
    REQUIRE_FALSE(equipment.equip(cloak)); // повторная экипировка идемпотентна
    REQUIRE(equipment.modifiers().size() == mods_before + 1);
    REQUIRE(equipment.unequip(cloak));
    REQUIRE(equipment.modifiers().size() == mods_before);
    REQUIRE_FALSE(equipment.unequip(cloak)); // повторное снятие не даёт отрицательных бонусов
    REQUIRE(equipment.modifiers().size() == mods_before);

    // Второй цикл: результат тот же
    REQUIRE(equipment.equip(cloak));
    REQUIRE(equipment.unequip(cloak));
    REQUIRE(equipment.modifiers().size() == mods_before);

    // Предмет, требующий настройки, нельзя экипировать без attunement
    REQUIRE_FALSE(equipment.equip(sword));
    equipment.attunement().request_attune(sword.id);
    equipment.attunement().rest();
    REQUIRE(equipment.equip(sword));
}

// --- FR-K90, FR-E121: особые умения Категории 1 --------------------------------
TEST_CASE("feature_resolvers_category1", "[rules][FR-K90][AC-K21]") {
    // Sneak Attack: 1d6 × уровень Плута при преимуществе ИЛИ союзнике рядом
    rules::FeatureContext ctx;
    ctx.class_level = 3;
    ctx.has_advantage = false;
    ctx.ally_adjacent_to_target = false;
    REQUIRE_FALSE(rules::resolve_sneak_attack(ctx).applies);

    ctx.has_advantage = true;
    auto sneak = rules::resolve_sneak_attack(ctx);
    REQUIRE(sneak.applies);
    REQUIRE(sneak.extra_dice_count == 3);
    REQUIRE(sneak.extra_dice_sides == 6);
    REQUIRE(sneak.feature_id == "Feature_SneakAttack");
    REQUIRE_FALSE(sneak.reason.empty()); // обоснование видно в UI (UI-E03)

    ctx.has_advantage = false;
    ctx.ally_adjacent_to_target = true;
    REQUIRE(rules::resolve_sneak_attack(ctx).applies);

    // Ярость (упрощение MVP): плоский бонус только в ближнем бою
    rules::FeatureContext rage_ctx;
    rage_ctx.melee = true;
    const auto rage = rules::resolve_rage_damage_bonus(rage_ctx);
    REQUIRE(rage.applies);
    REQUIRE(rage.flat_bonus == 2);
    rage_ctx.melee = false;
    REQUIRE_FALSE(rules::resolve_rage_damage_bonus(rage_ctx).applies);

    // В MVP применяются только умения Категории 1 (FR-K93, AC-K21)
    rules::FeatureSet features;
    features.add("Feature_SneakAttack", rules::FeatureCategory::Category1);
    features.add("Feature_ActionSurge", rules::FeatureCategory::Category2);
    REQUIRE(features.has("Feature_SneakAttack"));
    const auto category1 = features.category1_ids();
    REQUIRE(category1.size() == 1);
    REQUIRE(category1[0] == "Feature_SneakAttack");
}

// --- FR-E120, п. 4.1.11.5: стек модификаторов ----------------------------------
TEST_CASE("modifier_stack_layers", "[rules][FR-E120][E-R10]") {
    std::vector<rules::Modifier> mods;
    mods.push_back(rules::Modifier{10, rules::ModifierSource::Base, "Base_Score", true});
    mods.push_back(rules::Modifier{3, rules::ModifierSource::Ability, "Ability_Str", true});
    mods.push_back(rules::Modifier{2, rules::ModifierSource::Proficiency, "Proficiency", true});
    mods.push_back(rules::Modifier{1, rules::ModifierSource::Item, "Item_Cloak", true});
    REQUIRE(rules::aggregate(mods) == 16);

    // Одноимённые не-stacking эффекты применяются в одном экземпляре — сильнейший
    std::vector<rules::Modifier> blessings;
    blessings.push_back(rules::Modifier{1, rules::ModifierSource::Spell, "Spell_Bless", false});
    blessings.push_back(rules::Modifier{4, rules::ModifierSource::Spell, "Spell_Bless", false});
    REQUIRE(rules::aggregate(blessings) == 4);

    // Слои не смешиваются: каждый источник учтён ровно один раз
    std::vector<rules::Modifier> mixed = mods;
    mixed.insert(mixed.end(), blessings.begin(), blessings.end());
    REQUIRE(rules::aggregate(mixed) == 16 + 4);
}

// --- Формулы п. Г.13 и контрольные примеры раздела 8.8 ---------------------------
TEST_CASE("formulas_reference_table", "[rules][Г.13]") {
    // Контрольный пример 11: жрец, WIS 16 (mod +3), 5-й уровень (prof +3) → DC 14
    REQUIRE(rules::spell_save_dc(rules::ability_modifier(16),
                                rules::proficiency_bonus_for_level(5)) == 14);
    // Контрольный пример 4: уровень 5 → +3
    REQUIRE(rules::proficiency_bonus(5) == 3);
    // Контрольный пример 12: пассивная проверка = 10 + Mod + Prof
    REQUIRE(rules::passive_check(3, 2) == 15);
    // Контрольный пример 17: 30 футов = 6 клеток
    REQUIRE(rules::feet_to_cells(30) == 6);
    REQUIRE(rules::feet_to_cells(5) == 1);
    REQUIRE(rules::resistant_damage(9) == 4);
    REQUIRE(rules::vulnerable_damage(9) == 18);

    // Вероятности (формулы п. Г.13): success = (21 - DC + Mod)/20
    REQUIRE(approx_equal(rules::success_probability(/*dc=*/15, /*mod=*/5), 0.55));
    REQUIRE(approx_equal(rules::success_probability(20, 0), 0.05));
    // advantage_probability = 1 - (1 - p)^2
    REQUIRE(approx_equal(rules::advantage_probability(0.5), 0.75));
}

// --- Тактическая сетка и перемещение (п. 4.1.8, FR-K60) -------------------------
TEST_CASE("tactical_grid_movement_costs", "[rules][FR-K60]") {
    rules::TacticalGrid grid(20, 15); // комната вертикального среза (Приложение Д)
    REQUIRE(grid.width() == 20);
    REQUIRE(grid.height() == 15);
    REQUIRE(grid.in_bounds({0, 0}));
    REQUIRE_FALSE(grid.in_bounds({20, 0}));
    REQUIRE(grid.entry_cost_feet({1, 1}) == 5); // 1 клетка = 5 футов

    grid.set_terrain({5, 5}, rules::TerrainType::Difficult);
    REQUIRE(grid.entry_cost_feet({5, 5}) == 10); // сложная местность: двойная стоимость
    grid.set_terrain({6, 5}, rules::TerrainType::Impassable);
    REQUIRE_FALSE(grid.is_passable({6, 5}));

    // Вне границ — путь недоступен
    REQUIRE_FALSE(grid.path_cost_feet_straight({0, 0}, {25, 0}).has_value());

    const auto cost = grid.path_cost_feet_straight({0, 0}, {3, 0});
    REQUIRE(cost.has_value());
    REQUIRE(*cost == 15); // три обычные клетки

    REQUIRE(rules::movement_range_cells(30) == 6);
    REQUIRE(rules::TacticalGrid::manhattan({0, 0}, {2, 3}) == 5);
}

// --- Разрешение атаки: критический успех и провал (п. 4.1.11.2) ------------------
TEST_CASE("attack_resolution_critical_and_fumble", "[rules][AC-K08]") {
    FixedRng rng(20);
    rules::DiceRoller roller(rng);
    rules::AttackInput in;
    in.ability_modifier = 3;
    in.proficiency_bonus = 2;

    // Натуральный 20: попадание независимо от AC; крит обрабатывает резолвер, не DiceRoller
    const auto crit = rules::resolve_attack(roller, in, /*ac=*/30);
    REQUIRE(crit.critical);
    REQUIRE(crit.hit);
    REQUIRE_FALSE(crit.roll.cancelled);

    // Натуральный 1: автоматический промах даже против низкого AC
    rng.set_d20(1);
    const auto fumble = rules::resolve_attack(roller, in, /*ac=*/5);
    REQUIRE(fumble.fumble);
    REQUIRE_FALSE(fumble.hit);

    // Обычное попадание: total >= AC
    rng.set_d20(15);
    const auto hit = rules::resolve_attack(roller, in, /*ac=*/18);
    REQUIRE(hit.hit);
    REQUIRE_FALSE(hit.critical);
    REQUIRE(hit.roll.total == 20);
    REQUIRE_FALSE(hit.description.empty()); // пояснение для журнала боя (FR-E08)

    // Промах по итогу
    rng.set_d20(5);
    const auto miss = rules::resolve_attack(roller, in, /*ac=*/18);
    REQUIRE_FALSE(miss.hit);
}

// --- Детерминизм (NFR-E09, E-D07, 4.8.1) ----------------------------------------
TEST_CASE("rng_determinism_and_state_roundtrip", "[rules][NFR-E09][FR-E92]") {
    rules::SeededRandomProvider a(42);
    rules::SeededRandomProvider b(42);
    std::vector<int> seq_a;
    std::vector<int> seq_b;
    for (int i = 0; i < 32; ++i) {
        seq_a.push_back(a.roll_d20());
        seq_b.push_back(b.roll_d20());
    }
    REQUIRE(seq_a == seq_b); // одинаковый seed → одинаковая последовательность

    // Разные seed дают разные последовательности
    rules::SeededRandomProvider c(43);
    std::vector<int> seq_c;
    for (int i = 0; i < 32; ++i) seq_c.push_back(c.roll_d20());
    REQUIRE(seq_a != seq_c);

    // Сохранение/восстановление состояния RNG (FR-E92)
    rules::SeededRandomProvider d(42);
    for (int i = 0; i < 10; ++i) d.roll_d20();
    const std::string state = d.export_state();
    REQUIRE(state.size() == rules::SeededRandomProvider::kStateWords * 16);
    std::vector<int> after_export;
    for (int i = 0; i < 16; ++i) after_export.push_back(d.roll_d20());

    rules::SeededRandomProvider restored(1);
    REQUIRE(restored.restore_state(state));
    std::vector<int> after_restore;
    for (int i = 0; i < 16; ++i) after_restore.push_back(restored.roll_d20());
    REQUIRE(after_export == after_restore);

    // Недопустимое состояние отклоняется без изменения генератора (NFR-E14)
    REQUIRE_FALSE(restored.restore_state("не-шестнадцатеричное"));
    REQUIRE_FALSE(restored.restore_state("0000000000000000"));
    REQUIRE_FALSE(restored.restore_state(""));

    // Все значения d20 в диапазоне 1..20
    for (const int v : after_export) {
        REQUIRE(v >= 1);
        REQUIRE(v <= 20);
    }
}

// --- DTO-граница для UI (E-D12, UI-E02, UI-E03) ---------------------------------
TEST_CASE("dto_conversion_for_ui", "[rules][E-D12][UI-E03]") {
    FixedRng rng(10);
    rng.set_sequence({7, 18});
    rules::DiceRoller roller(rng);
    const auto roll = roller.roll(15, 5, /*advantage=*/true);
    const auto dto = rules::to_dto(roll, "Атака: Воин");
    REQUIRE(dto.advantage);
    REQUIRE_FALSE(dto.disadvantage);
    REQUIRE(dto.chosen_die == 18);
    REQUIRE(dto.dice_values.size() == 2);
    REQUIRE(dto.dice_values[0] == "7");

    // Отмена преимущества/помехи видна в DTO отдельным признаком (UI-E04)
    const auto cancelled = roller.roll(15, 0, true, true);
    const auto cancelled_dto = rules::to_dto(cancelled);
    REQUIRE(cancelled_dto.cancelled);

    // TurnResources → TurnResourceBarDTO: футы и клетки (формула 17 п. Г.13)
    rules::TurnResources resources;
    rules::reset_turn(resources, 30);
    const auto bar = rules::to_dto(resources, "Воин");
    REQUIRE(bar.action_available);
    REQUIRE(bar.movement_remaining_feet == 30);
    REQUIRE(bar.movement_remaining_cells == 6);
    REQUIRE(bar.entity_name == "Воин");
    resources.action_used = true;
    resources.movement_remaining_feet = 10;
    const auto bar2 = rules::to_dto(resources, "Воин");
    REQUIRE_FALSE(bar2.action_available);
    REQUIRE(bar2.movement_remaining_cells == 2);

    // ConditionDef → ConditionDTO: иконка обязательна (UI-E04)
    const auto table = rules::make_standard_conditions();
    std::vector<rules::ConditionInstance> active;
    active.push_back(rules::ConditionInstance{"Condition_Prone", "Effect_Shove", -1, false});
    const auto icons = rules::to_condition_dtos(table, active);
    REQUIRE(icons.size() == 1);
    REQUIRE(icons[0].id == "Condition_Prone");
    REQUIRE_FALSE(icons[0].icon_key.empty());
    REQUIRE(icons[0].name == "Лежит ничком");

    // InitiativeQueue → InitiativeTrackerDTO: порядок сохранён
    std::vector<rules::InitiativeEntry> entries;
    entries.push_back(rules::InitiativeEntry{1, 20, 3, 0});
    entries.push_back(rules::InitiativeEntry{2, 10, 1, 0});
    rules::InitiativeQueue queue;
    queue.build(entries);
    const auto tracker = rules::to_dto(queue, /*round=*/2, /*active_index=*/0);
    REQUIRE(tracker.round == 2);
    REQUIRE(tracker.entries.size() == 2);
    REQUIRE(tracker.entries[0].entity == 1);
    REQUIRE(tracker.entries[0].is_active);
    REQUIRE_FALSE(tracker.entries[1].is_active);
}
