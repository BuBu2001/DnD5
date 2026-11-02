// game/adapters/rules_adapter.h — адаптер ECS ↔ Rules Core (п. 3.3.1: адаптер имеет
// право знать и engine, и rules). Задача адаптера:
//   1) вызывать модуль правил (единственная точка броска, FR-K102);
//   2) преобразовывать результаты в DTO (E-D12) и публиковать события в шину;
//   3) обновлять компоненты ECS по итогам расчётов.
// Адаптер НЕ содержит расчётных формул правил (п. 3.3.1, «Запрещено» для Game Layer).
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "core/event_bus.h"
#include "ecs/registry.h"
#include "rules/ac.h"
#include "rules/attack.h"
#include "rules/conditions.h"
#include "rules/damage.h"
#include "rules/dice_roller.h"
#include "rules/dto_convert.h"
#include "rules/health.h"
#include "rules/initiative.h"
#include "rules/proficiency.h"
#include "rules/turn_state_machine.h"

#include "components/game_components.h"
#include "adapters/rules_events.h"

namespace game {

// Имена состояний автомата хода для событий и журнала (п. 4.1.7.2)
const char* turn_state_name(rules::TurnState state);

class RulesAdapter {
public:
    RulesAdapter(engine::core::EventBus& bus, rules::DiceRoller& roller,
                 engine::ecs::Registry& registry, const rules::ConditionTable& conditions)
        : bus_(bus), roller_(roller), registry_(registry), conditions_(conditions) {}

    // --- характеристики и производные (FR-E110…FR-E112) ----------------------
    // Класс доспеха по экипировке и характеристикам (формулы 5…9 п. Г.13)
    int compute_ac(const AbilityScoresComponent& abilities, rules::ArmorCategory category,
                   int armor_base, bool has_shield) const;

    // --- броски: единая точка (FR-K102, AC-K22) ------------------------------
    // Проверка характеристики/навыка/спасбросок: 1d20 + Mod + Prof >= DC
    rules::RollResult perform_check(const std::string& entity_name, const std::string& kind,
                                    int dc, int ability_modifier, int proficiency_bonus,
                                    bool advantage, bool disadvantage);

    // Бросок атаки с учётом состояний атакующего и цели (E-D08: флаги, не ветвления)
    rules::AttackOutcome perform_attack(const std::string& attacker_name,
                                        const rules::AttackInput& input, int target_ac,
                                        const std::vector<rules::ConditionInstance>& attacker_conditions,
                                        const std::vector<rules::ConditionInstance>& target_conditions,
                                        bool melee = true);

    // --- урон (FR-E113, FR-E115) ---------------------------------------------
    // Применяет урон к HealthComponent сущности: сопротивления → списание temp/HP,
    // публикует DamageAppliedEvent. Возвращает фактически нанесённый урон.
    int apply_damage_to(engine::ecs::EntityId target, const std::string& source_name,
                        const std::string& target_name, int raw_damage,
                        rules::DamageType type, const rules::DamageResistance& resistance);

    // Лечение: временные хиты не восстанавливает (п. 4.1.11.4)
    int heal_entity(engine::ecs::EntityId target, int amount);

    // --- состояния (FR-E116) --------------------------------------------------
    bool add_condition(engine::ecs::EntityId target, const std::string& target_name,
                       const rules::ConditionInstance& instance);
    bool remove_condition(engine::ecs::EntityId target, const std::string& target_name,
                          const std::string& condition_id, const std::string& reason);
    rules::ConditionSummary summarize_conditions(engine::ecs::EntityId target) const;

    // --- ход (FR-E118, FR-E119) ----------------------------------------------
    void publish_turn_state(const std::string& entity, rules::TurnState previous,
                            rules::TurnState next, int round);

    // --- инициатива (FR-E117) -------------------------------------------------
    rules::InitiativeQueue build_initiative(
        const std::vector<std::pair<engine::ecs::EntityId, int>>& entities_with_dex_mod);

    engine::core::EventBus& bus() { return bus_; }
    rules::DiceRoller& roller() { return roller_; }

private:
    engine::core::EventBus& bus_;
    rules::DiceRoller& roller_;
    engine::ecs::Registry& registry_;
    const rules::ConditionTable& conditions_;
};

} // namespace game
