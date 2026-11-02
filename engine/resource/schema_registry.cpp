// engine/resource/schema_registry.cpp — регистрация схем данных (FR-E47, DATA-E02).
#include "resource/schema_registry.h"

#include <algorithm>

#include "core/log.h"

namespace engine::resource {

core::Result<std::string> SchemaRegistry::load_schema(const std::string& virtual_path) {
    auto text = fs_.read_text(virtual_path);
    if (!text.is_ok()) {
        const std::string reason = "schema: " + text.error_message();
        core::Log::instance().error(reason);
        return core::Result<std::string>::err(reason);
    }

    auto parsed = JsonParser::parse(text.value());
    if (!parsed.is_ok()) {
        const std::string reason = virtual_path + ":\n" + parsed.error_message();
        core::Log::instance().error("schema: " + reason);
        return core::Result<std::string>::err(reason);
    }

    JsonValue schema = parsed.value();
    if (!schema.is_object()) {
        const std::string reason = virtual_path + ":\nсхема должна быть объектом JSON";
        core::Log::instance().error("schema: " + reason);
        return core::Result<std::string>::err(reason);
    }

    // Идентификатор схемы: поле "schema_id", иначе — виртуальный путь без "assets:/"
    std::string schema_id = schema.at("schema_id").as_string();
    if (schema_id.empty()) {
        schema_id = virtual_path;
        const std::string prefix = "assets:/";
        if (schema_id.rfind(prefix, 0) == 0) schema_id = schema_id.substr(prefix.size());
    }

    validator_.add_schema(schema_id, std::move(schema));
    if (std::find(registered_.begin(), registered_.end(), schema_id) == registered_.end()) {
        registered_.push_back(schema_id);
        std::sort(registered_.begin(), registered_.end());
    }
    core::Log::instance().debug("schema: зарегистрирована " + schema_id + " из " + virtual_path);
    return core::Result<std::string>::ok(schema_id);
}

std::size_t SchemaRegistry::load_directory(const std::string& virtual_dir) {
    std::size_t loaded = 0;
    for (const auto& path : fs_.list_directory(virtual_dir)) {
        const std::string suffix = ".schema.json";
        if (path.size() < suffix.size()) continue;
        if (path.compare(path.size() - suffix.size(), suffix.size(), suffix) != 0) continue;
        if (load_schema(path).is_ok()) ++loaded;
    }
    core::Log::instance().info("schema: зарегистрировано схем: " + std::to_string(loaded) +
                               " из " + virtual_dir);
    return loaded;
}

bool SchemaRegistry::contains(const std::string& schema_id) const {
    return std::find(registered_.begin(), registered_.end(), schema_id) != registered_.end();
}

} // namespace engine::resource
