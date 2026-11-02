# Матрица трассировки (каркас, Приложение В ТЗ DOC-04)

Формат: требование → модуль → символ кода → тест. Строки без соответствия блокируют закрытие этапа.

| Требование | Модуль | Символ | Тест |
|------------|--------|--------|------|
| FR-E08 (логирование) | engine/core/log | engine::core::Log | — (демо game/main.cpp) |
| FR-E09 (конфигурация) | engine/core/config | engine::core::Config | test_config_degradation |
| FR-E10 (шина событий) | engine/core/event_bus | engine::core::EventBus | test_event_bus |
| FR-E07, DATA-E05 (assets:/) | engine/core/file_system | engine::core::FileSystem | test_file_system_virtual_paths |
| п. 4.1.2.3–4 (главный цикл) | engine/platform/main_loop | engine::platform::MainLoop::tick | test_main_loop_fixed_step |
| п. 4.1.2.2 ф-ла (1) (масштаб) | engine/platform/window | IWindow::compute_scale | — (M1) |
| FR-E55…E60 (ECS-lite) | engine/ecs | Registry, ComponentStorage, make_view | test_ecs_lifecycle |
| FR-E70…E79 (сцены/FSM) | engine/scene | SceneStack, StateMachine | test_fsm_cancel_states |
| п. Г.2 (характеристики) | rules/abilities | ability_modifier, AbilityScores | test_ability_modifier_table |
| п. Г.13 (формулы) | rules/formulas.h | proficiency_bonus и др. | test_proficiency_and_formulas |
| п. 4.1.11.2 (DiceRoller) | rules/dice_roller | DiceRoller::roll | test_dice_roller_rules |
| E-D07 (IRandomProvider) | rules/random_provider | SeededRandomProvider (xoshiro256**) | test_dice_roller_rules (детерминизм) |
| п. 4.1.11.3 (tie-breaker) | rules/initiative | InitiativeQueue::build | test_initiative_tie_breaker |
| п. 4.1.11.4 (порядок урона) | rules/health | apply_damage, heal | test_health_damage_order |
| п. 4.1.7.3 (атомарность) | rules/turn_state_machine | spend, can_afford | test_turn_resources_atomic |
| п. 4.1.11.5 (слои) | rules/modifier_stack | aggregate | — (M1) |
| E-D02/E-D12 (DTO) | rules/dto.h | RollBreakdownDTO и др. | — (M8) |
| NFR-E09 (детерминизм) | rules/* | seed-провайдер | test_dice_roller_rules |

Не закрыты (следующие этапы): FR-E20…E54 (render/resource/audio, M2–M3, M9),
FR-E80…E95 (UI, M8), Save/Load (M10), отладочные инструменты (M6+).
