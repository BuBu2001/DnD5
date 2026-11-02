// rules/dto_convert.h — функции адаптации структур правил в DTO для UI (E-D12, п. Г.8).
// Единственное место, где правила «переводятся» в форму, пригодную для интерфейса.
// Слою engine/ui/* запрещено включать rules/* (п. 4.8.2): конвертация выполняется
// в игровом слое (game/adapters), который имеет право знать оба мира (п. 3.3.2).
#pragma once

#include <string>
#include <vector>

#include "rules/action_economy.h"
#include "rules/conditions.h"
#include "rules/dice_roller.h"
#include "rules/dto.h"
#include "rules/formulas.h"
#include "rules/grid.h"
#include "rules/health.h"
#include "rules/initiative.h"

namespace rules {

// RollResult -> RollBreakdownDTO: все слагаемые с источниками и значениями (UI-E03)
inline RollBreakdownDTO to_dto(const RollResult& r, const std::string& title = "Бросок") {
    RollBreakdownDTO dto;
    dto.title = title;
    dto.dice = r.dice;
    dto.dice_values.reserve(r.dice.size());
    for (const int d : r.dice) dto.dice_values.push_back(std::to_string(d));
    dto.chosen_die = r.taken;
    dto.total = r.total;
    dto.dc = r.target_dc;
    dto.success = r.success;
    dto.advantage = r.used_advantage;
    dto.disadvantage = r.used_disadvantage;
    dto.cancelled = r.cancelled;

    for (const auto& c : r.breakdown) {
        ModifierDTO m;
        m.label = c.label.empty() ? c.source_id : c.label;
        m.value = c.value;
        m.source_id = c.source_id;
        switch (c.type) {
            case RollComponentType::Die:         m.source = ModifierSource::Base; break;
            case RollComponentType::AbilityMod:  m.source = ModifierSource::Ability; break;
            case RollComponentType::Proficiency: m.source = ModifierSource::Proficiency; break;
            case RollComponentType::Feature:     m.source = ModifierSource::Feature; break;
            case RollComponentType::Item:        m.source = ModifierSource::Item; break;
            case RollComponentType::Condition:   m.source = ModifierSource::Condition; break;
            case RollComponentType::Other:       m.source = ModifierSource::TemporaryBuff; break;
        }
        dto.modifiers.push_back(std::move(m));
    }
    return dto;
}

// ConditionDef -> ConditionDTO (иконки состояний, UI-E04: различимы не только цветом)
inline ConditionDTO to_dto(const ConditionDef& def) {
    ConditionDTO dto;
    dto.id = def.id;
    dto.name = def.name;
    dto.icon_key = def.icon_key.empty() ? def.id : def.icon_key;
    return dto;
}

inline std::vector<ConditionDTO> to_condition_dtos(const ConditionTable& table,
                                                   const std::vector<ConditionInstance>& active) {
    std::vector<ConditionDTO> out;
    out.reserve(active.size());
    for (const auto& inst : active) {
        if (const ConditionDef* def = table.find(inst.id); def != nullptr) {
            out.push_back(to_dto(*def));
        }
    }
    return out;
}

// TurnResources -> TurnResourceBarDTO (перевод футов в клетки — формула 17 п. Г.13)
inline TurnResourceBarDTO to_dto(const TurnResources& r, const std::string& entity_name = "") {
    TurnResourceBarDTO dto;
    dto.action_available = !r.action_used;
    dto.bonus_action_available = !r.bonus_action_used;
    dto.reaction_available = !r.reaction_used;
    dto.free_interaction_available = !r.free_interaction_used;
    dto.movement_remaining_feet = r.movement_remaining_feet;
    dto.movement_remaining_cells = feet_to_cells(r.movement_remaining_feet);
    dto.entity_name = entity_name;
    return dto;
}

// InitiativeQueue -> InitiativeTrackerDTO (п. Г.8): порядок сохранён по убыванию счёта,
// активный ход помечается игровым слоем (движок не знает правил, E-D01).
inline InitiativeTrackerDTO to_dto(const InitiativeQueue& queue, int round = 1,
                                   std::size_t active_index = 0) {
    InitiativeTrackerDTO dto;
    dto.round = round;
    dto.active_index = active_index;
    dto.entries.reserve(queue.size());
    for (std::size_t i = 0; i < queue.size(); ++i) {
        const InitiativeEntry e = queue.at(i);
        InitiativeEntryDTO item;
        item.entity = e.entity;
        item.score = e.score;
        item.is_active = (i == active_index);
        dto.entries.push_back(item);
    }
    return dto;
}

// HealthState -> часть CharacterSummaryDTO
inline CharacterSummaryDTO make_summary(const std::string& name, int level, int ac,
                                        const HealthState& h, int speed_feet,
                                        std::vector<ConditionDTO> conditions = {}) {
    CharacterSummaryDTO dto;
    dto.name = name;
    dto.level = level;
    dto.ac = ac;
    dto.hp_current = h.hp_current;
    dto.hp_max = h.hp_max;
    dto.temp_hp = h.temp_hp;
    dto.speed_feet = speed_feet;
    dto.conditions = std::move(conditions);
    return dto;
}

} // namespace rules
