// engine/resource/schema_validator.h — валидация данных по схемам JSON (FR-E47, DATA-E02).
// При несоответствии схеме загрузка отклоняется с сообщением, позволяющим найти дефект
// данных без отладчика (FR-E48, формат п. 4.1.4.3):
//
//   assets:/data/items/longsword.json:
//   component[1].damage_type: unknown value "Slash"
//   expected one of: Bludgeoning, Piercing, Slashing, Fire, Cold, Lightning,
//                    Acid, Poison, Necrotic, Radiant, Force, Psychic, Thunder
//
// Схема — подмножество JSON Schema (draft-07) в объёме, достаточном для игровых
// данных: type, properties, required, items, enum, minimum, maximum, minLength,
// additionalProperties, $ref (только локальный "#/definitions/<имя>").
// Полная поддержка JSON Schema не требуется ТЗ и отнесена на V1 (docs/decisions.md DR-02).
#pragma once

#include <string>
#include <vector>

#include "resource/json.h"

namespace engine::resource {

// Одна ошибка валидации: путь до поля, сообщение и ожидаемое значение
struct ValidationError {
    std::string path;      // "components[1].damage_type"
    std::string message;   // "неизвестное значение \"Slash\""
    std::string expected;  // "Bludgeoning, Piercing, Slashing, ..." (непусто, если есть)

    std::string to_string() const;
};

struct ValidationResult {
    std::vector<ValidationError> errors;
    bool ok() const { return errors.empty(); }

    // Сообщение для журнала и для пользователя: виртуальный путь + все ошибки (FR-E48)
    std::string format(const std::string& virtual_path) const;
};

class SchemaValidator {
public:
    // Регистрация схемы по её идентификатору (значение "$schema" в данных)
    void add_schema(const std::string& schema_id, JsonValue schema);

    bool has_schema(const std::string& schema_id) const;

    // Проверка значения по зарегистрированной схеме.
    // Если схема не зарегистрирована — ошибка «схема не найдена» (отказ от загрузки,
    // а не молчаливый пропуск: п. 4.2.3, NFR-E14).
    ValidationResult validate(const std::string& schema_id, const JsonValue& value) const;

    // Проверка по «встроенной» схеме (без регистрации)
    ValidationResult validate_against(const JsonValue& schema, const JsonValue& value) const;

    // Проверка обязательных полей обёртки данных (п. 4.5.3):
    // "$schema", "schema_version", "id"; дополнительно сверяет schema_version.
    ValidationResult validate_envelope(const JsonValue& value, int expected_schema_version = -1) const;

    std::size_t schema_count() const { return schemas_.size(); }

private:
    // root — корень документа схемы: от него разрешаются локальные $ref (#/definitions/…)
    void validate_node(const JsonValue& schema, const JsonValue& value, const std::string& path,
                       ValidationResult& out, const JsonValue* root) const;

    JsonObject schemas_;
};

// Перечень допустимых значений типа урона (13 типов, DOC-01 FR-K40) — используется
// сообщениями валидатора и схемами данных (single source of truth)
const std::vector<std::string>& damage_type_names();

} // namespace engine::resource
