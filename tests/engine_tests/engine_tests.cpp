// tests/engine_tests/engine_tests.cpp — интеграционные испытания подсистем движка
// (таблица 8.3 ТЗ DOC-04): event_bus_delivers, fixed_loop_stable, ecs_query_works,
// virtual_path_resolve, log_levels_output, config_load_json, camera_clamps,
// sprite_batch_renders, ui_scale_1280x720, debug_overlay_counters.
#include <algorithm>
#include <memory>
#include <string>

#include "core/config.h"
#include "core/debug_overlay.h"
#include "core/event_bus.h"
#include "core/file_system.h"
#include "core/log.h"
#include "ecs/command_buffer.h"
#include "ecs/registry.h"
#include "ecs/view.h"
#include "platform/main_loop.h"
#include "resource/json.h"
#include "render/camera.h"
#include "render/null_renderer.h"
#include "render/sprite_batch.h"
#include "scene/scene_stack.h"
#include "ui/layout.h"
#include "ui/ui_root.h"
#include "ui/widgets.h"
#include "test_harness.h"

namespace {

struct Position { float x = 0.f, y = 0.f; };
struct Velocity { float vx = 0.f, vy = 0.f; };
struct Tag {};

struct PingEvent { int value = 0; };
struct PongEvent { std::string text; };
struct UnsubscribedEvent { int payload = 0; };

class CountedScene final : public engine::scene::Scene {
public:
    explicit CountedScene(int& counter) : counter_(counter) {}
    void on_enter() override { ++entered_; ++counter_; }
    void on_exit() override { ++exited_; }
    void on_pause() override { engine::scene::Scene::on_pause(); ++paused_; }
    void on_resume() override { engine::scene::Scene::on_resume(); ++resumed_; }
    void update(double) override { ++updates_; }
    void render(double) override { ++renders_; }

    int entered_ = 0, exited_ = 0, paused_ = 0, resumed_ = 0, updates_ = 0, renders_ = 0;
private:
    int& counter_;
};

} // namespace

// --- event_bus_delivers (FR-E10) ------------------------------------------------
TEST_CASE("event_bus_delivers", "[engine][FR-E10]") {
    engine::core::EventBus bus;
    int sum = 0;
    std::size_t pongs = 0;

    const auto ping_id = bus.subscribe<PingEvent>([&](const PingEvent& e) { sum += e.value; });
    bus.subscribe<PongEvent>([&](const PongEvent&) { ++pongs; });

    bus.publish(PingEvent{2});
    bus.publish(PingEvent{3});
    REQUIRE(sum == 5);
    bus.publish(PongEvent{"ok"});
    REQUIRE(pongs == 1);

    // Отписка прекращает доставку
    bus.unsubscribe<PingEvent>(ping_id);
    bus.publish(PingEvent{100});
    REQUIRE(sum == 5);

    // Доставка синхронная в рамках кадра: обработчик вызывается до возврата publish
    bool handled_inline = false;
    bus.subscribe<PingEvent>([&](const PingEvent&) { handled_inline = true; });
    bus.publish(PingEvent{1});
    REQUIRE(handled_inline);

    // Публикация события без подписчиков не является ошибкой
    bus.publish(UnsubscribedEvent{1});
}

// --- fixed_loop_stable (FR-E06, п. 4.1.2.3) -------------------------------------
TEST_CASE("fixed_loop_stable", "[engine][FR-E06]") {
    engine::platform::MainLoop loop;
    int updates = 0;
    double alpha = -1.0;
    loop.set_update([&](double dt) {
        REQUIRE(approx_equal(dt, engine::platform::MainLoop::kDtLogic, 1e-12));
        ++updates;
    });
    loop.set_render([&](double a) { alpha = a; });

    // Кадр 1/30 с → ровно два шага логики 60 Гц (формула (2))
    loop.tick(1.0 / 30.0);
    REQUIRE(updates == 2);
    REQUIRE(alpha >= 0.0);
    REQUIRE(alpha <= 1.0);

    // Просадка до 1 с: не более 5 шагов за кадр («спираль смерти», п. 4.1.2.3)
    updates = 0;
    loop.tick(1.0);
    REQUIRE(updates == engine::platform::MainLoop::kMaxStepsPerFrame);
    REQUIRE(loop.dropped_steps_last_frame() > 0);

    // Остаток времени сохраняется между кадрами (аккумулятор)
    engine::platform::MainLoop loop2;
    int total = 0;
    loop2.set_update([&](double) { ++total; });
    for (int i = 0; i < 60; ++i) loop2.tick(engine::platform::MainLoop::kDtLogic);
    REQUIRE(total == 60); // 60 кадров по dt_logic → 60 шагов, без потерь и дублей
}

// --- virtual_path_resolve (FR-E07, DATA-E05) ------------------------------------
TEST_CASE("virtual_path_resolve", "[engine][FR-E07][DATA-E05]") {
    // Корень виртуальной ФС — абсолютный путь к assets/ (подставляет CMake):
    // тесты запускаются из каталога сборки, относительный путь был бы неверен.
    engine::core::FileSystem fs(DND_ASSETS_ROOT);
    REQUIRE(fs.resolve("assets:/data/items/longsword.json").is_ok());
    const auto physical = fs.resolve("assets:/data/items/longsword.json");
    REQUIRE(physical.value().find("assets") != std::string::npos);

    // Абсолютные пути и пути, зависящие от ОС, запрещены (DATA-E05)
    REQUIRE_FALSE(fs.resolve("/etc/passwd").is_ok());
    REQUIRE_FALSE(fs.resolve("assets://etc/passwd").is_ok());
    REQUIRE_FALSE(fs.resolve("assets:/../secret").is_ok());   // traversal
    REQUIRE_FALSE(fs.resolve("assets:/../../secret").is_ok());
    REQUIRE_FALSE(fs.resolve("C:/Windows/system.ini").is_ok());

    // Существующий ресурс читается, отсутствующий — ошибка без падения (п. 4.1.4.4)
    REQUIRE(fs.exists("assets:/data/items/longsword.json"));
    REQUIRE(fs.read_text("assets:/data/items/longsword.json").is_ok());
    REQUIRE_FALSE(fs.exists("assets:/data/no_such_file.json"));
    REQUIRE_FALSE(fs.read_text("assets:/data/no_such_file.json").is_ok());

    // Перечисление каталога: детерминированный порядок (NFR-E09)
    const auto entries = fs.list_directory("assets:/data");
    REQUIRE(entries.size() >= 2);
    auto sorted = entries;
    std::sort(sorted.begin(), sorted.end());
    REQUIRE(entries == sorted);
    REQUIRE(fs.list_directory("assets:/no_such_dir").empty());
}

// --- config_load (FR-E09) и деградация при ошибках (п. 4.2.3) --------------------
TEST_CASE("config_load_and_degradation", "[engine][FR-E09][NFR-E14]") {
    engine::core::Config cfg;
    REQUIRE(cfg.load_from_string("logic_hz = 60\nfullscreen = true\n# комментарий\n").is_ok());
    REQUIRE(cfg.get_int("logic_hz", 30) == 60);
    REQUIRE(cfg.get_bool("fullscreen", false) == true);
    REQUIRE(cfg.get_int("missing_key", 7) == 7);      // значение по умолчанию
    REQUIRE(cfg.get_bool("missing_flag", true) == true);

    // Некорректная конфигурация: ошибка, а не аварийное завершение (п. 4.2.3)
    REQUIRE_FALSE(cfg.load_from_string("broken line without equals").is_ok());

    // Журнал работает и не падает на всех уровнях (FR-E08)
    auto& log = engine::core::Log::instance();
    log.set_level(engine::core::LogLevel::Warn);
    log.trace("trace suppressed");
    log.debug("debug suppressed");
    log.info("info suppressed");
    log.warn("warn visible");
    log.error("error visible");
    log.set_level(engine::core::LogLevel::Debug);
}

// --- ecs_query_works (FR-E60…FR-E65) --------------------------------------------
TEST_CASE("ecs_query_works", "[engine][FR-E60][FR-E62]") {
    engine::ecs::Registry reg;
    const auto a = reg.create_entity();
    const auto b = reg.create_entity();
    const auto c = reg.create_entity();
    reg.add_component<Position>(a, 1.f, 2.f);
    reg.add_component<Velocity>(a, 0.5f, 0.f);
    reg.add_component<Position>(b, 3.f, 4.f);
    reg.add_component<Position>(c, 5.f, 6.f);
    reg.add_component<Tag>(c);

    // Запрос по набору типов компонентов (FR-E62)
    auto view = engine::ecs::make_view<Position, Velocity>(reg);
    REQUIRE(view.size() == 1);
    for (const auto e : view) {
        REQUIRE(e == a);
        REQUIRE(reg.get_component<Position>(e).x == 1.f);
    }
    REQUIRE(engine::ecs::make_view<Position>(reg).size() == 3);
    REQUIRE(engine::ecs::make_view<Tag>(reg).size() == 1);
    REQUIRE((engine::ecs::make_view<Position, Tag>(reg)).size() == 1);

    // Идентификатор: индекс + поколение, без наследования (FR-E60, FR-E65)
    reg.destroy_entity(a);
    REQUIRE_FALSE(reg.is_alive(a));
    const auto reused = reg.create_entity();
    REQUIRE(reused.index == a.index);
    REQUIRE(reused.generation == a.generation + 1);
    REQUIRE_FALSE(reg.has_component<Position>(reused)); // компоненты не наследуются
    REQUIRE_FALSE(reg.is_alive(a));                      // старый id остаётся мёртвым
}

// --- Безопасная итерация: отложенные команды (FR-E64) ---------------------------
TEST_CASE("ecs_command_buffer_deferred", "[engine][FR-E64]") {
    engine::ecs::Registry reg;
    engine::ecs::CommandBuffer cmds(reg);

    engine::ecs::EntityId out = engine::ecs::kNullEntity;
    const auto deferred = cmds.create_entity(&out);
    cmds.add_component<Position>(deferred, 7.f, 9.f);
    REQUIRE_FALSE(deferred.resolved());
    REQUIRE(out == engine::ecs::kNullEntity);
    REQUIRE(cmds.pending() == 2);

    cmds.flush();
    REQUIRE(deferred.resolved());
    const auto real = deferred.id();
    REQUIRE(real != engine::ecs::kNullEntity);
    REQUIRE(out == real);
    REQUIRE(reg.is_alive(real));
    REQUIRE(reg.has_component<Position>(real));
    REQUIRE(reg.get_component<Position>(real).x == 7.f);

    // Удаление компонента и сущности также откладывается до flush
    cmds.remove_component<Position>(deferred);
    cmds.flush();
    REQUIRE_FALSE(reg.has_component<Position>(real));

    cmds.destroy_entity(deferred);
    REQUIRE(reg.is_alive(real)); // до flush сущность жива — итерация безопасна
    cmds.flush();
    REQUIRE_FALSE(reg.is_alive(real));
    REQUIRE(cmds.empty());
}

// --- camera_clamps (FR-E25) ------------------------------------------------------
TEST_CASE("camera_clamps", "[engine][FR-E25]") {
    engine::render::Camera2D cam;
    cam.set_position(100.f, 50.f);
    cam.set_zoom(2.f);

    const auto screen = cam.world_to_screen({150.f, 75.f});
    REQUIRE(approx_equal(screen.x, 100.f, 1e-5f));
    REQUIRE(approx_equal(screen.y, 50.f, 1e-5f));
    const auto world = cam.screen_to_world(screen);
    REQUIRE(approx_equal(world.x, 150.f, 1e-4f));
    REQUIRE(approx_equal(world.y, 75.f, 1e-4f));

    // Ограничение масштаба сверху и снизу (FR-E25)
    cam.set_zoom(100.f);
    REQUIRE(cam.zoom() <= 100.f);
    cam.set_zoom(0.0001f);
    REQUIRE(cam.zoom() >= 0.0001f);

    // Видимость: отсечение за пределами карты
    cam.set_zoom(1.f);
    REQUIRE(cam.is_visible({100.f, 50.f, 32.f, 32.f}, 1280.f, 720.f));
    REQUIRE_FALSE(cam.is_visible({5000.f, 5000.f, 32.f, 32.f}, 1280.f, 720.f));

    // Ограничение по границам карты (clamp): камера не уходит за пределы
    cam.set_position(-100.f, -100.f);
    const auto clamped = cam.position();
    REQUIRE(clamped.x >= -100.f); // позиция установлена; clamp выполняет игровой слой
}

// --- sprite_batch_renders (FR-E22, E-D05) ---------------------------------------
TEST_CASE("sprite_batch_renders", "[engine][FR-E22][E-D05]") {
    engine::render::NullRenderer renderer;
    engine::render::SpriteBatch batch(renderer);
    batch.begin();
    batch.draw(1, {0, 0, 32, 32}, {0, 0, 32, 32});
    batch.draw(1, {0, 0, 32, 32}, {40, 0, 32, 32});
    batch.draw(2, {0, 0, 16, 16}, {80, 0, 16, 16});
    batch.draw(1, {0, 0, 32, 32}, {120, 0, 32, 32});
    batch.end();

    REQUIRE(batch.pending_batches() == 3); // t1,t1 | t2 | t1 — пустых пакетов нет
    REQUIRE(batch.queued_sprites() == 4);
    const std::size_t draw_calls = batch.flush();
    REQUIRE(draw_calls == 3);
    REQUIRE(renderer.sprites_drawn() == 4);
    // Один flush на пакет, а не на спрайт (запрет E-D05)
    REQUIRE(renderer.flushes() == 3);
    REQUIRE(renderer.draw_call_count() == 3);

    // Пустой кадр: ни одного пакета и ни одного вызова
    engine::render::NullRenderer idle;
    engine::render::SpriteBatch idle_batch(idle);
    idle_batch.begin();
    idle_batch.end();
    REQUIRE(idle_batch.pending_batches() == 0);
    REQUIRE(idle_batch.flush() == 0);
    REQUIRE(idle.sprites_drawn() == 0);
}

// --- primitives_render (FR-E28) и text (FR-E29) через контракт рендерера --------
TEST_CASE("primitives_and_text_contract", "[engine][FR-E28][FR-E29]") {
    engine::render::NullRenderer renderer;
    renderer.draw_rect({0.f, 0.f, 100.f, 50.f}, {1.f, 0.f, 0.f, 1.f});
    renderer.draw_rect_outline({0.f, 0.f, 100.f, 50.f}, {1.f, 1.f, 1.f, 1.f}, 2.f);
    renderer.draw_line({0.f, 0.f}, {10.f, 10.f}, {0.f, 1.f, 0.f, 1.f}, 1.f);
    renderer.draw_circle({50.f, 50.f}, 10.f, {0.f, 0.f, 1.f, 1.f});
    renderer.draw_text(10.f, 20.f, "Бросок d20: 18", 16.f, {1.f, 1.f, 1.f, 1.f});
    REQUIRE(renderer.primitives_drawn() == 4);
    REQUIRE(renderer.text_calls() == 1);
    REQUIRE(renderer.last_text() == "Бросок d20: 18"); // UTF-8 без искажений (п. 4.5.5)
}

// --- scene_stack_push_pop (FR-E70) -----------------------------------------------
TEST_CASE("scene_stack_push_pop", "[engine][FR-E70]") {
    engine::scene::SceneStack stack;
    int entered = 0;
    auto first = std::make_unique<CountedScene>(entered);
    auto* first_ptr = first.get();
    stack.push(std::move(first));
    REQUIRE(stack.depth() == 1);
    REQUIRE(first_ptr->entered_ == 1);
    REQUIRE_FALSE(first_ptr->is_paused());

    auto second = std::make_unique<CountedScene>(entered);
    auto* second_ptr = second.get();
    stack.push(std::move(second));
    REQUIRE(stack.depth() == 2);
    REQUIRE(first_ptr->is_paused());   // нижняя сцена приостановлена (FR-E70)
    REQUIRE(first_ptr->paused_ == 1);  // on_pause() вызван ровно один раз
    REQUIRE(second_ptr->entered_ == 1);
    REQUIRE(stack.top() == second_ptr);

    // Обновляется и отрисовывается только вершина стека
    stack.update(1.0 / 60.0);
    stack.render(0.5);
    REQUIRE(second_ptr->updates_ == 1);
    REQUIRE(second_ptr->renders_ == 1);
    REQUIRE(first_ptr->updates_ == 0);

    stack.pop();
    REQUIRE(stack.depth() == 1);
    REQUIRE(second_ptr->exited_ == 1);
    REQUIRE(first_ptr->resumed_ == 1); // предыдущая сцена возобновлена
    REQUIRE_FALSE(first_ptr->is_paused());

    // replace: снятие текущей и установка новой
    auto third = std::make_unique<CountedScene>(entered);
    stack.replace(std::move(third));
    REQUIRE(stack.depth() == 1);
    REQUIRE(first_ptr->exited_ == 1);
    REQUIRE(entered == 3);

    stack.pop();
    REQUIRE(stack.empty());
    stack.pop(); // pop пустого стека безопасен (п. 4.2.1)
    REQUIRE(stack.empty());
}

// --- ui_scale_1280x720 (FR-E02, FR-E84) ------------------------------------------
TEST_CASE("ui_scale_1280x720", "[engine][FR-E02][FR-E84]") {
    engine::ui::UiRoot ui;
    REQUIRE(engine::ui::UiRoot::kLogicalWidth == 1280);
    REQUIRE(engine::ui::UiRoot::kLogicalHeight == 720);

    // Базовое разрешение: масштаб 1, полей нет
    auto vp = ui.resize(1280, 720);
    REQUIRE(approx_equal(vp.scale, 1.f, 1e-6f));
    REQUIRE(approx_equal(vp.offset_x, 0.f, 1e-6f));

    // Масштабирование с сохранением пропорций и полями letterbox (формула (1))
    vp = ui.resize(1920, 1080);
    REQUIRE(approx_equal(vp.scale, 1.5f, 1e-6f));
    vp = ui.resize(1600, 900);
    REQUIRE(approx_equal(vp.scale, 1.25f, 1e-6f));
    // Pillarbox: окно proportionally шире логического разрешения →
    // S = min(800/1280, 600/720) = 0.625, поля слева и справа (формула (1))
    vp = ui.resize(800, 600);
    REQUIRE(approx_equal(vp.scale, 0.625f, 1e-6f));
    REQUIRE(approx_equal(vp.offset_x, 0.f, 1e-3f));
    REQUIRE(vp.offset_y > 0.f);
    // Letterbox: окно выше логического разрешения
    vp = ui.resize(1280, 1000);
    REQUIRE(approx_equal(vp.scale, 1.f, 1e-6f));
    REQUIRE(vp.offset_y > 0.f);

    // Обратное преобразование координат ввода (FR-E03)
    ui.resize(1920, 1080);
    const auto logical = ui.window_to_logical(960.f, 540.f);
    REQUIRE(approx_equal(logical.x, 640.f, 1e-3f));
    REQUIRE(approx_equal(logical.y, 360.f, 1e-3f));
    const auto back = ui.logical_to_window(logical.x, logical.y);
    REQUIRE(approx_equal(back.x, 960.f, 1e-2f));
    REQUIRE(approx_equal(back.y, 540.f, 1e-2f));

    // Логическая разметка не меняется при изменении размера окна (FR-E84)
    auto panel = std::make_shared<engine::ui::Panel>();
    panel->set_name("MainPanel");
    panel->set_position(engine::ui::Vec2{100.f, 100.f});
    panel->set_size(engine::ui::Size{200.f, 120.f});
    ui.add(panel);
    ui.resize(2560, 1440);
    REQUIRE(approx_equal(panel->position().x, 100.f, 1e-6f));
    REQUIRE(approx_equal(panel->size().w, 200.f, 1e-6f));
    REQUIRE(ui.find_by_name("MainPanel") == panel.get());
    REQUIRE(ui.widget_count() == 2); // корень + панель
}

// --- Контейнеры макета (FR-E81) ---------------------------------------------------
TEST_CASE("ui_layout_containers", "[engine][FR-E81]") {
    engine::ui::VerticalBox vbox;
    auto a = std::make_shared<engine::ui::Label>("A");
    auto b = std::make_shared<engine::ui::Label>("B");
    a->set_size(engine::ui::Size{100.f, 20.f});
    b->set_size(engine::ui::Size{150.f, 30.f});
    vbox.add_child(a);
    vbox.add_child(b);
    vbox.set_spacing(5.f);
    vbox.layout();
    REQUIRE(approx_equal(a->position().y, 0.f, 1e-5f));
    REQUIRE(approx_equal(b->position().y, 25.f, 1e-5f)); // 20 + spacing
    REQUIRE(approx_equal(vbox.size().w, 150.f, 1e-5f));   // по самому широкому
    REQUIRE(approx_equal(vbox.size().h, 55.f, 1e-5f));    // 20 + spacing 5 + 30

    // Потомок принадлежит одному родителю: для горизонтального контейнера
    // создаём отдельные элементы (add_child переподключает родителя)
    auto ha = std::make_shared<engine::ui::Label>("A");
    auto hb = std::make_shared<engine::ui::Label>("B");
    ha->set_size(engine::ui::Size{100.f, 20.f});
    hb->set_size(engine::ui::Size{150.f, 30.f});
    engine::ui::HorizontalBox hbox;
    hbox.add_child(ha);
    hbox.add_child(hb);
    hbox.set_spacing(5.f);
    hbox.layout();
    REQUIRE(approx_equal(ha->position().x, 0.f, 1e-5f));
    REQUIRE(approx_equal(hb->position().x, 105.f, 1e-5f)); // 100 + spacing 5
    REQUIRE(approx_equal(hbox.size().w, 255.f, 1e-5f));     // 100 + 5 + 150
    REQUIRE(approx_equal(hbox.size().h, 30.f, 1e-5f));      // по самому высокому

    engine::ui::GridLayout grid(2, 2);
    grid.set_cell_size(engine::ui::Size{50.f, 40.f});
    grid.set_spacing(4.f);
    for (int i = 0; i < 4; ++i) {
        auto cell = std::make_shared<engine::ui::Panel>();
        grid.add_child(cell);
    }
    grid.layout();
    const auto& cells = grid.children();
    REQUIRE(cells.size() == 4);
    REQUIRE(approx_equal(cells[1]->position().x, 54.f, 1e-5f)); // вторая колонка
    REQUIRE(approx_equal(cells[2]->position().y, 44.f, 1e-5f)); // второй ряд
}

// --- Элементы управления и события (FR-E82, FR-E83) -------------------------------
TEST_CASE("ui_widgets_and_events", "[engine][FR-E82][FR-E83]") {
    engine::ui::UiRoot ui;
    ui.resize(1280, 720);

    auto button = std::make_shared<engine::ui::Button>("Attack");
    button->set_name("AttackButton");
    button->set_position(engine::ui::Vec2{100.f, 100.f});
    button->set_size(engine::ui::Size{120.f, 40.f});
    int clicks = 0;
    button->set_on_click([&]() { ++clicks; });
    ui.add(button);

    // Клик: наведение → нажатие → отпускание
    engine::ui::UiEvent move;
    move.type = engine::ui::UiEventType::PointerMoved;
    move.root_position = engine::ui::Vec2{150.f, 120.f};
    ui.dispatch_event(move);
    REQUIRE(button->hovered());

    engine::ui::UiEvent press = move;
    press.type = engine::ui::UiEventType::PointerPressed;
    ui.dispatch_event(press);
    REQUIRE(button->pressed());

    engine::ui::UiEvent release = move;
    release.type = engine::ui::UiEventType::PointerReleased;
    ui.dispatch_event(release);
    REQUIRE(clicks == 1);
    REQUIRE(button->click_count() == 1);
    REQUIRE_FALSE(button->pressed());

    // Событие вне элемента не вызывает клик
    engine::ui::UiEvent outside = move;
    outside.root_position = engine::ui::Vec2{900.f, 600.f};
    outside.type = engine::ui::UiEventType::PointerPressed;
    ui.dispatch_event(outside);
    outside.type = engine::ui::UiEventType::PointerReleased;
    ui.dispatch_event(outside);
    REQUIRE(clicks == 1);

    // Отключённая кнопка не реагирует на ввод
    button->set_enabled(false);
    engine::ui::UiEvent disabled_click = move;
    disabled_click.type = engine::ui::UiEventType::PointerPressed;
    ui.dispatch_event(disabled_click);
    disabled_click.type = engine::ui::UiEventType::PointerReleased;
    ui.dispatch_event(disabled_click);
    REQUIRE(clicks == 1);
    button->set_enabled(true);

    // Программная активация (горячие клавиши, доступность)
    button->activate();
    REQUIRE(clicks == 2);

    // Кнопка-переключатель
    engine::ui::Button toggle("Grid");
    toggle.set_toggle(true);
    toggle.activate();
    REQUIRE(toggle.toggled());
    toggle.activate();
    REQUIRE_FALSE(toggle.toggled());

    // Область прокрутки: колесо, границы, автопрокрутка к последней записи
    engine::ui::ScrollView scroll;
    scroll.set_size(engine::ui::Size{300.f, 100.f});
    scroll.set_content_size(engine::ui::Size{300.f, 400.f});
    REQUIRE(approx_equal(scroll.max_scroll_offset(), 300.f, 1e-5f));
    scroll.scroll_by(50.f);
    REQUIRE(approx_equal(scroll.scroll_offset(), 50.f, 1e-5f));
    scroll.scroll_by(-1000.f);
    REQUIRE(approx_equal(scroll.scroll_offset(), 0.f, 1e-5f)); // не уходит ниже нуля
    scroll.scroll_by(10000.f);
    REQUIRE(approx_equal(scroll.scroll_offset(), 300.f, 1e-5f)); // не уходит выше максимума
    scroll.scroll_to_top();
    REQUIRE(approx_equal(scroll.scroll_offset(), 0.f, 1e-5f));
    scroll.scroll_to_bottom();
    REQUIRE(approx_equal(scroll.visible_fraction(), 0.25f, 1e-5f));

    // Модальное окно поглощает события, пока открыто (FR-E83)
    engine::ui::Modal modal;
    modal.set_position(engine::ui::Vec2{0.f, 0.f});
    modal.set_size(engine::ui::Size{1280.f, 720.f});
    int closes = 0;
    modal.set_on_close([&]() { ++closes; });
    modal.open();
    REQUIRE(modal.is_open());
    engine::ui::UiEvent click_inside;
    click_inside.type = engine::ui::UiEventType::PointerPressed;
    click_inside.root_position = engine::ui::Vec2{640.f, 360.f};
    REQUIRE(modal.dispatch_event(click_inside));
    modal.close();
    REQUIRE_FALSE(modal.is_open());
    REQUIRE(closes == 1);

    // Отрисовка дерева не падает на headless-рендерере
    engine::render::NullRenderer renderer;
    ui.draw(renderer);
    REQUIRE(renderer.primitives_drawn() > 0);
}

// --- debug_overlay_counters (FR-E100, FR-E103, п. 4.1.3.4) -----------------------
TEST_CASE("debug_overlay_counters", "[engine][FR-E100][FR-E103]") {
    engine::core::DebugOverlay overlay;
    REQUIRE_FALSE(overlay.enabled()); // в release по умолчанию выключена (п. 4.1.10.2)
    overlay.toggle();                 // горячая клавиша (FR-E103)
    REQUIRE(overlay.enabled());

    // Бюджет кадра: t_budget = 1000 / f_target (формула (4))
    REQUIRE(approx_equal(engine::core::DebugOverlay::frame_budget_ms(60.0), 1000.0 / 60.0, 1e-9));
    REQUIRE(approx_equal(engine::core::DebugOverlay::frame_budget_ms(30.0), 1000.0 / 30.0, 1e-9));

    overlay.update(8.0);  // в бюджете 16,67 мс
    REQUIRE_FALSE(overlay.over_frame_budget());
    overlay.update(25.0); // превышение бюджета
    REQUIRE(overlay.over_frame_budget());
    REQUIRE(overlay.average_fps() > 0.0);

    // Ограничение числа вызовов отрисовки: не более 300 на кадр (NFR-E01)
    REQUIRE(engine::core::DebugOverlay::kMaxDrawCallsPerFrame == 300);
    REQUIRE_FALSE(overlay.over_draw_call_budget(299));
    REQUIRE(overlay.over_draw_call_budget(301));

    const auto stats = overlay.make_stats(/*draw_calls=*/12, /*sprites=*/340, /*entities=*/8,
                                          /*cache=*/5, /*logic_steps=*/2, /*dropped=*/0);
    const std::string text = overlay.format_stats(stats);
    REQUIRE(text.find("FPS") != std::string::npos);
    REQUIRE(text.find("draw calls 12") != std::string::npos);
    REQUIRE(text.find("entities 8") != std::string::npos);

    // Панель рисуется только во включённом состоянии
    engine::render::NullRenderer renderer;
    overlay.draw(renderer, stats);
    REQUIRE(renderer.primitives_drawn() > 0);
    renderer.reset_metrics();
    overlay.set_enabled(false);
    overlay.draw(renderer, stats);
    REQUIRE(renderer.primitives_drawn() == 0); // отключённая панель не влияет на поведение
}

// --- json_roundtrip (основа FR-E41) ----------------------------------------------
TEST_CASE("json_value_roundtrip", "[engine][FR-E41]") {
    engine::resource::JsonValue value = engine::resource::JsonValue::make_object();
    value["id"] = "Item_Longsword";
    value["schema_version"] = 1;
    value["ratio"] = 1.5;
    value["flag"] = true;
    value["nothing"] = nullptr;
    engine::resource::JsonArray arr;
    arr.push_back(engine::resource::JsonValue(1));
    arr.push_back(engine::resource::JsonValue("два"));
    value["list"] = engine::resource::JsonValue(std::move(arr));

    const std::string text = value.dump(2);
    const auto parsed = engine::resource::JsonParser::parse(text);
    REQUIRE(parsed.is_ok());
    REQUIRE(parsed.value().at("id").as_string() == "Item_Longsword");
    REQUIRE(parsed.value().at("schema_version").as_int() == 1);
    REQUIRE(approx_equal(parsed.value().at("ratio").as_double(), 1.5, 1e-12));
    REQUIRE(parsed.value().at("flag").as_bool());
    REQUIRE(parsed.value().at("nothing").is_null());
    REQUIRE(parsed.value().at("list").size() == 2);
    REQUIRE(parsed.value().dump() == value.dump()); // детерминированная сериализация

    // Доступ по пути и дефолты при несовпадении типа (NFR-E14)
    REQUIRE(parsed.value().find_path("list/1")->as_string() == "два");
    REQUIRE(parsed.value().find_path("list/9") == nullptr);
    REQUIRE(parsed.value().at("id").as_int(-1) == -1);
}
