# Матрица трассировки (Приложение В ТЗ DOC-04)

Формат: требование → модуль → символ кода → проверка. Строка без проверки
(тест, инструмент или замер) блокирует закрытие этапа.

Обозначения целей испытаний:
* `rules` — tests_rules (таблица 8.2 ТЗ DOC-04);
* `engine` — tests_engine (таблица 8.3);
* `platform` — tests_platform (слой платформы и отрисовки без окна и драйвера);
* `integration` — tests_integration (сквозные испытания, вертикальный срез Приложения Д);
* `golden` — tests_golden (подраздел 8.4, фиксированный seed 42);
* `bench` — tests/benchmarks (таблица 8.4);
* `tool` — вспомогательный инструмент tools/ (этап M11).

## 1. Ядро движка (engine/core)

| Требование | Модуль | Символ | Проверка |
|------------|--------|--------|----------|
| FR-E08 (журнал, уровни) | engine/core/log | `core::Log`, `LogLevel` | engine: `log_levels_and_format` |
| FR-E09 (конфигурация, деградация) | engine/core/config | `core::Config::load_from_file` | engine: `config_load_and_degradation` |
| FR-E10 (шина событий) | engine/core/event_bus | `core::EventBus::publish` | engine: `event_bus_delivers` |
| FR-E07, DATA-E05 (виртуальные пути) | engine/core/file_system | `core::FileSystem::resolve` | engine: `virtual_path_resolve` |
| NFR-E14 (отказ без аварии) | engine/core/result | `core::Result<T>` | engine: `config_load_and_degradation` |
| FR-E100…E103 (отладочная панель) | engine/core/debug_overlay | `core::DebugOverlay`, `FrameStats` | engine: `debug_overlay_counters` |
| NFR-E01 (бюджет 300 draw calls) | engine/core/debug_overlay | `kMaxDrawCallsPerFrame` | bench: оконный прогон `game --window` |

## 2. Платформа (engine/platform)

| Требование | Модуль | Символ | Проверка |
|------------|--------|--------|----------|
| FR-E01 (окно, логическое 1280×720) | engine/platform/window | `WindowSettings`, `SdlWindow` | platform: `viewport_formula_1_scale_and_bars` |
| FR-E02, формула (1) (масштаб, поля) | engine/platform/window | `Window::compute_viewport` | platform: `viewport_formula_1_scale_and_bars` |
| FR-E03 (клавиатура, мышь, колесо) | engine/platform/input | `InputState`, `poll_sdl_events` | platform: `input_frame_cycle` |
| FR-E03 → FR-E80 (адаптация ввода в UI) | engine/platform/input | `InputToUiAdapter::translate` | platform: `input_to_ui_adapter_translation`, `input_event_reaches_widget` |
| FR-E04, FR-E05 (геймпад, сенсор) | — | — | статус V1 (п. 4.1.2.5), этап M11 |
| FR-E06, формула (2) (фиксированный шаг) | engine/platform/main_loop | `MainLoop::tick` | engine: `fixed_loop_stable` |
| п. 4.1.2.4 (dt_frame) | engine/platform/timer | `platform::Timer` | platform: `frame_timer_accumulates_dt` |

## 3. Отрисовка (engine/render)

| Требование | Модуль | Символ | Проверка |
|------------|--------|--------|----------|
| FR-E20 (OpenGL 3.3 Core) | engine/render/gl_backend | `GlRenderer::init` | platform: `gl_function_loader_reports_missing`; оконный прогон |
| FR-E20 (загрузка функций GL) | engine/render/gl_functions | `load_gl_functions` | platform: `gl_function_loader_reports_missing` |
| FR-E21 (текстуры, атласы) | engine/render/gl_backend | `GlRenderer::create_texture_rgba` | tool: `asset_packer` (раскладка атласа) |
| FR-E22, FR-E23 (спрайты, src/dst) | engine/render/renderer.h | `IRenderer::draw_sprite` | engine: `primitives_and_text_contract` |
| FR-E25 (камера, отсечение) | engine/render/camera | `Camera2D` | engine: `camera_clamps` |
| FR-E28 (примитивы) | engine/render/renderer.h | `draw_rect`, `draw_line`, `draw_circle` | engine: `primitives_and_text_contract` |
| FR-E29 (текст) | engine/render/font_atlas | `FontAtlas::bake`, `draw_text` | platform: `font_atlas_bakes_glyphs`, `font_atlas_baseline_metrics` |
| FR-E30, FR-E31 (тонирование, разворот) | engine/render/gl_backend | `draw_sprite_flipped` | оконный прогон (визуально) |
| E-D05, NFR-E01 (пакетирование) | engine/render/sprite_batch | `SpriteBatch::flush` | platform: `sprite_batch_limits_draw_calls`; bench: пакет 1000 спрайтов |
| E-D11 (headless-тесты) | engine/render/null_renderer | `NullRenderer` | engine, platform, integration |
| FR-E02 (поля letterbox/pillarbox) | engine/ui/ui_root | `UiRoot::draw_letterbox` | platform: `ui_letterbox_does_not_cover_scene` |

## 4. Ресурсы и данные (engine/resource)

| Требование | Модуль | Символ | Проверка |
|------------|--------|--------|----------|
| FR-E41 (разбор JSON) | engine/resource/json | `JsonParser::parse`, `JsonValue` | engine: `json_value_roundtrip` |
| FR-E45 (кэш ресурсов) | engine/resource/json_loader | `JsonLoader::load`, `cache_size` | engine: `resource_cache_and_events` |
| FR-E46 (события загрузки) | engine/resource/json_loader | `ResourceLoadFailedEvent` | engine: `resource_cache_and_events` |
| FR-E47, DATA-E02 (схемы) | engine/resource/schema_validator | `SchemaValidator::validate` | engine: `schema_validator_*`; tool: `json_validator` |
| FR-E47 (реестр схем) | engine/resource/schema_registry | `SchemaRegistry::load_directory` | tool: `json_validator` (10 схем) |
| FR-E48 (сообщение об ошибке) | engine/resource/schema_validator | `ValidationResult::format` | engine: `schema_validator_*` |
| п. 4.5.3 (обёртка данных) | engine/resource/schema_validator | `validate_envelope` | tool: `json_validator --envelope-only` |
| DATA-E01…E05 (форматы данных) | docs/gost/data_formats.md | — | tool: `schema_gen` (генерация справочника) |
| DATA-E04 (карты, 1 клетка = 5 футов) | assets/maps | `Map_GoblinAmbush` | tool: `map_validator` |

## 5. Звук (engine/audio)

| Требование | Модуль | Символ | Проверка |
|------------|--------|--------|----------|
| FR-E50 (OGG/WAV) | engine/audio/sdl_audio | `SdlAudioSystem::play_music` | оконный прогон (SDL2_mixer) |
| FR-E54 (плавное затухание музыки) | engine/audio/audio_system, sdl_audio | `IAudioSystem::fade_out_music/fade_in_music` (SDL: Mix_FadeOutMusic/Mix_FadeInMusic) | engine: `audio_music_fade`; F2 в оконном режиме (600 мс) |
| FR-E51 (1 канал музыки, ≥8 эффектов) | engine/audio/sdl_audio | `Mix_AllocateChannels` | оконный прогон: «каналов эффектов: 8» |
| FR-E52 (громкость master/music/sfx) | engine/audio/audio_system.h | `AudioSettings` | оконный прогон (F2, +/−) |
| FR-E53 (отказ звука без аварии) | engine/audio/audio_system.h | `NullAudioSystem` | оконный прогон с SDL_AUDIODRIVER=dummy |
| FR-E54 (плавное затухание) | — | — | статус V1 (п. 4.1.5.2) |

## 6. ECS, сцены, состояния (engine/ecs, engine/scene)

| Требование | Модуль | Символ | Проверка |
|------------|--------|--------|----------|
| FR-E60…E63 (сущности, компоненты) | engine/ecs/registry | `Registry`, `ComponentStorage` | engine: `ecs_query_works` |
| FR-E64 (отложенные команды) | engine/ecs/command_buffer | `CommandBuffer` | engine: `ecs_command_buffer_deferred` |
| FR-E65…E69 (представления) | engine/ecs/view | `make_view` | engine: `ecs_query_works`; bench: обход 10 000 сущностей |
| FR-E70…E74 (стек сцен, пауза) | engine/scene/scene_stack | `SceneStack::push/pop` | engine: `scene_stack_push_pop` |
| FR-E75…E79 (конечный автомат) | engine/scene/state_machine | `StateMachine` | integration: `turn_fsm_cancel_before_spend` |
| FR-E90…E92 (Save/Load, миграции) | engine/scene/save_system | `SaveSystem`, `MigrationChain` | integration: `save_roundtrip`, `save_version_migration` |
| E-D07 (состояние ГСЧ в сохранении) | engine/scene/save_system | `rng_state` | integration: `save_roundtrip` |

## 7. Пользовательский интерфейс (engine/ui)

| Требование | Модуль | Символ | Проверка |
|------------|--------|--------|----------|
| FR-E80 (дерево виджетов) | engine/ui/control | `Control::draw_all`, `dispatch_event` | engine: `ui_widgets_and_events` |
| FR-E81 (контейнеры разметки) | engine/ui/layout | `VBoxLayout`, `HBoxLayout` | engine: `ui_layout_containers` |
| FR-E82, FR-E83 (виджеты, события) | engine/ui/widgets | `Label`, `Button`, `Panel`, `Modal` | engine: `ui_widgets_and_events` |
| FR-E84 (масштабирование UI) | engine/ui/ui_root | `UiRoot::resize`, `window_to_logical` | engine: `ui_scale_1280x720`; platform: `input_to_ui_adapter_translation` |
| FR-E85 (стили, тема) | engine/ui/control | `Style` | engine: `ui_widgets_and_events` |
| FR-E86, FR-E87 (прокрутка журнала) | engine/ui/widgets | `ScrollView` | engine: `ui_widgets_and_events` |
| E-D12 (UI получает только DTO) | game/ui_widgets | `DiceBreakdownPanel` | integration: `dto_boundary_ui` |

## 8. Модуль правил (rules/) — по ТЗ DOC-01

| Требование | Модуль | Символ | Проверка |
|------------|--------|--------|----------|
| FR-K10…K19 (характеристики) | rules/abilities | `ability_modifier` | rules: `ability_modifier_table`; golden |
| FR-K20…K29 (владение) | rules/proficiency | `proficiency_bonus` | rules: `proficiency_and_formulas` |
| FR-K30…K39 (проверки, атака) | rules/dice_roller, rules/attack | `DiceRoller::roll`, `resolve_attack` | rules: `dice_roller_rules`, `attack_resolution`; golden |
| FR-K40…K49 (урон, типы) | rules/damage | `apply_resistances` | rules: `damage_and_resistance` |
| FR-K50…K59 (здоровье) | rules/health | `apply_damage`, `heal` | rules: `health_damage_order` |
| FR-K60…K69 (инициатива) | rules/initiative | `InitiativeQueue::build` | rules: `initiative_tie_breaker` (стабильность очереди) |
| FR-K70…K79 (состояния) | rules/conditions | `ConditionTable`, `ConditionFlags` | rules: `conditions_prone_and_flags` |
| FR-K80…K89 (предметы) | rules/items | `ItemComponent` | rules: `items_components` |
| FR-K90…K99 (особенности) | rules/features | `FeatureTable` | rules: `features_lookup` |
| FR-K100…K109 (экономика хода) | rules/action_economy | `TurnResources` | rules: `turn_resources_atomic` |
| FR-K102, AC-K22 (единая точка ГСЧ) | rules/dice_roller | `DiceRoller`, `IRandomProvider` | rules: `dice_roller_rules`; golden (seed 42) |
| AC-K03 (средняя броня, DEX ≤ 2) | rules/ac | `calculate_ac`, `kMediumArmorDexCap` | rules: `armor_class_formula` |
| E-D01 (движок не знает правил) | scripts/check_includes.py | — | tool: `check_includes.py` (CI) |
| NFR-E10 (изоляция слоёв) | scripts/check_includes.py | — | tool: `check_includes.py` (CI) |
| NFR-E09 (детерминизм) | rules/random_provider | `SeededRandomProvider` | golden: `golden_values.txt` |

## 9. Нефункциональные требования

| Требование | Модуль | Символ | Проверка |
|------------|--------|--------|----------|
| NFR-E01 (≤300 draw calls) | engine/render, engine/core | `SpriteBatch`, `DebugOverlay` | bench; оконный прогон: 7 draw calls |
| NFR-E02 (карта 20×15 в бюджете кадра) | engine/render/sprite_batch | `SpriteBatch::flush` | bench: 1000 спрайтов за 0,05 мс |
| NFR-E03 (1 млн бросков/с) | rules/dice_roller | `DiceRoller::roll` | bench: ~656 000/с в Debug |
| NFR-E04 (загрузка ресурсов) | engine/resource/json_loader | `JsonLoader::load_uncached` | bench: ~2 100 документов/с |
| NFR-E05 (10 000 сущностей ECS) | engine/ecs | `Registry` | bench: ~710 000 созданий/с |
| NFR-E06 (отклик UI) | engine/ui | `UiRoot::draw` | bench: ~75 000 отрисовок дерева/с |
| NFR-E08 (покрытие правил ≥90 %) | tests/rules_tests | 22 кейса / 339 проверок | rules: таблица 8.2 |
| NFR-E09 (детерминированный вывод) | все модули | сортировка перечислений | golden; tool: `schema_gen`, `asset_packer` |
| NFR-E12 (предупреждения как ошибки) | CMakeLists.txt | `-Wall -Wextra -Werror` | сборка всех целей |
| NFR-E12 (самодостаточность заголовков) | scripts/hdrcheck.sh | — | tool: `hdrcheck.sh` |

## 10. Не закрыто в V0.4 (перенос на V1; статусы уточнены в V0.5)

| Требование | Причина | Этап |
|------------|---------|------|
| FR-E04, FR-E05 (геймпад, сенсорный ввод) | статус V1 по п. 4.1.2.5 ТЗ DOC-04 | M11 |
| FR-E36…E39 (траектории, зоны угрозы) | статус V1 по п. 4.1.3.2; подсветка достижимых клеток (основа FR-E35) реализована в V0.5 | M5 |
| FR-E96…E99 (редактор сцен) | не входит в MVP | M11 |
| Стилевой шрифт .ttf (stb_truetype) | используется встроенный растровый шрифт (DR-03); stb_image вендорен в V0.4.9 | M2 |
| nlohmann/json, tinyxml2, SDL2_image | DR-01/DR-02 (собственный парсер JSON); stb_image закрывает загрузку PNG | M0/M4 |

## 11. Дополнения V0.4.8…V0.5.1 (играбельный боевой режим)

| Требование | Модуль | Символы | Проверка |
|------------|--------|---------|----------|
| FR-E03 (клавиатура/мышь, действия) | engine/platform/input_actions | `InputActions::update/pressed/held/wheel/axis` | controls: `control_scheme_matches_controls_doc`, `input_actions_mouse_and_wheel` |
| FR-E03 (раскладка WASD + горячие клавиши) | game/controls/control_scheme | `build_control_scheme`, `actions::*` | controls: `control_scheme_matches_controls_doc`; docs/controls.md |
| FR-E25, FR-E35 (камера, зум 0.5×…4×) | game/controls/camera_controller | `CameraController::update/clamp_to_map/set_world` | controls: `camera_controller_pans_with_wasd_and_clamps`, `camera_controller_zoom_limits` |
| FR-E33 (курсор → клетка) | game/controls/cell_picking | `pick_cell` | controls: `cell_picking_maps_mouse_to_grid_cell` |
| FR-E03 (кадровый цикл «нажат в кадре») | engine/platform/input | `InputState::push_event` | platform: `input_frame_cycle` |
| п. 4.1.8 (перемещение по клеткам, стоимость) | game/combat/combat_scene | `CombatScene::move_unit`, `find_path`, `TacticalGrid::entry_cost_feet` | integration: `combat_play_select_and_move_by_rules`, `combat_play_reachable_respects_difficult_terrain` |
| FR-E35-основа (подсветка достижимых клеток) | game/combat/combat_scene | `CombatScene::reachable_cells` | integration: `combat_play_reachable_respects_difficult_terrain` |
| п. Г.6 (экономика действий в бою) | game/combat | `CombatScene::begin_turn`, `Combatant::action_used` | integration: `combat_play_attack_and_turn_flow` |
| Приложение Д (вертикальный срез — играбельно) | game/combat/combat_play_scene | `CombatPlayScene` | integration: `combat_play_loads_map_and_spawns`; оконный прогон `game --window` |
| DATA-E02…E04 (карта, спавны, террейн) | game/combat/combat_play_scene | `CombatPlayScene::load_map` | integration: `combat_play_loads_map_and_spawns`; tool: `map_validator` |
| FR-E45 (загрузка растров, PNG) | engine/resource/texture_loader | `TextureLoader::load_png/decode_png` | integration: `texture_loader_png` |
| FR-E50…E52 (звук в бою) | game/combat/combat_play_scene, engine/audio | `set_audio`, `play_sfx` | оконный прогон; отказ аудио не прерывает игру (п. 4.2.3) |
| E-D12 (боевые панели — только DTO) | game/main.cpp, game/ui_widgets | `initiative_dto/turn_resources_dto`, `AttackResolvedEvent` | integration: `ui_widgets_show_full_breakdown`; оконный прогон |
