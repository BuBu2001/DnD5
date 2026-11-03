// game/main.cpp — демонстрационный прогон каркаса движка (Приложение Д ТЗ DOC-04).
//
// Два режима:
//   headless (по умолчанию) — главный цикл, ECS-lite, стек сцен, событийная шина,
//                             вызовы модуля правил через DTO-границу слоёв (E-D12);
//   оконный (--window)      — слой платформы SDL2: окно и вьюпорт по формуле (1),
//                             OpenGL 3.3 Core (при отказе — NullRenderer), ввод и
//                             его адаптация в UI, звук SDL2_mixer, отладочная панель.
//
// Движок не содержит знаний о правилах D&D (E-D01): вся игровая семантика
// приходит из rules/ через DTO, а данные — из assets:/ (DATA-E05).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "core/config.h"
#include "core/debug_overlay.h"
#include "core/event_bus.h"
#include "core/file_system.h"
#include "core/log.h"
#include "ecs/registry.h"
#include "ecs/view.h"
#include "platform/input.h"
#include "platform/window.h"
#include "platform/main_loop.h"
#include "platform/timer.h"
#include "platform/window.h"
#include "render/null_renderer.h"
#include "render/renderer.h"
#include "render/sprite_batch.h"
#include "rules/dice_roller.h"
#include "rules/dto.h"
#include "rules/random_provider.h"
#include "scene/scene_stack.h"
#include "ui/ui_root.h"
#include "ui/widgets.h"

#include "combat/combat_play_scene.h"
#include "controls/control_scheme.h"
#include "ui_widgets/combat_widgets.h"
#include "ui_widgets/dice_breakdown_panel.h"

#ifdef DND_ENGINE_WITH_SDL2
#include <SDL.h>

#include "audio/audio_system.h"
#include "render/gl_renderer.h"
#endif

using namespace engine;

namespace {

// --- компоненты демо-сцены (движок не знает игровой семантики, E-D01) ----------
struct TransformComponent {
    float x = 0.f;
    float y = 0.f;
};

struct SpriteComponent {
    render::TextureId texture = render::kInvalidTexture;
    float w = 32.f;
    float h = 32.f;
};

// Событие-пример: состояние хода передаётся по шине (FR-E10)
struct TurnStateChanged {
    int state = 0;
};

constexpr int kDemoFrames = 10;       // headless: имитация кадров
constexpr float kTileSize = 32.f;     // размер клетки в логических пикселях
constexpr int kMapColumns = 20;       // Приложение Д: комната 20×15 клеток
constexpr int kMapRows = 15;

// --- демо-сцена ----------------------------------------------------------------
class DemoScene : public scene::Scene {
public:
    explicit DemoScene(render::IRenderer& renderer) : renderer_(renderer) {}

    void on_enter() override {
        auto& reg = registry();
        // Четыре сущности-«фигуры» в клетках комнаты (вертикальный срез, Прил. Д)
        const float positions[][2] = {{2.f, 2.f}, {3.f, 2.f}, {16.f, 12.f}, {17.f, 12.f}};
        for (const auto& p : positions) {
            const auto e = reg.create_entity();
            reg.add_component<TransformComponent>(e, p[0] * kTileSize, p[1] * kTileSize);
            reg.add_component<SpriteComponent>(e, texture_, kTileSize, kTileSize);
        }
    }

    void set_texture(render::TextureId texture) { texture_ = texture; }

    void update(double dt_logic) override {
        accumulated_ += dt_logic;
        for (auto e : ecs::make_view<TransformComponent>(registry())) {
            auto& t = registry().get_component<TransformComponent>(e);
            t.x += static_cast<float>(dt_logic) * 6.f; // простейшая «система движения»
            if (t.x > static_cast<float>(kMapColumns) * kTileSize) t.x = 0.f;
        }
    }

    void render(double alpha) override {
        (void)alpha;
        // Сетка комнаты: примитивы (FR-E28) — движок не знает правил, только геометрия
        const render::Color grid{0.22f, 0.26f, 0.32f, 1.f};
        for (int c = 0; c <= kMapColumns; ++c) {
            renderer_.draw_line(
                {static_cast<float>(c) * kTileSize, 0.f},
                {static_cast<float>(c) * kTileSize, static_cast<float>(kMapRows) * kTileSize},
                grid, 1.f);
        }
        for (int r = 0; r <= kMapRows; ++r) {
            renderer_.draw_line(
                {0.f, static_cast<float>(r) * kTileSize},
                {static_cast<float>(kMapColumns) * kTileSize, static_cast<float>(r) * kTileSize},
                grid, 1.f);
        }

        // Фигуры: пакет спрайтов (E-D05 — один draw call на текстуру)
        batch_.begin();
        for (auto e : ecs::make_view<TransformComponent, SpriteComponent>(registry())) {
            const auto& t = registry().get_component<TransformComponent>(e);
            const auto& s = registry().get_component<SpriteComponent>(e);
            batch_.draw(s.texture, {0.f, 0.f, s.w, s.h}, {t.x, t.y, s.w, s.h},
                        render::Color{0.85f, 0.75f, 0.45f, 1.f});
        }
        batch_.end();
        sprites_queued_ = static_cast<std::uint32_t>(batch_.queued_sprites());
        batch_.flush();

        // Круг — индикатор активной сущности (FR-E28)
        renderer_.draw_circle({64.f + 40.f * static_cast<float>(std::sin(accumulated_)), 520.f},
                              12.f, render::Color{0.35f, 0.75f, 0.45f, 0.9f});
        ++renders_;
    }

    std::size_t entity_count() {
        return ecs::make_view<TransformComponent>(registry()).size();
    }
    std::uint32_t sprites_queued() const { return sprites_queued_; }
    std::size_t renders() const { return renders_; }

private:
    render::IRenderer& renderer_;
    render::SpriteBatch batch_{renderer_};
    render::TextureId texture_ = render::kInvalidTexture;
    double accumulated_ = 0.0;
    std::uint32_t sprites_queued_ = 0;
    std::size_t renders_ = 0;
};

// --- процедура проверки правил через DTO-границу (E-D12) ----------------------
void run_rules_demo(core::EventBus& bus) {
    bus.publish(TurnStateChanged{1});

    rules::SeededRandomProvider rng(42);       // ГСЧ внедряется (FR-K102, E-D07)
    rules::DiceRoller roller(rng);
    const auto result = roller.roll(/*dc=*/15, /*modifier=*/5, /*advantage=*/true);
    std::printf("[rules] d20=[%d,%d] taken=%d total=%d success=%s\n", result.dice[0],
                result.dice[1], result.taken, result.total, result.success ? "yes" : "no");

    // Данные читаются через виртуальные пути assets:/ (FR-E07, DATA-E05)
    core::FileSystem fs("assets");
    std::printf("[data] conditions.json exists: %s\n",
                fs.exists("assets:/data/conditions.json") ? "yes" : "no");
}

#ifdef DND_ENGINE_WITH_SDL2

// Процедурная текстура-«фигура» 32×32 RGBA: позволяет демонстрировать спрайты
// до подключения атласов ассетов (этап M2, tools/sprite_atlas_packer).
std::vector<std::uint8_t> make_token_texture() {
    const int size = 32;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(size) * size * 4, 0);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = static_cast<float>(x) - 15.5f;
            const float dy = static_cast<float>(y) - 15.5f;
            const float dist = std::sqrt(dx * dx + dy * dy);
            const std::size_t i = (static_cast<std::size_t>(y) * size + x) * 4;
            if (dist < 13.f) {
                const float shade = 1.f - dist / 26.f;
                pixels[i + 0] = static_cast<std::uint8_t>(235.f * shade);
                pixels[i + 1] = static_cast<std::uint8_t>(200.f * shade);
                pixels[i + 2] = static_cast<std::uint8_t>(120.f * shade);
                pixels[i + 3] = 255;
            } else if (dist < 15.f) {
                pixels[i + 0] = 40;
                pixels[i + 1] = 44;
                pixels[i + 2] = 52;
                pixels[i + 3] = 255;
            }
        }
    }
    return pixels;
}

// Ограничение числа кадров (0 — без ограничения): используется для CI-прогона
// оконного режима без окна X11 и для замера бюджета draw calls (NFR-E01).
struct WindowedOptions {
    int max_frames = 0;
    std::string screenshot_bmp; // пусто — снимок не сохраняется
    bool demo_scene = false;    // true — старая демо-сцена, false — боевая (по умолчанию)
};

int run_windowed(const core::Config& config, const WindowedOptions& options) {
    // --- окно и контекст (FR-E01, FR-E02, FR-E20) -------------------------------
    platform::WindowSettings settings;
    settings.title = config.get_string("window.title", "dnd_engine V0.4");
    settings.window_width = static_cast<std::uint32_t>(config.get_int("window.width", 1280));
    settings.window_height = static_cast<std::uint32_t>(config.get_int("window.height", 720));
    settings.fullscreen = config.get_bool("window.fullscreen", false);
    settings.vsync = config.get_bool("window.vsync", true);

    platform::SdlWindow window;
    if (!window.create(settings)) {
        core::Log::instance().error("main: окно не создано — продолжаем в headless-режиме");
        return 1; // отказ без аварийного завершения (п. 4.2.3)
    }

    // --- рендер-бэкенд: OpenGL 3.3 Core, при отказе — NullRenderer --------------
    auto gl_renderer = std::make_unique<render::GlRenderer>();
    render::GlInitInfo info;
    render::IRenderer* renderer = nullptr;
    render::NullRenderer null_renderer;
    if (gl_renderer->init(settings.logical_width, settings.logical_height, &SDL_GL_GetProcAddress,
                          &info)) {
        renderer = gl_renderer.get();
        core::Log::instance().info("main: бэкенд OpenGL — " + info.version + " / " +
                                   info.renderer);
    } else {
        renderer = &null_renderer;
        core::Log::instance().warn("main: OpenGL недоступен (" + info.error +
                                   ") — отрисовка в NullRenderer");
    }

    // --- звук (FR-E50…FR-E52) ---------------------------------------------------
    audio::SdlAudioSystem audio_system;
    audio::NullAudioSystem null_audio;
    audio::IAudioSystem* audio = &null_audio;
    if (audio_system.init()) {
        audio = &audio_system;
        audio::AudioSettings levels;
        levels.master = static_cast<float>(config.get_double("audio.master", 0.8));
        levels.music = static_cast<float>(config.get_double("audio.music", 0.6));
        levels.sfx = static_cast<float>(config.get_double("audio.sfx", 0.9));
        audio->set_settings(levels);
    } else {
        core::Log::instance().warn("main: звук недоступен — работа без звука");
    }

    // --- сцена, UI, отладочная панель ------------------------------------------
    render::TextureId token = render::kInvalidTexture;
    if (gl_renderer->ready()) {
        const auto pixels = make_token_texture();
        token = gl_renderer->create_texture_rgba(32, 32, pixels.data());
    }

    scene::SceneStack scenes;
    core::FileSystem fs("assets");
    rules::SeededRandomProvider rng(42);   // ГСЧ внедряется в игровой слой (E-D07)
    rules::DiceRoller roller(rng);
    core::EventBus bus;

    game::CombatPlayScene* combat_ptr = nullptr;
    if (!options.demo_scene) {
        auto combat = std::make_unique<game::CombatPlayScene>(*renderer, bus, roller, fs);
        // Провайдер текстур: RGBA-пиксели → GL-текстуры (docs/assets.md)
        combat->set_texture_provider([&](int w, int h, const std::uint8_t* pixels) {
            return gl_renderer->ready() ? gl_renderer->create_texture_rgba(w, h, pixels)
                                        : render::kInvalidTexture;
        });
        combat->set_audio(audio);
        combat_ptr = combat.get();
        scenes.push(std::move(combat));
    } else {
        scenes.push(std::make_unique<DemoScene>(*renderer));
    }
    // Стек владеет сценой: указатель используется только для метрик отладочной панели
    auto* scene_ptr = dynamic_cast<DemoScene*>(scenes.top());
    if (scene_ptr != nullptr) scene_ptr->set_texture(token);

    ui::UiRoot ui_root;
    auto viewport = ui_root.resize(settings.window_width, settings.window_height);
    (void)viewport;

    auto title = std::make_shared<ui::Label>(
        options.demo_scene ? "D&D 5e — каркас движка V0.4"
                           : "D&D 5e — бой (вертикальный срез, Приложение Д)");
    title->set_name("title");
    title->set_position(ui::Vec2{16.f, 16.f});
    title->set_size(ui::Size{560.f, 28.f});
    title->set_font_size(20.f);
    title->set_color(render::Color{0.93f, 0.9f, 0.8f, 1.f});
    ui_root.add(title);

    auto status = std::make_shared<ui::Label>(
        "WASD — камера, ЛКМ — выбор/ход, ПКМ — отмена, E — конец хода, колесо — зум, "
        "F1 — отладка, Esc — выход");
    status->set_name("status");
    status->set_position(ui::Vec2{16.f, 668.f});
    status->set_size(ui::Size{1248.f, 24.f});
    status->set_font_size(14.f);
    ui_root.add(status);

    // Боевые панели (E-D12: виджеты принимают только DTO)
    std::shared_ptr<game::InitiativeTracker> initiative_panel;
    std::shared_ptr<game::TurnResourceBar> resources_panel;
    std::shared_ptr<game::CombatLog> combat_log;
    std::shared_ptr<game::DiceBreakdownPanel> breakdown_panel;
    std::shared_ptr<game::CharacterSheetPanel> sheet_panel;
    std::shared_ptr<ui::Panel> inventory_panel;
    std::shared_ptr<ui::Label> inventory_title;
    std::vector<std::shared_ptr<ui::Label>> inventory_rows;
    if (combat_ptr != nullptr) {
        initiative_panel = std::make_shared<game::InitiativeTracker>();
        initiative_panel->set_name("initiative");
        initiative_panel->set_position(ui::Vec2{964.f, 16.f});
        initiative_panel->set_size(ui::Size{300.f, 200.f});
        ui_root.add(initiative_panel);

        resources_panel = std::make_shared<game::TurnResourceBar>();
        resources_panel->set_name("resources");
        resources_panel->set_position(ui::Vec2{964.f, 224.f});
        resources_panel->set_size(ui::Size{300.f, 110.f});
        ui_root.add(resources_panel);

        breakdown_panel = std::make_shared<game::DiceBreakdownPanel>();
        breakdown_panel->set_name("breakdown");
        breakdown_panel->set_position(ui::Vec2{964.f, 342.f});
        breakdown_panel->set_size(ui::Size{300.f, 210.f});
        ui_root.add(breakdown_panel);

        combat_log = std::make_shared<game::CombatLog>();
        combat_log->set_name("combat_log");
        combat_log->set_position(ui::Vec2{16.f, 560.f});
        combat_log->set_size(ui::Size{620.f, 100.f});
        ui_root.add(combat_log);

        // Панель персонажа (C) и инвентарь (I) — скрыты до открытия (docs/controls.md)
        sheet_panel = std::make_shared<game::CharacterSheetPanel>();
        sheet_panel->set_name("sheet");
        sheet_panel->set_position(ui::Vec2{660.f, 540.f});
        sheet_panel->set_size(ui::Size{290.f, 150.f});
        sheet_panel->set_visible(false);
        ui_root.add(sheet_panel);

        inventory_panel = std::make_shared<ui::Panel>();
        inventory_panel->set_name("inventory");
        inventory_panel->set_position(ui::Vec2{660.f, 430.f});
        inventory_panel->set_size(ui::Size{290.f, 104.f});
        inventory_panel->set_visible(false);
        inventory_title = std::make_shared<ui::Label>("Инвентарь");
        inventory_title->set_position(ui::Vec2{8.f, 4.f});
        inventory_title->set_size(ui::Size{274.f, 18.f});
        inventory_title->set_font_size(14.f);
        inventory_panel->add_child(inventory_title);
        for (int i = 0; i < 4; ++i) {
            auto row = std::make_shared<ui::Label>("");
            row->set_position(ui::Vec2{8.f, 24.f + static_cast<float>(i) * 19.f});
            row->set_size(ui::Size{274.f, 18.f});
            row->set_font_size(13.f);
            inventory_panel->add_child(row);
            inventory_rows.push_back(row);
        }
        ui_root.add(inventory_panel);

        // L — показать/скрыть журнал боя
        combat_ptr->set_log_toggle([combat_log] {
            if (combat_log) combat_log->set_visible(!combat_log->visible());
        });

        // События боя → журнал и панель разбора (подписка на шину, FR-E10)
        bus.subscribe<game::AttackResolvedEvent>(
            [combat_log, breakdown_panel](const game::AttackResolvedEvent& e) {
                if (combat_log) {
                    combat_log->append(game::CombatLogFilter::Rolls, e.description);
                    if (!e.damage_notation.empty() && e.hit) {
                        combat_log->append(game::CombatLogFilter::Damage,
                                           "Урон: " + e.damage_notation + " = " +
                                               std::to_string(e.damage));
                    }
                    combat_log->scroll_to_latest();
                }
                if (breakdown_panel) breakdown_panel->set_breakdown(e.attack_breakdown);
            });
    }

    auto roll_button = std::make_shared<ui::Button>("Проверка (d20+5, помеха)");
    roll_button->set_name("roll");
    roll_button->set_position(ui::Vec2{16.f, 52.f});
    roll_button->set_size(ui::Size{260.f, 36.f});
    roll_button->set_on_click([&] {
        const auto r = roller.roll(15, 5, true);
        char buffer[160];
        std::snprintf(buffer, sizeof(buffer), "d20=[%d,%d] → %d + 5 = %d, AC 15: %s", r.dice[0],
                      r.dice[1], r.taken, r.total, r.success ? "успех" : "провал");
        status->set_text(buffer);
        audio->play_sfx("assets:/audio/ui_click.wav"); // ошибка загрузки — только журнал
    });
    ui_root.add(roll_button);

    core::DebugOverlay overlay;
    overlay.set_enabled(config.get_bool("debug.overlay", true));

    // --- главный цикл (п. 4.1.2.3–4.1.2.4, формула (2)) -------------------------
    platform::InputState input;
    platform::MainLoop loop;
    platform::Timer frame_timer;
    double frame_time_ms = 0.0;
    std::uint32_t logic_steps = 0;
    bool running = true;
    bool music_started = false;

    // Маппинг игровых действий (docs/controls.md, FR-E03)
    platform::InputActions actions;
    game::build_control_scheme(actions);

    loop.set_running([&] { return running; });
    loop.set_poll([&] {
        input.begin_frame();
        if (!platform::poll_sdl_events(input, window, ui_root)) running = false;

        // Горячие клавиши (FR-E103, таблица 8.6 DOC-01: F1 — панель отладки)
        for (const auto& e : input.frame_events()) {
            if (e.type != platform::InputEvent::Type::KeyDown) continue;
            if (e.key == platform::Key::Escape) running = false;
            if (e.key == platform::Key::F1) overlay.toggle();
            if (e.key == platform::Key::F2) {
                // FR-E54: плавное затухание/нарастание музыки (600 мс)
                if (!music_started) {
                    audio->fade_in_music("assets:/audio/battle_theme.wav", /*loop=*/true, 600);
                    music_started = true;
                } else {
                    audio->fade_out_music(600);
                    music_started = false;
                }
            }
            if (e.key == platform::Key::Plus) audio->set_master_volume(
                std::min(1.f, audio->settings().master + 0.1f));
            if (e.key == platform::Key::Minus) audio->set_master_volume(
                std::max(0.f, audio->settings().master - 0.1f));
        }

        // События мыши → события UI-дерева (FR-E80, FR-E84)
        for (auto e : platform::InputToUiAdapter::translate(input, ui_root)) {
            ui_root.dispatch_event(e);
        }

        // Игровые действия → боевая сцена (docs/controls.md). Клики по областям
        // боевых панелей не должны «проваливаться» на поле (E-D12: UI приоритетен).
        if (combat_ptr != nullptr) {
            actions.update(input);
            const float mx = input.mouse_x();
            const float my = input.mouse_y();
            const bool ui_captured =
                combat_ptr->panels_blocking() || (mx >= 964.f && my <= 560.f) ||
                (mx <= 640.f && my >= 556.f) || (mx <= 280.f && my <= 92.f);
            combat_ptr->set_mouse_logical(mx, my);
            combat_ptr->handle_actions(actions, ui_captured);
        }
        input.end_frame();
    });
    loop.set_update([&](double dt_logic) {
        scenes.update(dt_logic);
        ui_root.update(dt_logic);
        // Панели боя обновляются из DTO сцены (E-D12)
        if (combat_ptr != nullptr) {
            if (initiative_panel) initiative_panel->set_tracker(combat_ptr->initiative_dto());
            if (resources_panel) resources_panel->set_resources(combat_ptr->turn_resources_dto());
            if (status) status->set_text(combat_ptr->status());
            // Панели C/I: видимость и содержимое — из DTO сцены (E-D12)
            if (sheet_panel) {
                const bool open = combat_ptr->sheet_open();
                sheet_panel->set_visible(open);
                if (open) sheet_panel->set_summary(combat_ptr->character_sheet_dto());
            }
            if (inventory_panel) {
                const bool open = combat_ptr->inventory_open();
                inventory_panel->set_visible(open);
                if (open) {
                    const auto entries = combat_ptr->inventory_entries();
                    const auto summary = combat_ptr->character_sheet_dto();
                    inventory_title->set_text("Инвентарь: " + summary.name);
                    for (std::size_t i = 0; i < inventory_rows.size(); ++i) {
                        if (i < entries.size()) {
                            inventory_rows[i]->set_text(entries[i].name +
                                                        (entries[i].note.empty()
                                                             ? ""
                                                             : " — " + entries[i].note));
                        } else {
                            inventory_rows[i]->set_text("");
                        }
                    }
                }
            }
        }
    });
    loop.set_render([&](double alpha) {
        (void)alpha;
        renderer->begin_frame();
        scenes.render(alpha);
        ui_root.draw(*renderer);

        const std::uint32_t sprites = scene_ptr != nullptr
                                          ? scene_ptr->sprites_queued()
                                          : static_cast<std::uint32_t>(combat_ptr != nullptr
                                                                       ? combat_ptr->renders()
                                                                       : 0u);
        const std::size_t entities = scene_ptr != nullptr
                                         ? scene_ptr->entity_count()
                                         : (combat_ptr != nullptr ? combat_ptr->combat()
                                                                      .combatant_count()
                                                                  : 0u);
        const auto stats = overlay.make_stats(renderer->draw_call_count(), sprites, entities,
                                              audio->loaded_sounds(), logic_steps,
                                              static_cast<std::uint32_t>(
                                                  loop.dropped_steps_last_frame()));
        overlay.draw(*renderer, stats);
        renderer->end_frame();
        window.swap();
    });

    std::uint32_t max_draw_calls = 0;
    int frames = 0;
    while (running) {
        frame_time_ms = frame_timer.elapsed_and_restart() * 1000.0;
        logic_steps = static_cast<std::uint32_t>(loop.tick(frame_time_ms / 1000.0));
        overlay.update(frame_time_ms);
        max_draw_calls = std::max(max_draw_calls, renderer->draw_call_count());
        ++frames;
        if (options.max_frames > 0 && frames >= options.max_frames) running = false;
    }

    // Итоговые метрики кадра: бюджет draw calls (NFR-E01, не более 300)
    std::printf("[perf] frames=%d max_draw_calls=%u budget=%u %s\n", frames, max_draw_calls,
                core::DebugOverlay::kMaxDrawCallsPerFrame,
                overlay.over_draw_call_budget(max_draw_calls) ? "EXCEEDED" : "ok");
    std::printf("[perf] textures=%zu sprites=%zu glyphs=%zu gl_error=0x%x\n",
                gl_renderer->texture_count(), gl_renderer->sprites_drawn(),
                gl_renderer->text_glyphs_drawn(), gl_renderer->last_gl_error());
    if (!options.screenshot_bmp.empty() && gl_renderer->ready()) {
        const bool saved = gl_renderer->save_screenshot_bmp(options.screenshot_bmp);
        std::printf("[perf] screenshot %s -> %s\n", options.screenshot_bmp.c_str(),
                    saved ? "ok" : "FAILED");
    }

    audio->stop_music();
    audio->stop_all_sfx();
    audio_system.shutdown();
    if (token != render::kInvalidTexture) gl_renderer->destroy_texture(token);
    gl_renderer->shutdown();
    window.destroy();
    core::Log::instance().info("main: оконный режим завершён штатно");
    return 0;
}

#endif // DND_ENGINE_WITH_SDL2

int run_headless() {
    core::EventBus bus;
    bus.subscribe<TurnStateChanged>(
        [](const TurnStateChanged& e) { std::printf("[bus] TurnStateChanged: %d\n", e.state); });
    run_rules_demo(bus);

    render::NullRenderer renderer;
    scene::SceneStack scenes;
    scenes.push(std::make_unique<DemoScene>(renderer));

    platform::MainLoop loop;
    platform::Timer timer;
    loop.set_update([&](double dt) { scenes.update(dt); });
    loop.set_render([&](double alpha) { scenes.render(alpha); });

    // Имитация 10 кадров по 1/30 с: просадка частоты → 2 шага логики на кадр
    for (int i = 0; i < kDemoFrames; ++i) {
        loop.tick(1.0 / 30.0);
    }
    (void)timer;
    std::printf("[demo] headless-прогон завершён: кадров=%d, draw calls=%u\n", kDemoFrames,
                renderer.draw_call_count());
    core::Log::instance().info("main loop finished OK");
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    core::Log::instance().set_level(core::LogLevel::Info);
    core::Log::instance().info("dnd_engine V0.4 — каркас движка (АБВГ.00004)");

    core::Config config;
    const auto loaded = config.load_from_file("assets/config.ini");
    if (!loaded.is_ok()) {
        core::Log::instance().warn("main: конфигурация не найдена — значения по умолчанию");
    }

    bool windowed = false;
    int max_frames = 0;
    bool demo_scene = false;
    std::string screenshot;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--window") windowed = true;
        else if (arg == "--headless") windowed = false;
        else if (arg == "--demo") demo_scene = true;
        else if (arg.rfind("--frames=", 0) == 0) max_frames = std::atoi(arg.c_str() + 9);
        else if (arg.rfind("--screenshot=", 0) == 0) screenshot = arg.substr(13);
    }

#ifdef DND_ENGINE_WITH_SDL2
    if (windowed) {
        return run_windowed(config, WindowedOptions{max_frames, screenshot, demo_scene});
    }
#else
    if (windowed) {
        core::Log::instance().warn(
            "main: сборка без SDL2 (DND_WITH_SDL2=OFF) — оконный режим недоступен");
    }
#endif
    return run_headless();
}
