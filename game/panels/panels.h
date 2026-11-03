// game/panels/panels.h — загрузка данных панелей персонажа (C) и инвентаря (I).
// Панели получают ТОЛЬКО DTO и простые записи (E-D12, UI-E01): обращение к
// формулам правил из UI запрещено; здесь — только чтение данных assets:/.
#pragma once

#include <string>
#include <vector>

#include "core/file_system.h"
#include "rules/dto.h"

namespace game {

// Строка инвентаря: имя предмета и пояснение (урон/тип) — данные из JSON
struct EquipmentEntry {
    std::string item_id;
    std::string name;
    std::string note; // «1d8 рубящий» / описание компонента
};

// Экипировка персонажа: список equipment из characters/<file>.json, каждый
// предмет догружается из items/<id>.json. Отсутствующие файлы пропускаются
// (устойчивость к неполным данным, п. 4.2.3).
std::vector<EquipmentEntry> load_character_equipment(engine::core::FileSystem& fs,
                                                     const std::string& character_json_path);

// Сводка персонажа для CharacterSheetPanel из значений боевой сцены
// (CharacterSummaryDTO — п. Г.8; поля заполняет игровой слой, не UI).
rules::CharacterSummaryDTO make_character_summary(const std::string& name, int level, int ac,
                                                  int hp_current, int hp_max, int speed_feet);

} // namespace game
