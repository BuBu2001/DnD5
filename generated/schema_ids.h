// generated/schema_ids.h — ФАЙЛ СГЕНЕРИРОВАН tools/schema_gen, НЕ РЕДАКТИРОВАТЬ.
// Источник: assets/schemas/*.schema.json. Повторная генерация:
//   ./build/tools/schema_gen/schema_gen --assets assets
#pragma once

#include <array>
#include <string_view>

namespace dnd::data {

// Идентикаторы схем: значение поля "$schema" в файлах данных (п. 4.5.3)
inline constexpr std::string_view kSchemaAbilityTable = "schemas/ability_table.schema.json";
inline constexpr std::string_view kSchemaAtlas = "schemas/atlas.schema.json";
inline constexpr std::string_view kSchemaAtlasManifest = "schemas/atlas_manifest.schema.json";
inline constexpr std::string_view kSchemaCharacter = "schemas/character.schema.json";
inline constexpr std::string_view kSchemaClass = "schemas/class.schema.json";
inline constexpr std::string_view kSchemaCondition = "schemas/condition.schema.json";
inline constexpr std::string_view kSchemaEnemy = "schemas/enemy.schema.json";
inline constexpr std::string_view kSchemaItem = "schemas/item.schema.json";
inline constexpr std::string_view kSchemaMap = "schemas/map.schema.json";
inline constexpr std::string_view kSchemaSave = "schemas/save.schema.json";

// Текущая версия формата данных (DATA-E02: проверка при загрузке и миграции)
inline constexpr int kSchemaVersion = 1;

// Перечисления, извлечённые из схем (единая точка правды вместо литералов).
// Имя формируется как k<Схема><Поле>Values, например kItemDamageTypeValues.
inline constexpr std::array<std::string_view, 6> kClassPrimaryAbilityValues = {"Str", "Dex", "Con", "Int", "Wis", "Cha"};
inline constexpr std::array<std::string_view, 13> kItemDamageTypeValues = {"Bludgeoning", "Piercing", "Slashing", "Fire", "Cold", "Lightning", "Acid", "Poison", "Necrotic", "Radiant", "Force", "Psychic", "Thunder"};

} // namespace dnd::data
