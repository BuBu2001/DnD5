// engine/scene/save_slot.cpp — формат файла сохранения (FR-E90, п. 4.1.9.2).
#include "scene/save_slot.h"

namespace engine::scene {

resource::JsonValue SaveSlot::to_json() const {
    resource::JsonValue root = resource::JsonValue::make_object();
    root["$schema"] = "schemas/save.schema.json";
    root["schema_version"] = static_cast<int>(schema_version);
    root["save_version"] = static_cast<int>(save_version);
    root["seed"] = static_cast<std::int64_t>(seed);
    root["rng_state"] = rng_state;
    root["label"] = label;
    root["timestamp"] = timestamp;
    root["scene"] = scene.is_null() ? resource::JsonValue::make_object() : scene;
    root["settings"] = settings.is_null() ? resource::JsonValue::make_object() : settings;
    return root;
}

bool SaveSlot::from_json(const resource::JsonValue& value, std::string* out_error) {
    auto fail = [&](const std::string& message) {
        if (out_error != nullptr) *out_error = message;
        return false;
    };

    if (!value.is_object()) return fail("ожидается объект JSON");

    static const char* required[] = {"save_version", "scene"};
    for (const char* key : required) {
        if (!value.contains(key)) return fail(std::string("обязательное поле отсутствует: ") + key);
    }
    if (!value.at("save_version").is_number()) return fail("save_version: ожидается число");
    if (!value.at("scene").is_object()) return fail("scene: ожидается объект");

    save_version = static_cast<std::int32_t>(value.at("save_version").as_int(kCurrentSaveVersion));
    schema_version = static_cast<std::int32_t>(value.at("schema_version").as_int(1));
    seed = static_cast<std::uint64_t>(value.at("seed").as_int(0));
    rng_state = value.at("rng_state").as_string();
    label = value.at("label").as_string();
    timestamp = value.at("timestamp").as_string();
    scene = value.at("scene");
    settings = value.contains("settings") ? value.at("settings") : resource::JsonValue::make_object();
    return true;
}

} // namespace engine::scene
