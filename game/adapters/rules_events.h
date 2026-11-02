// game/adapters/rules_events.h — события, публикуемые адаптерами в шину (п. 4.1.11.6).
// Перечень строго по ТЗ: RollPerformed, DamageApplied, ConditionAdded, ConditionRemoved,
// TurnStateChanged. События несут DTO, а НЕ ссылки на внутренние структуры модуля
// правил (E-D12, UI-E02): подписчики — UI, журнал боя, отладочная панель.
#pragma once

#include <cstdint>
#include <string>

#include "rules/dto.h"

namespace game {

struct RollPerformedEvent {
    std::string source_entity;      // кто совершил бросок
    std::string kind;               // "attack" | "check" | "save" | "initiative" | "damage"
    rules::RollBreakdownDTO breakdown;
};

struct DamageAppliedEvent {
    std::string source_entity;
    std::string target_entity;
    int amount_dealt = 0;           // фактически списано с hp_current
    int amount_absorbed = 0;        // списано с временных хитов
    int hp_current = 0;
    int temp_hp = 0;
    std::string damage_type;        // "Slashing" (строка, а не enum правил — DTO-граница)
};

struct ConditionAddedEvent {
    std::string target_entity;
    rules::ConditionDTO condition;
    std::string source_id;
};

struct ConditionRemovedEvent {
    std::string target_entity;
    rules::ConditionDTO condition;
    std::string reason;             // "expired" | "dispelled" | "death"
};

struct TurnStateChangedEvent {
    std::string entity;
    std::string previous_state;     // имя состояния автомата хода (п. 4.1.7.2)
    std::string new_state;
    int round = 1;
};

} // namespace game
