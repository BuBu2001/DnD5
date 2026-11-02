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
#include "ecs/command_buffer.h"
#include "ecs/registry.h"
#include "ecs/view.h"
#include "platform/main_loop.h"
#include "render/camera.h"
#include "render/null_renderer.h"
#include "render/sprite_batch.h"
#include "rules/abilities.h"
#include "rules/attack.h"
#include "rules/grid.h"
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

// --- V0.3: новые модули (см. CHANGELOG.md) -----------------------------------

struct Pos { float x, y; };
struct Vel { float vx, vy; };

static void test_attack_resolution() {
    std::printf("TEST_CASE: разрешение атаки rules/attack.h (п. 4.1.11.2)\n");
    // Фиксированный провайдер: всегда d20 = value (тестовый двойник, E-D07)
    struct FixedRng : rules::IRandomProvider {
        int value;
        std::uint32_t next_u32() override { return static_cast<std::uint32_t>(value); }
        int roll_d20() override { return value; }
    } rng{15};
    rules::DiceRoller roller(rng);

    rules::AttackInput in;
    in.ability_modifier = 3;
    in.proficiency_bonus = 2;

    rng.value = 15; // 15 + 5 = 20 >= AC 18 → попадание
    auto hit = rules::resolve_attack(roller, in, /*ac*/ 18);
    REQUIRE(hit.hit);
    REQUIRE(!hit.critical);
    REQUIRE(hit.roll.total == 20);

    rng.value = 20; // natural 20 — автопопадание даже против AC 30
    auto crit = rules::resolve_attack(roller, in, 30);
    REQUIRE(crit.critical);
    REQUIRE(crit.hit);

    rng.value = 1; // natural 1 — автопромах, несмотря на высокий итог
    auto fumble = rules::resolve_attack(roller, in, 5);
    REQUIRE(fumble.fumble);
    REQUIRE(!fumble.hit);

    REQUIRE(!hit.description.empty()); // описание для журнала (FR-E08)
}

static void test_tactical_grid() {
    std::printf("TEST_CASE: тактическая сетка rules/grid.h (п. 4.1.8)\n");
    rules::TacticalGrid grid(10, 10);
    REQUIRE(grid.is_passable({5, 5}));
    REQUIRE(grid.entry_cost_feet({5, 5}) == 5); // обычная местность: клетка = 5 футов

    grid.set_terrain({3, 0}, rules::TerrainType::Difficult);
    REQUIRE(grid.entry_cost_feet({3, 0}) == 10); // сложная местность ×2 (п. 4.1.8.3)

    grid.set_terrain({5, 0}, rules::TerrainType::Impassable);
    REQUIRE(!grid.is_passable({5, 0}));
    REQUIRE(!grid.in_bounds({10, 0}));
    REQUIRE(!grid.path_cost_feet_straight({0, 0}, {10, 10}).has_value()); // вне границ

    // Обход стены: стоимость прямого пути через непроходимую клетку велика,
    // ортогональный маршрут вдоль рядов — проходит
    const auto cost = grid.path_cost_feet_straight({0, 0}, {4, 0});
    REQUIRE(cost.has_value());
    REQUIRE(*cost == 25 + 5); // клетки (1..4,0): три обычные (15) + сложная (10)
    REQUIRE(rules::movement_range_cells(30) == 6);
    REQUIRE(rules::TacticalGrid::manhattan({0, 0}, {2, 3}) == 5);
}

static void test_camera_transforms() {
    std::printf("TEST_CASE: камера engine/render/camera.h (FR-E30…FR-E39)\n");
    engine::render::Camera2D cam;
    cam.set_position(100.f, 50.f);
    cam.set_zoom(2.f);

    const auto s = cam.world_to_screen({150.f, 75.f});
    REQUIRE(std::abs(s.x - 100.f) < 1e-5f); // (150-100)*2
    REQUIRE(std::abs(s.y - 50.f) < 1e-5f);  // (75-50)*2

    const auto w = cam.screen_to_world(s);
    REQUIRE(std::abs(w.x - 150.f) < 1e-5f); // обратное преобразование точно
    REQUIRE(std::abs(w.y - 75.f) < 1e-5f);

    cam.set_zoom(10.f); // зум ограничен сверху (FR-E35)
    REQUIRE(cam.zoom() == 4.f);
    cam.set_zoom(0.1f); // и снизу
    REQUIRE(cam.zoom() == 0.5f);

    REQUIRE(cam.is_visible({100.f, 50.f, 32.f, 32.f}, 1280.f, 720.f));
    REQUIRE(!cam.is_visible({5000.f, 5000.f, 32.f, 32.f}, 1280.f, 720.f));
}

static void test_sprite_batch_grouping() {
    std::printf("TEST_CASE: SpriteBatch — группировка по текстуре (E-D05)\n");
    engine::render::NullRenderer renderer;
    engine::render::SpriteBatch batch(renderer);
    batch.begin();
    batch.draw(1, {0, 0, 32, 32}, {0, 0, 32, 32});
    batch.draw(1, {0, 0, 32, 32}, {40, 0, 32, 32});
    batch.draw(2, {0, 0, 16, 16}, {80, 0, 16, 16});
    batch.draw(1, {0, 0, 32, 32}, {120, 0, 32, 32}); // смена текстуры → новый пакет
    batch.end();
    REQUIRE(batch.pending_batches() == 4); // t1,t1 | t2 | t1 → 3 группы+смена kInvalid
    const std::size_t draw_calls = batch.flush();
    REQUIRE(draw_calls == 3);              // три пакета по текстурам
    REQUIRE(renderer.sprites_drawn() == 4);
    REQUIRE(renderer.flushes() == 3);      // flush один раз на пакет, не на спрайт
}

static void test_command_buffer() {
    std::printf("TEST_CASE: CommandBuffer ECS — отложенные команды (FR-E57)\n");
    engine::ecs::Registry reg;
    engine::ecs::CommandBuffer cmds(reg);

    engine::ecs::EntityId e{};
    cmds.create_entity(&e);
    cmds.add_component<Pos>(e, 7.f, 9.f); // e ещё kNullEntity — команда исполнится позже
    REQUIRE(e == engine::ecs::kNullEntity);
    REQUIRE(cmds.pending() == 2);

    cmds.flush();
    REQUIRE(e != engine::ecs::kNullEntity);
    REQUIRE(reg.is_alive(e));
    // Компонент добавлен к id, действительному на момент flush:
    auto real = e;
    reg.add_component<Pos>(real, 1.f, 2.f);
    REQUIRE(reg.get_component<Pos>(real).x == 1.f);

    cmds.destroy_entity(real);
    REQUIRE(reg.is_alive(real)); // до flush сущность жива
    cmds.flush();
    REQUIRE(!reg.is_alive(real));
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

// Интеграционный тест: боевая сцена связывает engine + rules (вертикальный срез).
#include "combat_scene.h"

static void test_combat_scene_integration() {
    std::printf("TEST_CASE: CombatScene — интеграция engine+rules (п. 3.3.2)\n");
    engine::core::EventBus bus;
    rules::SeededRandomProvider rng(7);
    rules::DiceRoller roller(rng);

    game::CombatScene scene(bus, roller);
    scene.add_combatant("Warrior", true, 22, 5, 1, 18, 2, 3);
    scene.add_combatant("Goblin", false, 9, 4, 2, 13, 6, 3);

    int events_seen = 0;
    auto sub = bus.subscribe<game::AttackResolvedEvent>(
        [&](const game::AttackResolvedEvent&) { ++events_seen; });

    scene.on_enter();
    REQUIRE(scene.registry().is_alive(scene.combatant(0).entity));
    REQUIRE((engine::ecs::make_view<game::GridPosition, game::CombatantTag>(
                 scene.registry()).size() == 2));

    const auto ev = scene.perform_attack(0, 1); // guaranteed hit: mod=5 vs AC=13
    REQUIRE(ev.hit);
    REQUIRE(events_seen == 1);
    REQUIRE(scene.combatant(1).health.hp_current <= scene.combatant(1).health.hp_max);
    REQUIRE(!scene.combat_log().empty());

    bus.unsubscribe<game::AttackResolvedEvent>(sub);
    scene.update(engine::platform::MainLoop::kDtLogic); // flush отложенных команд
}

int main() {
    test_ability_modifier_table();
    test_proficiency_and_formulas();
    test_dice_roller_rules();
    test_initiative_tie_breaker();
    test_health_damage_order();
    test_turn_resources_atomic();
    test_fsm_cancel_states();
    test_attack_resolution();
    test_tactical_grid();
    test_camera_transforms();
    test_sprite_batch_grouping();
    test_command_buffer();
    test_event_bus();
    test_ecs_lifecycle();
    test_main_loop_fixed_step();
    test_file_system_virtual_paths();
    test_config_degradation();
    test_combat_scene_integration();

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
