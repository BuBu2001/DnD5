// game/panels/panels.cpp — реализация загрузки данных панелей (docs/controls.md).
#include "panels/panels.h"

#include <algorithm>
#include <cctype>

#include "resource/json_loader.h"
#include "resource/schema_registry.h"
#include "resource/schema_validator.h"

namespace game {
namespace {

// Item_ChainShirt → chain_shirt; Item_Longsword → longsword
std::string item_id_to_filename(const std::string& id) {
    std::string base = id;
    const auto pos = base.find("Item_");
    if (pos == 0) base = base.substr(5);
    std::string snake;
    for (std::size_t i = 0; i < base.size(); ++i) {
        const char ch = base[i];
        if (std::isupper(static_cast<unsigned char>(ch)) && i > 0 &&
            !std::isupper(static_cast<unsigned char>(base[i - 1]))) {
            snake += '_';
        }
        snake += static_cast<char>(
            std::tolower(static_cast<unsigned char>(ch)));
    }
    return snake;
}

} // namespace

std::vector<EquipmentEntry> load_character_equipment(engine::core::FileSystem& fs,
                                                     const std::string& character_json_path) {
    std::vector<EquipmentEntry> out;
    engine::resource::SchemaValidator validator;
    engine::resource::SchemaRegistry registry(fs, validator);
    registry.load_directory("assets:/schemas");
    engine::resource::JsonLoader loader(fs, validator);

    const auto character = loader.load(character_json_path);
    if (!character.is_ok()) return out;

    for (const auto& item_id : character.value().value.at("equipment").as_array()) {
        const std::string id = item_id.as_string_or("");
        if (id.empty()) continue;
        EquipmentEntry entry;
        entry.item_id = id;
        entry.name = id;
        const auto item = loader.load("assets:/data/items/" + item_id_to_filename(id) + ".json");
        if (item.is_ok()) {
            const auto& doc = item.value().value;
            entry.name = doc.at("name").as_string_or(id);
            for (const auto& comp : doc.at("components").as_array()) {
                if (comp.at("type").as_string_or("") == "Weapon") {
                    entry.note = comp.at("base_damage").as_string_or("") + " " +
                                 comp.at("damage_type").as_string_or("");
                    break;
                }
                entry.note = comp.at("type").as_string_or("");
            }
        }
        out.push_back(std::move(entry));
    }
    return out;
}

rules::CharacterSummaryDTO make_character_summary(const std::string& name, int level, int ac,
                                                  int hp_current, int hp_max, int speed_feet) {
    rules::CharacterSummaryDTO dto;
    dto.name = name;
    dto.level = level;
    dto.ac = ac;
    dto.hp_current = hp_current;
    dto.hp_max = hp_max;
    dto.temp_hp = 0;
    dto.speed_feet = speed_feet;
    return dto;
}

} // namespace game
