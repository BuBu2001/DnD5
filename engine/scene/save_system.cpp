// engine/scene/save_system.cpp — реализация сохранения и загрузки (FR-E90…FR-E93).
#include "scene/save_system.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <fstream>

#include "core/log.h"

namespace engine::scene {

namespace {

// Метка времени ISO 8601 (локальное время) — для отображения слота в UI
std::string current_timestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &now);
#else
    localtime_r(&now, &tm_buf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_buf);
    return std::string(buf);
}

} // namespace

void SaveSystem::register_migration(std::int32_t from_version, SaveMigration migration) {
    migrations_.insert_or_assign(from_version, std::move(migration));
}

bool SaveSystem::migrate(resource::JsonValue& save_json, std::string& out_error) const {
    if (!save_json.is_object()) {
        out_error = "сохранение: ожидается объект JSON";
        return false;
    }
    auto version = static_cast<std::int32_t>(save_json.at("save_version").as_int(0));
    if (version <= 0) {
        out_error = "сохранение: поле save_version отсутствует или некорректно";
        return false;
    }
    if (version > kCurrentSaveVersion) {
        // Сохранение создано более новой версией программы: отклоняем с сообщением
        // (п. 4.1.9.3), аварийное завершение не допускается.
        out_error = "сохранение создано более новой версией программы: save_version=" +
                    std::to_string(version) + ", поддерживается до " +
                    std::to_string(kCurrentSaveVersion);
        return false;
    }

    // Последовательные миграции version → version+1 до текущей версии
    while (version < kCurrentSaveVersion) {
        const auto it = migrations_.find(version);
        if (it == migrations_.end()) {
            out_error = "отсутствует миграция сохранения с версии " + std::to_string(version);
            return false;
        }
        std::string step_error;
        if (!it->second(save_json, step_error)) {
            out_error = "ошибка миграции с версии " + std::to_string(version) + ": " + step_error;
            return false;
        }
        ++version;
        save_json["save_version"] = version;
        ++migration_count_;
    }
    return true;
}

core::Result<std::string> SaveSystem::save(const std::string& virtual_path, const SaveSlot& slot) {
    const auto physical = fs_.resolve(virtual_path);
    if (!physical) {
        core::Log::instance().error("save: " + physical.error_message());
        return core::Result<std::string>::err(physical.error_message());
    }

    SaveSlot prepared = slot;
    if (prepared.save_version <= 0) prepared.save_version = kCurrentSaveVersion;
    if (prepared.timestamp.empty()) prepared.timestamp = current_timestamp();

    const std::string text = prepared.to_json().dump(2) + "\n";
    std::ofstream out(physical.value(), std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        const std::string reason = "save: не удалось открыть файл для записи: " + virtual_path;
        core::Log::instance().error(reason);
        return core::Result<std::string>::err(reason);
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    out.close();
    if (out.fail()) {
        const std::string reason = "save: ошибка записи файла: " + virtual_path;
        core::Log::instance().error(reason);
        return core::Result<std::string>::err(reason);
    }

    ++save_count_;
    core::Log::instance().info("save: сохранено в " + virtual_path +
                               " (save_version=" + std::to_string(prepared.save_version) + ")");
    return core::Result<std::string>::ok(virtual_path);
}

core::Result<SaveSlot> SaveSystem::load(const std::string& virtual_path) {
    auto text = fs_.read_text(virtual_path);
    if (!text.is_ok()) {
        core::Log::instance().error("load: " + text.error_message());
        return core::Result<SaveSlot>::err(text.error_message());
    }

    auto parsed = resource::JsonParser::parse(text.value());
    if (!parsed.is_ok()) {
        const std::string reason = virtual_path + ":\n" + parsed.error_message();
        core::Log::instance().error("load: " + reason);
        return core::Result<SaveSlot>::err(reason);
    }

    // Валидация по схеме, если валидатор предоставлен (FR-E47)
    if (validator_ != nullptr) {
        const auto schema_id = parsed.value().at("$schema").as_string();
        if (!schema_id.empty()) {
            const auto result = validator_->validate(schema_id, parsed.value());
            if (!result.ok()) {
                const std::string reason = result.format(virtual_path);
                core::Log::instance().error("load: " + reason);
                return core::Result<SaveSlot>::err(reason);
            }
        }
    }

    resource::JsonValue migrated = parsed.value();
    std::string migration_error;
    if (!migrate(migrated, migration_error)) {
        const std::string reason = virtual_path + ":\n" + migration_error;
        core::Log::instance().error("load: " + reason);
        return core::Result<SaveSlot>::err(reason);
    }

    SaveSlot slot;
    std::string parse_error;
    if (!slot.from_json(migrated, &parse_error)) {
        const std::string reason = virtual_path + ":\n" + parse_error;
        core::Log::instance().error("load: " + reason);
        return core::Result<SaveSlot>::err(reason);
    }
    slot.virtual_path = virtual_path;

    ++load_count_;
    core::Log::instance().info("load: загружено из " + virtual_path);
    return core::Result<SaveSlot>::ok(std::move(slot));
}

core::Result<std::string> SaveSystem::save_state(const std::string& virtual_path,
                                                 const SaveSlot& meta,
                                                 const ISerializable& state) {
    SaveSlot slot = meta;
    slot.scene = state.serialize_state();
    return save(virtual_path, slot);
}

core::Result<SaveSlot> SaveSystem::load_state(const std::string& virtual_path,
                                              ISerializable& state) {
    auto loaded = load(virtual_path);
    if (!loaded.is_ok()) return loaded;
    if (!state.deserialize_state(loaded.value().scene)) {
        const std::string reason = virtual_path + ":\nсостояние сцены не восстановлено";
        core::Log::instance().error("load: " + reason);
        return core::Result<SaveSlot>::err(reason);
    }
    return loaded;
}

std::vector<SaveSummary> SaveSystem::list_slots(const std::string& virtual_dir) const {
    std::vector<SaveSummary> out;
    for (const auto& path : fs_.list_directory(virtual_dir)) {
        if (path.size() < 5 || path.compare(path.size() - 5, 5, ".json") != 0) continue;
        auto text = fs_.read_text(path);
        if (!text.is_ok()) continue;
        auto parsed = resource::JsonParser::parse(text.value());
        if (!parsed.is_ok()) continue; // битый файл пропускаем: каталог остаётся читаемым
        SaveSlot slot;
        if (!slot.from_json(parsed.value())) continue;
        SaveSummary summary;
        summary.virtual_path = path;
        summary.label = slot.label;
        summary.timestamp = slot.timestamp;
        summary.save_version = slot.save_version;
        summary.seed = slot.seed;
        out.push_back(std::move(summary));
    }
    std::sort(out.begin(), out.end(), [](const SaveSummary& a, const SaveSummary& b) {
        return a.timestamp > b.timestamp; // свежие первыми
    });
    return out;
}

bool SaveSystem::remove_slot(const std::string& virtual_path) const {
    const auto physical = fs_.resolve(virtual_path);
    if (!physical) return false;
    return std::remove(physical.value().c_str()) == 0;
}

} // namespace engine::scene
