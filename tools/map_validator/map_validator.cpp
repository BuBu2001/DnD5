// tools/map_validator — проверка карт (этап M11; данные карт — DATA-E04, FR-E33).
//
// Кроме соответствия схеме (schemas/map.schema.json) проверяются структурные
// инварианты, которые схемой не выражаются:
//   * число строк каждого слоя равно grid.rows, длина каждой строки — grid.columns;
//   * каждый символ слоя описан в tiles[].key (иначе клетка неотрисуема);
//   * точки появления (spawns) попадают в границы сетки и не накладываются друг на друга;
//   * клетка точки появления проходима (passable == true);
//   * идентификаторы сущностей точек появления имеют соответствующий файл данных
//     (assets:/data/characters/, assets:/data/classes/, assets:/data/enemies/);
//   * ровно один слой terrain и все слои имеют уникальные id;
//   * границы карты непроходимы (периметр) — предупреждение, если есть «дыра».
//
// Код возврата: 0 — карты корректны, 1 — есть ошибки, 2 — ошибка запуска.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "core/file_system.h"
#include "core/log.h"
#include "resource/json.h"
#include "resource/json_loader.h"
#include "resource/schema_registry.h"
#include "resource/schema_validator.h"

namespace {

using engine::core::FileSystem;
using engine::resource::JsonLoader;
using engine::resource::JsonParser;
using engine::resource::JsonValue;
using engine::resource::SchemaRegistry;
using engine::resource::SchemaValidator;

struct Options {
    std::string assets_root = "assets";
    std::vector<std::string> files;   // пусто — все карты в assets:/maps/
    bool verbose = false;
    bool warnings_as_errors = false;
};

void print_usage() {
    std::printf(
        "map_validator — проверка карт (сетка, слои, плитки, точки появления)\n"
        "\n"
        "Использование:\n"
        "  map_validator [--assets <каталог>] [--file <путь>]... [--verbose]\n"
        "                [--warnings-as-errors]\n"
        "\n"
        "Код возврата: 0 — успешно, 1 — есть ошибки, 2 — ошибка запуска\n");
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
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return false;
        } else if (arg == "--assets") {
            const char* v = next("--assets");
            if (!v) return false;
            out.assets_root = v;
        } else if (arg == "--file") {
            const char* v = next("--file");
            if (!v) return false;
            out.files.emplace_back(v);
        } else if (arg == "--verbose") {
            out.verbose = true;
        } else if (arg == "--warnings-as-errors") {
            out.warnings_as_errors = true;
        } else {
            std::fprintf(stderr, "ошибка: неизвестный параметр %s\n", arg.c_str());
            return false;
        }
    }
    return true;
}

bool is_physical(const std::string& path) {
    return !path.empty() && (path.front() == '/' || path.front() == '.' ||
                             (path.size() > 1 && path[1] == ':'));
}

std::string to_virtual(const std::string& path) {
    if (path.rfind("assets:/", 0) == 0) return path;
    if (is_physical(path)) return path;   // файл вне корня ассетов (например, в CI)
    std::string p = path;
    while (!p.empty() && p.front() == '/') p.erase(p.begin());
    if (p.rfind("assets/", 0) == 0) p = p.substr(std::string("assets/").size());
    return "assets:/" + p;
}

bool ends_with_json(const std::string& p) {
    return p.size() >= 5 && p.compare(p.size() - 5, 5, ".json") == 0;
}

// Чтение документа: виртуальный путь assets:/… — через FileSystem (DATA-E05),
// физический путь — напрямую (проверка файла вне корня ассетов, например в CI)
std::string read_any(const FileSystem& fs, const std::string& path, std::string& error) {
    if (!is_physical(path)) {
        const auto text = fs.read_text(path);
        if (!text.is_ok()) {
            error = text.error_message();
            return {};
        }
        return text.value();
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        error = "файл не найден: " + path;
        return {};
    }
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return text;
}

// Декодирование UTF-8 в кодовые точки: ключ плитки — один символ, но в UTF-8
// он может занимать 2–4 байта (п. 4.5.5), поэтому посимвольный обход обязателен.
std::vector<std::uint32_t> to_codepoints(const std::string& text) {
    std::vector<std::uint32_t> out;
    std::size_t i = 0;
    while (i < text.size()) {
        const auto b0 = static_cast<unsigned char>(text[i]);
        std::uint32_t cp = 0;
        std::size_t len = 1;
        if (b0 < 0x80) {
            cp = b0;
        } else if ((b0 & 0xE0) == 0xC0 && i + 1 < text.size()) {
            cp = (static_cast<std::uint32_t>(b0 & 0x1F) << 6) |
                 (static_cast<unsigned char>(text[i + 1]) & 0x3F);
            len = 2;
        } else if ((b0 & 0xF0) == 0xE0 && i + 2 < text.size()) {
            cp = (static_cast<std::uint32_t>(b0 & 0x0F) << 12) |
                 ((static_cast<std::uint32_t>(static_cast<unsigned char>(text[i + 1]) & 0x3F)) << 6) |
                 (static_cast<unsigned char>(text[i + 2]) & 0x3F);
            len = 3;
        } else if ((b0 & 0xF8) == 0xF0 && i + 3 < text.size()) {
            cp = (static_cast<std::uint32_t>(b0 & 0x07) << 18) |
                 ((static_cast<std::uint32_t>(static_cast<unsigned char>(text[i + 1]) & 0x3F)) << 12) |
                 ((static_cast<std::uint32_t>(static_cast<unsigned char>(text[i + 2]) & 0x3F)) << 6) |
                 (static_cast<unsigned char>(text[i + 3]) & 0x3F);
            len = 4;
        }
        out.push_back(cp);
        i += len;
    }
    return out;
}

struct Report {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    void error(const std::string& m) { errors.push_back(m); }
    void warn(const std::string& m) { warnings.push_back(m); }
};

// Проверка структурных инвариантов карты
void validate_map(const JsonValue& map, const FileSystem& fs, Report& report) {
    const JsonValue& grid = map.at("grid");
    const int columns = static_cast<int>(grid.at("columns").as_int(0));
    const int rows = static_cast<int>(grid.at("rows").as_int(0));
    const int cell_feet = static_cast<int>(grid.at("cell_feet").as_int(0));

    if (columns <= 0 || rows <= 0) {
        report.error("grid: размеры должны быть положительными");
        return;
    }
    // 1 клетка = 5 футов (DOC-01, DATA-E04): иное значение ломает расчёт дистанций
    if (cell_feet != 5) {
        report.error("grid.cell_feet: ожидается 5 (1 клетка = 5 футов, DATA-E04), получено " +
                     std::to_string(cell_feet));
    }

    // Ключи плиток и их проходимость
    std::map<std::uint32_t, std::string> tile_names;
    std::map<std::uint32_t, bool> tile_passable;
    std::set<std::uint32_t> duplicate_keys;
    for (const auto& tile : map.at("tiles").as_array()) {
        const std::string key_text = tile.at("key").as_string();
        const auto cps = to_codepoints(key_text);
        if (cps.size() != 1) {
            report.error("tiles[].key: ожидается ровно один символ, получено \"" + key_text + "\"");
            continue;
        }
        if (tile_names.count(cps[0]) != 0) duplicate_keys.insert(cps[0]);
        tile_names[cps[0]] = tile.at("name").as_string();
        tile_passable[cps[0]] = tile.at("passable").as_bool(false);
    }
    for (const auto cp : duplicate_keys) {
        report.error("tiles[].key: дублирующийся ключ U+" + std::to_string(cp));
    }

    // Слои: уникальный id, ровно один terrain, размеры строк
    const JsonValue& layers = map.at("layers");
    std::set<std::string> layer_ids;
    int terrain_layers = 0;
    // Карта проходимости по слою terrain (для проверки точек появления)
    std::vector<std::vector<std::uint32_t>> terrain_grid;

    for (const auto& layer : layers.as_array()) {
        const std::string id = layer.at("id").as_string();
        const std::string kind = layer.at("kind").as_string();
        if (layer_ids.count(id) != 0) report.error("layers[].id: дублирующийся идентификатор \"" + id + "\"");
        layer_ids.insert(id);
        if (kind == "terrain") ++terrain_layers;

        const auto& row_array = layer.at("rows").as_array();
        if (static_cast<int>(row_array.size()) != rows) {
            report.error("layers[\"" + id + "\"]: строк " + std::to_string(row_array.size()) +
                         ", ожидается " + std::to_string(rows) + " (grid.rows)");
            continue;
        }
        std::vector<std::vector<std::uint32_t>> decoded;
        for (int r = 0; r < rows; ++r) {
            const auto cps = to_codepoints(row_array[static_cast<std::size_t>(r)].as_string());
            if (static_cast<int>(cps.size()) != columns) {
                report.error("layers[\"" + id + "\"].rows[" + std::to_string(r) + "]: ширина " +
                             std::to_string(cps.size()) + ", ожидается " + std::to_string(columns));
            }
            for (const auto cp : cps) {
                if (tile_names.find(cp) == tile_names.end()) {
                    report.error("layers[\"" + id + "\"].rows[" + std::to_string(r) +
                                 "]: неописанный ключ плитки U+" + std::to_string(cp));
                }
            }
            decoded.push_back(cps);
        }
        if (kind == "terrain") terrain_grid = std::move(decoded);
    }
    if (terrain_layers != 1) {
        report.error("layers: ожидается ровно один слой с kind=\"terrain\", найдено " +
                     std::to_string(terrain_layers));
    }

    // Точки появления: границы, наложения, проходимость клетки
    std::set<std::pair<int, int>> occupied;
    for (const auto& spawn : map.at("spawns").as_array()) {
        const std::string entity_id = spawn.at("entity_id").as_string();
        const int column = static_cast<int>(spawn.at("column").as_int(-1));
        const int row = static_cast<int>(spawn.at("row").as_int(-1));
        const std::string where = "spawns[\"" + entity_id + "\"]";

        if (column < 0 || column >= columns || row < 0 || row >= rows) {
            report.error(where + ": клетка (" + std::to_string(column) + ", " +
                         std::to_string(row) + ") вне сетки " + std::to_string(columns) + "x" +
                         std::to_string(rows));
            continue;
        }
        if (!occupied.insert({column, row}).second) {
            report.error(where + ": клетка (" + std::to_string(column) + ", " +
                         std::to_string(row) + ") занята другой точкой появления");
        }
        if (!terrain_grid.empty() &&
            static_cast<std::size_t>(column) < terrain_grid[static_cast<std::size_t>(row)].size()) {
            const std::uint32_t cp = terrain_grid[static_cast<std::size_t>(row)][
                static_cast<std::size_t>(column)];
            const auto it = tile_passable.find(cp);
            if (it != tile_passable.end() && !it->second) {
                report.error(where + ": клетка занята непроходимой плиткой \"" +
                             tile_names[cp] + "\"");
            }
        }
        // Данные сущности должны существовать (иначе загрузка карты в игре
        // завершится ошибкой ресурса, п. 4.1.4.4)
        const std::string candidates[] = {
            "assets:/data/characters/" + entity_id + ".json",
            "assets:/data/enemies/" + entity_id + ".json",
            "assets:/data/classes/" + entity_id + ".json"};
        bool found = false;
        for (const auto& c : candidates) {
            if (fs.exists(c)) {
                found = true;
                break;
            }
        }
        if (!found) {
            // Файлы данных именуются по-разному (Char_Fighter ↔ fighter.json):
            // ищем по содержимому поля "id" в каталогах данных
            const std::string dirs[] = {"assets:/data/characters/", "assets:/data/enemies/",
                                        "assets:/data/classes/"};
            for (const auto& dir : dirs) {
                for (const auto& file : fs.list_directory(dir)) {
                    const auto text = fs.read_text(file);
                    if (!text.is_ok()) continue;
                    const auto parsed = JsonParser::parse(text.value());
                    if (!parsed.is_ok()) continue;
                    if (parsed.value().at("id").as_string() == entity_id) {
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
        }
        if (!found) {
            report.warn(where + ": не найден файл данных с id=\"" + entity_id + "\"");
        }
    }

    // Периметр: предупреждение о «дырах» в границах (герой может покинуть карту)
    if (!terrain_grid.empty()) {
        auto passable_at = [&](int column, int row) {
            if (row < 0 || row >= static_cast<int>(terrain_grid.size())) return false;
            const auto& line = terrain_grid[static_cast<std::size_t>(row)];
            if (column < 0 || column >= static_cast<int>(line.size())) return false;
            const auto it = tile_passable.find(line[static_cast<std::size_t>(column)]);
            return it != tile_passable.end() && it->second;
        };
        int holes = 0;
        for (int c = 0; c < columns; ++c) {
            if (passable_at(c, 0)) ++holes;
            if (passable_at(c, rows - 1)) ++holes;
        }
        for (int r = 0; r < rows; ++r) {
            if (passable_at(0, r)) ++holes;
            if (passable_at(columns - 1, r)) ++holes;
        }
        if (holes > 0) {
            report.warn("периметр карты проходим в " + std::to_string(holes) +
                        " клетках (границы должны быть непроходимы)");
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse_args(argc, argv, options)) return 2;
    engine::core::Log::instance().set_level(engine::core::LogLevel::Error);

    FileSystem fs(options.assets_root);
    SchemaValidator validator;
    SchemaRegistry registry(fs, validator);
    if (registry.load_directory("assets:/schemas/") == 0) {
        std::fprintf(stderr, "ошибка: схемы не найдены в %s/schemas\n", options.assets_root.c_str());
        return 2;
    }
    JsonLoader loader(fs, validator, nullptr);

    std::vector<std::string> targets;
    if (options.files.empty()) {
        for (const auto& entry : fs.list_directory("assets:/maps/")) {
            if (ends_with_json(entry) && fs.list_directory(entry).empty()) targets.push_back(entry);
        }
    } else {
        for (const auto& f : options.files) targets.push_back(to_virtual(f));
    }
    std::sort(targets.begin(), targets.end());
    if (targets.empty()) {
        std::printf("Карты не найдены: проверка не требуется\n");
        return 0;
    }

    std::size_t ok_count = 0;
    std::size_t fail_count = 0;
    std::size_t warn_count = 0;

    for (const auto& path : targets) {
        Report report;
        std::string document_id;
        if (is_physical(path)) {
            // Физический путь: разбор + проверка обёртки и схемы без загрузчика ресурсов
            std::string read_error;
            const std::string text = read_any(fs, path, read_error);
            if (text.empty() && !read_error.empty()) {
                report.error(read_error);
            } else {
                const auto parsed = JsonParser::parse(text);
                if (!parsed.is_ok()) {
                    report.error(parsed.error_message());
                } else {
                    const JsonValue& value = parsed.value();
                    document_id = value.at("id").as_string();
                    const auto envelope = validator.validate_envelope(value, /*expected=*/1);
                    if (!envelope.ok()) report.error(envelope.format(path));
                    const std::string schema_id = value.at("$schema").as_string();
                    if (validator.has_schema(schema_id)) {
                        const auto result = validator.validate(schema_id, value);
                        if (!result.ok()) report.error(result.format(path));
                    } else {
                        report.error("схема не найдена: " + schema_id);
                    }
                    if (report.errors.empty()) validate_map(value, fs, report);
                }
            }
        } else {
            const auto loaded = loader.load_uncached(path);
            if (!loaded.is_ok()) {
                report.error(loaded.error_message());
            } else {
                document_id = loaded.value().id;
                validate_map(loaded.value().value, fs, report);
            }
        }

        if (!report.warnings.empty()) {
            warn_count += report.warnings.size();
            for (const auto& w : report.warnings) {
                std::printf("[WARN] %s — %s\n", path.c_str(), w.c_str());
            }
        }
        if (!report.errors.empty()) {
            ++fail_count;
            for (const auto& e : report.errors) {
                std::printf("[FAIL] %s — %s\n", path.c_str(), e.c_str());
            }
        } else {
            ++ok_count;
            if (options.verbose) {
                std::printf("[ ok ] %s (id=%s)\n", path.c_str(),
                            document_id.empty() ? "?" : document_id.c_str());
            }
        }
    }

    std::printf("\nКарт проверено: %zu; корректно: %zu; с ошибками: %zu; предупреждений: %zu\n",
                targets.size(), ok_count, fail_count, warn_count);
    const bool failed = fail_count > 0 || (options.warnings_as_errors && warn_count > 0);
    return failed ? 1 : 0;
}
