// game/save/combat_save_state.cpp — реализация сериализации боевой сцены (FR-E90).
#include "save/combat_save_state.h"

namespace game {

using engine::resource::JsonArray;
using engine::resource::JsonObject;
using engine::resource::JsonValue;

JsonValue CombatSaveState::serialize_state() const {
    JsonValue root = JsonValue::make_object();
    root["round"] = round;

    JsonArray entities;
    for (const auto& c : combatants) {
        JsonValue e = JsonValue::make_object();
        e["name"] = c.name;
        e["player_side"] = c.player_side;
        e["hp_max"] = c.hp_max;
        e["hp_current"] = c.hp_current;
        e["temp_hp"] = c.temp_hp;
        e["ac"] = c.ac;
        e["attack_mod"] = c.attack_mod;
        e["dex_mod"] = c.dex_mod;
        e["cell_x"] = c.cell_x;
        e["cell_y"] = c.cell_y;
        entities.push_back(std::move(e));
    }
    root["entities"] = JsonValue(std::move(entities));

    JsonArray order;
    for (const auto& entry : initiative) {
        JsonValue i = JsonValue::make_object();
        i["entity"] = static_cast<std::int64_t>(entry.entity);
        i["score"] = entry.score;
        i["dex_modifier"] = entry.dex_modifier;
        order.push_back(std::move(i));
    }
    root["initiative"] = JsonValue(std::move(order));
    return root;
}

bool CombatSaveState::deserialize_state(const JsonValue& state) {
    if (!state.is_object()) return false;
    if (!state.contains("round")) return false;

    round = static_cast<int>(state.at("round").as_int(1));
    if (round < 1) return false;

    combatants.clear();
    const JsonValue& entities = state.at("entities");
    if (entities.is_array()) {
        for (const auto& e : entities.as_array()) {
            if (!e.is_object()) return false; // битые данные — отказ без падения (NFR-E14)
            SavedCombatant c;
            c.name = e.at("name").as_string();
            c.player_side = e.at("player_side").as_bool(true);
            c.hp_max = static_cast<int>(e.at("hp_max").as_int(1));
            c.hp_current = static_cast<int>(e.at("hp_current").as_int(0));
            c.temp_hp = static_cast<int>(e.at("temp_hp").as_int(0));
            c.ac = static_cast<int>(e.at("ac").as_int(10));
            c.attack_mod = static_cast<int>(e.at("attack_mod").as_int(0));
            c.dex_mod = static_cast<int>(e.at("dex_mod").as_int(0));
            c.cell_x = static_cast<int>(e.at("cell_x").as_int(0));
            c.cell_y = static_cast<int>(e.at("cell_y").as_int(0));
            if (c.hp_max < 1 || c.hp_current < 0 || c.temp_hp < 0) return false;
            combatants.push_back(std::move(c));
        }
    }

    initiative.clear();
    const JsonValue& order = state.at("initiative");
    if (order.is_array()) {
        for (const auto& i : order.as_array()) {
            if (!i.is_object()) return false;
            rules::InitiativeEntry entry;
            entry.entity = static_cast<rules::EntityId>(i.at("entity").as_int(0));
            entry.score = static_cast<int>(i.at("score").as_int(0));
            entry.dex_modifier = static_cast<int>(i.at("dex_modifier").as_int(0));
            initiative.push_back(entry);
        }
    }
    return true;
}

} // namespace game
