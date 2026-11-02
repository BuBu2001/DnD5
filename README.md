# Двухмерный игровой движок для пошаговой ролевой игры на механиках D&D 5e

Шифр: **АБВГ.00004**. Комплект документации проекта: DOC-01…DOC-04 (ГОСТ 19.201-78).
Текущая версия: **V0.4.5** (см. `CHANGELOG.md`).

## Быстрый старт (Linux, GCC 12+, CMake 3.20+)

```bash
# Headless-сборка (ядро + правила + тесты, без SDL2/OpenGL)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure        # 58 кейсов / 957 проверок

# Полная сборка с окном SDL2 и OpenGL 3.3 Core
cmake -S . -B build -DDND_WITH_SDL2=ON
cmake --build build -j
./build/game/game                                 # боевая сцена (вертикальный срез)
./build/tests/benchmarks --assets assets --strict # бенчмарки с порогами NFR
```

Пресеты: `windows-msvc-debug|release` (статическая CRT /MTd|/MT, NFR-E14),
`linux-gcc-debug|release`.

## Инструменты (этап M11)

```bash
./build/tools/json_validator --assets assets            # конверт + схемы + ссылки
./build/tools/map_validator  --assets assets --verbose  # карты: слои, проходимость
./build/tools/schema_gen     --assets assets --markdown docs/gost/data_formats.md \
                                                --header generated/schema_ids.h
./build/tools/asset_packer --scan assets/textures --output atlas.json
python3 scripts/check_includes.py                       # изоляция слоёв (NFR-E10)
```

## Структура проекта (Приложение Г, п. Г.14 ТЗ DOC-04)

```
engine/          — слой движка (не знает правил D&D, E-D01)
  core/            log, config, event_bus, file_system, result, json_value
  platform/        window, input, timer, main_loop, font_atlas
  render/          gl_loader, sprite_batch, camera, primitives, debug_overlay
  resource/        resource_manager, json_loader, texture_loader
  audio/           audio_system (SDL2_mixer)
  ecs/             entity, registry, component_storage, view, system, command_buffer
  ui/              widgets, containers, events (1280×720, letterbox)
  scene/           scene, scene_stack, state_machine
rules/           — модуль правил D&D 5e (только stdlib, без SDL/GL — E-D11)
game/            — combat_scene (вертикальный срез, Приложение Д), adapters/, main.cpp
assets/          — данные: maps/, textures/, fonts/, audio/, data/ (пути assets:/…)
tests/           — rules/engine/platform/integration/golden + benchmarks
tools/           — json_validator, schema_gen, map_validator, asset_packer
scripts/         — check_includes.py, hdrcheck.sh, генераторы шрифтовых таблиц
docs/            — architecture.md, decisions.md (DR-01…10), traceability.md, gost/
third_party/     — заготовка (stb); SDL2 — системный (таблица 4.16, DR-01)
```

## Направления зависимостей (п. 3.3.2)

`game → engine`, `game → rules`, `adapters → {engine, rules}`.
Запрещено: `engine → rules`, `engine → game`, `rules → engine`.
Проверка — `scripts/check_includes.py` (блокирует CI при нарушении).

## Что реализовано в V0.4

- **Платформа/рендер (M1–M2):** окно SDL2 + headless-режим, OpenGL 3.3 Core
  (собственный загрузчик функций), SpriteBatch (≤8 draw call/кадр), камера,
  примитивы, процедурный шрифтовой атлас, letterbox 1280×720, DebugOverlay.
- **Ресурсы и данные (M3):** JSON-конверт `{$schema, schema_version, id}`,
  кэш документов, TGA/процедурные текстуры, схемы и валидаторы.
- **Сохранения (M6):** состояние партии + RNG, миграция версий.
- **UI (M8):** виджеты и контейнеры, события, боевая панель с полным
  разбором броска — только DTO (E-D12).
- **Правила:** DiceRoller с инъекцией ГСЧ (xoshiro256**), криты/осечки,
  инициатива с tie-breaker, temp HP, сопротивления, состояния, attunement,
  экономика действий, слои модификаторов, 19 формул п. Г.13.
- **Вертикальный срез (Приложение Д):** комната 20×15, Fighter + Rogue vs 2 гоблина.
- **Качество:** `-Werror`, пресеты MSVC/GCC, 5 уровней тестов + золотой эталон
  (seed 42) + бенчмарки с порогами; 58 кейсов / 957 проверок, ctest 5/5.

## Документация

- `docs/architecture.md` — архитектура по слоям и модулям;
- `docs/decisions.md` — решения DR-01…DR-10 с обоснованиями (Приложение Б);
- `docs/traceability.md` — матрица «требование → модуль → проверка» (Приложение В);
- `docs/gost/data_formats.md` — справочник форматов данных (10 схем);
- `CHANGELOG.md` — история версий в формате «VX.Y — описание».

## Дальнейшие этапы (таблица 7.2)

Подключение стора в `third_party/` (Catch2, nlohmann_json, stb, tinyxml2),
звук M9 в полном объёме, редактор карт, packaging. Отложённое перечислено
в разделе 10 `docs/traceability.md`.
