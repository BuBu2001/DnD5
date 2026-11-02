// rules/action_economy.h — экономика действий хода (п. Г.6).
#pragma once

namespace rules {

struct TurnResources {
    bool action_used = false;
    bool bonus_action_used = false;
    int movement_remaining_feet = 0;
    bool reaction_used = false;
    bool free_interaction_used = false;
};

inline void reset_turn(TurnResources& r, int speed_feet) {
    r.action_used = false;
    r.bonus_action_used = false;
    r.movement_remaining_feet = speed_feet;
    r.reaction_used = false;
    r.free_interaction_used = false;
}

} // namespace rules
