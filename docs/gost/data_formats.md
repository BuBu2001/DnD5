# Форматы данных (DATA-E01…DATA-E05)

Файл СГЕНЕРИРОВАН tools/schema_gen по схемам `assets/schemas/*.schema.json`.
Руками не редактируется: изменения вносятся в схемы, затем справочник
пересоздаётся командой `schema_gen --assets assets`.

## Общие требования к файлам данных

* Кодировка UTF-8 без BOM (п. 4.5.5).
* Обязательная обёртка: `$schema`, `schema_version`, `id` (п. 4.5.3).
* Пути к ресурсам — виртуальные `assets:/…`; абсолютные пути и пути,
  зависящие от ОС, в данных запрещены (DATA-E05).
* Проверка соответствия выполняется при загрузке; файл с ошибкой не
  загружается, факт пишется в журнал и публикуется событие шины
  (FR-E47, FR-E48, п. 4.1.4.4).

## Схемы

### ability_table

Файл схемы: `assets:/schemas/ability_table.schema.json`  
Идентификатор (`$schema` в данных): `schemas/ability_table.schema.json`

| Поле | Тип | Обязательное | Ограничения |
| --- | --- | --- | --- |
| `$schema` | string | да | — |
| `abilities` | array of object | да | элементов ≥ 6 |
|   `abilities[].affects` | string | нет | — |
|   `abilities[].code` | string | да | одно из: Str, Dex, Con, Int, Wis, Cha |
|   `abilities[].name` | string | да | длина ≥ 1 |
|   `abilities[].name_en` | string | нет | — |
| `allowed_range` | object | нет | — |
|   `allowed_range.max` | integer | нет | — |
|   `allowed_range.min` | integer | нет | — |
| `comment` | string | нет | — |
| `feet_per_cell` | integer | нет | минимум 1; максимум 10 |
| `id` | string | да | длина ≥ 3 |
| `modifier_table` | array of object | нет | — |
|   `modifier_table[].modifier` | integer | да | — |
|   `modifier_table[].score` | integer | да | — |
| `player_start_range` | object | нет | — |
|   `player_start_range.max` | integer | нет | — |
|   `player_start_range.min` | integer | нет | — |
| `schema_version` | integer | да | минимум 1 |
| `standard_speed_feet` | integer | нет | минимум 0; максимум 120 |

### atlas

Файл схемы: `assets:/schemas/atlas.schema.json`  
Идентификатор (`$schema` в данных): `schemas/atlas.schema.json`

Атлас спрайтов: результат работы tools/asset_packer. Одна страница атласа — одна текстура, поэтому отрисовка сцены укладывается в бюджет draw calls (E-D05, NFR-E01).

| Поле | Тип | Обязательное | Ограничения |
| --- | --- | --- | --- |
| `$schema` | string | да | — |
| `border` | integer | нет | минимум 0; максимум 16 |
| `frames` | array of object | да | элементов ≥ 1 |
|   `frames[].frame` | object | да | — |
|     `frames[].frame.h` | integer | да | минимум 1 |
|     `frames[].frame.w` | integer | да | минимум 1 |
|     `frames[].frame.x` | integer | да | минимум 0 |
|     `frames[].frame.y` | integer | да | минимум 0 |
|   `frames[].id` | string | да | длина ≥ 1 |
|   `frames[].page` | integer | да | минимум 0 |
|   `frames[].rotated` | boolean | нет | — |
|   `frames[].source` | string | нет | — |
| `id` | string | да | длина ≥ 3 |
| `padding` | integer | нет | минимум 0; максимум 16 |
| `pages` | array of object | да | элементов ≥ 1 |
|   `pages[].height` | integer | да | минимум 16; максимум 4096 |
|   `pages[].index` | integer | да | минимум 0 |
|   `pages[].texture` | string | да | длина ≥ 1 |
|   `pages[].width` | integer | да | минимум 16; максимум 4096 |
| `schema_version` | integer | да | минимум 1 |

### atlas_manifest

Файл схемы: `assets:/schemas/atlas_manifest.schema.json`  
Идентификатор (`$schema` в данных): `schemas/atlas_manifest.schema.json`

Манифест атласа: вход tools/asset_packer. Перечисляет кадры (id, w, h, source) до упаковки; раскладка формируется упаковщиком и записывается в атлас (schemas/atlas.schema.json).

| Поле | Тип | Обязательное | Ограничения |
| --- | --- | --- | --- |
| `$schema` | string | да | — |
| `atlas_id` | string | да | длина ≥ 3 |
| `comment` | string | нет | — |
| `frames` | array of object | да | элементов ≥ 1 |
|   `frames[].h` | integer | да | минимум 1; максимум 4096 |
|   `frames[].id` | string | да | длина ≥ 1 |
|   `frames[].source` | string | нет | длина ≥ 1 |
|   `frames[].w` | integer | да | минимум 1; максимум 4096 |
| `id` | string | да | длина ≥ 3 |
| `schema_version` | integer | да | минимум 1 |
| `texture` | string | нет | — |

### character

Файл схемы: `assets:/schemas/character.schema.json`  
Идентификатор (`$schema` в данных): `schemas/character.schema.json`

| Поле | Тип | Обязательное | Ограничения |
| --- | --- | --- | --- |
| `$schema` | string | да | — |
| `abilities` | object | да | — |
|   `abilities.Cha` | integer | да | минимум 1; максимум 20 |
|   `abilities.Con` | integer | да | минимум 1; максимум 20 |
|   `abilities.Dex` | integer | да | минимум 1; максимум 20 |
|   `abilities.Int` | integer | да | минимум 1; максимум 20 |
|   `abilities.Str` | integer | да | минимум 1; максимум 20 |
|   `abilities.Wis` | integer | да | минимум 1; максимум 20 |
| `class_id` | string | да | длина ≥ 3 |
| `equipment` | array of string | нет | — |
| `features` | array of string | нет | — |
| `hp_max` | integer | нет | минимум 1; максимум 200 |
| `id` | string | да | длина ≥ 3 |
| `level` | integer | да | минимум 1; максимум 5 |
| `name` | string | да | длина ≥ 1 |
| `schema_version` | integer | да | минимум 1 |
| `speed_feet` | integer | нет | минимум 0; максимум 60 |

### class

Файл схемы: `assets:/schemas/class.schema.json`  
Идентификатор (`$schema` в данных): `schemas/class.schema.json`

| Поле | Тип | Обязательное | Ограничения |
| --- | --- | --- | --- |
| `$schema` | string | да | — |
| `armor_proficiency` | array of string | нет | — |
| `features` | array of object | да | — |
|   `features[].category` | integer | да | минимум 1; максимум 3 |
|   `features[].id` | string | да | длина ≥ 3 |
|   `features[].min_level` | integer | нет | минимум 1; максимум 5 |
|   `features[].name` | string | нет | — |
| `hit_die` | integer | да | минимум 4; максимум 20 |
| `id` | string | да | длина ≥ 3 |
| `name` | string | да | длина ≥ 1 |
| `primary_ability` | string | нет | одно из: Str, Dex, Con, Int, Wis, Cha |
| `saving_throw_proficiency` | array of string | нет | — |
| `schema_version` | integer | да | минимум 1 |
| `weapon_proficiency` | array of string | нет | — |

### condition

Файл схемы: `assets:/schemas/condition.schema.json`  
Идентификатор (`$schema` в данных): `schemas/condition.schema.json`

| Поле | Тип | Обязательное | Ограничения |
| --- | --- | --- | --- |
| `$schema` | string | да | — |
| `conditions` | array of object | да | элементов ≥ 6 |
|   `conditions[].description` | string | нет | — |
|   `conditions[].flags` | array of string | да | — |
|   `conditions[].icon_key` | string | нет | — |
|   `conditions[].id` | string | да | длина ≥ 3 |
|   `conditions[].name` | string | да | длина ≥ 1 |
|   `conditions[].speed_override_feet` | integer | нет | минимум -1; максимум 60 |
| `id` | string | да | длина ≥ 3 |
| `schema_version` | integer | да | минимум 1 |

### enemy

Файл схемы: `assets:/schemas/enemy.schema.json`  
Идентификатор (`$schema` в данных): `schemas/enemy.schema.json`

| Поле | Тип | Обязательное | Ограничения |
| --- | --- | --- | --- |
| `$schema` | string | да | — |
| `abilities` | object | да | — |
|   `abilities.Cha` | integer | да | минимум 1; максимум 30 |
|   `abilities.Con` | integer | да | минимум 1; максимум 30 |
|   `abilities.Dex` | integer | да | минимум 1; максимум 30 |
|   `abilities.Int` | integer | да | минимум 1; максимум 30 |
|   `abilities.Str` | integer | да | минимум 1; максимум 30 |
|   `abilities.Wis` | integer | да | минимум 1; максимум 30 |
| `ac` | integer | да | минимум 1; максимум 40 |
| `attacks` | array of object | нет | — |
|   `attacks[].attack_bonus` | integer | да | минимум -5; максимум 20 |
|   `attacks[].damage` | string | да | — |
|   `attacks[].damage_type` | string | да | одно из: Bludgeoning, Piercing, Slashing, Fire, Cold, Lightning, Acid, Poison, Necrotic, Radiant, Force, Psychic, Thunder |
|   `attacks[].id` | string | да | длина ≥ 3 |
|   `attacks[].name` | string | нет | — |
|   `attacks[].range_feet` | integer | нет | минимум 5; максимум 600 |
| `challenge_rating` | number | нет | минимум 0; максимум 30 |
| `hp` | integer | да | минимум 1; максимум 1000 |
| `id` | string | да | длина ≥ 3 |
| `name` | string | да | длина ≥ 1 |
| `schema_version` | integer | да | минимум 1 |
| `speed_feet` | integer | да | минимум 0; максимум 120 |

### item

Файл схемы: `assets:/schemas/item.schema.json`  
Идентификатор (`$schema` в данных): `schemas/item.schema.json`

| Поле | Тип | Обязательное | Ограничения |
| --- | --- | --- | --- |
| `$schema` | string | да | — |
| `components` | array of any | да | элементов ≥ 1 |
| `description` | string | нет | — |
| `id` | string | да | длина ≥ 3 |
| `name` | string | да | длина ≥ 1 |
| `schema_version` | integer | да | минимум 1 |

### map

Файл схемы: `assets:/schemas/map.schema.json`  
Идентификатор (`$schema` в данных): `schemas/map.schema.json`

| Поле | Тип | Обязательное | Ограничения |
| --- | --- | --- | --- |
| `$schema` | string | да | — |
| `description` | string | нет | — |
| `grid` | object | да | — |
|   `grid.cell_feet` | integer | да | минимум 5; максимум 5 |
|   `grid.columns` | integer | да | минимум 1; максимум 512 |
|   `grid.rows` | integer | да | минимум 1; максимум 512 |
| `id` | string | да | длина ≥ 3 |
| `layers` | array of object | да | элементов ≥ 1 |
|   `layers[].id` | string | да | длина ≥ 1 |
|   `layers[].kind` | string | да | одно из: terrain, objects, light |
|   `layers[].rows` | array of string | да | — |
|   `layers[].visible` | boolean | нет | — |
| `name` | string | да | длина ≥ 1 |
| `schema_version` | integer | да | минимум 1 |
| `spawns` | array of object | нет | — |
|   `spawns[].column` | integer | да | минимум 0 |
|   `spawns[].entity_id` | string | да | длина ≥ 3 |
|   `spawns[].facing` | string | нет | одно из: North, East, South, West |
|   `spawns[].row` | integer | да | минимум 0 |
| `tile_size_px` | integer | нет | минимум 8; максимум 256 |
| `tiles` | array of object | нет | — |
|   `tiles[].cover` | string | нет | одно из: None, Half, ThreeQuarters, Full |
|   `tiles[].difficult_terrain` | boolean | нет | — |
|   `tiles[].key` | string | да | длина ≥ 1; длина ≤ 1 |
|   `tiles[].name` | string | да | длина ≥ 1 |
|   `tiles[].passable` | boolean | да | — |
|   `tiles[].texture_key` | string | нет | — |

### save

Файл схемы: `assets:/schemas/save.schema.json`  
Идентификатор (`$schema` в данных): `schemas/save.schema.json`

| Поле | Тип | Обязательное | Ограничения |
| --- | --- | --- | --- |
| `$schema` | string | да | — |
| `rng_state` | string | нет | — |
| `save_version` | integer | да | минимум 1 |
| `scene` | object | да | — |
|   `scene.entities` | array | нет | — |
|   `scene.initiative` | array | нет | — |
|   `scene.round` | integer | да | минимум 1 |
| `schema_version` | integer | да | минимум 1 |
| `seed` | integer | да | минимум 0 |
| `settings` | object | нет | — |
|   `settings.master` | number | нет | минимум 0; максимум 1 |
|   `settings.music` | number | нет | минимум 0; максимум 1 |
|   `settings.sfx` | number | нет | минимум 0; максимум 1 |

