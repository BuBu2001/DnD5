# CHANGELOG — Двухмерный игровой движок D&D 5e (шифр АБВГ.00004)

Формат версий: V<major>.<minor>. Записи ведутся в обратном хронологическом порядке.

## V0.2 — добавил Т3 для игрового движка.

- Проанализирован комплект документации проекта (DOC-01…DOC-04, ГОСТ 19.201-78).
- Создан каркас программного кода движка по Приложению Г (п. Г.14) ТЗ DOC-04:
  каталоги `engine/` (core, platform, render, resource, audio, ecs, ui, scene),
  `rules/`, `game/`, `assets/`, `tests/`, `tools/`, `third_party/`, `docs/`.
- Добавлена система сборки CMake (заготовка п. Г.11): цели `engine`, `rules`,
  `game`, `tests`; стандарт C++20.
- Реализованы первые рабочие модули ядра движка (этап M0/M1):
  - `engine/core/log` — логирование с уровнями (FR-E08);
  - `engine/core/result` — тип результата без исключений;
  - `engine/core/event_bus` — событийная шина subscribe/publish/unsubscribe (FR-E10);
  - `engine/core/config` — конфигурация из файла ключ=значение (FR-E09);
  - `engine/core/file_system` — виртуальные пути `assets:/...` (FR-E07, DATA-E05);
  - `engine/platform/timer`, `engine/platform/main_loop` — главный цикл с фиксированным
    шагом логики 60 Гц и ограничением до 5 шагов за кадр (п. 4.1.2.3, формула (2));
  - `engine/ecs` — каркас ECS-lite (entity, registry, component_storage, view, system);
  - `engine/scene` — каркас стека сцен и конечного автомата (state_machine);
  - заглушки интерфейсов слоёв `render`, `resource`, `audio`, `ui`, `platform`
    (реализация — этап M2+).
- Реализованы опорные интерфейсы модуля правил по Приложению Г (п. Г.2–Г.8):
  abilities (modifier = floor((score−10)/2)), dice_roller + random_provider
  (IRandomProvider, E-D07), damage (сопротивления), initiative (tie-breaker п. 4.1.11.3),
  action_economy (TurnResources), turn_state_machine (TurnState, п. 4.1.7.2),
  modifier_stack (слои модификаторов), health (порядок списания урона, п. 4.1.11.4),
  formulas.h (перечень формул п. Г.13).
- Добавлены модульные тесты (п. Г.12, таблица 8.2): таблица модификаторов характеристик,
  DiceRoller (преимущество/помеха/cancelled), инициатива с tie-breaker, списание урона.
- Добавлен исполняемый пример `game/main.cpp` (прогон главного цикла без окна).
- Создана документация каркаса: `README.md`, `docs/architecture.md`,
  `docs/traceability.md` (каркас матрицы трассировки, Приложение В).
- Проверка сборки: `cmake -S . -B build && cmake --build build && ctest` — успешно.

## V0.1 — добавил базовые файлы для работоспособности

- Добавлены базовые документы комплекта проекта (ГОСТ 19.201-78):
  - DOC-01 «Кодификация правил расчёта игровых механик D&D 5e»;
  - DOC-02 «Адаптация механик D&D 5e в цифровых подсистемах игры»;
  - DOC-03 «Реестр ресурсов и ассетов для разработки игры»;
  - DOC-04 «Техническое задание на двухмерный игровой движок».
- Инициализирован репозиторий Git (ветка main).
