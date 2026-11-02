// tools/asset_packer — упаковщик атласов спрайтов (этап M11; E-D05, FR-E21, FR-E22).
//
// Назначение: объединить набор мелких изображений в атлас(ы), чтобы отрисовка сцены
// укладывалась в бюджет draw calls (NFR-E01: не более 300 на кадр). Один атлас —
// одна текстура — один пакет спрайтов (E-D05).
//
// Алгоритм: полочная упаковка (shelf/next-fit decreasing height) с сортировкой
// кадров по убыванию высоты. Полки заполняются слева направо; кадр, не помещающийся
// по ширине, начинает новую полку. Страницы атласа — степени двойки (256…4096),
// при заполнении страницы создаётся следующая. Алгоритм детерминирован: одинаковый
// манифест всегда даёт одинаковую раскладку (NFR-E09).
//
// Вход: манифест JSON со списком кадров (размеры в пикселях) или сканирование
// каталога PNG (размер читается из заголовка IHDR — декодирование не требуется).
// Выход: атлас в формате данных движка (обёртка $schema/schema_version/id, п. 4.5.3)
// и отчёт об эффективности использования площади.
//
// Код возврата: 0 — успешно, 1 — ошибка упаковки/входных данных, 2 — ошибка запуска.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "core/file_system.h"
#include "core/log.h"
#include "resource/json.h"

namespace {

using engine::core::FileSystem;
using engine::resource::JsonArray;
using engine::resource::JsonObject;
using engine::resource::JsonParser;
using engine::resource::JsonValue;

struct Options {
    std::string assets_root = "assets";
    std::string manifest;                 // assets:/textures/<имя>.manifest.json
    std::string scan_dir;                 // альтернатива манифесту: каталог PNG
    std::string output;                   // путь записи атласа (физический)
    int padding = 2;                      // зазор между кадрами, px
    int border = 2;                       // отступ от края страницы, px
    int max_page_size = 2048;             // предельный размер страницы, px
    bool power_of_two = true;             // размеры страницы — степени двойки
    bool allow_rotation = false;          // поворот кадра на 90° (V1: не используется)
    bool verbose = false;
};

void print_usage() {
    std::printf(
        "asset_packer — упаковщик атласов спрайтов (shelf packing)\n"
        "\n"
        "Использование:\n"
        "  asset_packer --manifest assets:/textures/ui_icons.manifest.json --output out.json\n"
        "  asset_packer --scan assets:/textures/tokens --output out.json\n"
        "\n"
        "Параметры:\n"
        "  --assets <каталог>     корень ассетов, по умолчанию assets\n"
        "  --manifest <путь>      манифест со списком кадров (JSON)\n"
        "  --scan <каталог>       сканировать PNG и взять размеры из IHDR\n"
        "  --output <файл>        путь записи атласа (по умолчанию <имя>.atlas.json)\n"
        "  --padding N            зазор между кадрами, px (по умолчанию 2)\n"
        "  --border N             отступ от края страницы, px (по умолчанию 2)\n"
        "  --max-page-size N      предельный размер страницы, px (по умолчанию 2048)\n"
        "  --no-power-of-two      не приводить размер страницы к степени двойки\n"
        "  --verbose              печатать раскладку по кадрам\n"
        "\n"
        "Формат манифеста:\n"
        "  { \"atlas_id\": \"Atlas_Tokens\", \"texture\": \"textures/tokens.png\",\n"
        "    \"frames\": [ { \"id\": \"goblin\", \"w\": 32, \"h\": 32 }, ... ] }\n"
        "\n"
        "Код возврата: 0 — успешно, 1 — ошибка данных, 2 — ошибка запуска\n");
}

bool parse_args(int argc, char** argv, Options& out) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "ошибка: %s требует значения\n", name);
                return nullptr;
            }
            return argv[++i];
        };
        if (arg == "--help" || arg == "-h") { print_usage(); return false; }
        else if (arg == "--assets") { const char* v = next("--assets"); if (!v) return false; out.assets_root = v; }
        else if (arg == "--manifest") { const char* v = next("--manifest"); if (!v) return false; out.manifest = v; }
        else if (arg == "--scan") { const char* v = next("--scan"); if (!v) return false; out.scan_dir = v; }
        else if (arg == "--output") { const char* v = next("--output"); if (!v) return false; out.output = v; }
        else if (arg == "--padding") { const char* v = next("--padding"); if (!v) return false; out.padding = std::atoi(v); }
        else if (arg == "--border") { const char* v = next("--border"); if (!v) return false; out.border = std::atoi(v); }
        else if (arg == "--max-page-size") { const char* v = next("--max-page-size"); if (!v) return false; out.max_page_size = std::atoi(v); }
        else if (arg == "--no-power-of-two") { out.power_of_two = false; }
        else if (arg == "--verbose") { out.verbose = true; }
        else { std::fprintf(stderr, "ошибка: неизвестный параметр %s\n", arg.c_str()); return false; }
    }
    if (out.manifest.empty() && out.scan_dir.empty()) {
        std::fprintf(stderr, "ошибка: требуется --manifest или --scan\n");
        print_usage();
        return false;
    }
    return true;
}

std::string to_virtual(const std::string& path) {
    if (path.rfind("assets:/", 0) == 0) return path;
    std::string p = path;
    while (!p.empty() && p.front() == '/') p.erase(p.begin());
    if (p.rfind("assets/", 0) == 0) p = p.substr(std::string("assets/").size());
    return "assets:/" + p;
}

struct Frame {
    std::string id;
    std::string source;   // виртуальный путь исходного изображения
    int w = 0;
    int h = 0;
};

struct PlacedFrame {
    Frame frame;
    int page = 0;
    int x = 0;
    int y = 0;
    bool rotated = false;
};

struct Page {
    int width = 0;
    int height = 0;
    int used_area = 0;
};

// Размер PNG из IHDR: байты 16..23 — ширина и высота (big-endian), без декодирования
bool png_size(const std::vector<unsigned char>& bytes, int& w, int& h) {
    static const unsigned char signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (bytes.size() < 33) return false;
    for (int i = 0; i < 8; ++i) {
        if (bytes[static_cast<std::size_t>(i)] != signature[i]) return false;
    }
    // 8..15 — длина и тип chunks ("IHDR")
    if (bytes[12] != 'I' || bytes[13] != 'H' || bytes[14] != 'D' || bytes[15] != 'R') return false;
    w = (static_cast<int>(bytes[16]) << 24) | (static_cast<int>(bytes[17]) << 16) |
        (static_cast<int>(bytes[18]) << 8) | static_cast<int>(bytes[19]);
    h = (static_cast<int>(bytes[20]) << 24) | (static_cast<int>(bytes[21]) << 16) |
        (static_cast<int>(bytes[22]) << 8) | static_cast<int>(bytes[23]);
    return w > 0 && h > 0;
}

bool load_manifest(const FileSystem& fs, const Options& options, std::vector<Frame>& frames,
                   std::string& atlas_id, std::string& texture_key, std::string& error) {
    const std::string path = to_virtual(options.manifest);
    const auto text = fs.read_text(path);
    if (!text.is_ok()) {
        error = text.error_message();
        return false;
    }
    const auto parsed = JsonParser::parse(text.value());
    if (!parsed.is_ok()) {
        error = parsed.error_message();
        return false;
    }
    const JsonValue& root = parsed.value();
    atlas_id = root.at("atlas_id").as_string_or("Atlas_Untitled");
    texture_key = root.at("texture").as_string_or("textures/atlas.png");
    for (const auto& item : root.at("frames").as_array()) {
        Frame frame;
        frame.id = item.at("id").as_string();
        frame.w = static_cast<int>(item.at("w").as_int(0));
        frame.h = static_cast<int>(item.at("h").as_int(0));
        frame.source = item.at("source").as_string_or(texture_key);
        if (frame.id.empty()) {
            error = "frames[]: кадр без идентификатора";
            return false;
        }
        if (frame.w <= 0 || frame.h <= 0) {
            error = "frames[\"" + frame.id + "\"]: размер должен быть положительным";
            return false;
        }
        frames.push_back(frame);
    }
    if (frames.empty()) {
        error = "manifest: список frames пуст";
        return false;
    }
    return true;
}

bool load_scan(const FileSystem& fs, const Options& options, std::vector<Frame>& frames,
               std::string& atlas_id, std::string& texture_key, std::string& error) {
    const std::string dir = to_virtual(options.scan_dir);
    std::vector<std::string> files = fs.list_directory(dir);
    std::sort(files.begin(), files.end());   // детерминированный порядок (NFR-E09)
    for (const auto& path : files) {
        if (path.size() < 4 || path.compare(path.size() - 4, 4, ".png") != 0) continue;
        const auto bytes = fs.read_bytes(path);
        if (!bytes.is_ok()) {
            error = bytes.error_message();
            return false;
        }
        int w = 0;
        int h = 0;
        if (!png_size(bytes.value(), w, h)) {
            error = path + ": не удалось прочитать размер из IHDR";
            return false;
        }
        Frame frame;
        // Идентификатор кадра — имя файла без каталога и расширения
        std::string id = path;
        const auto slash = id.find_last_of('/');
        if (slash != std::string::npos) id = id.substr(slash + 1);
        id = id.substr(0, id.size() - 4);
        frame.id = id;
        frame.source = path;
        frame.w = w;
        frame.h = h;
        frames.push_back(frame);
    }
    if (frames.empty()) {
        error = "в каталоге " + dir + " нет файлов .png";
        return false;
    }
    atlas_id = "Atlas_" + dir.substr(dir.find_last_of('/') + 1);
    texture_key = dir.substr(std::string("assets:/").size()) + ".png";
    return true;
}

int next_power_of_two(int v) {
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}

// Результат одного варианта раскладки: страницы, размещённые кадры, эффективность
struct PackPlan {
    std::vector<Page> pages;
    std::vector<PlacedFrame> placed;
    std::size_t total_area = 0;
    std::size_t used_area = 0;
    double efficiency = 0.0;   // % использования площади
    std::string error;
    bool ok = false;
};

// Полочная упаковка (shelf first-fit decreasing height).
//
// Кадры сортируются по убыванию высоты, затем ширины, затем идентификатора —
// сортировка обеспечивает детерминированный результат (NFR-E09). Страницы имеют
// ОДИНАКОВЫЙ размер (минимальная степень двойки, вмещающая самый крупный кадр):
// растущие страницы приводили к почти пустой второй странице и падению
// эффективности использования площади (дефект первой реализации).
//
// Размещение выполняется в несколько проходов по оставшимся кадрам: на каждом
// проходе кадр помещается в первую подходящую полку, иначе открывает новую полку.
// Проход повторяется, пока размещается хотя бы один кадр, — это заполняет «хвосты»
// полок мелкими кадрами. Когда ни один кадр не помещается, страница закрывается.
// Упаковка при фиксированном размере страницы. Кадры, не поместившиеся на
// странице, переходят на следующую; число страниц ограничено (защита от зацикливания).
PackPlan pack_plan(const std::vector<Frame>& frames, const Options& options, int page_size) {
    PackPlan plan;
    std::vector<Frame> remaining = frames;
    std::sort(remaining.begin(), remaining.end(), [](const Frame& a, const Frame& b) {
        if (a.h != b.h) return a.h > b.h;      // decreasing height
        if (a.w != b.w) return a.w > b.w;
        return a.id < b.id;                    // детерминированный порядок равных (NFR-E09)
    });

    for (const auto& f : frames) {
        if (f.w + 2 * options.border > page_size || f.h + 2 * options.border > page_size) {
            plan.error = "кадр \"" + f.id + "\" (" + std::to_string(f.w) + "x" +
                         std::to_string(f.h) + ") не помещается в страницу " +
                         std::to_string(page_size) + " px";
            return plan;
        }
    }

    struct Shelf {
        int y = 0;
        int height = 0;
        int x_cursor = 0;
    };

    while (!remaining.empty()) {
        Page page;
        page.width = page_size;
        page.height = page_size;
        std::vector<Shelf> shelves;
        int y_cursor = options.border;
        const std::size_t before = remaining.size();
        bool progress = true;

        // Несколько проходов: мелкие кадры докладываются в «хвосты» прежних полок
        while (!remaining.empty() && progress) {
            progress = false;
            for (std::size_t i = 0; i < remaining.size();) {
                const Frame frame = remaining[i];
                const int frame_w = frame.w + options.padding;
                const int frame_h = frame.h + options.padding;
                const int limit = page_size - options.border;

                bool fitted = false;
                for (auto& shelf : shelves) {                 // first-fit по полкам
                    if (shelf.height >= frame_h && shelf.x_cursor + frame_w <= limit) {
                        plan.placed.push_back(
                            PlacedFrame{frame, static_cast<int>(plan.pages.size()), shelf.x_cursor,
                                        shelf.y, false});
                        shelf.x_cursor += frame_w;
                        page.used_area += frame.w * frame.h;
                        remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i));
                        fitted = true;
                        progress = true;
                        break;
                    }
                }
                if (fitted) continue;

                if (y_cursor + frame_h <= limit) {             // новая полка
                    plan.placed.push_back(
                        PlacedFrame{frame, static_cast<int>(plan.pages.size()), options.border,
                                    y_cursor, false});
                    page.used_area += frame.w * frame.h;
                    shelves.push_back(Shelf{y_cursor, frame_h, options.border + frame_w});
                    y_cursor += frame_h;
                    remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i));
                    progress = true;
                    continue;
                }
                ++i; // кадр ждёт следующей страницы
            }
        }

        if (remaining.size() == before) {
            plan.error = "страница " + std::to_string(page_size) +
                         " px не приняла ни одного кадра — упаковка невозможна";
            return plan;
        }
        plan.pages.push_back(page);
        if (plan.pages.size() > 64) {
            plan.error = "превышено число страниц атласа (64)";
            return plan;
        }
    }

    for (const auto& page : plan.pages) {
        plan.total_area += static_cast<std::size_t>(page.width) * page.height;
        plan.used_area += static_cast<std::size_t>(page.used_area);
    }
    plan.efficiency = plan.total_area > 0
                          ? 100.0 * static_cast<double>(plan.used_area) /
                                static_cast<double>(plan.total_area)
                          : 0.0;
    std::sort(plan.placed.begin(), plan.placed.end(),
              [](const PlacedFrame& a, const PlacedFrame& b) { return a.frame.id < b.frame.id; });
    plan.ok = true;
    return plan;
}

// Выбор размера страницы: минимум числа страниц, при равенстве — максимум
// эффективности использования площади (метрика этапа M11). Кандидаты — степени
// двойки от минимально возможной до --max-page-size.
bool pack_frames(const std::vector<Frame>& frames, const Options& options,
                 std::vector<PlacedFrame>& placed, std::vector<Page>& pages,
                 std::string& error) {
    if (options.padding < 0 || options.border < 0) {
        error = "padding и border не могут быть отрицательными";
        return false;
    }
    if (frames.empty()) {
        error = "нет кадров для упаковки";
        return false;
    }

    int max_w = 0;
    int max_h = 0;
    for (const auto& f : frames) {
        max_w = std::max(max_w, f.w);
        max_h = std::max(max_h, f.h);
    }
    const int min_size = options.power_of_two
                             ? std::max(64, next_power_of_two(std::max(max_w, max_h) +
                                                              2 * options.border))
                             : options.max_page_size;
    if (min_size > options.max_page_size) {
        error = "крупнейший кадр " + std::to_string(max_w) + "x" + std::to_string(max_h) +
                " не помещается в страницу " + std::to_string(options.max_page_size) + " px";
        return false;
    }

    PackPlan best;
    for (int size = min_size; size <= options.max_page_size; size *= 2) {
        const PackPlan plan = pack_plan(frames, options, size);
        if (!plan.ok) continue;
        const bool better = !best.ok || plan.pages.size() < best.pages.size() ||
                            (plan.pages.size() == best.pages.size() &&
                             plan.efficiency > best.efficiency + 1e-9);
        if (better) best = plan;
        if (!options.power_of_two) break;   // размер задан явно — перебор не нужен
    }

    if (!best.ok) {
        error = "не удалось упаковать кадры при ограничении страницы " +
                std::to_string(options.max_page_size) + " px";
        return false;
    }
    placed = std::move(best.placed);
    pages = std::move(best.pages);
    return true;
}

JsonValue build_atlas(const std::string& atlas_id, const std::vector<PlacedFrame>& placed,
                      const std::vector<Page>& pages, const Options& options) {
    JsonObject root;
    root["$schema"] = JsonValue("schemas/atlas.schema.json");
    root["schema_version"] = JsonValue(1);
    root["id"] = JsonValue(atlas_id);
    root["padding"] = JsonValue(options.padding);
    root["border"] = JsonValue(options.border);

    JsonArray page_array;
    for (std::size_t i = 0; i < pages.size(); ++i) {
        JsonObject page;
        page["index"] = JsonValue(static_cast<int>(i));
        page["width"] = JsonValue(pages[i].width);
        page["height"] = JsonValue(pages[i].height);
        page["texture"] = JsonValue("textures/" + atlas_id + "_" + std::to_string(i) + ".png");
        page_array.push_back(JsonValue(page));
    }
    root["pages"] = JsonValue(page_array);

    JsonArray frames_array;
    for (const auto& pf : placed) {
        JsonObject frame;
        frame["id"] = JsonValue(pf.frame.id);
        frame["page"] = JsonValue(pf.page);
        JsonObject position;
        position["x"] = JsonValue(pf.x);
        position["y"] = JsonValue(pf.y);
        position["w"] = JsonValue(pf.frame.w);
        position["h"] = JsonValue(pf.frame.h);
        frame["frame"] = JsonValue(position);
        frame["rotated"] = JsonValue(pf.rotated);
        frame["source"] = JsonValue(pf.frame.source);
        frames_array.push_back(JsonValue(frame));
    }
    root["frames"] = JsonValue(frames_array);
    return JsonValue(root);
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse_args(argc, argv, options)) return 2;
    engine::core::Log::instance().set_level(engine::core::LogLevel::Error);

    FileSystem fs(options.assets_root);
    std::vector<Frame> frames;
    std::string atlas_id;
    std::string texture_key;
    std::string error;

    const bool loaded = options.manifest.empty()
                            ? load_scan(fs, options, frames, atlas_id, texture_key, error)
                            : load_manifest(fs, options, frames, atlas_id, texture_key, error);
    if (!loaded) {
        std::fprintf(stderr, "ошибка входных данных: %s\n", error.c_str());
        return 1;
    }

    std::vector<PlacedFrame> placed;
    std::vector<Page> pages;
    if (!pack_frames(frames, options, placed, pages, error)) {
        std::fprintf(stderr, "ошибка упаковки: %s\n", error.c_str());
        return 1;
    }

    const JsonValue atlas = build_atlas(atlas_id, placed, pages, options);
    const std::string text = atlas.dump(2) + "\n";

    const std::string output_path =
        options.output.empty() ? atlas_id + ".atlas.json" : options.output;
    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::fprintf(stderr, "ошибка: не удалось открыть %s для записи\n", output_path.c_str());
        return 2;
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    out.close();

    // Отчёт: эффективность использования площади (метрика этапа M11)
    std::size_t total_area = 0;
    for (const auto& page : pages) {
        total_area += static_cast<std::size_t>(page.width) * static_cast<std::size_t>(page.height);
    }
    std::size_t used_area = 0;
    for (const auto& pf : placed) {
        used_area += static_cast<std::size_t>(pf.frame.w) * static_cast<std::size_t>(pf.frame.h);
    }
    // Проверка пересечений кадров: раскладка не должна накладывать кадры друг на друга
    for (std::size_t i = 0; i < placed.size(); ++i) {
        for (std::size_t j = i + 1; j < placed.size(); ++j) {
            const PlacedFrame& a = placed[i];
            const PlacedFrame& b = placed[j];
            if (a.page != b.page) continue;
            const bool overlap = a.x < b.x + b.frame.w && b.x < a.x + a.frame.w &&
                                 a.y < b.y + b.frame.h && b.y < a.y + a.frame.h;
            if (overlap) {
                std::fprintf(stderr, "ошибка упаковки: кадры %s и %s пересекаются\n",
                             a.frame.id.c_str(), b.frame.id.c_str());
                return 1;
            }
        }
    }
    const double efficiency = total_area > 0
                                  ? 100.0 * static_cast<double>(used_area) /
                                        static_cast<double>(total_area)
                                  : 0.0;

    std::printf("Атлас: %s\n", atlas_id.c_str());
    std::printf("Кадров: %zu; страниц: %zu\n", placed.size(), pages.size());
    for (std::size_t i = 0; i < pages.size(); ++i) {
        std::printf("  страница %zu: %dx%d px\n", i, pages[i].width, pages[i].height);
    }
    std::printf("Использование площади: %zu из %zu px² (%.1f%%)\n", used_area, total_area,
                efficiency);
    std::printf("Результат: %s (%zu байт)\n", output_path.c_str(), text.size());
    if (options.verbose) {
        std::printf("Раскладка:\n");
        for (const auto& pf : placed) {
            std::printf("  %-24s стр.%d (%d,%d) %dx%d\n", pf.frame.id.c_str(), pf.page, pf.x,
                        pf.y, pf.frame.w, pf.frame.h);
        }
    }
    (void)texture_key;
    return 0;
}
