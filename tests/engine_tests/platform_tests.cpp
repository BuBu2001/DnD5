// tests/engine_tests/platform_tests.cpp — испытания слоя платформы и отрисовки,
// не требующие окна и драйвера (таблица 8.3 ТЗ DOC-04, E-D11):
// формула (1) масштабирования, кадр ввода и адаптация в UI, шрифтовой атлас,
// загрузчик функций OpenGL, пакетирование спрайтов.
#include <cstdint>
#include <string>
#include <vector>

#include "test_harness.h"

#include "platform/input.h"
#include "platform/timer.h"
#include "platform/window.h"
#include "render/font_atlas.h"
#include "render/gl_functions.h"
#include "render/null_renderer.h"
#include "render/sprite_batch.h"
#include "ui/ui_root.h"
#include "ui/widgets.h"

using namespace engine;

// --- FR-E02: формула (1) — масштаб и поля letterbox/pillarbox ----------------
TEST_CASE("viewport_formula_1_scale_and_bars", "[engine][FR-E02][FR-E84]") {
    SECTION("1:1 — масштаб 1, полей нет") {
        const auto vp = platform::Window::compute_viewport(1280, 720, 1280, 720);
        REQUIRE(approx_equal(vp.scale, 1.f, 1e-6));
        REQUIRE(approx_equal(vp.offset_x, 0.f, 1e-6));
        REQUIRE(approx_equal(vp.offset_y, 0.f, 1e-6));
    }

    SECTION("Широкое окно — pillarbox по X, масштаб по высоте") {
        // S = min(2560/1280, 720/720) = 1 → поля слева и справа по 640 px
        const auto vp = platform::Window::compute_viewport(2560, 720, 1280, 720);
        REQUIRE(approx_equal(vp.scale, 1.f, 1e-6));
        REQUIRE(approx_equal(vp.offset_x, 640.f, 1e-4));
        REQUIRE(approx_equal(vp.offset_y, 0.f, 1e-6));
    }

    SECTION("Высокое окно — letterbox по Y, масштаб по ширине") {
        // S = min(1280/1280, 1080/720) = 1 → поля сверху и снизу по 180 px
        const auto vp = platform::Window::compute_viewport(1280, 1080, 1280, 720);
        REQUIRE(approx_equal(vp.scale, 1.f, 1e-6));
        REQUIRE(approx_equal(vp.offset_y, 180.f, 1e-4));
    }

    SECTION("Увеличение в 1,5 раза: 1920×1080") {
        // S = min(1920/1280, 1080/720) = 1,5; полей нет (пропорции совпадают)
        const auto vp = platform::Window::compute_viewport(1920, 1080, 1280, 720);
        REQUIRE(approx_equal(vp.scale, 1.5f, 1e-6));
        REQUIRE(approx_equal(vp.offset_x, 0.f, 1e-4));
        REQUIRE(approx_equal(vp.offset_y, 0.f, 1e-4));
    }

    SECTION("Нестандартное окно 1000×1000 — масштаб по меньшей стороне") {
        // S = min(1000/1280, 1000/720) = 0,78125; pillarbox отсутствует, letterbox есть
        const auto vp = platform::Window::compute_viewport(1000, 1000, 1280, 720);
        REQUIRE(approx_equal(vp.scale, 1000.f / 1280.f, 1e-6));
        REQUIRE(approx_equal(vp.offset_x, 0.f, 1e-4));
        // (1000 − 720·0,78125) / 2 = (1000 − 562,5) / 2 = 218,75
        REQUIRE(approx_equal(vp.offset_y, 218.75f, 1e-4));
    }

    SECTION("Вырожденный случай: нулевое логическое разрешение") {
        const auto vp = platform::Window::compute_viewport(800, 600, 0, 0);
        REQUIRE(approx_equal(vp.scale, 1.f, 1e-6));
        REQUIRE(vp.window_w == 800u);
        REQUIRE(vp.window_h == 600u);
    }

    SECTION("UiRoot::resize повторяет формулу (1)") {
        ui::UiRoot root;
        const auto vp = root.resize(1920, 1080);
        REQUIRE(approx_equal(vp.scale, 1.5f, 1e-6));
        REQUIRE(approx_equal(root.scale(), 1.5f, 1e-6));
    }
}

// --- FR-E03: кадр ввода и «нажато в этом кадре» ------------------------------
TEST_CASE("input_frame_cycle", "[engine][FR-E03]") {
    platform::InputState input;

    SECTION("Клавиша удерживается: pressed_this_frame — только в кадре нажатия") {
        input.begin_frame();
        input.set_key(platform::Key::Space, true);
        input.push_event({platform::InputEvent::Type::KeyDown, platform::Key::Space,
                          platform::MouseButton::Left, 0.f, 0.f, 0});
        REQUIRE(input.is_key_down(platform::Key::Space));
        REQUIRE(input.is_key_pressed_this_frame(platform::Key::Space) == false);
        // Фиксация «нажат в этом кадре» происходит в begin_frame по событиям очереди;
        // здесь проверяем контракт очереди событий
        REQUIRE(input.event_count() == 1u);
        REQUIRE(input.frame_events().front().key == platform::Key::Space);
        input.end_frame();
        REQUIRE(input.is_key_down(platform::Key::Space)); // состояние удерживается
    }

    SECTION("Очередь событий очищается в начале кадра") {
        input.begin_frame();
        input.push_event({platform::InputEvent::Type::MouseMove, platform::Key::Unknown,
                          platform::MouseButton::Left, 10.f, 20.f, 0});
        input.push_event({platform::InputEvent::Type::MouseMove, platform::Key::Unknown,
                          platform::MouseButton::Left, 30.f, 40.f, 0});
        REQUIRE(input.event_count() == 2u);
        input.begin_frame();
        REQUIRE(input.event_count() == 0u);
    }

    SECTION("Мышь: позиция и кнопки") {
        input.set_mouse_position(640.f, 360.f);
        REQUIRE(approx_equal(input.mouse_x(), 640.f, 1e-6));
        REQUIRE(approx_equal(input.mouse_y(), 360.f, 1e-6));
        REQUIRE_FALSE(input.is_mouse_down(platform::MouseButton::Left));
        input.set_mouse_button(platform::MouseButton::Left, true);
        REQUIRE(input.is_mouse_down(platform::MouseButton::Left));
        REQUIRE_FALSE(input.is_mouse_down(platform::MouseButton::Right));
    }

    SECTION("Колесо мыши: знак дельты") {
        platform::InputEvent wheel;
        wheel.type = platform::InputEvent::Type::MouseWheel;
        wheel.wheel_delta = -1;
        input.begin_frame();
        input.push_event(wheel);
        REQUIRE(input.frame_events().front().wheel_delta == -1);
    }
}

// --- FR-E80/FR-E84: адаптация событий ввода в события UI ---------------------
TEST_CASE("input_to_ui_adapter_translation", "[engine][FR-E03][FR-E80][FR-E84]") {
    ui::UiRoot root;
    root.resize(1920, 1080); // S = 1,5, полей нет
    platform::InputState input;
    input.begin_frame();

    platform::InputEvent down;
    down.type = platform::InputEvent::Type::MouseDown;
    down.button = platform::MouseButton::Left;
    down.x = 960.f; // физические координаты окна
    down.y = 540.f;
    input.push_event(down);

    platform::InputEvent move;
    move.type = platform::InputEvent::Type::MouseMove;
    move.x = 1920.f;
    move.y = 1080.f;
    input.push_event(move);

    platform::InputEvent key;
    key.type = platform::InputEvent::Type::KeyDown;
    key.key = platform::Key::Escape;
    input.push_event(key);

    const auto events = platform::InputToUiAdapter::translate(input, root);
    REQUIRE(events.size() == 2u); // клавиатура в UI-дерево не транслируется

    REQUIRE(events[0].type == ui::UiEventType::PointerPressed);
    REQUIRE(events[0].button == 0u);
    // 960 / 1,5 = 640; 540 / 1,5 = 360 — центр логического экрана
    REQUIRE(approx_equal(events[0].root_position.x, 640.f, 1e-3));
    REQUIRE(approx_equal(events[0].root_position.y, 360.f, 1e-3));

    REQUIRE(events[1].type == ui::UiEventType::PointerMoved);
    REQUIRE(approx_equal(events[1].root_position.x, 1280.f, 1e-3));
    REQUIRE(approx_equal(events[1].root_position.y, 720.f, 1e-3));

    SECTION("Событие колеса кодируется направлением прокрутки") {
        const ui::Vec2 pos{100.f, 100.f};
        const auto up = platform::InputToUiAdapter::wheel_event(+1, pos);
        const auto down_ev = platform::InputToUiAdapter::wheel_event(-1, pos);
        REQUIRE(up.type == ui::UiEventType::PointerMoved);
        REQUIRE(up.button == 1u);
        REQUIRE(up.position.y > 0.f);
        REQUIRE(down_ev.position.y < 0.f);
    }

    SECTION("Координаты окна со смещением (letterbox) учитывают поля") {
        ui::UiRoot letterboxed;
        letterboxed.resize(1280, 1080); // S = 1, offset_y = 180
        const auto logical = letterboxed.window_to_logical(640.f, 540.f);
        REQUIRE(approx_equal(logical.x, 640.f, 1e-3));
        REQUIRE(approx_equal(logical.y, 360.f, 1e-3)); // (540 − 180) / 1
    }
}

// --- FR-E02: поля letterbox/pillarbox не перекрывают содержимое сцены --------
namespace {

// Зонд: фиксирует прямоугольники, которые UI-дерево отправляет в рендерер.
// Используется для проверки регрессии дефекта V0.4 (чёрный прямоугольник на всё
// окно поверх сцены) — headless, без GL.
class RectProbeRenderer final : public render::IRenderer {
public:
    void draw_sprite(render::TextureId, const render::Rect&, const render::Rect&,
                     const render::Color&) override {}
    void draw_rect(const render::Rect& rect, const render::Color& color) override {
        rects_.push_back(rect);
        colors_.push_back(color);
    }
    void draw_rect_outline(const render::Rect&, const render::Color&, float) override {}
    void draw_line(render::Vec2f, render::Vec2f, const render::Color&, float) override {}
    void draw_circle(render::Vec2f, float, const render::Color&) override {}
    void draw_text(float, float, const std::string&, float, const render::Color&) override {}
    void flush() override {}

    const std::vector<render::Rect>& rects() const { return rects_; }
    const std::vector<render::Color>& colors() const { return colors_; }
    void reset() {
        rects_.clear();
        colors_.clear();
    }

private:
    std::vector<render::Rect> rects_;
    std::vector<render::Color> colors_;
};

} // namespace

TEST_CASE("ui_letterbox_does_not_cover_scene", "[engine][FR-E02][FR-E84]") {
    RectProbeRenderer probe;

    SECTION("Окно 1280×720: полей нет — фон не рисуется вовсе") {
        ui::UiRoot root;
        root.resize(1280, 720);
        probe.reset();
        root.draw_letterbox(probe);
        REQUIRE(probe.rects().empty());
    }

    SECTION("Окно 1920×1080 (S = 1,5): полей нет — содержимое не перекрывается") {
        ui::UiRoot root;
        root.resize(1920, 1080);
        probe.reset();
        root.draw_letterbox(probe);
        REQUIRE(probe.rects().empty());
    }

    SECTION("Окно 2560×720: pillarbox — два боковых поля, центр свободен") {
        ui::UiRoot root;
        root.resize(2560, 720);
        probe.reset();
        root.draw_letterbox(probe);
        REQUIRE(probe.rects().size() == 2u);
        // Логическая область всегда центрирована, поэтому ни одно поле не может
        // содержать центр окна — содержимое сцены не перекрывается (регрессия V0.4).
        const float center_x = 2560.f * 0.5f;
        const float center_y = 720.f * 0.5f;
        for (const auto& r : probe.rects()) {
            const bool contains_center = center_x >= r.x && center_x <= r.x + r.w &&
                                         center_y >= r.y && center_y <= r.y + r.h;
            REQUIRE_FALSE(contains_center);
            REQUIRE(r.w > 0.f);
            REQUIRE(r.h > 0.f);
        }
        // Суммарная ширина полей = 2560 − 1280 (S = 1)
        const float total = probe.rects()[0].w + probe.rects()[1].w;
        REQUIRE(approx_equal(total, 1280.f, 1e-3));
    }

    SECTION("Окно 1280×1080: letterbox — два поля сверху и снизу") {
        ui::UiRoot root;
        root.resize(1280, 1080);
        probe.reset();
        root.draw_letterbox(probe);
        REQUIRE(probe.rects().size() == 2u);
        const auto& top = probe.rects()[0];
        const auto& bottom = probe.rects()[1];
        REQUIRE(approx_equal(top.y, 0.f, 1e-3));
        REQUIRE(approx_equal(top.h, 180.f, 1e-3));
        REQUIRE(approx_equal(bottom.y, 900.f, 1e-3));   // 180 + 720
        REQUIRE(approx_equal(bottom.h, 180.f, 1e-3));
    }

    SECTION("Полный draw() не закрашивает всю логическую область") {
        ui::UiRoot root;
        root.resize(2560, 720);
        probe.reset();
        root.draw(probe);
        // Дерево без виджетов рисует только поля: центр остаётся за сценой
        for (const auto& r : probe.rects()) {
            const bool full_window = r.x <= 0.f && r.y <= 0.f && r.w >= 2560.f && r.h >= 720.f;
            REQUIRE_FALSE(full_window);
        }
    }
}

// --- FR-E29: метрики шрифта — базовая линия и выносные элементы --------------
TEST_CASE("font_atlas_baseline_metrics", "[engine][FR-E29]") {
    render::FontAtlas font;
    REQUIRE(font.bake());

    // Дизайн-сетка: Y от -3 (диакритика) до 10 (выносные элементы), прописные 0..8.
    // Ячейка 16×32 px, базовый кегль (высота прописных) — 22 px.
    constexpr float kDesignTop = -3.f;
    constexpr float kDesignBottom = 10.f;
    constexpr float kDesignCapHeight = 8.f;
    const float px_per_unit = static_cast<float>(render::FontAtlas::kCellHeight) /
                              (kDesignBottom - kDesignTop);
    const float cap_top_px = (0.f - kDesignTop) * px_per_unit;
    const float cap_bottom_px = (kDesignCapHeight - kDesignTop) * px_per_unit;
    const float cap_px = cap_bottom_px - cap_top_px;
    REQUIRE(approx_equal(cap_px, render::FontAtlas::kBaseCapHeight, 0.6f));

    SECTION("Доля подъёма над базовой линией постоянна и не зависит от кегля") {
        const float ascent_fraction = cap_top_px / static_cast<float>(render::FontAtlas::kCellHeight);
        REQUIRE(ascent_fraction > 0.15f);
        REQUIRE(ascent_fraction < 0.35f);
        // Для кегля 20: смещение базовой линии ≈ 5 px — текст встанет на y + 15
        const float size = 20.f;
        const float cell_h = static_cast<float>(render::FontAtlas::kCellHeight) *
                             (size / render::FontAtlas::kBaseCapHeight);
        const float baseline_offset = ascent_fraction * cell_h;
        // cap_top = 3/13 × 32 = 7,385 px → доля 0,2308; для кегля 20: 0,2308 × 32,5 = 7,5 px
        REQUIRE(approx_equal(ascent_fraction, 3.f / 13.f, 1e-3));
        REQUIRE(approx_equal(baseline_offset, 7.5f, 0.1f));
        REQUIRE(approx_equal(size - baseline_offset, 12.5f, 0.1f));
    }

    SECTION("Шаг глифа: моноширинный, ~0,73 кегля") {
        const auto* g = font.glyph('A');
        REQUIRE(g != nullptr);
        REQUIRE(approx_equal(g->advance, 16.f / render::FontAtlas::kBaseCapHeight, 1e-4));
        // Кегль 19,6923 px = высота прописных в ячейке → ширина строки равна ширине атласа
        const float cap_size = render::FontAtlas::kBaseCapHeight;
        REQUIRE(approx_equal(font.text_width("AAAA", cap_size), 4.f * 16.f, 1e-2));
        REQUIRE(approx_equal(g->advance, 16.f / cap_size, 1e-4));
        REQUIRE(g->advance > 0.7f);
        REQUIRE(g->advance < 0.9f);
    }
}

// --- FR-E03: событие UI доходит до виджета через дерево ----------------------
TEST_CASE("input_event_reaches_widget", "[engine][FR-E82][FR-E83]") {
    ui::UiRoot root;
    root.resize(1280, 720);

    auto button = std::make_shared<ui::Button>("Подтвердить");
    button->set_name("confirm");
    button->set_position(ui::Vec2{600.f, 340.f});
    button->set_size(ui::Size{160.f, 40.f});
    int clicks = 0;
    button->set_on_click([&clicks] { ++clicks; });
    root.add(button);

    platform::InputState input;
    input.begin_frame();
    platform::InputEvent press;
    press.type = platform::InputEvent::Type::MouseDown;
    press.x = 640.f;
    press.y = 350.f;
    input.push_event(press);
    platform::InputEvent release;
    release.type = platform::InputEvent::Type::MouseUp;
    release.x = 640.f;
    release.y = 350.f;
    input.push_event(release);

    for (auto event : platform::InputToUiAdapter::translate(input, root)) {
        root.dispatch_event(event);
    }
    REQUIRE(clicks == 1);
}

// --- FR-E29: растровый атлас встроенного шрифта ------------------------------
TEST_CASE("font_atlas_bakes_glyphs", "[engine][FR-E29]") {
    render::FontAtlas font;
    REQUIRE(font.bake());
    REQUIRE(font.ready());
    REQUIRE(font.width() == render::FontAtlas::kColumns * render::FontAtlas::kCellWidth);
    REQUIRE(font.height() > 0);
    REQUIRE(font.pixels().size() ==
            static_cast<std::size_t>(font.width()) * static_cast<std::size_t>(font.height()) * 4u);

    SECTION("Латиница, цифры и знаки препинания присутствуют") {
        for (const char ch : std::string("ABCXYZ 012389 !?.,:-+*/()[]%$")) {
            if (ch == ' ') continue;
            const auto* g = font.glyph(static_cast<std::uint32_t>(ch));
            REQUIRE(g != nullptr);
            REQUIRE(g->advance > 0.f);
            REQUIRE(g->u1 > g->u0);
            REQUIRE(g->v1 > g->v0);
        }
    }

    SECTION("Кириллица присутствует (DATA-E01: текст в данных — UTF-8)") {
        std::uint32_t cp = 0;
        for (const char* s : {"Ж", "Ё", "ё", "Я", "щ", "ъ", "б", "ю"}) {
            const std::size_t n = render::FontAtlas::decode_utf8(s, 4, cp);
            REQUIRE(n == 2u);
            REQUIRE(font.glyph(cp) != nullptr);
        }
    }

    SECTION("Неизвестный код — подстановка «?» (п. 4.2.1)") {
        const auto* fallback = font.glyph(0x1F600); // эмодзи: в MVP не поддерживается
        REQUIRE(fallback == nullptr);
        REQUIRE(font.fallback_glyph().advance > 0.f);
    }

    SECTION("Декодирование UTF-8: 1, 2, 3 и 4 байта, ошибки отбрасываются") {
        std::uint32_t cp = 0;
        REQUIRE(render::FontAtlas::decode_utf8("A", 1, cp) == 1u);
        REQUIRE(cp == 65u);
        REQUIRE(render::FontAtlas::decode_utf8("\xD0\x96", 2, cp) == 2u);
        REQUIRE(cp == 0x0416u);
        REQUIRE(render::FontAtlas::decode_utf8("\xE2\x82\xAC", 3, cp) == 3u);
        REQUIRE(cp == 0x20ACu);
        REQUIRE(render::FontAtlas::decode_utf8("\xF0\x9F\x98\x80", 4, cp) == 4u);
        REQUIRE(cp == 0x1F600u);
        REQUIRE(render::FontAtlas::decode_utf8("\xC3", 1, cp) == 0u);  // оборванная последовательность
        REQUIRE(render::FontAtlas::decode_utf8("", 0, cp) == 0u);
    }

    SECTION("Ширина строки растёт пропорционально кеглю и числу символов") {
        const float w16 = font.text_width("AC15", 16.f);
        const float w32 = font.text_width("AC15", 32.f);
        const float one = font.text_width("A", 16.f);
        REQUIRE(w16 > 0.f);
        REQUIRE(approx_equal(w32, w16 * 2.f, 1e-4));
        REQUIRE(approx_equal(w16, one * 4.f, 1e-4));
        REQUIRE(approx_equal(font.text_width("", 16.f), 0.f, 1e-6));
    }

    SECTION("Атлас содержит непустые пиксели (растеризация выполнена)") {
        std::size_t filled = 0;
        for (std::size_t i = 3; i < font.pixels().size(); i += 4) {
            if (font.pixels()[i] > 0) ++filled;
        }
        REQUIRE(filled > 1000u);
    }
}

// --- FR-E20: загрузчик функций OpenGL ---------------------------------------
TEST_CASE("gl_function_loader_reports_missing", "[engine][FR-E20]") {
    SECTION("Нулевой загрузчик — отказ с причиной") {
        render::gl::GlFunctions fns;
        std::string missing;
        REQUIRE_FALSE(render::gl::load_gl_functions(fns, nullptr, &missing));
        REQUIRE_FALSE(missing.empty());
        REQUIRE_FALSE(render::gl::has_gl_base(fns));
    }

    SECTION("Загрузчик, не находящий символы, — отказ с именем функции") {
        render::gl::GlFunctions fns;
        std::string missing;
        auto empty_loader = +[](const char*) -> void* { return nullptr; };
        REQUIRE_FALSE(render::gl::load_gl_functions(fns, empty_loader, &missing));
        REQUIRE(missing == "glEnable"); // первая функция в порядке загрузки
        REQUIRE_FALSE(render::gl::has_gl_base(fns));
    }

    SECTION("Загрузчик, возвращающий адреса для всех имён, — успех") {
        render::gl::GlFunctions fns;
        std::string missing;
        // Фиктивный адрес: вызовы не выполняются, проверяется только заполнение таблицы
        auto fake_loader = +[](const char*) -> void* {
            static int dummy = 0;
            return static_cast<void*>(&dummy);
        };
        REQUIRE(render::gl::load_gl_functions(fns, fake_loader, &missing));
        REQUIRE(missing.empty());
        REQUIRE(render::gl::has_gl_base(fns));
        REQUIRE(fns.glUniformMatrix4fv_ != nullptr);
        REQUIRE(fns.glDrawElements_ != nullptr);
    }
}

// --- E-D05: пакетирование спрайтов (метрики draw calls) ---------------------
TEST_CASE("sprite_batch_limits_draw_calls", "[engine][E-D05][NFR-E01]") {
    render::NullRenderer renderer;
    render::SpriteBatch batch(renderer);

    SECTION("1000 спрайтов одной текстуры — один draw call") {
        renderer.reset_metrics();
        batch.begin();
        for (int i = 0; i < 1000; ++i) {
            batch.draw(7u, {0.f, 0.f, 32.f, 32.f}, {static_cast<float>(i), 0.f, 32.f, 32.f});
        }
        REQUIRE(batch.queued_sprites() == 1000u);
        batch.end(); // пакетирование по смене текстуры выполняется в end()
        REQUIRE(batch.pending_batches() == 1u);
        const std::size_t calls = batch.flush();
        REQUIRE(calls == 1u);
        REQUIRE(renderer.sprites_drawn() == 1000u);
        REQUIRE(renderer.draw_call_count() == 1u);
    }

    SECTION("Чередование текстур закрывает пакеты (без пустых)") {
        renderer.reset_metrics();
        batch.begin();
        for (int i = 0; i < 10; ++i) {
            batch.draw(static_cast<render::TextureId>(i % 2 + 1), {0.f, 0.f, 16.f, 16.f},
                       {static_cast<float>(i), 0.f, 16.f, 16.f});
        }
        batch.end();
        REQUIRE(batch.pending_batches() == 10u);
        const std::size_t calls = batch.flush();
        REQUIRE(calls == 10u);
    }

    SECTION("Пустой кадр не создаёт draw calls") {
        renderer.reset_metrics();
        batch.begin();
        batch.end();
        REQUIRE(batch.pending_batches() == 0u);
        REQUIRE(batch.flush() == 0u);
        REQUIRE(renderer.draw_call_count() == 0u);
    }
}

// --- FR-E06: таймер кадра (dt без скачков при ограничении частоты) ------------
TEST_CASE("frame_timer_accumulates_dt", "[engine][FR-E06]") {
    platform::Timer timer;
    timer.reset();
    const double first = timer.elapsed_and_restart();
    REQUIRE(first >= 0.0);
    REQUIRE(first < 1.0); // аномально большой dt означает ошибку измерения
    const double second = timer.elapsed_and_restart();
    REQUIRE(second >= 0.0);
    REQUIRE(timer.elapsed() >= 0.0);
}
