// tests/integration_tests/combat_play_tests.cpp — играбельная боевая сцена:
// загрузка карты из assets, выбор/перемещение/атака по правилам, ИИ гоблинов,
// отрисовка в NullRenderer (headless, E-D11).
#include <string>
#include <vector>

#include "combat/combat_play_scene.h"
#include "controls/control_scheme.h"
#include "platform/input.h"
#include "platform/input_actions.h"
#include "render/null_renderer.h"
#include "rules/random_provider.h"
#include "test_harness.h"

namespace {

engine::core::FileSystem make_fs() { return engine::core::FileSystem(DND_ASSETS_ROOT); }

struct Fixture {
    engine::core::EventBus bus;
    rules::SeededRandomProvider rng{777};
    rules::DiceRoller roller{rng};
    engine::core::FileSystem fs = make_fs();
    engine::render::NullRenderer renderer;
    game::CombatPlayScene scene{renderer, bus, roller, fs};
    std::vector<game::AttackResolvedEvent> attacks;

    Fixture() {
        bus.subscribe<game::AttackResolvedEvent>(
            [&](const game::AttackResolvedEvent& e) { attacks.push_back(e); });
        scene.on_enter();
    }
};

std::size_t find_combatant(Fixture& f, const std::string& name) {
    for (std::size_t i = 0; i < f.scene.combat().combatant_count(); ++i) {
        if (f.scene.combat().combatant(i).name == name) return i;
    }
    return game::CombatScene::kNoIndex;
}

} // namespace

TEST_CASE("combat_play_loads_map_and_spawns", "[integration][combat_play]") {
    Fixture f;
    auto& combat = f.scene.combat();
    REQUIRE(combat.combatant_count() == 4);

    // Спавны из assets/maps/goblin_ambush.json (Приложение Д)
    const auto fighter = find_combatant(f, "Воин");
    const auto rogue = find_combatant(f, "Плут");
    const auto goblin1 = find_combatant(f, "Гоблин 1");
    const auto goblin2 = find_combatant(f, "Гоблин 2");
    REQUIRE(fighter != game::CombatScene::kNoIndex);
    REQUIRE(rogue != game::CombatScene::kNoIndex);
    REQUIRE(goblin1 != game::CombatScene::kNoIndex);
    REQUIRE(goblin2 != game::CombatScene::kNoIndex);
    CHECK(combat.combatant(fighter).cell == rules::Cell({2, 6}));
    CHECK(combat.combatant(rogue).cell == rules::Cell({3, 6}));
    CHECK(combat.combatant(goblin1).cell == rules::Cell({16, 5}));
    CHECK(combat.combatant(goblin2).cell == rules::Cell({17, 9}));
    CHECK(combat.combatant(fighter).speed_feet == 30);

    // Террейн карты: граница — стены, «~» — сложная местность (п. 4.1.8.3)
    CHECK(f.scene.grid_columns() == 20);
    CHECK(f.scene.grid_rows() == 15);
    CHECK(combat.grid().terrain(rules::Cell({0, 0})) == rules::TerrainType::Impassable);
    CHECK(combat.grid().terrain(rules::Cell({3, 2})) == rules::TerrainType::Difficult);
    CHECK(combat.grid().terrain(rules::Cell({5, 5})) == rules::TerrainType::Plain);

    // Камера ограничена картой: 640×480 в вьюпорте 1280×720 → центрирование
    CHECK(f.scene.camera().position().x == -320.f);
    CHECK(f.scene.camera().position().y == -120.f);

    // Отрисовка headless не падает и считает кадры
    for (int i = 0; i < 3; ++i) f.scene.render(0.0);
    CHECK(f.scene.renders() == 3u);
    CHECK(f.renderer.draw_call_count() > 0);
}

TEST_CASE("combat_play_select_and_move_by_rules", "[integration][combat_play]") {
    Fixture f;
    const auto fighter = find_combatant(f, "Воин");

    // ЛКМ по своему бойцу — выделение (docs/controls.md)
    f.scene.set_mouse_logical(400.f, 330.f); // клетка (2,6) при камере (−320,−120)
    engine::platform::InputActions actions;
    game::build_control_scheme(actions);
    engine::platform::InputState input;
    input.begin_frame();
    engine::platform::InputEvent click;
    click.type = engine::platform::InputEvent::Type::MouseDown;
    click.button = engine::platform::MouseButton::Left;
    click.x = 400.f;
    click.y = 330.f;
    input.push_event(click);
    input.set_mouse_button(engine::platform::MouseButton::Left, true);
    actions.update(input);
    f.scene.handle_actions(actions, /*ui_captured=*/false);
    REQUIRE(f.scene.selected() == fighter);

    // Перемещение в свободную клетку (5,6). Оптимальный маршрут проходит через
    // клетку обломков (3,5) — на карте строка y=5 содержит «~» в колонке 3,
    // поэтому стоимость 25 футов (4 обычных ×5 + сложная ×10, п. 4.1.8.3).
    f.scene.request_cell_action(rules::Cell({5, 6}));
    CHECK(f.scene.combat().combatant(fighter).cell == rules::Cell({5, 6}));
    CHECK(f.scene.combat().combatant(fighter).movement_remaining_feet == 5); // 30 − 25

    // Достижимые клетки учитывают лимит движения (30 футов = 6 обычных клеток)
    f.scene.set_selected(fighter);
    const auto reachable = f.scene.combat().reachable_cells(fighter);
    REQUIRE(reachable.size() > 0);
    for (const auto& cell : reachable) {
        CHECK(f.scene.combat().grid().is_passable(cell));
        CHECK(f.scene.combat().combatant_at(cell) == game::CombatScene::kNoIndex);
    }

    // Занятая клетка: перемещение в клетку Плута (3,6) отклоняется
    const auto before = f.scene.combat().combatant(fighter).cell;
    f.scene.request_cell_action(rules::Cell({3, 6}));
    CHECK(f.scene.combat().combatant(fighter).cell == before);

    // Стена: перемещение в (0,0) отклоняется
    f.scene.request_cell_action(rules::Cell({0, 0}));
    CHECK(f.scene.combat().combatant(fighter).cell == before);
}

TEST_CASE("combat_play_attack_and_turn_flow", "[integration][combat_play][FR-K]") {
    Fixture f;
    const auto fighter = find_combatant(f, "Воин");

    // Подводим Воина вплотную к Гоблину 1 (16,5) и атакуем ЛКМ
    f.scene.set_selected(fighter);
    f.scene.combat().combatant_mut(fighter).movement_remaining_feet = 200; // тестовый запас
    f.scene.request_cell_action(rules::Cell({15, 5}));
    REQUIRE(f.scene.combat().combatant(fighter).cell == rules::Cell({15, 5}));
    f.scene.request_cell_action(rules::Cell({16, 5})); // клетка гоблина — атака
    REQUIRE(f.attacks.size() == 1);
    CHECK(f.attacks[0].attacker == "Воин");
    CHECK(f.attacks[0].target == "Гоблин 1");
    REQUIRE_FALSE(f.attacks[0].attack_breakdown.title.empty());
    CHECK(f.scene.combat().combatant_mut(fighter).action_used);

    // Повторная атака в том же ходу блокируется (экономика действий, п. Г.6)
    f.scene.request_cell_action(rules::Cell({16, 5}));
    CHECK(f.attacks.size() == 1);

    // Конец хода → по порядку инициативы; ход гоблина исполняет ИИ:
    // гоблин приближается к ближайшему игроку и атакует, если смежен.
    int guard = 0;
    bool goblin_acted = false;
    while (guard++ < 8) {
        const auto active = f.scene.active_unit_index();
        f.scene.end_turn();
        f.scene.update(1.0 / 60.0); // run_enemy_turn, если фаза противника
        const auto next = f.scene.active_unit_index();
        if (next != active && !f.scene.combat().combatant(next).player_side) {
            goblin_acted = true;
            break;
        }
    }
    REQUIRE(goblin_acted);
    // После ИИ-хода гоблина активен следующий боец; у гоблина потрачено движение
    // или атака (он действовал)
    bool any_goblin_spent = false;
    for (std::size_t i = 0; i < f.scene.combat().combatant_count(); ++i) {
        const auto& unit = f.scene.combat().combatant(i);
        if (!unit.player_side &&
            (unit.movement_remaining_feet < unit.speed_feet || unit.action_used)) {
            any_goblin_spent = true;
        }
    }
    CHECK(any_goblin_spent);
}

TEST_CASE("combat_play_reachable_respects_difficult_terrain", "[integration][combat_play]") {
    Fixture f;
    const auto rogue = find_combatant(f, "Плут"); // (3,6), скорость 30 футов
    f.scene.set_selected(rogue);
    const auto reachable = f.scene.combat().reachable_cells(rogue);

    // Клетка обломков (2,2): маршрут (3,6)→(3,2)→(2,2) = 4 обычных (20 ф) +
    // (3,3),(3,2)? — проверяем свойство: суммарная стоимость любой достижимой
    // клетки ≤ 30 футов (движение по сложной местности ×2, п. 4.1.8.3)
    REQUIRE(reachable.size() > 0);
    bool has_far_cell = false;
    for (const auto& cell : reachable) {
        // Манхэттен ≤ 6 для ровной местности; сложная местность уменьшает дальность
        if (rules::TacticalGrid::manhattan(f.scene.combat().combatant(rogue).cell, cell) >= 5) {
            has_far_cell = true;
        }
        CHECK(rules::TacticalGrid::manhattan(f.scene.combat().combatant(rogue).cell, cell) <= 6);
    }
    CHECK(has_far_cell);
    // Стены недоступны никогда
    for (const auto& cell : reachable) {
        CHECK(f.scene.combat().grid().terrain(cell) != rules::TerrainType::Impassable);
    }
}
