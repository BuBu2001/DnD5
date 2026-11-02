// engine/scene/save_slot.h — слот сохранения (FR-E90, состав п. 4.1.9.2).
// Слот хранит только данные; запись на диск выполняет SaveSystem.
#pragma once

#include <cstdint>
#include <string>

#include "resource/json.h"

namespace engine::scene {

struct SaveSlot {
    std::int32_t save_version = 1;    // версия формата сохранения (FR-E91)
    std::int32_t schema_version = 1;  // версия схемы schemas/save.schema.json
    std::string virtual_path;         // assets-относительный путь (DATA-E05)
    std::uint64_t seed = 0;           // seed генератора (детерминизм, NFR-E09)
    std::string rng_state;            // состояние RNG для воспроизведения боя (FR-E92)
    resource::JsonValue scene;        // состояние сцены (состав — игровой слой)
    resource::JsonValue settings;     // настройки: master/music/sfx (FR-E52)
    std::string label;                // человекочитаемое имя слота для UI
    std::string timestamp;            // строка даты/времени сохранения (ISO 8601)

    // Полное представление файла сохранения в формате п. 4.1.9.2
    resource::JsonValue to_json() const;

    // Разбор файла сохранения. Возвращает false при несоответствии состава полей;
    // сообщение об ошибке — в out_error (формат п. 4.1.4.3).
    bool from_json(const resource::JsonValue& value, std::string* out_error = nullptr);
};

// Текущая версия формата сохранения: увеличивается при изменении состава данных,
// старые сохранения мигрируют (FR-E91) или отклоняются с понятным сообщением.
inline constexpr std::int32_t kCurrentSaveVersion = 1;

} // namespace engine::scene
