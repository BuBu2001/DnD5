// game/adapters/rules_adapter.cpp — реализация адаптера Rules Core ↔ ECS/шина.
#include "adapters/rules_adapter.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "core/log.h"

namespace game {

const char* turn_state_name(rules::TurnState state) {
    switch (state) {
        case rules::TurnState::WaitingForAction: return "WaitingForAction";
        case rules::TurnState::Targeting:        return "Targeting";
        case rules::TurnState::Confirming:       return "Confirming";
        case rules::TurnState::SpendingResources:return "SpendingResources";
        case rules::TurnState::Resolving:        return "Resolving";
        case rules::TurnState::EndTurn:          return "EndTurn";
    }
    return "Unknown";
}

int RulesAdapter::compute_ac(const AbilityScoresComponent& abilities,
                             rules::ArmorCategory category, int armor_base,
                             bool has_shield) const {
    rules::ACInput input;
    input.base = armor_base;
    input.dex_modifier = abilities.scores.modifier(rules::Ability::Dex);
    input.has_shield = has_shield;
    input.category = category;
    return rules::calculate_ac(input);
}

rules::RollResult RulesAdapter::perform_check(const std::string& entity_name,
                                             const std::string& kind, int dc,
                                             int ability_modifier, int proficiency_bonus,
                                             bool advantage, bool disadvantage) {
    // Порядок применения (FR-K35): кубик → модификатор характеристики → бонус
    // мастерства → прочие. Бонус мастерства добавляется не более одного раза.
    const auto result = roller_.roll(dc, ability_modifier + proficiency_bonus, advantage,
                                     disadvantage);
    RollPerformedEvent event;
    event.source_entity = entity_name;
    event.kind = kind;
    event.breakdown = rules::to_dto(result, kind + ": " + entity_name);
    bus_.publish(event);
    return result;
}

rules::AttackOutcome RulesAdapter::perform_attack(
    const std::string& attacker_name, const rules::AttackInput& input, int target_ac,
    const std::vector<rules::ConditionInstance>& attacker_conditions,
    const std::vector<rules::ConditionInstance>& target_conditions, bool melee) {
    // Преимущество/помеха собираются из флагов состояний (E-D08), а не ветвлениями
    const auto attacker_summary = rules::collect(conditions_, attacker_conditions);
    const auto target_summary = rules::collect(conditions_, target_conditions);

    rules::AttackContext ctx;
    ctx.melee = melee;
    ctx.requires_sight = true;
    const auto from_attacker = rules::resolve_attacker_attack_modifiers(attacker_summary, ctx);
    const auto from_target = rules::resolve_target_attack_modifiers(target_summary, ctx);
    const auto combined = rules::combine(from_attacker, from_target);

    rules::AttackInput effective = input;
    effective.has_advantage = input.has_advantage || combined.advantage;
    effective.has_disadvantage = input.has_disadvantage || combined.disadvantage;

    auto outcome = rules::resolve_attack(roller_, effective, target_ac);

    // Критический удар по ошеломлённой цели в ближнем бою (DOC-01 таблица 4.9, OQ-08)
    if (melee && rules::has_flag(target_summary.flags, rules::ConditionFlags::CritOnMeleeHit) &&
        outcome.hit) {
        outcome.critical = true;
        outcome.description += " CRIT(Stunned)";
    }

    RollPerformedEvent event;
    event.source_entity = attacker_name;
    event.kind = "attack";
    event.breakdown = rules::to_dto(outcome.roll, "Атака: " + attacker_name);
    bus_.publish(event);
    return outcome;
}

int RulesAdapter::apply_damage_to(engine::ecs::EntityId target, const std::string& source_name,
                                  const std::string& target_name, int raw_damage,
                                  rules::DamageType type,
                                  const rules::DamageResistance& resistance) {
    auto* health = registry_.try_get_component<HealthComponent>(target);
    if (health == nullptr) {
        engine::core::Log::instance().warn("adapter: у сущности нет HealthComponent — урон пропущен");
        return 0;
    }

    // Множители применяются после сложения всех модификаторов (DOC-01 FR-K42)
    const int mitigated = rules::apply_resistance(raw_damage, type, resistance);
    const int temp_before = health->state.temp_hp;
    const int hp_before = health->state.hp_current;
    rules::apply_damage(health->state, mitigated);
    const int absorbed = temp_before - health->state.temp_hp;
    const int dealt = hp_before - health->state.hp_current;

    DamageAppliedEvent event;
    event.source_entity = source_name;
    event.target_entity = target_name;
    event.amount_dealt = dealt;
    event.amount_absorbed = absorbed;
    event.hp_current = health->state.hp_current;
    event.temp_hp = health->state.temp_hp;
    static const char* kNames[] = {"Bludgeoning", "Piercing", "Slashing", "Fire", "Cold",
                                   "Lightning", "Acid", "Poison", "Necrotic", "Radiant",
                                   "Force", "Psychic", "Thunder"};
    const auto idx = static_cast<std::size_t>(type);
    event.damage_type = idx < 13 ? kNames[idx] : "Unknown";
    bus_.publish(event);
    return dealt + absorbed;
}

int RulesAdapter::heal_entity(engine::ecs::EntityId target, int amount) {
    auto* health = registry_.try_get_component<HealthComponent>(target);
    if (health == nullptr) return 0;
    return rules::heal(health->state, amount); // temp_hp не восстанавливает
}

bool RulesAdapter::add_condition(engine::ecs::EntityId target, const std::string& target_name,
                                 const rules::ConditionInstance& instance) {
    const rules::ConditionDef* def = conditions_.find(instance.id);
    if (def == nullptr) {
        // Неизвестное состояние: отказ с сообщением, без аварийного завершения (NFR-E14)
        engine::core::Log::instance().error("adapter: неизвестное состояние " + instance.id);
        return false;
    }
    auto* component = registry_.try_get_component<ConditionComponent>(target);
    if (component == nullptr) return false;
    for (const auto& existing : component->conditions) {
        if (existing.id == instance.id) return false; // одно состояние — один экземпляр
    }
    component->conditions.push_back(instance);

    ConditionAddedEvent event;
    event.target_entity = target_name;
    event.condition = rules::to_dto(*def);
    event.source_id = instance.source_id;
    bus_.publish(event);
    return true;
}

bool RulesAdapter::remove_condition(engine::ecs::EntityId target, const std::string& target_name,
                                    const std::string& condition_id, const std::string& reason) {
    auto* component = registry_.try_get_component<ConditionComponent>(target);
    if (component == nullptr) return false;
    auto& list = component->conditions;
    const auto it = std::find_if(list.begin(), list.end(),
                                 [&](const rules::ConditionInstance& c) { return c.id == condition_id; });
    if (it == list.end()) return false;
    list.erase(it);

    ConditionRemovedEvent event;
    event.target_entity = target_name;
    if (const rules::ConditionDef* def = conditions_.find(condition_id); def != nullptr) {
        event.condition = rules::to_dto(*def);
    } else {
        event.condition.id = condition_id;
        event.condition.name = condition_id;
        event.condition.icon_key = condition_id;
    }
    event.reason = reason;
    bus_.publish(event);
    return true;
}

rules::ConditionSummary RulesAdapter::summarize_conditions(engine::ecs::EntityId target) const {
    const auto* component = registry_.try_get_component<ConditionComponent>(target);
    if (component == nullptr) return {};
    return rules::collect(conditions_, component->conditions);
}

void RulesAdapter::publish_turn_state(const std::string& entity, rules::TurnState previous,
                                      rules::TurnState next, int round) {
    TurnStateChangedEvent event;
    event.entity = entity;
    event.previous_state = turn_state_name(previous);
    event.new_state = turn_state_name(next);
    event.round = round;
    bus_.publish(event); // п. 4.1.7.4: каждое изменение состояния публикуется в шине
}

rules::InitiativeQueue RulesAdapter::build_initiative(
    const std::vector<std::pair<engine::ecs::EntityId, int>>& entities_with_dex_mod) {
    std::vector<rules::InitiativeEntry> entries;
    entries.reserve(entities_with_dex_mod.size());
    for (const auto& [entity, dex_mod] : entities_with_dex_mod) {
        // Инициатива: 1d20 + Mod(DEX), один раз на бой (FR-K61, FR-E117)
        const auto roll = roller_.roll(0, dex_mod, false);
        rules::InitiativeEntry entry;
        entry.entity = static_cast<rules::EntityId>(entity.index);
        entry.score = roll.total;
        entry.dex_modifier = dex_mod;
        entries.push_back(entry);
    }
    rules::InitiativeQueue queue;
    queue.build(std::move(entries));
    return queue;
}

} // namespace game
