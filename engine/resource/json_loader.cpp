// engine/resource/json_loader.cpp — реализация загрузки и валидации JSON (FR-E41…FR-E48).
#include "resource/json_loader.h"

namespace engine::resource {

void JsonLoader::report_failure(const std::string& path, const std::string& stage,
                                const std::string& reason) {
    ++failures_;
    core::Log::instance().error("resource: не удалось загрузить " + path + " [" + stage + "]\n" +
                                reason);
    if (bus_ != nullptr) {
        ResourceLoadFailedEvent event;
        event.virtual_path = path;
        event.reason = reason;
        event.stage = stage;
        bus_->publish(event);
    }
}

void JsonLoader::report_loaded(const std::string& path, bool from_cache) {
    if (bus_ != nullptr) {
        ResourceLoadedEvent event;
        event.virtual_path = path;
        event.from_cache = from_cache;
        bus_->publish(event);
    }
}

core::Result<JsonDocument> JsonLoader::load(const std::string& virtual_path) {
    if (const auto it = cache_.find(virtual_path); it != cache_.end()) {
        ++cache_hits_;
        report_loaded(virtual_path, /*from_cache=*/true);
        return core::Result<JsonDocument>::ok(it->second); // повторная загрузка → кэш (FR-E45)
    }
    auto result = load_uncached(virtual_path);
    if (result.is_ok()) {
        cache_.insert_or_assign(virtual_path, result.value());
        report_loaded(virtual_path, /*from_cache=*/false);
    }
    return result;
}

core::Result<JsonDocument> JsonLoader::load_uncached(const std::string& virtual_path) {
    ++load_count_;

    // 1. Чтение файла по виртуальному пути (FR-E07, DATA-E05)
    auto text = fs_.read_text(virtual_path);
    if (!text.is_ok()) {
        const std::string reason = virtual_path + ":\n" + text.error_message();
        report_failure(virtual_path, "read", reason);
        return core::Result<JsonDocument>::err(reason);
    }

    // 2. Разбор JSON (FR-E41)
    auto parsed = JsonParser::parse(text.value());
    if (!parsed.is_ok()) {
        const std::string reason = virtual_path + ":\n" + parsed.error_message();
        report_failure(virtual_path, "parse", reason);
        return core::Result<JsonDocument>::err(reason);
    }
    const JsonValue& value = parsed.value();

    // 3. Проверка обёртки данных: $schema, schema_version, id (п. 4.5.3)
    auto envelope = validator_.validate_envelope(value, expected_schema_version_);
    if (!envelope.ok()) {
        const std::string reason = envelope.format(virtual_path);
        report_failure(virtual_path, "envelope", reason);
        return core::Result<JsonDocument>::err(reason);
    }

    // 4. Валидация по схеме (FR-E47)
    const std::string schema_id = value.at("$schema").as_string();
    auto schema_result = validator_.validate(schema_id, value);
    if (!schema_result.ok()) {
        const std::string reason = schema_result.format(virtual_path);
        report_failure(virtual_path, "schema", reason);
        return core::Result<JsonDocument>::err(reason);
    }

    JsonDocument doc;
    doc.virtual_path = virtual_path;
    doc.schema_version = static_cast<int>(value.at("schema_version").as_int());
    doc.id = value.at("id").as_string();
    doc.value = value;
    core::Log::instance().debug("resource: загружен " + virtual_path + " (id=" + doc.id + ")");
    return core::Result<JsonDocument>::ok(std::move(doc));
}

void JsonLoader::invalidate(const std::string& virtual_path) { cache_.erase(virtual_path); }

void JsonLoader::invalidate_all() { cache_.clear(); }

bool JsonLoader::is_cached(const std::string& virtual_path) const {
    return cache_.find(virtual_path) != cache_.end();
}

} // namespace engine::resource
