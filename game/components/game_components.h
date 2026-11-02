// game/components/game_components.h — состав начальных компонентов ECS (п. 4.1.6.2).
// Компоненты хранят ДАННЫЕ и не содержат поведения (п. 4.1.6.3); поведение —
// в системах game/systems/. Компоненты, знающие правила (Health, Condition,
// TurnResource), намеренно размещены в игровом слое: каталогу engine/* запрещено
// включать rules/* (E-D01, NFR-E10, п. 4.8.2).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "render/renderer.h"
#include "rules/abilities.h"
#include "rules/action_economy.h"
#include "rules/conditions.h"
#include "rules/damage.h"
#include "rules/health.h"
#include "rules/items.h"
#include "rules/proficiency.h"

namespace game {

// Позиция и масштаб спрайта в пикселях сцены (п. 4.1.6.2)
struct TransformComponent {
    float x = 0.f;
    float y = 0.f;
    float rotation = 0.f;   // градусы; используется с V1 (FR-E32)
    float scale_x = 1.f;
    float scale_y = 1.f;
};

// Внешний вид сущности (FR-E22…FR-E31)
struct SpriteComponent {
    engine::render::TextureId texture = engine::render::kInvalidTexture;
    engine::render::Rect source_rect{};   // область в атласе (FR-E24)
    engine::render::Color tint{};         // тонирование (FR-E30)
    bool flip_x = false;                  // разворот (FR-E31)
    bool flip_y = false;
    int layer = 0;                        // слой отрисовки (FR-E27)
};

// Положение на тактической сетке: клетка и уровень высоты (п. 3.1.1, 1 клетка = 5 футов)
struct GridPositionComponent {
    int x = 0;
    int y = 0;
    int elevation = 0;
};

// Состояние здоровья (rules::HealthState, FR-E113)
struct HealthComponent {
    rules::HealthState state;
};

// Активные состояния сущности (FR-E116, E-D08)
struct ConditionComponent {
    std::vector<rules::ConditionInstance> conditions;
};

// Ресурсы хода (FR-E118, экономика действий)
struct TurnResourceComponent {
    rules::TurnResources resources;
};

// Сопротивления, уязвимости и иммунитеты (FR-E115)
struct ResistanceComponent {
    rules::DamageResistance resistance;
};

// Идентификатор данных сущности: связь с JSON (DATA-E03, NFR-E13)
struct DataRefComponent {
    std::string data_id;         // "Enemy_Goblin", "Character_PlayerFighter"
    std::string source_path;     // "assets:/data/enemies/goblin.json"
};

// Инвентарь и экипировка (FR-K80…FR-K83, AC-K18…AC-K20)
struct InventoryComponent {
    rules::Inventory inventory;
};

struct EquipmentComponent {
    rules::Equipment equipment;
};

// Характеристики и владение (FR-E110, FR-E111)
struct AbilityScoresComponent {
    rules::AbilityScores scores;
    rules::ProficiencySet proficiencies;
    int level = 1;
};

// Тег стороны: игровой слой, а не движок (движок не знает о «игроках», п. 1.5)
struct SideTagComponent {
    bool player_side = true;
};

} // namespace game
