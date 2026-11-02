// tests/controls_tests/controls_tests.cpp — испытания схемы управления
// (docs/controls.md): маппинг действий, камера WASD + зум, выбор клетки мышью.
#include <cmath>
#include <initializer_list>
#include <string>

#include "controls/camera_controller.h"
#include "controls/cell_picking.h"
#include "controls/control_scheme.h"
#include "platform/input.h"
#include "platform/input_actions.h"
#include "render/camera.h"
#include "rules/grid.h"
#include "test_harness.h"

using engine::platform::InputActions;
using engine::platform::InputEvent;
using engine::platform::InputState;
using engine::platform::Key;
using engine::platform::MouseButton;

namespace {

// Кадр ввода: события + удерживаемые клавиши, begin/end frame как в главном цикле
InputState make_frame(std::initializer_list<Key> held_keys,
                      std::initializer_list<InputEvent> events = {}) {
    InputState state;
    state.begin_frame();
    for (const auto& e : events) state.push_event(e);
    for (auto k : held_keys) state.set_key(k, true);
    return state;
}

InputEvent key_down(Key k) {
    InputEvent e;
    e.type = InputEvent::Type::KeyDown;
    e.key = k;
    return e;
}

InputEvent mouse_down(MouseButton b) {
    InputEvent e;
    e.type = InputEvent::Type::MouseDown;
    e.button = b;
    return e;
}

InputEvent wheel(int delta) {
    InputEvent e;
    e.type = InputEvent::Type::MouseWheel;
    e.wheel_delta = delta;
    return e;
}

// Сравнение float с допуском (харнесс не предоставляет Approx)
bool doctest_approx(float value, float expected, float eps = 1e-3f) {
    return std::fabs(value - expected) <= eps;
}

} // namespace

TEST_CASE("control_scheme_matches_controls_doc", "[game][controls]") {
    InputActions actions;
    game::build_control_scheme(actions);

    // WASD + стрелки — камера (docs/controls.md)
    {
        auto state = make_frame({Key::W}, {key_down(Key::W)});
        actions.update(state);
        REQUIRE(actions.held(game::actions::kCameraUp));
        REQUIRE(actions.pressed(game::actions::kCameraUp));
        CHECK(!(actions.held(game::actions::kCameraDown)));
    }
    {
        auto state = make_frame({Key::Left}, {key_down(Key::Left)});
        actions.update(state);
        CHECK(actions.held(game::actions::kCameraLeft));
    }
    // E — конец хода, Q — отмена, Space — подтвердить, Tab — следующий боец
    {
        auto state = make_frame({}, {key_down(Key::E)});
        actions.update(state);
        REQUIRE(actions.pressed(game::actions::kEndTurn));
    }
    {
        auto state = make_frame({}, {key_down(Key::Q)});
        actions.update(state);
        CHECK(actions.pressed(game::actions::kCancel));
    }
    {
        auto state = make_frame({}, {key_down(Key::Space)});
        actions.update(state);
        CHECK(actions.pressed(game::actions::kConfirm));
    }
    {
        auto state = make_frame({}, {key_down(Key::Tab)});
        actions.update(state);
        CHECK(actions.pressed(game::actions::kNextUnit));
    }
    // Цифры 1…4 — слоты бойцов; C/I/L/M — панели
    {
        auto state = make_frame({}, {key_down(Key::Digit3)});
        actions.update(state);
        REQUIRE(actions.pressed(game::actions::kSelectSlot3));
        CHECK(!(actions.pressed(game::actions::kSelectSlot1)));
    }
    {
        auto state = make_frame({}, {key_down(Key::C), key_down(Key::I), key_down(Key::L),
                                     key_down(Key::M)});
        actions.update(state);
        CHECK(actions.pressed(game::actions::kCharacterSheet));
        CHECK(actions.pressed(game::actions::kInventory));
        CHECK(actions.pressed(game::actions::kCombatLog));
        CHECK(actions.pressed(game::actions::kMap));
    }
}

TEST_CASE("input_actions_mouse_and_wheel", "[game][controls]") {
    InputActions actions;
    game::build_control_scheme(actions);

    // ЛКМ — select/command
    {
        auto state = make_frame({}, {mouse_down(MouseButton::Left)});
        state.set_mouse_button(MouseButton::Left, true);
        actions.update(state);
        REQUIRE(actions.pressed(game::actions::kSelect));
        REQUIRE(actions.pressed(game::actions::kCommand));
        CHECK(actions.held(game::actions::kSelect));
    }
    // ПКМ — cancel
    {
        auto state = make_frame({}, {mouse_down(MouseButton::Right)});
        state.set_mouse_button(MouseButton::Right, true);
        actions.update(state);
        CHECK(actions.pressed(game::actions::kCancel));
    }
    // Колесо: знак дельты → zoom_in / zoom_out
    {
        auto state = make_frame({}, {wheel(+1)});
        actions.update(state);
        REQUIRE(actions.wheel(game::actions::kZoomIn));
        CHECK(!(actions.wheel(game::actions::kZoomOut)));
    }
    {
        auto state = make_frame({}, {wheel(-2)});
        actions.update(state);
        CHECK(actions.wheel(game::actions::kZoomOut));
    }
    // Пустой кадр: pressed/wheel не залипают
    {
        auto state = make_frame({});
        actions.update(state);
        CHECK(!(actions.pressed(game::actions::kZoomIn)));
        CHECK(!(actions.wheel(game::actions::kZoomIn)));
    }
}

TEST_CASE("camera_controller_pans_with_wasd_and_clamps", "[game][controls]") {
    InputActions actions;
    game::build_control_scheme(actions);

    // Карта 20×15 по 32 px = 640×480 меньше вьюпорта 1280×720 → центрирование
    engine::render::Camera2D small_camera;
    game::CameraController small(small_camera, 640.f, 480.f, 1280.f, 720.f);
    small.set_pan_speed(600.f);
    {
        auto state = make_frame({Key::D}, {key_down(Key::D)});
        actions.update(state);
        small.update(actions, 0.5f);
    }
    CHECK((doctest_approx(small_camera.position().x, -320.f))); // (640 − 1280)/2
    CHECK((doctest_approx(small_camera.position().y, -120.f))); // (480 − 720)/2

    // Большая карта 4000×3000: панорамирование реально перемещает камеру
    engine::render::Camera2D camera;
    game::CameraController controller(camera, 4000.f, 3000.f, 1280.f, 720.f);
    controller.set_pan_speed(600.f);
    {
        auto state = make_frame({Key::D}, {key_down(Key::D)});
        actions.update(state);
        controller.update(actions, 0.5f); // +300 px по x
        CHECK((doctest_approx(camera.position().x, 300.f)));
    }
    {
        auto state = make_frame({Key::S}, {key_down(Key::S)});
        actions.update(state);
        controller.update(actions, 0.5f); // S — вниз: +300 px по y
        CHECK((doctest_approx(camera.position().y, 300.f)));
    }
    {
        auto state = make_frame({Key::W}, {key_down(Key::W)});
        actions.update(state);
        controller.update(actions, 1.0f); // W — вверх: −600 px по y…
        // …но кламп карты не выпускает камеру за верхнюю границу: y ≥ 0
        CHECK((doctest_approx(camera.position().y, 0.f)));
    }
    // Кламп границ: уезжаем вправо до упора
    for (int i = 0; i < 40; ++i) {
        auto state = make_frame({Key::D}, {key_down(Key::D)});
        actions.update(state);
        controller.update(actions, 1.0f);
    }
    CHECK((doctest_approx(camera.position().x, 4000.f - 1280.f)));
    // И вверх до упора: y не меньше 0
    for (int i = 0; i < 40; ++i) {
        auto state = make_frame({Key::W}, {key_down(Key::W)});
        actions.update(state);
        controller.update(actions, 1.0f);
    }
    CHECK((doctest_approx(camera.position().y, 0.f)));
}

TEST_CASE("camera_controller_zoom_limits", "[game][controls]") {
    engine::render::Camera2D camera;
    game::CameraController controller(camera, 4000.f, 3000.f, 1280.f, 720.f);
    InputActions actions;
    game::build_control_scheme(actions);

    for (int i = 0; i < 30; ++i) {
        auto state = make_frame({}, {wheel(+1)});
        actions.update(state);
        controller.update(actions, 0.016f);
    }
    CHECK((doctest_approx(camera.zoom(), 4.0f))); // предел Camera2D (FR-E35)

    for (int i = 0; i < 60; ++i) {
        auto state = make_frame({}, {wheel(-1)});
        actions.update(state);
        controller.update(actions, 0.016f);
    }
    CHECK((doctest_approx(camera.zoom(), 0.5f)));
}

TEST_CASE("cell_picking_maps_mouse_to_grid_cell", "[game][controls]") {
    rules::TacticalGrid grid(20, 15);
    engine::render::Camera2D camera;

    // Камера в нуле, zoom 1: курсор (100, 70) → клетка (3, 2)
    auto pick = game::pick_cell(camera, 100.f, 70.f, grid, 32.f);
    REQUIRE(pick.has_value());
    CHECK(pick->cell.x == 3);
    CHECK(pick->cell.y == 2);

    // Zoom 2: экранные (100, 70) → мировые (50, 35) → клетка (1, 1)
    camera.set_zoom(2.f);
    pick = game::pick_cell(camera, 100.f, 70.f, grid, 32.f);
    REQUIRE(pick.has_value());
    CHECK(pick->cell.x == 1);
    CHECK(pick->cell.y == 1);

    // Смещение камеры: world = screen/zoom + position
    camera.set_zoom(1.f);
    camera.set_position(64.f, 32.f);
    pick = game::pick_cell(camera, 100.f, 70.f, grid, 32.f);
    REQUIRE(pick.has_value());
    CHECK(pick->cell.x == 5); // (100+64)/32 = 5.125
    CHECK(pick->cell.y == 3); // (70+32)/32 = 3.1875

    // За пределами карты — пустой результат
    camera.set_position(0.f, 0.f);
    pick = game::pick_cell(camera, 700.f, 700.f, grid, 32.f);
    CHECK(!(pick.has_value()));
    pick = game::pick_cell(camera, -10.f, 5.f, grid, 32.f);
    CHECK(!(pick.has_value()));
}
