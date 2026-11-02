// tests/benchmarks/benchmarks.cpp — контрольные замеры производительности
// (таблица 8.4 и NFR-E02…NFR-E06 ТЗ DOC-04).
//
// Каждый замер печатает фактическую производительность и сравнивает её с
// минимально допустимой. Порог намеренно выбран ниже норматива ТЗ: измерение
// выполняется на сборке Debug и на машинах CI разной мощности, поэтому порог
// фиксирует деградацию на порядок, а не колебания нагрузки.
//
//   NFR-E02 — отрисовка карты 20×15 и 1000 спрайтов в пределах бюджета кадра;
//   NFR-E03 — не менее 1 000 000 бросков костей в секунду;
//   NFR-E04 — загрузка ресурса с разбором JSON и валидацией схемы;
//   NFR-E05 — объём кэша и числа сущностей ECS (10 000 сущностей);
//   NFR-E06 — отклик UI: отрисовка дерева виджетов в пределах кадра 60 Гц.
//
// Запуск: benchmarks [--assets <каталог>] [--repeats N] [--strict]
//   --strict — код возврата 1, если фактическое значение ниже порога.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "core/file_system.h"
#include "core/log.h"
#include "ecs/registry.h"
#include "ecs/view.h"
#include "render/font_atlas.h"
#include "render/null_renderer.h"
#include "render/sprite_batch.h"
#include "resource/json_loader.h"
#include "resource/schema_registry.h"
#include "resource/schema_validator.h"
#include "rules/ac.h"
#include "rules/attack.h"
#include "rules/dice_roller.h"
#include "rules/random_provider.h"
#include "ui/ui_root.h"
#include "ui/widgets.h"

namespace {

using Clock = std::chrono::steady_clock;

// --- каркас замеров ----------------------------------------------------------

struct BenchmarkResult {
    std::string name;
    std::string requirement;   // ссылка на NFR/таблицу ТЗ
    std::uint64_t operations = 0;
    double best_ms = 0.0;      // лучшее время прогона (минимум шума)
    double per_second = 0.0;
    double threshold_per_second = 0.0;
    bool passed = true;
};

std::vector<BenchmarkResult> g_results;

double elapsed_ms(const Clock::time_point& start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// Прогон тела замера repeats раз; учитывается лучшее время (устойчиво к шуму ЦП)
template <typename Body>
double time_best_ms(int repeats, const Body& body) {
    double best = 1e18;
    for (int i = 0; i < repeats; ++i) {
        body();                       // прогрев: тело замера идемпотентно
        const auto start = Clock::now();
        body();
        best = std::min(best, elapsed_ms(start));
    }
    return best;
}

void record(const std::string& name, const std::string& requirement, std::uint64_t operations,
            double best_ms, double threshold_per_second) {
    BenchmarkResult r;
    r.name = name;
    r.requirement = requirement;
    r.operations = operations;
    r.best_ms = best_ms;
    r.per_second = best_ms > 0.0 ? static_cast<double>(operations) / (best_ms / 1000.0) : 0.0;
    r.threshold_per_second = threshold_per_second;
    r.passed = r.per_second >= threshold_per_second;
    g_results.push_back(r);
}

// --- компоненты ECS для замера (движок не знает игровой семантики, E-D01) ----

struct BenchTransform {
    float x = 0.f;
    float y = 0.f;
};

struct BenchSprite {
    int texture = 0;
};

// --- данные для замера загрузки ресурсов (NFR-E04) --------------------------

std::vector<std::string> collect_data_files(const engine::core::FileSystem& fs) {
    std::vector<std::string> files;
    const std::string dirs[] = {"assets:/data/", "assets:/maps/", "assets:/textures/"};
    for (const auto& dir : dirs) {
        std::vector<std::string> stack{dir};
        while (!stack.empty()) {
            const std::string current = stack.back();
            stack.pop_back();
            for (const auto& entry : fs.list_directory(current)) {
                if (!fs.list_directory(entry).empty()) {
                    stack.push_back(entry);
                } else if (entry.size() >= 5 &&
                           entry.compare(entry.size() - 5, 5, ".json") == 0 &&
                           entry.find("/schemas/") == std::string::npos) {
                    files.push_back(entry);
                }
            }
        }
    }
    std::sort(files.begin(), files.end());   // детерминированный порядок (NFR-E09)
    return files;
}

} // namespace

int main(int argc, char** argv) {
    std::string assets_root = "assets";
    int repeats = 3;
    bool strict = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--assets" && i + 1 < argc) assets_root = argv[++i];
        else if (arg == "--repeats" && i + 1 < argc) repeats = std::atoi(argv[++i]);
        else if (arg == "--strict") strict = true;
        else if (arg == "--help" || arg == "-h") {
            std::printf("benchmarks [--assets <каталог>] [--repeats N] [--strict]\n");
            return 0;
        }
    }
    if (repeats < 1) repeats = 1;

    // Журнал — только предупреждения и ошибки: вывод замеров должен оставаться
    // машиночитаемым (п. 4.1.10.1, FR-E08)
    engine::core::Log::instance().set_level(engine::core::LogLevel::Warn);

    std::printf("Контрольные замеры производительности (таблица 8.4, NFR-E02…NFR-E06)\n");
    std::printf("Сборка: %s; повторов на замер: %d; корень ассетов: %s\n\n",
#ifdef NDEBUG
                "Release",
#else
                "Debug",
#endif
                repeats, assets_root.c_str());

    // --- NFR-E03: броски костей ---------------------------------------------
    {
        constexpr std::uint64_t kRolls = 200000;
        rules::SeededRandomProvider rng(42);
        rules::DiceRoller roller(rng);
        std::uint64_t checksum = 0;
        const double ms = time_best_ms(repeats, [&] {
            for (std::uint64_t i = 0; i < kRolls; ++i) {
                // Преимущество/помеха чередуются: измеряются обе ветви (FR-K30)
                const auto r = roller.roll(15, 5, (i & 1u) == 0u, (i & 2u) == 2u);
                checksum += static_cast<std::uint64_t>(r.total);
            }
        });
        record("dice_roller.roll (1d20 ± преимущество/помеха)", "NFR-E03", kRolls, ms,
               300000.0); // норматив 1 000 000/с, порог — 30 %
        if (checksum == 0) std::printf("(контрольная сумма пуста — оптимизатор исключил цикл)\n");
    }

    // --- NFR-E03: расчёт атаки (бросок + итог против AC) ---------------------
    {
        constexpr std::uint64_t kAttacks = 100000;
        rules::SeededRandomProvider rng(7);
        rules::DiceRoller roller(rng);
        std::uint64_t hits = 0;
        const double ms = time_best_ms(repeats, [&] {
            for (std::uint64_t i = 0; i < kAttacks; ++i) {
                rules::AttackInput in;
                in.ability_modifier = 3;
                in.proficiency_bonus = 2;
                in.has_advantage = (i % 3u) == 0u;
                const auto out = rules::resolve_attack(roller, in, 15);
                hits += out.hit ? 1u : 0u;
            }
        });
        record("rules::resolve_attack", "NFR-E03, FR-K31", kAttacks, ms, 60000.0);
        if (hits == 0) std::printf("(попаданий не зафиксировано)\n");
    }

    // --- NFR-E03: расчёт AC (формула (5) DOC-01) -----------------------------
    {
        constexpr std::uint64_t kOps = 1000000;
        std::uint64_t sum = 0;
        const double ms = time_best_ms(repeats, [&] {
            for (std::uint64_t i = 0; i < kOps; ++i) {
                rules::ACInput in;
                in.base = 13;
                in.dex_modifier = static_cast<int>(i % 5u);
                in.has_shield = (i & 1u) == 0u;
                in.category = static_cast<rules::ArmorCategory>(i % 4u);
                sum += static_cast<std::uint64_t>(rules::calculate_ac(in));
            }
        });
        record("rules::calculate_ac", "NFR-E03, AC-K03", kOps, ms, 2000000.0);
        if (sum == 0) std::printf("(сумма AC равна нулю)\n");
    }

    // --- NFR-E04: загрузка данных с разбором JSON и валидацией схемы ---------
    {
        engine::core::FileSystem fs(assets_root);
        engine::resource::SchemaValidator validator;
        engine::resource::SchemaRegistry registry(fs, validator);
        registry.load_directory("assets:/schemas/");
        engine::resource::JsonLoader loader(fs, validator, nullptr);

        const auto files = collect_data_files(fs);
        if (files.empty()) {
            std::printf("ПРЕДУПРЕЖДЕНИЕ: файлы данных не найдены — замер загрузки пропущен\n");
        } else {
            // Прогоны: 20 полных проходов по набору файлов (кэш отключён —
            // load_uncached, чтобы измерялись чтение, разбор и валидация)
            const int passes = 20;
            std::uint64_t loaded = 0;
            const double ms = time_best_ms(repeats, [&] {
                loaded = 0;   // счётчик относится к одному прогону, повторы не суммируются
                for (int p = 0; p < passes; ++p) {
                    for (const auto& path : files) {
                        const auto doc = loader.load_uncached(path);
                        loaded += doc.is_ok() ? 1u : 0u;
                    }
                }
            });
            const std::uint64_t docs = static_cast<std::uint64_t>(files.size()) *
                                       static_cast<std::uint64_t>(passes);
            record("resource::JsonLoader (чтение + разбор + схема)", "NFR-E04, FR-E41…FR-E48",
                   docs, ms, 200.0); // норматив — 1000 док/с, порог 200 док/с
            std::printf("    (файлов данных: %zu; успешно загружено: %llu из %llu)\n",
                        files.size(), static_cast<unsigned long long>(loaded),
                        static_cast<unsigned long long>(docs));
        }
    }

    // --- NFR-E05: ECS — создание сущностей и добавление компонентов ----------
    {
        constexpr std::uint64_t kEntities = 10000;
        engine::ecs::Registry registry;
        std::uint64_t alive = 0;
        const double ms = time_best_ms(repeats, [&] {
            registry = engine::ecs::Registry{};
            for (std::uint64_t i = 0; i < kEntities; ++i) {
                const auto e = registry.create_entity();
                registry.add_component<BenchTransform>(e, static_cast<float>(i), 0.f);
                registry.add_component<BenchSprite>(e, static_cast<int>(i % 4u));
                alive += registry.is_alive(e) ? 1u : 0u;
            }
        });
        record("ecs: 10 000 сущностей × 2 компонента", "NFR-E05, FR-E60", kEntities, ms, 20000.0);
        if (alive == 0) std::printf("(сущности не созданы)\n");
    }

    // --- NFR-E05: ECS — обход представления (система «движения») -------------
    {
        engine::ecs::Registry registry;
        constexpr int kEntities = 10000;
        std::vector<engine::ecs::EntityId> entities;
        entities.reserve(kEntities);
        for (int i = 0; i < kEntities; ++i) {
            const auto e = registry.create_entity();
            registry.add_component<BenchTransform>(e, static_cast<float>(i), 0.f);
            registry.add_component<BenchSprite>(e, i % 4);
            entities.push_back(e);
        }
        float sink = 0.f;
        const double ms = time_best_ms(repeats, [&] {
            for (auto e : engine::ecs::make_view<BenchTransform, BenchSprite>(registry)) {
                auto& t = registry.get_component<BenchTransform>(e);
                t.x += 0.5f;
                sink += t.x;
            }
        });
        record("ecs: обход 10 000 сущностей (view + get_component)", "NFR-E05, FR-E62",
               kEntities, ms, 100000.0);
        if (sink == 0.f) std::printf("(обход не выполнен)\n");
    }

    // --- NFR-E02: пакетирование 1000 спрайтов (E-D05) ------------------------
    {
        engine::render::NullRenderer renderer;
        engine::render::SpriteBatch batch(renderer);
        constexpr std::uint64_t kSprites = 1000;
        const double ms = time_best_ms(repeats, [&] {
            batch.begin();
            for (std::uint64_t i = 0; i < kSprites; ++i) {
                batch.draw(1u, {0.f, 0.f, 32.f, 32.f},
                           {static_cast<float>(i % 40u) * 32.f, static_cast<float>(i / 40u) * 32.f,
                            32.f, 32.f});
            }
            batch.end();
            batch.flush();
        });
        record("render: пакет 1000 спрайтов (begin/draw/end/flush)", "NFR-E02, E-D05", kSprites,
               ms, 50000.0);
        std::printf("    (бюджет кадра 60 Гц = 16,67 мс; факт %.3f мс на 1000 спрайтов)\n", ms);
    }

    // --- NFR-E06: отрисовка дерева UI из 100 виджетов ------------------------
    {
        engine::ui::UiRoot root;
        root.resize(1280, 720);
        for (int p = 0; p < 10; ++p) {
            auto panel = std::make_shared<engine::ui::Panel>();
            panel->set_position(engine::ui::Vec2{static_cast<float>(p) * 120.f, 100.f});
            panel->set_size(engine::ui::Size{110.f, 300.f});
            panel->set_border_width(1.f);
            for (int c = 0; c < 9; ++c) {
                if (c % 3 == 0) {
                    auto label = std::make_shared<engine::ui::Label>("Строка " + std::to_string(c));
                    label->set_position(engine::ui::Vec2{4.f, static_cast<float>(c) * 30.f});
                    label->set_size(engine::ui::Size{100.f, 24.f});
                    panel->add_child(label);
                } else {
                    auto button = std::make_shared<engine::ui::Button>("Кнопка " + std::to_string(c));
                    button->set_position(engine::ui::Vec2{4.f, static_cast<float>(c) * 30.f});
                    button->set_size(engine::ui::Size{100.f, 26.f});
                    panel->add_child(button);
                }
            }
            root.add(panel);
        }
        engine::render::NullRenderer renderer;
        constexpr std::uint64_t kDraws = 200;
        const double ms = time_best_ms(repeats, [&] {
            for (std::uint64_t i = 0; i < kDraws; ++i) {
                renderer.reset_metrics();
                root.draw(renderer);
            }
        });
        record("ui: отрисовка дерева (10 панелей, 100 виджетов)", "NFR-E06, FR-E80", kDraws, ms,
               300.0); // 300 отрисовок/с ≈ 5 кадров 60 Гц с запасом
        std::printf("    (виджетов в дереве: %zu; примитивов за отрисовку: %zu)\n",
                    root.widget_count(), renderer.primitives_drawn());
    }

    // --- NFR-E05: запекание шрифтового атласа (однократно при старте) --------
    {
        constexpr std::uint64_t kBakes = 3;
        std::size_t pixels = 0;
        const double ms = time_best_ms(repeats, [&] {
            for (std::uint64_t i = 0; i < kBakes; ++i) {
                engine::render::FontAtlas font;
                font.bake();
                pixels = font.pixels().size();
            }
        });
        record("render: запекание шрифтового атласа 256×352", "NFR-E05, FR-E29", kBakes, ms, 3.0);
        std::printf("    (байт атласа: %zu)\n", pixels);
    }

    // --- сводка --------------------------------------------------------------
    // Ширина печатного поля: %-52s считает БАЙТЫ, поэтому кириллические названия
    // (UTF-8, 2 байта на знак) сдвигают таблицу — считаем знакоместа по символам.
    auto display_width = [](const std::string& text) {
        std::size_t width = 0;
        for (const char c : text) {
            // Продолжающие байты UTF-8 (10xxxxxx) не увеличивают ширину
            if ((static_cast<unsigned char>(c) & 0xC0u) != 0x80u) ++width;
        }
        return width;
    };
    auto pad = [&](const std::string& text, std::size_t width) {
        const std::size_t actual = display_width(text);
        return text + std::string(actual < width ? width - actual : 1, ' ');
    };

    constexpr std::size_t kNameWidth = 52;
    std::printf("\n%s%10s %12s %14s %14s  %s\n", pad("Замер", kNameWidth).c_str(),
                "Операций", "Время, мс", "Факт, 1/с", "Порог, 1/с", "Итог");
    std::printf("%s\n", std::string(116, '-').c_str());
    int failed = 0;
    for (const auto& r : g_results) {
        std::printf("%s%10llu %12.3f %14.0f %14.0f  %s\n", pad(r.name, kNameWidth).c_str(),
                    static_cast<unsigned long long>(r.operations), r.best_ms, r.per_second,
                    r.threshold_per_second, r.passed ? "ok" : "НИЖЕ ПОРОГА");
        if (!r.passed) ++failed;
    }
    std::printf("\nЗамеров: %zu; ниже порога: %d\n", g_results.size(), failed);
    if (failed != 0 && strict) return 1;
    return 0;
}
