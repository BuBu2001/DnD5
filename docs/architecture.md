# Архитектура движка (каркас V0.2)

Документ фиксирует состояние каркаса по ТЗ DOC-04 (АБВГ.00004-01 90 01-1).

## 1. Три слоя программы (таблица 3.1)

| Слой | Каталог | Знание о D&D | Зависимости |
|------|---------|--------------|-------------|
| Engine Layer | `engine/` | нет (E-D01) | stdlib; далее SDL2/gl |
| Rules Core   | `rules/`    | да | stdlib (без engine!) |
| Game Layer   | `game/`     | связывает | engine, rules |

Проверка запрета включений (п. 4.8.2) планируется скриптом CI.

## 2. Главный цикл (п. 4.1.2.4)

Реализован в `engine/platform/main_loop.{h,cpp}`: аккумулятор времени, фиксированный
шаг dt_logic = 1/60 с, не более 5 шагов за кадр, рендер с alpha = accumulator/dt_logic.

## 3. ECS-lite (подраздел 4.1.6)

- `EntityId{index, generation}` — проверка живости при разыменровании;
- `ComponentStorage<T>` — map index→{generation, T}; компоненты — только данные;
- `Registry` — создание/удаление сущностей, свободные индексы, системы;
- `make_view<Cs...>` — сущности со всеми компонентами;
- `ISystem::update(registry, dt)` — поведение только в системах (п. 4.1.6.3).

Начальные компоненты TransformComponent/SpriteComponent/GridPositionComponent будут
добавлены на M2-M4 (п. 4.1.6.2); HealthComponent и др. хранят типы rules::* — это
исключение оформляется адаптером game/adapters, чтобы движок не включал rules напрямую.

## 4. Сцены и FSM (подраздел 4.1.7)

`Scene` (update/render с alpha), `SceneStack` (push/pop/replace), `StateMachine`
(transition по trigger + guard). Автомат хода WaitingForAction→Targeting→Confirming→
SpendingResources→Resolving→EndTurn собирается игрой поверх универсального FSM;
правило атомарности трат реализовано в `rules/turn_state_machine.h` (spend — все или ничего).

## 5. Правила (подраздел 4.1.11, Приложение Г)

- DiceRoller: adv+disp → один бросок и cancelled=true; breakdown заполняется; криты — вне DiceRoller;
- IRandomProvider инжектируется (E-D07); SeededRandomProvider — xoshiro256**;
- Инициатива: score ↓, dex_modifier ↓, EntityId ↑ (п. 4.1.11.3);
- Урон: temp_hp → hp_current; лечение temp_hp не восстанавливает (п. 4.1.11.4);
- Modifier stack: слои, одноимённые не-stacking — сильнейший (п. 4.1.11.5).

## 6. Заглушки (реализация — этапы M2+)

render/renderer.h (IRenderer: draw_sprite/flush, контракт п. Г.10),
resource/resource_manager.h, audio/audio_system.h, ui/control.h, platform/window.h (IWindow).
