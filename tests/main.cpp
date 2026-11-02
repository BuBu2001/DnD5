// tests/main.cpp — минимальный фреймворк тестов (Catch2 подключается на этапе M1,
// таблица 4.16; здесь — каркас по п. Г.12 и таблице 8.2 ТЗ DOC-04).
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "core/config.h"
#include "core/event_bus.h"
#include "core/file_system.h"
#include "ecs/registry.h"
#include "ecs/view.h"
#include "platform/main_loop.h"
#include "rules/abilities.h"
#include <algorithm>
#include "rules/dice_roller.h"
#include "rules/formulas.h"
#include "rules/health.h"
#include "rules/initiative.h"
#include "rules/random_provider.h"
#include "rules/turn_state_machine.h"
#include "scene/state_machine.h"

static int g_failures = 0;
static int g_checks = 0;

#define REQUIRE(expr)                                                        \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(expr)) {                                                       \
            ++g_failures;                                                    \
            std::printf("  FAIL: %s (%s:%d)\n", #expr, __FILE__, __LINE__);  \
        }                                                                    \
    } while (0)

static void test_ability_modifier_table() {
    std::printf("TEST_CASE: Ability modifier table (п. Г.12)\n");
    using rules::ability_modifier;
    REQUIRE(ability_modifier(8) == -1);
    REQUIRE(ability_modifier(10) == 0);
    REQUIRE(ability_modifier(16) == 3);
    REQUIRE(ability_modifier(20) == 5);
    REQUIRE(ability_modifier(9) == -1); // floor, а не округление к нулю
}

static void test_proficiency_and_formulas() {
    std::printf("TEST_CASE: formulas (п. Г.13)\n");
    REQUIRE(rules::proficiency_bonus(1) == 2);
    REQUIRE(rules::proficiency_bonus(5) == 3);
    REQUIRE(rules::proficiency_bonus(20) == 6);
    REQUIRE(rules::spell_save_dc(3, 2) == 13);
    REQUIRE(rules::passive_check(3, 2) == 15);
    REQUIRE(rules::feet_to_cells(30) == 6);
    REQUIRE(rules::resistant_damage(7) == 3);
    REQUIRE(rules::vulnerable_damage(7) == 14);
}

static void test_dice_roller_rules() {
    std::printf("TEST_CASE: DiceRoller жёсткие правила (п. 4.1.11.2)\n");
    rules::SeededRandomProvider rng(42);
    rules::DiceRoller roller(rng);

    auto adv = roller.roll(15, 5, true, false);
    REQUIRE(adv.dice.size() == 2);
    REQUIRE(adv.used_advantage);
    REQUIRE(adv.taken == std::max(adv.dice[0], adv.dice[1]));
    REQUIRE(adv.total == adv.taken + 5);
    REQUIRE(!adv.breakdown.empty()); // breakdown для UI (UI-E03)

    auto dis = roller.roll(15, 0, false, true);
    REQUIRE(dis.taken == std::min(dis.dice[0], dis.dice[1]));

    auto both = roller.roll(15, 0, true, true); // adv + disp → cancelled
    REQUIRE(both.cancelled);
    REQUIRE(both.dice.size() == 1);
    REQUIRE(!both.used_advantage && !both.used_disadvantage);

    // детерминизм: одинаковый seed → одинаковый результат (NFR-E09)
    rules::SeededRandomProvider rng2(42);
    rules::DiceRoller roller2(rng2);
    auto adv2 = roller2.roll(15, 5, true, false);
    REQUIRE(adv2.dice == adv.dice);
}

static void test_initiative_tie_breaker() {
    std::printf("TEST_CASE: tie-breaker инициативы (п. 4.1.11.3)\n");
    rules::InitiativeQueue q;
    q.build({{10, 15, 3}, {20, 15, 5}, {30, 15, 5}, {40, 12, 2}});
    REQUIRE(q.next().entity == 20); // больший DEX mod выше
    REQUIRE(q.next().entity == 30); // равный DEX → меньший EntityId
    REQUIRE(q.next().entity == 10);
    REQUIRE(q.next().entity == 40);
    REQUIRE(q.contains(10));
}

static void test_health_damage_order() {
    std::printf("TEST_CASE: порядок списания урона (п. 4.1.11.4)\n");
    rules::HealthState h{20, 10, 5};
    rules::apply_damage(h, 3); // temp_hp 5→2, hp_current без изменений
    REQUIRE(h.temp_hp == 2);
    REQUIRE(h.hp_current == 10);
    rules::apply_damage(h, 8); // temp 2→0, hp 10→4
    REQUIRE(h.temp_hp == 0);
    REQUIRE(h.hp_current == 4);
    rules::heal(h, 100);
    REQUIRE(h.hp_current == 20);
    REQUIRE(h.temp_hp == 0); // лечение временные хиты не восстанавливает
}

static void test_turn_resources_atomic() {
    std::printf("TEST_CASE: атомарность траты ресурсов (п. 4.1.7.3)\n");
    rules::TurnResources r;
    rules::reset_turn(r, 30);
    rules::CostCandidate ok{true, false, 10, false};
    REQUIRE(rules::spend(r, ok));
    REQUIRE(r.action_used);
    REQUIRE(r.movement_remaining_feet == 20);

    rules::CostCandidate too_much{true, false, 0, false}; // действие уже использовано
    const auto before = r;
    REQUIRE(!rules::spend(r, too_much)); // ничего не списано
    REQUIRE(r.action_used == before.action_used);
    REQUIRE(r.movement_remaining_feet == before.movement_remaining_feet);
}

static void test_fsm_cancel_states() {
    std::printf("TEST_CASE: FSM хода — отмена из Targeting/Confirming (п. 4.1.7.2)\n");
    engine::scene::StateMachine fsm(0); // 0=WaitingForAction,1=Targeting,2=Confirming,
                                         // 3=SpendingResources,4=Resolving,5=EndTurn
    fsm.add_transition(0, 1, "select_target");
    fsm.add_transition(1, 2, "confirm_start");
    fsm.add_transition(2, 3, "confirm");
    fsm.add_transition(1, 0, "cancel");
    fsm.add_transition(2, 0, "cancel");
    fsm.add_transition(3, 4, "resolved"); // cancel из SpendingResources — нет перехода
    REQUIRE(fsm.dispatch("select_target"));
    REQUIRE(fsm.dispatch("cancel"));
    REQUIRE(fsm.current() == 0);
    REQUIRE(fsm.dispatch("select_target"));
    REQUIRE(fsm.dispatch("confirm_start"));
    REQUIRE(fsm.dispatch("cancel"));
    REQUIRE(fsm.current() == 0);
    REQUIRE(!fsm.dispatch("nonexistent_trigger"));
}

struct Ev { int v; };

static void test_event_bus() {
    std::printf("TEST_CASE: EventBus subscribe/publish/unsubscribe (FR-E10)\n");
    engine::core::EventBus bus;
    int sum = 0;
    auto id = bus.subscribe<Ev>([&](const Ev& e) { sum += e.v; });
    bus.publish(Ev{2});
    bus.publish(Ev{3});
    REQUIRE(sum == 5);
    bus.unsubscribe<Ev>(id);
    bus.publish(Ev{100});
    REQUIRE(sum == 5);
}

struct Pos { float x, y; };
struct Vel { float vx, vy; };

static void test_ecs_lifecycle() {
    std::printf("TEST_CASE: ECS-lite create/components/destroy (FR-E55…FR-E60)\n");
    engine::ecs::Registry reg;
    auto a = reg.create_entity();
    auto b = reg.create_entity();
    reg.add_component<Pos>(a, 1.f, 2.f);
    reg.add_component<Vel>(a, 0.5f, 0.f);
    reg.add_component<Pos>(b, 0.f, 0.f);

    auto view = engine::ecs::make_view<Pos, Vel>(reg);
    REQUIRE(view.size() == 1);
    REQUIRE(reg.get_component<Pos>(a).x == 1.f);

    reg.destroy_entity(a);
    REQUIRE(!reg.is_alive(a));
    REQUIRE((engine::ecs::make_view<Pos, Vel>(reg)).empty());
    auto c = reg.create_entity(); // переиспользование индекса с новым поколением
    REQUIRE(c.index == a.index);
    REQUIRE(c.generation == a.generation + 1);
    REQUIRE(!reg.has_component<Pos>(c));
}

static void test_main_loop_fixed_step() {
    std::printf("TEST_CASE: главный цикл, фиксированный шаг, лимит 5 шагов\n");
    engine::platform::MainLoop loop;
    int updates = 0;
    double last_alpha = -1.0;
    loop.set_update([&](double dt) {
        REQUIRE(std::abs(dt - engine::platform::MainLoop::kDtLogic) < 1e-12);
        ++updates;
    });
    loop.set_render([&](double alpha) { last_alpha = alpha; });

    loop.tick(1.0 / 30.0); // ровно 2 шага логики за кадр
    REQUIRE(updates == 2);
    REQUIRE(last_alpha >= 0.0 && last_alpha <= 1.0);

    updates = 0;
    loop.tick(1.0); // просадка: 60 шагов накопилось → выполнится не более 5 (п. 4.1.2.3)
    REQUIRE(updates == engine::platform::MainLoop::kMaxStepsPerFrame);
}

static void test_file_system_virtual_paths() {
    std::printf("TEST_CASE: виртуальные пути assets:/ (FR-E07, DATA-E05)\n");
    engine::core::FileSystem fs("assets");
    REQUIRE(fs.resolve("assets:/data/items/longsword.json").is_ok());
    REQUIRE(!fs.resolve("/etc/passwd").is_ok());       // не виртуальный путь
    REQUIRE(!fs.resolve("assets:/../secret").is_ok()); // traversal запрещён
    REQUIRE(!fs.exists("assets:/no_such_file.json"));
}

static void test_config_degradation() {
    std::printf("TEST_CASE: конфигурация — деградация при ошибках (п. 4.2.3)\n");
    engine::core::Config cfg;
    REQUIRE(cfg.load_from_string("logic_hz = 60\nfullscreen = true\n# comment\n").is_ok());
    REQUIRE(cfg.get_int("logic_hz", 30) == 60);
    REQUIRE(cfg.get_bool("fullscreen", false) == true);
    REQUIRE(cfg.get_int("missing_key", 7) == 7); // отсутствие значения — default
    REQUIRE(!cfg.load_from_string("broken line without equals").is_ok());
}

int main() {
    test_ability_modifier_table();
    test_proficiency_and_formulas();
    test_dice_roller_rules();
    test_initiative_tie_breaker();
    test_health_damage_order();
    test_turn_resources_atomic();
    test_fsm_cancel_states();
    test_event_bus();
    test_ecs_lifecycle();
    test_main_loop_fixed_step();
    test_file_system_virtual_paths();
    test_config_degradation();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
