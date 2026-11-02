// engine/scene/serializable.h — контракт сериализуемого состояния (FR-E90).
// Реализует объект игрового слоя (game/save); движок знает только интерфейс,
// но не состав игровых данных (п. 1.5, E-D01).
#pragma once

#include "resource/json.h"

namespace engine::scene {

class ISerializable {
public:
    virtual ~ISerializable() = default;

    // Состояние в формате JSON-объекта (поле "scene" файла сохранения, п. 4.1.9.2)
    virtual resource::JsonValue serialize_state() const = 0;

    // Восстановление состояния. Возвращает false при недопустимых данных —
    // аварийное завершение не допускается (п. 4.1.9.3, NFR-E14).
    virtual bool deserialize_state(const resource::JsonValue& state) = 0;
};

} // namespace engine::scene
