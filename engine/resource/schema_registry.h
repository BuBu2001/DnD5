// engine/resource/schema_registry.h — реестр схем данных (FR-E47, DATA-E02).
// Схемы лежат в assets:/schemas/*.schema.json и регистрируются по значению поля
// "schema_id" внутри файла (если поле отсутствует — по виртуальному пути).
// Ключ регистрации совпадает со значением "$schema" в файлах данных (п. 4.5.3),
// поэтому загрузка данных не требует знания физического расположения схем.
#pragma once

#include <string>
#include <vector>

#include "core/file_system.h"
#include "core/result.h"
#include "resource/json.h"
#include "resource/schema_validator.h"

namespace engine::resource {

class SchemaRegistry {
public:
    explicit SchemaRegistry(const core::FileSystem& fs, SchemaValidator& validator)
        : fs_(fs), validator_(validator) {}

    // Загрузка одной схемы по виртуальному пути
    core::Result<std::string> load_schema(const std::string& virtual_path);

    // Загрузка всех *.schema.json из каталога (обычно "assets:/schemas/")
    std::size_t load_directory(const std::string& virtual_dir);

    std::size_t size() const { return registered_.size(); }
    bool contains(const std::string& schema_id) const;
    const std::vector<std::string>& registered_ids() const { return registered_; }

private:
    const core::FileSystem& fs_;
    SchemaValidator& validator_;
    std::vector<std::string> registered_;
};

} // namespace engine::resource
