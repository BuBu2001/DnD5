// tests/golden/golden_tests.cpp — golden-тесты (подраздел 8.4 ТЗ DOC-04, NFR-E09).
// Назначение: обнаружить изменение математики при рефакторинге. Фиксируются seed
// и эталонные значения; эталоны хранятся в tests/golden/golden_values.txt.
//
// Формат эталона (п. 8.4):
//   seed = 42
//   attack roll: dice = [7, 18]; taken = 18; modifier = +5; total = 23; AC = 15; success = true
//
// Регенерация эталонов выполняется ТОЛЬКО по решению о изменении правил
// (лист регистрации изменений, п. 2.6): DND_REGENERATE_GOLDEN=1 ./tests_golden
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "rules/attack.h"
#include "rules/conditions.h"
#include "rules/dice_roller.h"
#include "rules/initiative.h"
#include "rules/random_provider.h"
#include "test_harness.h"

namespace {

constexpr std::uint64_t kGoldenSeed = 42;
constexpr const char* kGoldenFile = DND_GOLDEN_FILE;

// Эталонный сценарий: последовательность бросков с фиксированным seed.
// Любое изменение формул или порядка потребления случайных чисел меняет вывод.
struct GoldenScenario {
    std::vector<int> d20_sequence;        // 20 значений d20 подряд
    std::vector<int> initiative_sequence; // инициатива четырёх участников
    rules::AttackOutcome attack_advantage; // атака с преимуществом, mod +5, AC 15 (пример п. 8.4)
    rules::AttackOutcome attack_plain;     // та же атака без преимущества
    int damage_1d8_plus_3 = 0;            // бросок урона
    int damage_crit_2d8_plus_3 = 0;       // критический бросок урона
};

GoldenScenario run_scenario(std::uint64_t seed) {
    GoldenScenario out;
    rules::SeededRandomProvider rng(seed);
    rules::DiceRoller roller(rng);

    for (int i = 0; i < 20; ++i) out.d20_sequence.push_back(rng.roll_d20());

    // Инициатива: 1d20 + Mod(DEX), четыре участника (FR-K61)
    const int dex_mods[] = {2, 4, -1, 1};
    std::vector<rules::InitiativeEntry> entries;
    for (rules::EntityId id = 1; id <= 4; ++id) {
        const auto roll = roller.roll(0, dex_mods[id - 1], false);
        entries.push_back(rules::InitiativeEntry{id, roll.total, dex_mods[id - 1], 0});
        out.initiative_sequence.push_back(roll.total);
    }
    rules::InitiativeQueue queue;
    queue.build(entries);
    for (std::size_t i = 0; i < queue.size(); ++i) {
        out.initiative_sequence.push_back(static_cast<int>(queue.at(i).entity));
    }

    // Атака: mod = +3 (характеристика) +2 (мастерство) = +5, AC 15 (пример п. 8.4)
    rules::AttackInput with_advantage;
    with_advantage.ability_modifier = 3;
    with_advantage.proficiency_bonus = 2;
    with_advantage.has_advantage = true;
    out.attack_advantage = rules::resolve_attack(roller, with_advantage, 15);

    rules::AttackInput plain;
    plain.ability_modifier = 3;
    plain.proficiency_bonus = 2;
    out.attack_plain = rules::resolve_attack(roller, plain, 15);

    out.damage_1d8_plus_3 = roller.roll_damage(1, 8, 3).total;
    out.damage_crit_2d8_plus_3 = roller.roll_damage(1, 8, 3, /*critical=*/true).total;
    return out;
}

std::string scenario_to_text(const GoldenScenario& s) {
    std::ostringstream os;
    os << "# golden-эталон (п. 8.4 ТЗ DOC-04). seed = " << kGoldenSeed << "\n";
    os << "d20_sequence =";
    for (const int v : s.d20_sequence) os << " " << v;
    os << "\n";
    os << "initiative =";
    for (const int v : s.initiative_sequence) os << " " << v;
    os << "\n";
    const auto attack = [&](const char* name, const rules::AttackOutcome& a) {
        os << name << ": dice =";
        for (const int d : a.roll.dice) os << " " << d;
        os << "; taken = " << a.roll.taken << "; modifier = " << (a.roll.modifier >= 0 ? "+" : "")
           << a.roll.modifier << "; total = " << a.roll.total << "; AC = " << a.ac
           << "; success = " << (a.roll.success ? "true" : "false")
           << "; critical = " << (a.critical ? "true" : "false") << "\n";
    };
    attack("attack_advantage", s.attack_advantage);
    attack("attack_plain", s.attack_plain);
    os << "damage_1d8_plus_3 = " << s.damage_1d8_plus_3 << "\n";
    os << "damage_crit_2d8_plus_3 = " << s.damage_crit_2d8_plus_3 << "\n";
    return os.str();
}

std::string read_file(const char* path) {
    std::ifstream in(path);
    if (!in.is_open()) return {};
    std::ostringstream os;
    os << in.rdbuf();
    return os.str();
}

} // namespace

TEST_CASE("golden_seed42_matches_reference", "[golden][NFR-E09][8.4]") {
    const GoldenScenario actual = run_scenario(kGoldenSeed);
    const std::string actual_text = scenario_to_text(actual);

    if (std::getenv("DND_REGENERATE_GOLDEN") != nullptr) {
        std::ofstream out(kGoldenFile);
        REQUIRE(out.is_open());
        out << actual_text;
        out.close();
        INFO("эталоны перегенерированы");
        return;
    }

    const std::string expected_text = read_file(kGoldenFile);
    if (expected_text.empty()) {
        // Эталон отсутствует: фиксируем текущее поведение и требуем явной проверки
        INFO("golden-эталон отсутствует — создайте его: DND_REGENERATE_GOLDEN=1");
        REQUIRE_FALSE(expected_text.empty());
        return;
    }
    if (expected_text != actual_text) {
        std::printf("---- эталон ----\n%s---- факт ----\n%s----------------\n",
                    expected_text.c_str(), actual_text.c_str());
    }
    REQUIRE(expected_text == actual_text); // математика не изменилась
}

TEST_CASE("golden_reproducibility_same_seed", "[golden][NFR-E09][E-D07]") {
    // Одинаковый seed → идентичная последовательность (воспроизводимость боёв)
    const auto a = run_scenario(kGoldenSeed);
    const auto b = run_scenario(kGoldenSeed);
    REQUIRE(scenario_to_text(a) == scenario_to_text(b));

    // Разный seed → иной результат (генератор не вырожден)
    const auto c = run_scenario(kGoldenSeed + 1);
    REQUIRE(scenario_to_text(a) != scenario_to_text(c));
}

TEST_CASE("golden_attack_roll_shape", "[golden][AC-K08][8.4]") {
    // Форма результата из примера п. 8.4: два кубика при преимуществе, взят больший
    const GoldenScenario s = run_scenario(kGoldenSeed);
    const auto& adv = s.attack_advantage;
    REQUIRE(adv.roll.dice.size() == 2); // преимущество: два d20, взят больший (FR-K31)
    REQUIRE(adv.roll.taken == std::max(adv.roll.dice[0], adv.roll.dice[1]));
    REQUIRE(adv.roll.used_advantage);
    REQUIRE(adv.roll.total == adv.roll.taken + adv.roll.modifier);
    REQUIRE(adv.roll.success == (adv.roll.total >= adv.ac));
    // Критический успех/провал определяется резолвером атаки, а не DiceRoller (п. 4.1.11.2)
    REQUIRE(adv.critical == (adv.roll.taken == 20));
    // Все слагаемые сохранены для UI (UI-E03)
    REQUIRE(adv.roll.breakdown.size() >= 2);
}
