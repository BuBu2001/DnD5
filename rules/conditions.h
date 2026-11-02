// rules/conditions.h — состояния как данные (FR-E116, E-D08, DOC-01 FR-K50/DATA-K50).
// Жёсткое правило E-D08: проверки вида «if (condition == Blinded)» в боевом коде
// ЗАПРЕЩЕНЫ. Состояние описывается декларативно — флагами и модификаторами,
// а итог собирает резолвер collect()/resolve_attack_modifiers().
// Шесть обязательных состояний DOC-01 (таблица 4.9) объявлены перечислением
// StandardCondition; их эффекты задаются таблицей (в релизе — из assets:/data/
// conditions.json, NFR-E13/DATA-E01), здесь — эталонная таблица для тестов.
// ВАЖНО (E-D01): файл не включает заголовки engine/*.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "rules/modifier_stack.h"

namespace rules {

// Флаги состояний (п. Г.6 ТЗ DOC-04)
enum class ConditionFlags : std::uint32_t {
    None                         = 0,
    SpeedZero                    = 1u << 0,
    CannotTakeReactions          = 1u << 1,
    AttackAdvantageAgainst       = 1u << 2, // атакующие по существу получают преимущество
    AttackDisadvantageBy         = 1u << 3, // существо атакует с помехой
    SavingThrowDisadvantageDex   = 1u << 4,
    SavingThrowDisadvantageStr   = 1u << 5,
    AutomaticFailureStrDex       = 1u << 6,
    Incapacitated                = 1u << 7,
    // Расширение DATA-K50 (декларативное описание вместо ветвлений):
    AutoFailVisionChecks         = 1u << 8, // автоматический провал проверок, требующих зрения
    GrantsAttackAdvantage        = 1u << 9, // существо атакует с преимуществом
    CritOnMeleeHit               = 1u << 10,// любое попадание в ближнем бою — крит (авторское
                                            // усиление «Ошеломлён», DOC-01 OQ-08)
    RequiresGuessedCell          = 1u << 11,// атакующий обязан указать клетку (Невидим)
    MeleeAttackDisadvantage      = 1u << 12,// помеха на ближние атаки (Стоит ничком)
    RangedAttackDisadvantage     = 1u << 13,// помеха на дальние атаки (Стоит ничком)
    MeleeAttackAdvantage         = 1u << 14,// преимущество атакующим в ближнем бою
                                            // (цель лежит ничком)
    RangedAttackDisadvantageAgainst = 1u << 15, // помеха атакующим из дальней атаки
                                                // (цель лежит ничком)
    StandUpCostsHalfSpeed        = 1u << 16,// подняться стоит половину скорости
    CrawlCostsExtra              = 1u << 17,// ползание: +1 фут расхода за каждый фут
    EndsWhenGrapplerReleases     = 1u << 18// спадает, если схвативший отпустил/оттолкнут
};

inline ConditionFlags operator|(ConditionFlags a, ConditionFlags b) {
    return static_cast<ConditionFlags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}
inline ConditionFlags operator&(ConditionFlags a, ConditionFlags b) {
    return static_cast<ConditionFlags>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}
inline bool has_flag(ConditionFlags set, ConditionFlags flag) {
    return (static_cast<std::uint32_t>(set) & static_cast<std::uint32_t>(flag)) != 0u;
}

using ConditionId = std::string; // "Condition_Prone" (DATA-E03, п. 4.5.4)

// Объявление состояния: данные, а не код (DATA-K50)
struct ConditionDef {
    ConditionId id;
    std::string name;
    std::string icon_key;                     // для иконок UI (ConditionIcons, п. 4.1.8.3)
    ConditionFlags flags = ConditionFlags::None;
    std::vector<Modifier> modifiers;          // слой Condition в стеке модификаторов
    int speed_override_feet = -1;             // <0 — скорость не переопределена
    std::string description;                  // текст подсказки (Tooltip, FR-E83)
};

// Экземпляр состояния на сущности: ссылка на определение + источник и длительность
struct ConditionInstance {
    ConditionId id;
    std::string source_id;       // кто наложил (Effect_Spell_Hold, Ability_Shove…)
    int rounds_remaining = -1;   // −1 — до снятия внешним условием
    bool concentration = false;  // спадает при потере концентрации (DOC-02)
};

// Таблица определений состояний: data-driven, заполняется из JSON (FR-E116, NFR-E13)
class ConditionTable {
public:
    void add(ConditionDef def) { defs_.insert_or_assign(def.id, std::move(def)); }

    const ConditionDef* find(const ConditionId& id) const {
        const auto it = defs_.find(id);
        return it == defs_.end() ? nullptr : &it->second;
    }

    bool contains(const ConditionId& id) const { return find(id) != nullptr; }
    std::size_t size() const { return defs_.size(); }

    std::vector<ConditionId> ids() const {
        std::vector<ConditionId> out;
        out.reserve(defs_.size());
        for (const auto& [id, def] : defs_) out.push_back(id);
        std::sort(out.begin(), out.end());
        return out;
    }

private:
    std::unordered_map<ConditionId, ConditionDef> defs_;
};

// Идентификаторы шести обязательных состояний (DOC-01 FR-K50, таблица 4.9)
enum class StandardCondition : std::uint8_t {
    Restrained, Blinded, Prone, Grappled, Stunned, Invisible
};

inline const char* standard_condition_id(StandardCondition c) {
    switch (c) {
        case StandardCondition::Restrained: return "Condition_Restrained";
        case StandardCondition::Blinded:    return "Condition_Blinded";
        case StandardCondition::Prone:      return "Condition_Prone";
        case StandardCondition::Grappled:   return "Condition_Grappled";
        case StandardCondition::Stunned:    return "Condition_Stunned";
        case StandardCondition::Invisible:  return "Condition_Invisible";
    }
    return "Condition_Unknown";
}

// Эталонная таблица шести обязательных состояний (для тестов и как fallback,
// если данные не загружены; в релизе источник — assets:/data/conditions.json)
ConditionTable make_standard_conditions();

// --- Резолвер: сборка итога из флагов и модификаторов (E-D08) ----------------
struct ConditionSummary {
    ConditionFlags flags = ConditionFlags::None;
    std::vector<Modifier> modifiers;   // все модификаторы активных состояний
    int speed_override_feet = -1;      // максимальный приоритет: 0 при SpeedZero
    bool incapacitated = false;
    bool cannot_take_reactions = false;
};

// Сборка сводки по активным состояниям сущности
ConditionSummary collect(const ConditionTable& table,
                        const std::vector<ConditionInstance>& active);

// Итоговая скорость с учётом состояний (Опутан/Схвачен/Ошеломлён/Ничком → 0)
inline int effective_speed_feet(int base_speed_feet, const ConditionSummary& s) {
    if (has_flag(s.flags, ConditionFlags::SpeedZero)) return 0;
    return s.speed_override_feet >= 0 ? s.speed_override_feet : base_speed_feet;
}

// Контекст атаки для расчёта преимущества/помехи от состояний
struct AttackContext {
    bool melee = true;                 // ближний бой (иначе дальний)
    bool requires_sight = false;       // проверка/атака требует зрения
    bool attacker_guessed_wrong_cell = false; // Невидим: клетка указана неверно
};

struct AdvantageResolution {
    bool advantage = false;
    bool disadvantage = false;
    std::vector<std::string> reasons;  // источники для UI (UI-E04: не только цвет)
};

// Помеха/преимущество атакующего от ЕГО состояний (Ослеплён, Ничком, Невидим)
AdvantageResolution resolve_attacker_attack_modifiers(const ConditionSummary& attacker,
                                                     const AttackContext& ctx);

// Помеха/преимущество от состояний ЦЕЛИ (Опутан, Ослеплён, Ошеломлён, Невидим, Ничком)
AdvantageResolution resolve_target_attack_modifiers(const ConditionSummary& target,
                                                   const AttackContext& ctx);

// Объединение двух источников: преимущество и помеха одновременно → cancelled
// (правило DOC-01 FR-K31; аннулирование выполняет DiceRoller, здесь — только сбор флагов)
struct CombinedAdvantage {
    bool advantage = false;
    bool disadvantage = false;
    std::vector<std::string> reasons;
};

CombinedAdvantage combine(const AdvantageResolution& attacker,
                          const AdvantageResolution& target);

// Автоматический провал спасброска Силы/Ловкости (Ошеломлён, DOC-01 таблица 4.9)
inline bool auto_fails_save(const ConditionSummary& s, bool is_strength_or_dexterity) {
    if (!is_strength_or_dexterity) return false;
    return has_flag(s.flags, ConditionFlags::AutomaticFailureStrDex);
}

// Стоимость поднятия из положения «ничком»: половина скорости (DOC-01 таблица 4.9)
inline int stand_up_cost_feet(int speed_feet) { return speed_feet / 2; }

} // namespace rules
