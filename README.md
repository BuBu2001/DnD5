# Двухмерный игровой движок для пошаговой ролевой игры на механиках D&D 5e

Шифр: **АБВГ.00004**. Комплект документации проекта: DOC-01…DOC-04 (ГОСТ 19.201-78).
Текущая версия каркаса: **V0.2** (см. `CHANGELOG.md`).

## Быстрый старт (Linux, GCC 12+, CMake 3.20+)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure   # модульные тесты
./build/game/game                             # демонстрация главного цикла
```

## Структура проекта (Приложение Г, п. Г.14 ТЗ DOC-04)

```
engine/          — слой движка (не знает правил D&D, E-D01)
  core/            log, config, event_bus, file_system, result
  platform/        window, input, timer, main_loop
  render/          renderer (заглушка до M2), sprite_batch, camera…
  resource/        resource_manager (заглушка до M3)
  audio/           audio_system (заглушка до M9)
  ecs/             entity, registry, component_storage, view, system
  ui/              control (заглушка до M8)
  scene/           scene, scene_stack, state_machine
rules/           — модуль правил D&D 5e (статическая библиотека, без зависимостей от engine)
game/            — игровая логика + main.cpp (связывает engine и rules)
assets/          — данные: maps/, textures/, fonts/, audio/, data/ (виртуальные пути assets:/…)
tests/           — модульные/интеграционные тесты (раздел 8 ТЗ)
tools/           — asset_packer, map_validator, schema_gen
third_party/     — SDL2, nlohmann_json, stb, tinyxml2, glad, Catch2 (таблица 4.16)
docs/            — документация; docs/gost/ — ГОСТ-документы
```

## Направления зависимостей (п. 3.3.2)

`game → engine`, `game → rules`, `adapters → {engine, rules}`.
Запрещено: `engine → rules`, `engine → game`, `rules → engine`.

## Что реализовано в V0.2

- Главный цикл с фиксированным шагом логики 60 Гц и лимитом 5 шагов (п. 4.1.2.3–4).
- ECS-lite: Registry, ComponentStorage (index+generation), View, ISystem.
- Стек сцен и универсальный FSM с guard-условиями.
- Событийная шина, конфигурация, виртуальная файловая система `assets:/`, логирование.
- Модуль правил: AbilityScores, DiceRoller (+ IRandomProvider/xoshiro256**), HealthState,
  InitiativeQueue (tie-breaker), TurnResources (атомарная трата), слои модификаторов,
  DTO для UI, формулы из п. Г.13.
- 12 тестовых кейсов (~70 проверок) по таблице 8.2 и контрольным примерам раздела 8.

## Дальнейшие этапы (таблица 7.2)

M1 — SDL2-окно и ввод; M2 — OpenGL-рендерер и спрайт-батчинг; M3 — ресурсы и JSON-валидация;
далее M4…M11. Обновления фиксируются в `CHANGELOG.md` в формате «VX.Y — описание».
