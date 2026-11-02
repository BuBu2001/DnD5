// game/save/combat_save_state.h — сериализация боевой сцены (FR-E90, FR-E91, FR-E92).
// Игровой слой имеет право знать и engine, и rules (п. 3.3.2): здесь состояние боя
// преобразуется в JSON формата п. 4.1.9.2 и обратно. Состав поля "scene":
//   { "round": 1,
//     "entities": [ { "name", "player_side", "hp_max", "hp_current", "temp_hp",
//                     "ac", "attack_mod", "dex_mod", "cell_x", "cell_y" } ],
//     "initiative": [ { "entity": 0, "score": 15, "dex_modifier": 3 } ] }
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "resource/json.h"
#include "rules/initiative.h"
#include "scene/serializable.h"

namespace game {

struct SavedCombatant {
    std::string name;
    bool player_side = true;
    int hp_max = 1;
    int hp_current = 1;
    int temp_hp = 0;
    int ac = 10;
    int attack_mod = 0;
    int dex_mod = 0;
    int cell_x = 0;
    int cell_y = 0;
};

class CombatSaveState final : public engine::scene::ISerializable {
public:
    int round = 1;
    std::vector<SavedCombatant> combatants;
    std::vector<rules::InitiativeEntry> initiative;

    engine::resource::JsonValue serialize_state() const override;
    bool deserialize_state(const engine::resource::JsonValue& state) override;
};

} // namespace game
