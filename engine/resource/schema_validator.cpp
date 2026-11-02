// engine/resource/schema_validator.cpp — реализация валидации данных по схемам (FR-E47).
#include "resource/schema_validator.h"

#include <algorithm>
#include <sstream>

namespace engine::resource {

namespace {

bool is_type_match(const JsonValue& value, const std::string& type) {
    if (type == "object") return value.is_object();
    if (type == "array") return value.is_array();
    if (type == "string") return value.is_string();
    if (type == "boolean") return value.is_bool();
    if (type == "integer") return value.type() == JsonType::Int;
    if (type == "number") return value.is_number();
    if (type == "null") return value.is_null();
    return true; // неизвестный тип не ограничивает значение
}

std::string type_name(const JsonValue& v) {
    switch (v.type()) {
        case JsonType::Null:   return "null";
        case JsonType::Bool:   return "boolean";
        case JsonType::Int:    return "integer";
        case JsonType::Double: return "number";
        case JsonType::String: return "string";
        case JsonType::Array:  return "array";
        case JsonType::Object: return "object";
    }
    return "unknown";
}

std::string value_to_text(const JsonValue& v) {
    if (v.is_string()) return "\"" + v.as_string() + "\"";
    return v.dump();
}

std::string join_enum(const JsonArray& values) {
    std::ostringstream os;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) os << ", ";
        const auto& e = values[i];
        os << (e.is_string() ? e.as_string() : e.dump());
    }
    return os.str();
}

} // namespace

std::string ValidationError::to_string() const {
    std::ostringstream os;
    os << path << ": " << message;
    if (!expected.empty()) os << "\nexpected one of: " << expected;
    return os.str();
}

std::string ValidationResult::format(const std::string& virtual_path) const {
    std::ostringstream os;
    os << virtual_path << ":";
    for (const auto& e : errors) os << "\n" << e.to_string();
    return os.str();
}

const std::vector<std::string>& damage_type_names() {
    // Порядок — по rules::DamageType (п. Г.5 ТЗ DOC-04, DOC-01 таблица 4.7)
    static const std::vector<std::string> names = {
        "Bludgeoning", "Piercing", "Slashing", "Fire", "Cold", "Lightning", "Acid",
        "Poison", "Necrotic", "Radiant", "Force", "Psychic", "Thunder"};
    return names;
}

void SchemaValidator::add_schema(const std::string& schema_id, JsonValue schema) {
    schemas_.insert_or_assign(schema_id, std::move(schema));
}

bool SchemaValidator::has_schema(const std::string& schema_id) const {
    return schemas_.find(schema_id) != schemas_.end();
}

ValidationResult SchemaValidator::validate(const std::string& schema_id,
                                          const JsonValue& value) const {
    const auto it = schemas_.find(schema_id);
    if (it == schemas_.end()) {
        ValidationResult out;
        out.errors.push_back({"$schema", "схема не зарегистрирована: \"" + schema_id + "\"",
                              "зарегистрированная схема данных (DATA-E02)"});
        return out;
    }
    return validate_against(it->second, value);
}

ValidationResult SchemaValidator::validate_against(const JsonValue& schema,
                                                  const JsonValue& value) const {
    ValidationResult out;
    validate_node(schema, value, "", out, &schema);
    return out;
}

ValidationResult SchemaValidator::validate_envelope(const JsonValue& value,
                                                   int expected_schema_version) const {
    ValidationResult out;
    if (!value.is_object()) {
        out.errors.push_back({"<корень>", "ожидается объект JSON", "object"});
        return out;
    }
    static const char* required[] = {"$schema", "schema_version", "id"};
    for (const char* key : required) {
        if (!value.contains(key)) {
            out.errors.push_back({key, "обязательное поле отсутствует", "поле по п. 4.5.3 ТЗ"});
        }
    }
    if (const JsonValue* v = value.find("schema_version"); v != nullptr && !v->is_number()) {
        out.errors.push_back({"schema_version", "ожидается число, получено " + type_name(*v),
                              "integer"});
    }
    if (const JsonValue* id = value.find("id"); id != nullptr && !id->is_string()) {
        out.errors.push_back({"id", "ожидается строка, получено " + type_name(*id), "string"});
    }
    if (expected_schema_version >= 0) {
        const auto actual = static_cast<int>(value.at("schema_version").as_int(-1));
        if (actual != expected_schema_version) {
            out.errors.push_back({"schema_version",
                                  "несовместимая версия схемы: " + std::to_string(actual),
                                  std::to_string(expected_schema_version)});
        }
    }
    return out;
}

void SchemaValidator::validate_node(const JsonValue& schema, const JsonValue& value,
                                   const std::string& path, ValidationResult& out,
                                   const JsonValue* root) const {
    const JsonValue& document_root = (root != nullptr) ? *root : schema;
    if (!schema.is_object()) return; // не схема — ограничений нет

    const std::string node_path = path.empty() ? "<корень>" : path;

    // $ref: поддерживаются только локальные определения "#/definitions/<имя>"
    if (const JsonValue* ref = schema.find("$ref"); ref != nullptr && ref->is_string()) {
        const std::string& r = ref->as_string();
        const std::string prefix = "#/definitions/";
        if (r.compare(0, prefix.size(), prefix) == 0) {
            const JsonValue* def = document_root.find_path("definitions/" + r.substr(prefix.size()));
            if (def == nullptr) {
                out.errors.push_back({node_path, "не найдено определение \"" + r + "\"",
                                      "definitions схемы"});
                return;
            }
            validate_node(*def, value, path, out, &document_root);
        }
        return;
    }

    // type
    if (const JsonValue* t = schema.find("type"); t != nullptr && t->is_string()) {
        if (!is_type_match(value, t->as_string())) {
            out.errors.push_back({node_path,
                                  "ожидается тип " + t->as_string() + ", получено " + type_name(value),
                                  t->as_string()});
            return; // дальнейшие проверки для неверного типа бессмысленны
        }
    }

    // enum
    if (const JsonValue* e = schema.find("enum"); e != nullptr && e->is_array()) {
        const auto& allowed = e->as_array();
        const bool found = std::any_of(allowed.begin(), allowed.end(), [&](const JsonValue& a) {
            return a.dump() == value.dump();
        });
        if (!found) {
            out.errors.push_back({node_path, "неизвестное значение " + value_to_text(value),
                                  join_enum(allowed)});
        }
    }

    // числовые ограничения
    if (value.is_number()) {
        if (const JsonValue* mn = schema.find("minimum"); mn != nullptr && mn->is_number()) {
            if (value.as_double() < mn->as_double()) {
                out.errors.push_back({node_path,
                                      "значение " + std::to_string(value.as_int()) + " меньше минимума",
                                      "minimum " + mn->dump()});
            }
        }
        if (const JsonValue* mx = schema.find("maximum"); mx != nullptr && mx->is_number()) {
            if (value.as_double() > mx->as_double()) {
                out.errors.push_back({node_path,
                                      "значение " + std::to_string(value.as_int()) + " больше максимума",
                                      "maximum " + mx->dump()});
            }
        }
    }

    // строковые ограничения
    if (value.is_string()) {
        if (const JsonValue* ml = schema.find("minLength"); ml != nullptr && ml->is_number()) {
            if (static_cast<std::int64_t>(value.as_string().size()) < ml->as_int()) {
                out.errors.push_back({node_path, "строка короче допустимой", "minLength " + ml->dump()});
            }
        }
    }

    // массивы
    if (value.is_array()) {
        if (const JsonValue* items = schema.find("items"); items != nullptr) {
            const auto& arr = value.as_array();
            for (std::size_t i = 0; i < arr.size(); ++i) {
                validate_node(*items, arr[i], path + "[" + std::to_string(i) + "]", out,
                            &document_root);
            }
        }
        if (const JsonValue* mn = schema.find("minItems"); mn != nullptr && mn->is_number()) {
            if (static_cast<std::int64_t>(value.size()) < mn->as_int()) {
                out.errors.push_back({node_path, "массив короче требуемого", "minItems " + mn->dump()});
            }
        }
    }

    // объекты
    if (value.is_object()) {
        if (const JsonValue* req = schema.find("required"); req != nullptr && req->is_array()) {
            for (const auto& key : req->as_array()) {
                if (!key.is_string()) continue;
                if (!value.contains(key.as_string())) {
                    out.errors.push_back({path.empty() ? key.as_string() : path + "." + key.as_string(),
                                          "обязательное поле отсутствует", key.as_string()});
                }
            }
        }
        if (const JsonValue* props = schema.find("properties"); props != nullptr && props->is_object()) {
            for (const auto& [key, prop_schema] : props->as_object()) {
                if (const JsonValue* child = value.find(key); child != nullptr) {
                    validate_node(prop_schema, *child, path.empty() ? key : path + "." + key, out,
                                &document_root);
                }
            }
        }
        // additionalProperties: false — запрет неизвестных полей (опечатки в данных)
        if (const JsonValue* ap = schema.find("additionalProperties");
            ap != nullptr && ap->is_bool() && !ap->as_bool()) {
            if (const JsonValue* props = schema.find("properties"); props != nullptr) {
                for (const auto& [key, child] : value.as_object()) {
                    if (props->contains(key)) continue;
                    out.errors.push_back({path.empty() ? key : path + "." + key,
                                          "неизвестное поле (additionalProperties: false)",
                                          "перечень полей схемы"});
                    (void)child;
                }
            }
        }
    }
}

} // namespace engine::resource
