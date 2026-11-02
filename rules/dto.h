// rules/dto.h — объекты передачи данных для UI (E-D02, E-D12, UI-E01, UI-E02, п. Г.8).
// Правило адаптации (п. Г.8):
//   rules::RollResult      -> RollBreakdownDTO      -> UI (панель разбора броска)
//   rules::ConditionDef    -> ConditionDTO          -> UI (иконки состояний)
//   rules::TurnResources   -> TurnResourceBarDTO    -> UI (панель ресурсов хода)
//   rules::InitiativeQueue -> InitiativeTrackerDTO  -> UI (очередь инициативы)
// UI получает ТОЛЬКО DTO и события: прямой доступ UI к структурам rules запрещён
// (NFR-E10, п. 4.8.2). DTO не содержат ссылок и указателей на внутренние объекты.
#pragma once

#include <string>
#include <vector>

#include "rules/modifier_stack.h"

namespace rules {

struct ModifierDTO {
    std::string label;
    int value = 0;
    std::string source_id;
    ModifierSource source = ModifierSource::Base;
};

struct RollBreakdownDTO {
    std::string title;
    std::vector<int> dice;
    std::vector<std::string> dice_values;
    int chosen_die = 0;
    std::vector<ModifierDTO> modifiers;
    int total = 0;
    int dc = 0;
    bool success = false;
    bool advantage = false;
    bool disadvantage = false;
    bool cancelled = false;
};

struct ConditionDTO {
    std::string id;   // "Condition_Prone"
    std::string name;
    std::string icon_key;
};

struct CharacterSummaryDTO {
    std::string name;
    int level = 1;
    int ac = 10;
    int hp_current = 0;
    int hp_max = 0;
    int temp_hp = 0;
    int speed_feet = 30;
    std::vector<ConditionDTO> conditions;
};

// Панель ресурсов хода (TurnResourceBar, п. 4.1.8.3)
struct TurnResourceBarDTO {
    bool action_available = true;
    bool bonus_action_available = false;   // доступна только при наличии разрешения
    bool reaction_available = true;
    bool free_interaction_available = true;
    int movement_remaining_feet = 0;
    int movement_remaining_cells = 0;      // футы / 5 (формула 17 п. Г.13)
    std::string entity_name;
};

// Очередь инициативы (InitiativeTracker, п. 4.1.8.3)
struct InitiativeEntryDTO {
    std::uint64_t entity = 0;
    std::string name;
    int score = 0;
    bool is_active = false;   // чей сейчас ход
    bool is_player_side = true;
};

struct InitiativeTrackerDTO {
    std::vector<InitiativeEntryDTO> entries; // порядок = порядок ходов (по убыванию)
    int round = 1;
    std::size_t active_index = 0;
};

} // namespace rules
