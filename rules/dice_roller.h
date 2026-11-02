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

// Результат броска урона: все кости и слагаемые сохраняются для UI (UI-E03)
// и для применения множителей сопротивления ПОСЛЕ сложения (DOC-01 FR-K42).
struct DamageRollResult {
    std::vector<int> dice;        // значения всех костей урона
    int dice_total = 0;           // сумма костей
    int modifier = 0;             // модификаторы урона
    int total = 0;                // dice_total + modifier (до сопротивлений)
    bool critical = false;        // удвоены кости урона (не модификатор)
    std::string notation;         // исходная запись, например "1d8+2"
    std::vector<RollComponent> breakdown;
};

class DiceRoller {
public:
    explicit DiceRoller(IRandomProvider& rng) : rng_(rng) {}

    // Проверка/атака/спасбросок: 1d20 + модификаторы против порога (FR-K30, FR-K33)
    RollResult roll(int target_dc, int modifier, bool has_advantage,
                    bool has_disadvantage = false);

    // Бросок урона по записи "NdM" с плоским модификатором.
    // Единая точка броска (FR-K102, AC-K22): боевой код обязан вызывать этот метод,
    // прямые обращения к генератору случайных чисел в боевом коде запрещены.
    // При critical = true число костей удваивается, модификатор НЕ удваивается (5e RAW).
    DamageRollResult roll_damage(int dice_count, int dice_sides, int modifier,
                                 bool critical = false,
                                 RollComponentType extra_type = RollComponentType::Other);

    // Произвольная кость 1..sides (для бросков урона, таблиц, инициативы)
    int roll_die(int sides);

    IRandomProvider& random_provider() { return rng_; }

private:
    IRandomProvider& rng_;
};

} // namespace rules
