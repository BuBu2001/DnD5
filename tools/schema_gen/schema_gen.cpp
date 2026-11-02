// tools/schema_gen — генерация артефактов по схемам данных (этап M11, DR-02).
//
// Читает assets:/schemas/*.schema.json и формирует:
//   1) справочник данных в Markdown (docs/gost/data_formats.md) — таблица полей:
//      тип, обязательность, ограничения, допустимые значения (DATA-E02, п. 4.5.3);
//   2) заголовочный файл C++ с идентификаторами схем и перечислениями
//      (generated/schema_ids.h) — единая точка правды вместо «магических строк».
//
// Инструмент НЕ содержит знаний о правилах D&D (E-D01): он оперирует только
// структурой схем. Вывод детерминирован: файлы сортируются, ключи обходятся
// в порядке std::map (NFR-E09) — повторный запуск не меняет результат.
//
// Код возврата: 0 — успешно, 1 — ошибка схемы, 2 — ошибка аргументов/ввода-вывода.
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "core/file_system.h"
#include "core/log.h"
#include "resource/json.h"
#include "resource/schema_validator.h"

namespace {

using engine::core::FileSystem;
using engine::resource::JsonArray;
using engine::resource::JsonObject;
using engine::resource::JsonParser;
using engine::resource::JsonValue;

struct Options {
    std::string assets_root = "assets";
    std::string markdown_out = "docs/gost/data_formats.md";
    std::string header_out = "generated/schema_ids.h";
    bool print_only = false;   // не писать файлы, печатать в stdout
};

void print_usage() {
    std::printf(
        "schema_gen — генерация справочника данных и констант по схемам JSON\n"
        "\n"
        "Использование:\n"
        "  schema_gen [--assets <каталог>] [--markdown <файл>] [--header <файл>] [--stdout]\n"
        "\n"
        "Параметры:\n"
        "  --assets <каталог>   корень ассетов, по умолчанию assets\n"
        "  --markdown <файл>    путь справочника данных (по умолчанию docs/gost/data_formats.md)\n"
        "  --header <файл>      путь заголовка C++ (по умолчанию generated/schema_ids.h)\n"
        "  --stdout             печать результата вместо записи файлов\n"
        "\n"
        "Код возврата: 0 — успешно, 1 — ошибка схемы, 2 — ошибка запуска\n");
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
        } else if (arg == "--markdown") {
            const char* v = next("--markdown");
            if (!v) return false;
            out.markdown_out = v;
        } else if (arg == "--header") {
            const char* v = next("--header");
            if (!v) return false;
            out.header_out = v;
        } else if (arg == "--stdout") {
            out.print_only = true;
        } else {
            std::fprintf(stderr, "ошибка: неизвестный параметр %s\n", arg.c_str());
            return false;
        }
    }
    return true;
}

bool ends_with_json(const std::string& p) {
    return p.size() >= 5 && p.compare(p.size() - 5, 5, ".json") == 0;
}

std::string schema_name_of(const std::string& schema_id) {
    // "schemas/condition.schema.json" → "condition"
    std::string name = schema_id;
    const auto slash = name.find_last_of('/');
    if (slash != std::string::npos) name = name.substr(slash + 1);
    const auto dot = name.find(".schema.json");
    if (dot != std::string::npos) name = name.substr(0, dot);
    return name;
}

// Имя в CamelCase без префикса: primary_ability → PrimaryAbility
std::string to_camel(const std::string& name) {
    std::string out;
    bool upper_next = true;
    for (char c : name) {
        if (c == '_' || c == '-' || c == '.') {
            upper_next = true;
            continue;
        }
        if (upper_next) {
            out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            upper_next = false;
        } else {
            out += c;
        }
    }
    return out;
}

// Идентификатор схемы → имя C++: condition → kSchemaCondition
std::string to_const_name(const std::string& name) {
    return "kSchema" + to_camel(name);
}

// Тип поля в терминах справочника: «string», «integer ≥ 1», «array of object», …
std::string describe_type(const JsonValue& node) {
    const JsonValue* type = node.find("type");
    std::string text = type != nullptr ? type->as_string() : "any";
    if (text == "array") {
        const JsonValue* items = node.find("items");
        if (items != nullptr) text = "array of " + describe_type(*items);
    }
    return text;
}

// Ограничения поля для справочника (DATA-E02: ограничения проверяются валидатором)
std::string describe_constraints(const JsonValue& node) {
    std::vector<std::string> parts;
    if (const JsonValue* v = node.find("minimum")) parts.push_back("минимум " + std::to_string(v->as_int()));
    if (const JsonValue* v = node.find("maximum")) parts.push_back("максимум " + std::to_string(v->as_int()));
    if (const JsonValue* v = node.find("minLength")) parts.push_back("длина ≥ " + std::to_string(v->as_int()));
    if (const JsonValue* v = node.find("maxLength")) parts.push_back("длина ≤ " + std::to_string(v->as_int()));
    if (const JsonValue* v = node.find("minItems")) parts.push_back("элементов ≥ " + std::to_string(v->as_int()));
    if (const JsonValue* v = node.find("maxItems")) parts.push_back("элементов ≤ " + std::to_string(v->as_int()));
    if (const JsonValue* v = node.find("enum")) {
        std::string values;
        for (const auto& item : v->as_array()) {
            if (!values.empty()) values += ", ";
            values += item.as_string();
        }
        parts.push_back("одно из: " + values);
    }
    if (const JsonValue* v = node.find("$ref")) parts.push_back("ссылка " + v->as_string());
    if (const JsonValue* v = node.find("pattern")) parts.push_back("шаблон " + v->as_string());
    if (parts.empty()) return "—";
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) out += "; ";
        out += parts[i];
    }
    // Экранирование '|' для ячеек таблицы Markdown
    for (auto& c : out) {
        if (c == '|') c = '/';
    }
    return out;
}

bool required_list_contains(const JsonValue& parent, const std::string& key) {
    const JsonValue* required = parent.find("required");
    if (required == nullptr || !required->is_array()) return false;
    for (const auto& item : required->as_array()) {
        if (item.as_string() == key) return true;
    }
    return false;
}

struct SchemaDoc {
    std::string path;         // assets:/schemas/x.schema.json
    std::string schema_id;    // schemas/x.schema.json
    std::string name;         // x
    JsonValue root;
};

// Обход свойств объекта: строки таблицы справочника (рекурсия до 3 уровней)
void emit_object_table(const JsonValue& node, const std::string& prefix, std::string& out,
                       int depth) {
    const JsonValue* props = node.find("properties");
    if (props == nullptr) return;
    for (const auto& [key, child] : props->as_object()) {
        const std::string path = prefix.empty() ? key : prefix + "." + key;
        const std::string indent(depth * 2, ' ');
        out += "| " + indent + "`" + path + "` | " + describe_type(child) + " | " +
               (required_list_contains(node, key) ? "да" : "нет") + " | " +
               describe_constraints(child) + " |\n";
        // Вложенные структуры: объект и массив объектов раскрываются рекурсивно
        if (depth < 3) {
            if (child.is_object()) {
                const JsonValue* child_type = child.find("type");
                if (child_type != nullptr && child_type->as_string() == "object") {
                    emit_object_table(child, path, out, depth + 1);
                } else if (child_type != nullptr && child_type->as_string() == "array") {
                    const JsonValue* items = child.find("items");
                    if (items != nullptr) emit_object_table(*items, path + "[]", out, depth + 1);
                }
            }
        }
    }
}

std::string markdown_for(const SchemaDoc& doc) {
    std::string out;
    out += "### " + doc.name + "\n\n";
    out += "Файл схемы: `" + doc.path + "`  \n";
    out += "Идентификатор (`$schema` в данных): `" + doc.schema_id + "`\n\n";
    if (const JsonValue* d = doc.root.find("description")) {
        out += d->as_string() + "\n\n";
    }
    out += "| Поле | Тип | Обязательное | Ограничения |\n";
    out += "| --- | --- | --- | --- |\n";
    emit_object_table(doc.root, "", out, 0);
    out += "\n";
    return out;
}

std::string header_for(const std::vector<SchemaDoc>& docs) {
    std::string out;
    out += "// generated/schema_ids.h — ФАЙЛ СГЕНЕРИРОВАН tools/schema_gen, НЕ РЕДАКТИРОВАТЬ.\n";
    out += "// Источник: assets/schemas/*.schema.json. Повторная генерация:\n";
    out += "//   ./build/tools/schema_gen/schema_gen --assets assets\n";
    out += "#pragma once\n\n";
    out += "#include <array>\n#include <string_view>\n\n";
    out += "namespace dnd::data {\n\n";
    out += "// Идентикаторы схем: значение поля \"$schema\" в файлах данных (п. 4.5.3)\n";
    for (const auto& d : docs) {
        out += "inline constexpr std::string_view " + to_const_name(d.name) + " = \"" +
               d.schema_id + "\";\n";
    }
    out += "\n// Текущая версия формата данных (DATA-E02: проверка при загрузке и миграции)\n";
    out += "inline constexpr int kSchemaVersion = 1;\n\n";
    out += "// Перечисления, извлечённые из схем (единая точка правды вместо литералов).\n";
    out += "// Имя формируется как k<Схема><Поле>Values, например kItemDamageTypeValues.\n";
    for (const auto& d : docs) {
        const JsonValue* defs = d.root.find("definitions");
        const JsonValue* props = d.root.find("properties");
        std::set<std::string> emitted;  // защита от дублей при совпадении имён полей
        auto emit_enum = [&](const JsonValue& node) {
            for (const auto& [key, child] : node.as_object()) {
                const JsonValue* en = child.find("enum");
                if (en == nullptr || en->as_array().empty()) continue;
                const std::string enum_name = "k" + to_camel(d.name) + to_camel(key) + "Values";
                if (!emitted.insert(enum_name).second) continue;
                out += "inline constexpr std::array<std::string_view, " +
                       std::to_string(en->size()) + "> " + enum_name + " = {";
                bool first = true;
                for (const auto& item : en->as_array()) {
                    if (!first) out += ", ";
                    out += "\"" + item.as_string() + "\"";
                    first = false;
                }
                out += "};\n";
            }
        };
        if (props != nullptr) emit_enum(*props);
        if (defs != nullptr) emit_enum(*defs);
    }
    out += "\n} // namespace dnd::data\n";
    return out;
}

bool write_file(const std::string& path, const std::string& content) {
    // Каталог создаётся вызывающей стороной (CI); здесь — только запись
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        std::fprintf(stderr, "ошибка: не удалось открыть %s для записи\n", path.c_str());
        return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(out);
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse_args(argc, argv, options)) return 2;
    engine::core::Log::instance().set_level(engine::core::LogLevel::Error);

    FileSystem fs(options.assets_root);
    std::vector<std::string> schema_paths;
    for (const auto& entry : fs.list_directory("assets:/schemas/")) {
        if (ends_with_json(entry)) schema_paths.push_back(entry);
    }
    std::sort(schema_paths.begin(), schema_paths.end());
    if (schema_paths.empty()) {
        std::fprintf(stderr, "ошибка: в %s/schemas нет файлов .schema.json\n",
                     options.assets_root.c_str());
        return 2;
    }

    std::vector<SchemaDoc> docs;
    int failures = 0;
    for (const auto& path : schema_paths) {
        const auto text = fs.read_text(path);
        if (!text.is_ok()) {
            std::fprintf(stderr, "[FAIL] %s — %s\n", path.c_str(), text.error_message().c_str());
            ++failures;
            continue;
        }
        const auto parsed = JsonParser::parse(text.value());
        if (!parsed.is_ok()) {
            std::fprintf(stderr, "[FAIL] %s — %s\n", path.c_str(), parsed.error_message().c_str());
            ++failures;
            continue;
        }
        SchemaDoc doc;
        doc.path = path;
        doc.root = parsed.value();
        doc.schema_id = doc.root.at("schema_id").as_string_or(path);
        doc.name = schema_name_of(doc.schema_id);
        if (doc.name.empty()) doc.name = schema_name_of(path);
        docs.push_back(std::move(doc));
    }
    if (failures != 0) return 1;

    std::string markdown;
    markdown += "# Форматы данных (DATA-E01…DATA-E05)\n\n";
    markdown += "Файл СГЕНЕРИРОВАН tools/schema_gen по схемам `assets/schemas/*.schema.json`.\n";
    markdown += "Руками не редактируется: изменения вносятся в схемы, затем справочник\n";
    markdown += "пересоздаётся командой `schema_gen --assets assets`.\n\n";
    markdown += "## Общие требования к файлам данных\n\n";
    markdown += "* Кодировка UTF-8 без BOM (п. 4.5.5).\n";
    markdown += "* Обязательная обёртка: `$schema`, `schema_version`, `id` (п. 4.5.3).\n";
    markdown += "* Пути к ресурсам — виртуальные `assets:/…`; абсолютные пути и пути,\n";
    markdown += "  зависящие от ОС, в данных запрещены (DATA-E05).\n";
    markdown += "* Проверка соответствия выполняется при загрузке; файл с ошибкой не\n";
    markdown += "  загружается, факт пишется в журнал и публикуется событие шины\n";
    markdown += "  (FR-E47, FR-E48, п. 4.1.4.4).\n\n";
    markdown += "## Схемы\n\n";
    for (const auto& doc : docs) markdown += markdown_for(doc);

    const std::string header = header_for(docs);

    if (options.print_only) {
        std::printf("%s\n", markdown.c_str());
        std::printf("\n// ===== %s =====\n%s", options.header_out.c_str(), header.c_str());
        return 0;
    }
    if (!write_file(options.markdown_out, markdown)) return 2;
    if (!write_file(options.header_out, header)) return 2;

    std::printf("Схем обработано: %zu\n", docs.size());
    std::printf("Справочник данных: %s (%zu байт)\n", options.markdown_out.c_str(), markdown.size());
    std::printf("Заголовок констант: %s (%zu байт)\n", options.header_out.c_str(), header.size());
    return 0;
}
