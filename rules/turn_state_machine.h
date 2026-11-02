// rules/turn_state_machine.h — состояния автомата хода (п. 4.1.7.2, п. Г.6).
// Атомарность траты ресурсов обеспечивается в SpendingResources (п. 4.1.7.3):
// либо все списания выполнены, либо ни одного.
#pragma once

#include "rules/action_economy.h"

namespace rules {

enum class TurnState {
    WaitingForAction, Targeting, Confirming,
    SpendingResources, Resolving, EndTurn
};

// Отмена допускается только из Targeting и Confirming (п. 4.1.7.2).
inline bool can_cancel(TurnState s) {
    return s == TurnState::Targeting || s == TurnState::Confirming;
}

// Кандидат на списание ресурса хода; применяется атомарно (все или ничего).
struct CostCandidate {
    bool use_action = false;
    bool use_bonus_action = false;
    int movement_feet = 0;
    bool use_reaction = false;
};

inline bool can_afford(const TurnResources& r, const CostCandidate& c) {
    if (c.use_action && r.action_used) return false;
    if (c.use_bonus_action && r.bonus_action_used) return false;
    if (c.use_reaction && r.reaction_used) return false;
    if (c.movement_feet > r.movement_remaining_feet) return false;
    return true;
}

inline bool spend(TurnResources& r, const CostCandidate& c) {
    if (!can_afford(r, c)) return false; // атомарность: ничего не списываем
    if (c.use_action) r.action_used = true;
    if (c.use_bonus_action) r.bonus_action_used = true;
    if (c.use_reaction) r.reaction_used = true;
    r.movement_remaining_feet -= c.movement_feet;
    return true;
}

} // namespace rules
