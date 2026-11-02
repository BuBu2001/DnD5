// engine/resource/json.h — минимальный разбор и сериализация JSON (FR-E41).
//
// ПРОЕКТНОЕ РЕШЕНИЕ (docs/decisions.md, DR-01): таблица 4.16 ТЗ DOC-04 допускает
// nlohmann/json, однако сторонние библиотеки в репозиторий ещё не внесены
// (third_party/ пуст, подключение — этап M0/M4). Чтобы не блокировать требования
// FR-E41, FR-E45, FR-E47 и DATA-E02 собственной реализацией парсера, введён
// нейтральный тип engine::resource::JsonValue. Замена на nlohmann::json выполняется
// в одном месте — в json_loader.cpp — без изменения интерфейсов подсистем.
//
// Ограничения реализации (достаточно для данных игры, п. 4.1.4.2):
//   * UTF-8 без BOM (п. 4.5.5); управляющие символы в строках недопустимы;
//   * числа — double/целые; экспоненциальная запись поддерживается;
//   * комментарии не поддерживаются (JSON не допускает их).
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "core/result.h"

namespace engine::resource {

class JsonValue;

using JsonArray = std::vector<JsonValue>;
// map (а не unordered_map): детерминированный порядок ключей при сериализации (NFR-E09)
using JsonObject = std::map<std::string, JsonValue>;

enum class JsonType : std::uint8_t { Null, Bool, Int, Double, String, Array, Object };

class JsonValue {
public:
    JsonValue() : data_(std::monostate{}) {}
    JsonValue(std::nullptr_t) : data_(std::monostate{}) {}
    JsonValue(bool b) : data_(b) {}
    JsonValue(int v) : data_(static_cast<std::int64_t>(v)) {}
    JsonValue(std::int64_t v) : data_(v) {}
    JsonValue(double v) : data_(v) {}
    JsonValue(const char* s) : data_(std::string(s)) {}
    JsonValue(std::string s) : data_(std::move(s)) {}
    JsonValue(JsonArray a) : data_(std::move(a)) {}
    JsonValue(JsonObject o) : data_(std::move(o)) {}

    JsonType type() const { return static_cast<JsonType>(data_.index()); }
    bool is_null() const { return type() == JsonType::Null; }
    bool is_bool() const { return type() == JsonType::Bool; }
    bool is_number() const { return type() == JsonType::Int || type() == JsonType::Double; }
    bool is_string() const { return type() == JsonType::String; }
    bool is_array() const { return type() == JsonType::Array; }
    bool is_object() const { return type() == JsonType::Object; }

    // Доступ с дефолтным значением: при несовпадении типа возвращается default_value
    // (устойчивость к ошибочным данным, п. 4.2.3, NFR-E14).
    bool as_bool(bool default_value = false) const;
    std::int64_t as_int(std::int64_t default_value = 0) const;
    double as_double(double default_value = 0.0) const;
    const std::string& as_string() const;             // пустая строка, если не строка
    std::string as_string_or(const std::string& d) const;
    const JsonArray& as_array() const;
    const JsonObject& as_object() const;

    bool contains(const std::string& key) const;
    const JsonValue* find(const std::string& key) const;   // nullptr, если нет/не объект
    const JsonValue& at(const std::string& key) const;     // Null-значение, если нет
    std::size_t size() const;                              // длина массива или число ключей

    // Доступ по JSON Pointer-подобному пути "a/b/0/c" (для сообщений об ошибках, FR-E48)
    const JsonValue* find_path(const std::string& path) const;

    // Сериализация: compact — без отступов; иначе шаг 2 пробела
    std::string dump(int indent = 0) const;

    static JsonValue make_object() { return JsonValue(JsonObject{}); }
    static JsonValue make_array() { return JsonValue(JsonArray{}); }

    // Удобство заполнения объекта
    JsonValue& operator[](const std::string& key);
    void push_back(JsonValue v);

private:
    using Storage = std::variant<std::monostate, bool, std::int64_t, double, std::string,
                                 JsonArray, JsonObject>;
    Storage data_;

    void dump_impl(std::string& out, int indent, int depth) const;
};

// Ошибка разбора: путь до узла, строка и колонка — для сообщения по формату п. 4.1.4.3
struct JsonParseError {
    std::string path;      // "components[1].damage_type"
    std::int32_t line = 0;
    std::int32_t column = 0;
    std::string message;

    std::string to_string() const;
};

class JsonParser {
public:
    static core::Result<JsonValue> parse(const std::string& text);
};

} // namespace engine::resource
