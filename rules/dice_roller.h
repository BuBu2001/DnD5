// rules/dice_roller.h — сервис бросков (п. Г.4, жёсткие правила п. 4.1.11.2).
#pragma once

#include <string>
#include <vector>

#include "rules/random_provider.h"

namespace rules {

enum class RollComponentType : std::uint8_t { Die, AbilityMod, Proficiency, Feature, Item, Condition, Other };

struct RollComponent {
    int value = 0;
    std::string source_id;
    std::string label;
    RollComponentType type = RollComponentType::Other;
};

struct RollResult {
    std::vector<int> dice;      // все выпавшие значения d20
    int taken = 0;              // выбранное значение
    int modifier = 0;
    int total = 0;
    int target_dc = 0;
    bool success = false;
    bool used_advantage = false;
    bool used_disadvantage = false;
    bool cancelled = false;     // преимущество и помеха одновременно (п. 4.1.11.2)
    std::vector<RollComponent> breakdown; // все слагаемые для UI (UI-E03)
};

class DiceRoller {
public:
    explicit DiceRoller(IRandomProvider& rng) : rng_(rng) {}

    RollResult roll(int target_dc, int modifier, bool has_advantage,
                    bool has_disadvantage = false);

private:
    IRandomProvider& rng_;
};

} // namespace rules
