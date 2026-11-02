# Архитектура движка (состояние V0.4)

Документ фиксирует архитектуру по ТЗ DOC-04 (АБВГ.00004-01 90 01-1).
Решения с обоснованиями — `docs/decisions.md`; соответствие требованиям —
`docs/traceability.md`.

## 1. Три слоя программы (таблица 3.1, п. 3.3.2)

| Слой | Каталог | Знание о D&D | Зависимости |
|------|---------|--------------|-------------|
| Engine Layer | `engine/` | нет (E-D01) | stdlib, SDL2/GL (опционально, DR-01) |
| Rules Core   | `rules/`  | да | только stdlib (E-D11, NFR-E10) |
| Game Layer   | `game/`   | связывает | engine, rules, adapters |

Направления: `game → engine`, `game → rules`, `adapters → {engine, rules}`.
Запрещены: `engine → rules`, `engine → game`, `rules → engine`.
Изоляция проверяется статически — `scripts/check_includes.py` (CI).

## 2. Ядро движка (engine/core)

- `log` — уровни Debug/Info/Warn/Error, порог уровня (FR-E08);
- `result` — ошибки без исключений (NFR-E09);
- `event_bus` — subscribe/publish/unsubscribe, типизация по C++-типу события (FR-E10);
- `config` — ключ=значение с деградацией при отсутствии файла (FR-E09);
- `file_system` — виртуальные пути `assets:/…` → физические (FR-E07, DATA-E05);
- `json_value` — собственный парсер JSON (DR-02).

## 3. Платформа (engine/platform)

- `window` — SDL2-окно или headless-заглушка (`DND_WITH_SDL2`, DR-01);
  letterbox 1280×720 → окно любого размера (UI-E02, формула п. 4.1.3.4);
- `input` — кадровый цикл событий (опрос → маппинг → адаптер ввода→UI),
  действия из таблицы 8.6 DOC-01;
- `timer`, `main_loop` — аккумулятор времени, фиксированный шаг dt_logic=1/60 с,
  ≤5 шагов за кадр, рендер с интерполяцией alpha (формула (2) п. 4.1.2.4);
- `font_atlas` — процедурный растровый атлас 16×32, cap height 19.6923 px (DR-03).

## 4. Отрисовка (engine/render)

- `gl_loader` — загрузка функций OpenGL 3.3 Core, диагностика отсутствующих;
- `sprite_batch` — батчинг по текстурам, 1 flush на пакет (E-D05),
  ≤8 draw call на кадр; в headless — программный растеризатор в CPU-буфер;
- `camera` — 2D-камера с клампом по границам карты;
- `primitives` — прямоугольник/линия/эллипс; `draw_text` — цвет, масштаб,
  выравнивание (контракт п. Г.10);
- `debug_overlay` — FPS, draw calls, счётчик сущностей (FR-E78).

## 5. Ресурсы и данные (engine/resource)

`resource_manager` + `ResourceCache` (одна загрузка на документ, FR-E68),
`json_loader` (конверт `{$schema, schema_version, id}`, п. 4.5.3),
`texture_loader` (TGA + процедурные текстуры). Схемы данных —
`assets/data/schemas/`, справочник — `docs/gost/data_formats.md` (DR-07).

## 6. ECS-lite (engine/ecs, подраздел 4.1.6)

- `EntityId{index, generation}` — поколение защищает от доступа к мёртвой сущности;
- `ComponentStorage<T>` — sparse set: плотный массив + разреженный индекс,
  удаление O(1) swap-and-pop (DR-04);
- `Registry`, `make_view<Cs...>`, `ISystem::update(registry, dt)` — поведение
  только в системах (п. 4.1.6.3);
- `CommandBuffer` — структурные изменения откладываются и применяются между
  системами;
- компоненты — только данные; обёртки над типами `rules::*` живут в `game/`.

## 7. Сцены и FSM (engine/scene, подраздел 4.1.7)

`Scene` (update/render с alpha), `SceneStack` (push/pop/replace),
`StateMachine` (transition по trigger + guard). Автомат хода
WaitingForAction→Targeting→Confirming→SpendingResources→Resolving→EndTurn
собирается игрой поверх универсального FSM; атомарность трат — в
`rules/turn_state_machine.h` (spend — все или ничего, отмена до траты).

## 8. UI (engine/ui, раздел 4.1.10)

Виджеты: Label, Button, Panel, ProgressBar, Tooltip, Grid; контейнеры
HBox/VBox/Stack; события (hover/press/click); координаты в фиксированной
системе 1280×720 с letterbox-преобразованием. Виджеты получают только DTO —
заголовки `rules/` в `engine/ui/` запрещены (E-D12, E-D02).

## 9. Звук (engine/audio)

`audio_system` — SDL2_mixer при `DND_WITH_SDL2=ON`, заглушка в headless (M9);
интерфейс загружает/воспроизводит по виртуальным путям `assets:/audio/…`.

## 10. Правила (rules/, подраздел 4.1.11, Приложение Г)

- `DiceRoller`: adv+disp → один бросок и `cancelled=true`; breakdown
  заполняется всегда; криты/осечки — по натуральному кубику; +5 не добавляется;
- `IRandomProvider` инжектируется (E-D07), `SeededRandomProvider` —
  xoshiro256** с сериализуемым состоянием (DR-05);
- инициатива: score ↓, dex_modifier ↓, EntityId ↑ (п. 4.1.11.3), фиксируется один раз;
- урон: temp_hp → hp_current; лечение temp_hp не восстанавливает;
  сопротивление/уязвимость — конфликт разрешается в пользу сопротивления;
- стек модификаторов: слои, одноимённые не-stacking — берётся сильнейший;
- состояния, attunement (≤3), экономика действий, тактическая сетка
  (стоимость движения по типам местности), 19 формул п. Г.13.

## 11. Игровой слой (game/)

`combat_scene` — вертикальный срез (Приложение Д): комната 20×15,
PlayerFighter + PlayerRogue + 2 Goblins; `adapters/rules_adapter` — вызовы
правил по запросам UI, публикация событий в `EventBus`, конвертация результатов
в DTO (DR-06). Сохранения: состояние партии + RNG, миграция версий.

## 12. Инструменты и качество

`tools/`: `json_validator`, `schema_gen`, `map_validator`, `asset_packer` (M11).
Сборка: CMake ≥3.20, пресеты (MSVC /MTd|/MT, NFR-E14), `-Wall -Wextra -Werror`;
статические проверки: `scripts/check_includes.py`, `scripts/hdrcheck.sh`.
Тесты: 5 целей ctest + золотой эталон (seed 42) + бенчмарки `tests/benchmarks`
с порогами (`--strict`) — DR-09, DR-10.
