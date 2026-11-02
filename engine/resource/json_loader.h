// engine/resource/json_loader.h — загрузка JSON-данных из виртуальной ФС с
// валидацией по схемам (FR-E41, FR-E45, FR-E47, FR-E48, DATA-E02, п. 4.5.3).
//
// Контракт поведения (п. 4.1.4.4): ошибка загрузки ОДНОГО ресурса не приводит к
// аварийному завершению программы — возвращается признак ошибки, факт пишется в
// журнал уровнем error (FR-E08) и публикуется событие в шине (FR-E10).
#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

#include "core/event_bus.h"
#include "core/file_system.h"
#include "core/log.h"
#include "core/result.h"
#include "resource/json.h"
#include "resource/schema_validator.h"

namespace engine::resource {

// Событие шины: ресурс не загружен (подписчики — отладочная панель, журнал боя)
struct ResourceLoadFailedEvent {
    std::string virtual_path;
    std::string reason;   // полное сообщение по формату п. 4.1.4.3
    std::string stage;    // "read" | "parse" | "envelope" | "schema"
};

// Событие шины: ресурс успешно загружен или взят из кэша
struct ResourceLoadedEvent {
    std::string virtual_path;
    bool from_cache = false;
};

// Результат загрузки: документ + сведения для журнала
struct JsonDocument {
    std::string virtual_path;
    int schema_version = 0;
    std::string id;                 // значение поля "id" (DATA-E03)
    JsonValue value;
};

class JsonLoader {
public:
    // bus может быть nullptr — тогда события не публикуются (например, в тестах)
    explicit JsonLoader(const core::FileSystem& fs, SchemaValidator& validator,
                        core::EventBus* bus = nullptr)
        : fs_(fs), validator_(validator), bus_(bus) {}

    // Загрузка с кэшированием: повторный вызов для того же пути возвращает кэш (FR-E45)
    core::Result<JsonDocument> load(const std::string& virtual_path);

    // Загрузка без кэша и без записи в кэш (инструменты, «горячая» перезагрузка — V1, FR-E46)
    core::Result<JsonDocument> load_uncached(const std::string& virtual_path);

    // Ожидание версии схемы: <0 — не проверять (п. 4.5.3)
    void set_expected_schema_version(int version) { expected_schema_version_ = version; }
    int expected_schema_version() const { return expected_schema_version_; }

    // Кэш (FR-E45) и метрики для отладочной панели (FR-E100)
    void invalidate(const std::string& virtual_path);
    void invalidate_all();
    std::size_t cache_size() const { return cache_.size(); }
    bool is_cached(const std::string& virtual_path) const;
    std::size_t load_count() const { return load_count_; }
    std::size_t cache_hit_count() const { return cache_hits_; }
    std::size_t failure_count() const { return failures_; }

    SchemaValidator& validator() { return validator_; }

private:
    void report_failure(const std::string& path, const std::string& stage,
                        const std::string& reason);
    void report_loaded(const std::string& path, bool from_cache);

    const core::FileSystem& fs_;
    SchemaValidator& validator_;
    core::EventBus* bus_;
    std::unordered_map<std::string, JsonDocument> cache_;
    int expected_schema_version_ = -1;
    std::size_t load_count_ = 0;
    std::size_t cache_hits_ = 0;
    std::size_t failures_ = 0;
};

} // namespace engine::resource
