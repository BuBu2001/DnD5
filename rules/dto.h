// rules/dto.h — объекты передачи данных для UI (E-D02, E-D12, п. Г.8).
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
    std::string id;   // Condition_Prone
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

} // namespace rules
