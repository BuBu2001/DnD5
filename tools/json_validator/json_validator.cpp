// tools/json_validator — проверка файлов данных по схемам (этап M11, DOC-04 п. 4.5.3).
//
// Назначение: запуск в CI до сборки игры. Инструмент использует те же подсистемы,
// что и движок (resource::SchemaRegistry, resource::SchemaValidator, resource::JsonLoader),
// поэтому результат проверки совпадает с поведением игры при загрузке данных
// (FR-E47, FR-E48, DATA-E02). Ошибка одного файла не прерывает проверку остальных
// (п. 4.1.4.4) — в конце печатается сводка.
//
// Код возврата: 0 — все файлы соответствуют схемам; 1 — есть несоответствия;
// 2 — ошибка аргументов или недоступен корень ассетов.
//
// Запуск:
//   json_validator --assets assets                       # data/, maps/, schemas/
//   json_validator --assets assets --schema-version 1     # сверить версию схемы
//   json_validator --assets assets --file data/items/longsword.json
//   json_validator --assets assets --verbose              # печатать успешные файлы
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
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
    int expected_schema_version = -1;   // <0 — не проверять (п. 4.5.3)
    bool verbose = false;
    bool envelope_only = false;
    std::vector<std::string> files;     // виртуальные пути assets:/…
};

void print_usage() {
    std::printf(
        "json_validator — проверка данных по схемам JSON (ТЗ DOC-04, п. 4.5.3)\n"
        "\n"
        "Использование:\n"
        "  json_validator [--assets <каталог>] [--file <путь>]... [--schema-version N]\n"
        "                 [--envelope-only] [--verbose]\n"
        "\n"
        "Параметры:\n"
        "  --assets <каталог>   корень ассетов (префикс assets:/), по умолчанию assets\n"
        "  --file <путь>        проверить файл или каталог; без параметра проверяются\n"
        "                       assets:/data/, assets:/maps/, assets:/schemas/\n"
        "  --schema-version N   ожидать schema_version == N\n"
        "  --envelope-only      проверить только обёртку ($schema, schema_version, id)\n"
        "  --verbose            печатать успешно проверенные файлы\n"
        "\n"
        "Код возврата: 0 — успешно, 1 — есть несоответствия, 2 — ошибка запуска\n");
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
            if (v == nullptr) return false;
            out.assets_root = v;
        } else if (arg == "--file") {
            const char* v = next("--file");
            if (v == nullptr) return false;
            out.files.emplace_back(v);
        } else if (arg == "--schema-version") {
            const char* v = next("--schema-version");
            if (v == nullptr) return false;
            out.expected_schema_version = std::atoi(v);
        } else if (arg == "--envelope-only") {
            out.envelope_only = true;
        } else if (arg == "--verbose") {
            out.verbose = true;
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

// Приведение пути к виртуальному виду assets:/… (DATA-E05: ОС-пути в данных запрещены)
std::string to_virtual(const std::string& path) {
    if (path.rfind("assets:/", 0) == 0) return path;
    if (is_physical(path)) return path;   // файл вне корня ассетов (например, в CI)
    std::string p = path;
    while (!p.empty() && p.front() == '/') p.erase(p.begin());
    if (p.rfind("assets/", 0) == 0) p = p.substr(std::string("assets/").size());
    return "assets:/" + p;
}

bool ends_with_json(const std::string& path) {
    return path.size() >= 5 && path.compare(path.size() - 5, 5, ".json") == 0;
}

// Чтение документа: assets:/… — через FileSystem (DATA-E05), физический путь — напрямую
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
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// Рекурсивный обход каталога: *.json в алфавитном порядке (детерминизм, NFR-E09)
void collect_json(const FileSystem& fs, const std::string& virtual_dir,
                  std::vector<std::string>& out) {
    for (const auto& entry : fs.list_directory(virtual_dir)) {
        if (!fs.list_directory(entry).empty()) {
            collect_json(fs, entry, out);      // вложенный каталог
        } else if (ends_with_json(entry)) {
            out.push_back(entry);
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse_args(argc, argv, options)) return 2;

    // Журнал инструментов — только ошибки: вывод остаётся машиночитаемым
    engine::core::Log::instance().set_level(engine::core::LogLevel::Error);

    FileSystem fs(options.assets_root);
    if (fs.list_directory("assets:/schemas/").empty()) {
        std::fprintf(stderr, "ошибка: каталог схем недоступен: %s/schemas\n",
                     options.assets_root.c_str());
        return 2;
    }

    SchemaValidator validator;
    SchemaRegistry registry(fs, validator);
    const std::size_t schemas = registry.load_directory("assets:/schemas/");
    std::printf("Схем загружено: %zu\n", schemas);
    if (schemas == 0) {
        std::fprintf(stderr, "ошибка: схемы не найдены — проверка невозможна\n");
        return 2;
    }

    JsonLoader loader(fs, validator, /*bus=*/nullptr);
    loader.set_expected_schema_version(options.expected_schema_version);

    std::vector<std::string> targets;
    if (options.files.empty()) {
        collect_json(fs, "assets:/data/", targets);
        collect_json(fs, "assets:/maps/", targets);
        collect_json(fs, "assets:/schemas/", targets);
    } else {
        for (const auto& f : options.files) {
            const std::string virtual_path = to_virtual(f);
            if (!fs.list_directory(virtual_path).empty()) {
                collect_json(fs, virtual_path, targets);
            } else {
                targets.push_back(virtual_path);
            }
        }
    }
    std::sort(targets.begin(), targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());

    if (targets.empty()) {
        std::fprintf(stderr, "ошибка: не найдено ни одного файла .json\n");
        return 2;
    }

    std::size_t ok_count = 0;
    std::size_t fail_count = 0;
    std::vector<std::string> failures;

    auto report_fail = [&](const std::string& path, const std::string& reason) {
        ++fail_count;
        failures.push_back(path + ": " + reason);
        std::printf("[FAIL] %s — %s\n", path.c_str(), reason.c_str());
    };

    for (const auto& path : targets) {
        const bool is_schema = path.find("/schemas/") != std::string::npos;

        std::string read_error;
        const std::string text = read_any(fs, path, read_error);
        if (text.empty() && !read_error.empty()) {
            report_fail(path, read_error);
            continue;
        }
        const auto parsed = JsonParser::parse(text);
        if (!parsed.is_ok()) {
            report_fail(path, parsed.error_message());
            continue;
        }

        if (is_schema) {
            // Схема обязана содержать schema_id и type: иначе реестр её не зарегистрирует
            const auto& value = parsed.value();
            std::string problem;
            if (!value.is_object()) problem = "ожидается объект";
            else if (!value.contains("schema_id")) problem = "нет поля schema_id";
            else if (!value.contains("type")) problem = "нет поля type";
            if (!problem.empty()) report_fail(path, problem);
            else {
                ++ok_count;
                if (options.verbose) std::printf("[ ok ] %s (схема)\n", path.c_str());
            }
            continue;
        }

        if (options.envelope_only) {
            const auto result =
                validator.validate_envelope(parsed.value(), options.expected_schema_version);
            if (result.ok()) {
                ++ok_count;
                if (options.verbose) std::printf("[ ok ] %s (обёртка)\n", path.c_str());
            } else {
                report_fail(path, result.format(path));
            }
            continue;
        }

        if (is_physical(path)) {
            // Физический путь: обёртка + схема проверяются напрямую (загрузчик работает
            // только с виртуальными путями assets:/…, DATA-E05)
            const JsonValue& value = parsed.value();
            const auto envelope = validator.validate_envelope(value, options.expected_schema_version);
            if (!envelope.ok()) {
                report_fail(path, envelope.format(path));
                continue;
            }
            const std::string schema_id = value.at("$schema").as_string();
            if (!validator.has_schema(schema_id)) {
                report_fail(path, "схема не найдена: " + schema_id);
                continue;
            }
            const auto result = validator.validate(schema_id, value);
            if (!result.ok()) {
                report_fail(path, result.format(path));
                continue;
            }
            ++ok_count;
            if (options.verbose) {
                std::printf("[ ok ] %s (id=%s, schema_version=%d)\n", path.c_str(),
                            value.at("id").as_string().c_str(),
                            static_cast<int>(value.at("schema_version").as_int(0)));
            }
            continue;
        }
        const auto loaded = loader.load_uncached(path);
        if (!loaded.is_ok()) {
            report_fail(path, loaded.error_message());
            continue;
        }
        ++ok_count;
        if (options.verbose) {
            std::printf("[ ok ] %s (id=%s, schema_version=%d)\n", path.c_str(),
                        loaded.value().id.c_str(), loaded.value().schema_version);
        }
    }

    std::printf("\nПроверено файлов: %zu; соответствует: %zu; несоответствий: %zu\n",
                targets.size(), ok_count, fail_count);
    if (!failures.empty()) {
        std::printf("Файлы с несоответствиями:\n");
        for (const auto& f : failures) std::printf("  - %s\n", f.c_str());
    }
    return fail_count == 0 ? 0 : 1;
}
