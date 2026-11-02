// engine/scene/save_system.h — подсистема сохранения и загрузки (FR-E90…FR-E93).
// Требования:
//   * формат — JSON (FR-E90, DATA-E02);
//   * версионирование и миграция между версиями (FR-E91);
//   * сохранение состояния RNG для воспроизводимости боя (FR-E92);
//   * битое или старое сохранение НЕ приводит к аварийному завершению:
//     либо миграция, либо отказ с понятным сообщением (п. 4.1.9.3, NFR-E14).
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/file_system.h"
#include "core/result.h"
#include "resource/json.h"
#include "resource/json_loader.h"
#include "resource/schema_validator.h"
#include "scene/save_slot.h"
#include "scene/serializable.h"

namespace engine::scene {

// Миграция одной версии на следующую: version → version+1 (FR-E91)
using SaveMigration = std::function<bool(resource::JsonValue& save_json, std::string& error)>;

struct SaveSummary {
    std::string virtual_path;
    std::string label;
    std::string timestamp;
    std::int32_t save_version = 0;
    std::uint64_t seed = 0;
};

class SaveSystem {
public:
    explicit SaveSystem(const core::FileSystem& fs, resource::SchemaValidator* validator = nullptr)
        : fs_(fs), validator_(validator) {}

    // Регистрация миграции version → version+1
    void register_migration(std::int32_t from_version, SaveMigration migration);

    // Приведение сохранения к текущей версии. Возвращает false, если цепочка
    // миграций неполная (сохранение отклоняется с сообщением, а не падает).
    bool migrate(resource::JsonValue& save_json, std::string& out_error) const;

    // Сохранение состояния сцены в слот (FR-E90)
    core::Result<std::string> save(const std::string& virtual_path, const SaveSlot& slot);

    // Загрузка слота с валидацией и миграцией (FR-E91)
    core::Result<SaveSlot> load(const std::string& virtual_path);

    // Сохранение/загрузка с участием сериализуемого состояния сцены
    core::Result<std::string> save_state(const std::string& virtual_path, const SaveSlot& meta,
                                         const ISerializable& state);
    core::Result<SaveSlot> load_state(const std::string& virtual_path, ISerializable& state);

    // Перечень слотов в каталоге сохранений (обычно "assets:/saves/")
    std::vector<SaveSummary> list_slots(const std::string& virtual_dir) const;

    bool remove_slot(const std::string& virtual_path) const;

    std::size_t save_count() const { return save_count_; }
    std::size_t load_count() const { return load_count_; }
    std::size_t migration_count() const { return migration_count_; }

private:
    const core::FileSystem& fs_;
    resource::SchemaValidator* validator_;
    std::unordered_map<std::int32_t, SaveMigration> migrations_;
    mutable std::size_t save_count_ = 0;
    mutable std::size_t load_count_ = 0;
    mutable std::size_t migration_count_ = 0;
};

} // namespace engine::scene
