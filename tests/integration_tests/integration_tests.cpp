// tests/integration_tests/integration_tests.cpp — интеграционные испытания:
// загрузка данных по схемам (json_loader_valid, json_loader_invalid_does_not_crash,
// resource_cache_single_load), сохранение/загрузка (save_roundtrip), боевой срез.
// Данные берутся из каталога assets/ репозитория (DATA-E01: вне исходного кода).
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "core/event_bus.h"
#include "core/file_system.h"
#include "combat_scene.h"
#include "components/game_components.h"
#include "adapters/rules_adapter.h"
#include "adapters/rules_events.h"
#include "ecs/registry.h"
#include "platform/main_loop.h"
#include "render/null_renderer.h"
#include "resource/json_loader.h"
#include "resource/schema_registry.h"
#include "resource/texture_loader.h"
#include "rules/conditions.h"
#include "rules/random_provider.h"
#include "save/combat_save_state.h"
#include "scene/save_system.h"
#include "ui/ui_root.h"
#include "ui_widgets/combat_widgets.h"
#include "ui_widgets/dice_breakdown_panel.h"
#include "test_harness.h"

namespace {

// Каталог assets/ относительно корня репозитория: тесты запускаются из build/,
// поэтому корень определяется по переменной, которую подставляет CMake.
#ifndef DND_ASSETS_ROOT
#define DND_ASSETS_ROOT "assets"
#endif

engine::core::FileSystem make_fs() { return engine::core::FileSystem(DND_ASSETS_ROOT); }

// Запись временного файла в каталог assets (для испытаний отказоустойчивости)
bool write_temp_asset(const engine::core::FileSystem& fs, const std::string& virtual_path,
                      const std::string& content) {
    const auto physical = fs.resolve(virtual_path);
    if (!physical) return false;
    std::FILE* f = std::fopen(physical.value().c_str(), "wb");
    if (f == nullptr) return false;
    std::fwrite(content.data(), 1, content.size(), f);
    std::fclose(f);
    return true;
}

void remove_temp_asset(const engine::core::FileSystem& fs, const std::string& virtual_path) {
    const auto physical = fs.resolve(virtual_path);
    if (physical) std::remove(physical.value().c_str());
}

} // namespace

// --- json_loader_valid (FR-E41, FR-E47, DATA-E02) --------------------------------
TEST_CASE("json_loader_valid", "[integration][FR-E41][FR-E47]") {
    const auto fs = make_fs();
    engine::resource::SchemaValidator validator;
    engine::resource::SchemaRegistry registry(fs, validator);
    REQUIRE(registry.load_directory("assets:/schemas") >= 6);
    REQUIRE(registry.contains("schemas/item.schema.json"));

    engine::core::EventBus bus;
    engine::resource::JsonLoader loader(fs, validator, &bus);

    // Состав данных MVP (п. 4.1.4.2) загружается и проходит валидацию по схемам
    const char* required_files[] = {
        "assets:/data/abilities.json",
        "assets:/data/conditions.json",
        "assets:/data/classes/fighter.json",
        "assets:/data/classes/rogue.json",
        "assets:/data/classes/barbarian.json",
        "assets:/data/items/longsword.json",
        "assets:/data/enemies/goblin.json",
        "assets:/data/characters/fighter.json",
        "assets:/data/characters/rogue.json",
    };
    for (const char* path : required_files) {
        const auto doc = loader.load(path);
        INFO(path);
        REQUIRE(doc.is_ok());
        REQUIRE_FALSE(doc.value().id.empty());            // идентификатор обязателен (DATA-E03)
        REQUIRE(doc.value().schema_version >= 1);         // версия схемы (п. 4.5.3)
        REQUIRE(doc.value().value.contains("$schema"));   // ссылка на схему (п. 4.5.3)
    }

    // Идентификаторы данных именуются по схеме <Тип>_<Имя> (п. 4.5.4)
    const auto sword = loader.load("assets:/data/items/longsword.json");
    REQUIRE(sword.is_ok());
    REQUIRE(sword.value().id == "Item_Longsword");
    const auto& components = sword.value().value.at("components").as_array();
    REQUIRE(components.size() >= 1);
    REQUIRE(components[0].at("damage_type").as_string() == "Slashing");

    // Гоблин: состав полей противника (Приложение Д, вертикальный срез)
    const auto goblin = loader.load("assets:/data/enemies/goblin.json");
    REQUIRE(goblin.is_ok());
    REQUIRE(goblin.value().value.at("ac").as_int() == 15);
    REQUIRE(goblin.value().value.at("hp").as_int() == 7);
    REQUIRE(goblin.value().value.at("abilities").at("Dex").as_int() == 14);
}

// --- resource_cache_single_load (FR-E45) -----------------------------------------
TEST_CASE("resource_cache_single_load", "[integration][FR-E45]") {
    const auto fs = make_fs();
    engine::resource::SchemaValidator validator;
    engine::resource::SchemaRegistry registry(fs, validator);
    registry.load_directory("assets:/schemas");
    engine::resource::JsonLoader loader(fs, validator);

    const std::string path = "assets:/data/items/longsword.json";
    REQUIRE_FALSE(loader.is_cached(path));
    const auto first = loader.load(path);
    const auto second = loader.load(path);
    REQUIRE(first.is_ok());
    REQUIRE(second.is_ok());
    // Повторная загрузка того же ресурса возвращает кэшированный объект (FR-E45)
    REQUIRE(loader.load_count() == 1);
    REQUIRE(loader.cache_hit_count() == 1);
    REQUIRE(loader.cache_size() == 1);
    REQUIRE(loader.is_cached(path));
    REQUIRE(second.value().id == first.value().id);

    // Инвалидация кэша — точка «горячей» перезагрузки (FR-E46, V1)
    loader.invalidate(path);
    REQUIRE_FALSE(loader.is_cached(path));
    REQUIRE(loader.load(path).is_ok());
    REQUIRE(loader.load_count() == 2);

    loader.invalidate_all();
    REQUIRE(loader.cache_size() == 0);
}

// --- json_loader_invalid_does_not_crash (FR-E47, FR-E48, NFR-E14) ----------------
TEST_CASE("json_loader_invalid_does_not_crash", "[integration][FR-E48][NFR-E14][AC-E05]") {
    const auto fs = make_fs();
    engine::resource::SchemaValidator validator;
    engine::resource::SchemaRegistry registry(fs, validator);
    registry.load_directory("assets:/schemas");

    engine::core::EventBus bus;
    int failures = 0;
    std::string last_reason;
    bus.subscribe<engine::resource::ResourceLoadFailedEvent>(
        [&](const engine::resource::ResourceLoadFailedEvent& e) {
            ++failures;
            last_reason = e.reason;
        });
    engine::resource::JsonLoader loader(fs, validator, &bus);

    // 1. Отсутствующий файл: признак ошибки, событие в шине, запись в журнале
    const auto missing = loader.load("assets:/data/no_such_file.json");
    REQUIRE_FALSE(missing.is_ok());
    REQUIRE(failures == 1);

    // 2. Битый JSON: программа продолжает работу (AC-E05)
    const std::string broken_path = "assets:/data/_test_broken.json";
    REQUIRE(write_temp_asset(fs, broken_path, "{\"id\": \"Item_Broken\", "));
    const auto broken = loader.load(broken_path);
    REQUIRE_FALSE(broken.is_ok());
    REQUIRE(broken.error_message().find(broken_path) != std::string::npos);
    REQUIRE(failures == 2);
    remove_temp_asset(fs, broken_path);

    // 3. Отсутствие обязательных полей обёртки (п. 4.5.3)
    const std::string no_envelope = "assets:/data/_test_no_envelope.json";
    REQUIRE(write_temp_asset(fs, no_envelope, "{\"name\": \"без схемы\"}"));
    const auto envelope_failure = loader.load(no_envelope);
    REQUIRE_FALSE(envelope_failure.is_ok());
    REQUIRE(envelope_failure.error_message().find("$schema") != std::string::npos);
    remove_temp_asset(fs, no_envelope);

    // 4. Несовпадение значения со схемой: сообщение по формату п. 4.1.4.3 —
    //    виртуальный путь, имя поля, перечень допустимых значений
    const std::string bad_enum = "assets:/data/items/_test_bad_enum.json";
    REQUIRE(write_temp_asset(fs, bad_enum, R"({
        "$schema": "schemas/item.schema.json",
        "schema_version": 1,
        "id": "Item_BadEnum",
        "name": "Меч с опечаткой",
        "components": [ { "type": "Weapon", "base_damage": "1d8", "damage_type": "Slash" } ]
    })"));
    const auto enum_failure = loader.load(bad_enum);
    REQUIRE_FALSE(enum_failure.is_ok());
    const std::string& message = enum_failure.error_message();
    REQUIRE(message.find(bad_enum) != std::string::npos);
    REQUIRE(message.find("components[0].damage_type") != std::string::npos);
    REQUIRE(message.find("Slash") != std::string::npos);
    REQUIRE(message.find("expected one of") != std::string::npos);
    REQUIRE(message.find("Slashing") != std::string::npos);
    remove_temp_asset(fs, bad_enum);

    // 5. Несовместимая версия схемы: отказ с понятным сообщением
    const std::string bad_version = "assets:/data/items/_test_bad_version.json";
    REQUIRE(write_temp_asset(fs, bad_version, R"({
        "$schema": "schemas/item.schema.json",
        "schema_version": 99,
        "id": "Item_Future",
        "name": "Предмет из будущего",
        "components": [ { "type": "Weapon", "base_damage": "1d8", "damage_type": "Slashing" } ]
    })"));
    loader.set_expected_schema_version(1);
    const auto version_failure = loader.load(bad_version);
    REQUIRE_FALSE(version_failure.is_ok());
    REQUIRE(version_failure.error_message().find("schema_version") != std::string::npos);
    remove_temp_asset(fs, bad_version);
    loader.set_expected_schema_version(-1);

    // 6. Неизвестная схема: загрузка отклоняется, а не пропускается молча
    const std::string unknown_schema = "assets:/data/_test_unknown_schema.json";
    REQUIRE(write_temp_asset(fs, unknown_schema, R"({
        "$schema": "schemas/nonexistent.schema.json",
        "schema_version": 1,
        "id": "Item_Unknown"
    })"));
    const auto unknown = loader.load(unknown_schema);
    REQUIRE_FALSE(unknown.is_ok());
    REQUIRE(unknown.error_message().find("схема не зарегистрирована") != std::string::npos);
    remove_temp_asset(fs, unknown_schema);

    // Программа осталась управляемой: корректный файл по-прежнему загружается (п. 4.2.3)
    REQUIRE(loader.load("assets:/data/items/longsword.json").is_ok());
    REQUIRE(failures >= 5);
    REQUIRE_FALSE(last_reason.empty());
}

// --- save_roundtrip (FR-E90, FR-E91) ----------------------------------------------
TEST_CASE("save_roundtrip", "[integration][FR-E90][FR-E91][AC-E14]") {
    const auto fs = make_fs();
    engine::resource::SchemaValidator validator;
    engine::resource::SchemaRegistry registry(fs, validator);
    registry.load_directory("assets:/schemas");
    engine::scene::SaveSystem saves(fs, &validator);

    const std::string path = "assets:/saves/_test_slot.json";

    game::CombatSaveState state;
    state.round = 3;
    state.combatants.push_back(game::SavedCombatant{"Воин", true, 12, 9, 5, 18, 5, 2, 3, 4});
    state.combatants.push_back(game::SavedCombatant{"Гоблин", false, 7, 3, 0, 15, 4, 8, 9});
    state.initiative.push_back(rules::InitiativeEntry{1, 19, 2, 0});
    state.initiative.push_back(rules::InitiativeEntry{2, 15, 4, 0});

    engine::scene::SaveSlot slot;
    slot.seed = 42;
    slot.label = "Тестовый слот";
    slot.settings = engine::resource::JsonValue::make_object();
    slot.settings["master"] = 0.8;
    slot.settings["music"] = 0.6;
    slot.settings["sfx"] = 0.9;

    // Состояние RNG сохраняется для воспроизводимости боя (FR-E92)
    rules::SeededRandomProvider rng(42);
    for (int i = 0; i < 5; ++i) rng.roll_d20();
    slot.rng_state = rng.export_state();

    REQUIRE(saves.save_state(path, slot, state).is_ok());
    REQUIRE(saves.save_count() == 1);

    game::CombatSaveState restored;
    const auto loaded = saves.load_state(path, restored);
    REQUIRE(loaded.is_ok());
    REQUIRE(loaded.value().save_version == engine::scene::kCurrentSaveVersion);
    REQUIRE(loaded.value().seed == 42);
    REQUIRE(loaded.value().label == "Тестовый слот");
    REQUIRE(approx_equal(loaded.value().settings.at("music").as_double(), 0.6, 1e-9));

    REQUIRE(restored.round == 3);
    REQUIRE(restored.combatants.size() == 2);
    REQUIRE(restored.combatants[0].name == "Воин");
    REQUIRE(restored.combatants[0].hp_current == 9);
    REQUIRE(restored.combatants[0].temp_hp == 5);
    REQUIRE(restored.combatants[0].cell_x == 3);
    REQUIRE(restored.combatants[1].ac == 15);
    REQUIRE(restored.initiative.size() == 2);
    REQUIRE(restored.initiative[0].score == 19);

    // Состояние RNG восстанавливается: продолжение последовательности совпадает (FR-E92)
    std::vector<int> expected;
    for (int i = 0; i < 8; ++i) expected.push_back(rng.roll_d20());
    rules::SeededRandomProvider revived(1);
    REQUIRE(revived.restore_state(loaded.value().rng_state));
    std::vector<int> actual;
    for (int i = 0; i < 8; ++i) actual.push_back(revived.roll_d20());
    REQUIRE(actual == expected);

    // Перечень слотов и удаление
    const auto slots = saves.list_slots("assets:/saves");
    REQUIRE(slots.size() >= 1);
    REQUIRE(saves.remove_slot(path));

    // Битое сохранение: отказ с сообщением, без аварийного завершения (п. 4.1.9.3)
    REQUIRE(write_temp_asset(fs, path, "{ это не JSON"));
    const auto broken = saves.load(path);
    REQUIRE_FALSE(broken.is_ok());

    // Сохранение от более новой версии программы отклоняется с понятным сообщением
    REQUIRE(write_temp_asset(fs, path,
        R"({"$schema":"schemas/save.schema.json","schema_version":1,"save_version":999,)"
        R"("seed":1,"scene":{"round":1}})"));
    const auto from_future = saves.load(path);
    REQUIRE_FALSE(from_future.is_ok());
    REQUIRE(from_future.error_message().find("более новой версией") != std::string::npos);
    remove_temp_asset(fs, path);
}

// --- Миграция версий сохранений (FR-E91) ------------------------------------------
TEST_CASE("save_version_migration", "[integration][FR-E91]") {
    const auto fs = make_fs();
    engine::scene::SaveSystem saves(fs);

    const std::string path = "assets:/saves/_test_migration.json";
    // Сохранение «старой» версии 1 с устаревшим составом поля scene
    REQUIRE(write_temp_asset(fs, path,
        R"({"$schema":"schemas/save.schema.json","schema_version":1,"save_version":1,)"
        R"("seed":7,"scene":{"round":2,"hp":10}})"));

    // Миграция 1 → 2: переименование поля hp в hp_current внутри scene
    saves.register_migration(1, [](engine::resource::JsonValue& json, std::string& error) {
        if (!json.at("scene").is_object()) { error = "нет поля scene"; return false; }
        engine::resource::JsonValue scene = json.at("scene");
        if (scene.contains("hp")) {
            scene["hp_current"] = scene.at("hp");
            // удаление устаревшего поля выполняется перезаписью объекта scene
            engine::resource::JsonObject cleaned;
            for (const auto& [key, value] : scene.as_object()) {
                if (key == "hp") continue;
                cleaned.insert_or_assign(key, value);
            }
            scene = engine::resource::JsonValue(std::move(cleaned));
        }
        json["scene"] = scene;
        return true;
    });
    // Текущая версия формата в тесте повышается через kCurrentSaveVersion;
    // если kCurrentSaveVersion == 1, миграция не вызывается — проверка условная.
    const auto loaded = saves.load(path);
    if (engine::scene::kCurrentSaveVersion > 1) {
        REQUIRE(loaded.is_ok());
        REQUIRE(saves.migration_count() >= 1);
        REQUIRE_FALSE(loaded.value().scene.contains("hp"));
        REQUIRE(loaded.value().scene.at("hp_current").as_int() == 10);
    } else {
        REQUIRE(loaded.is_ok());
        REQUIRE(loaded.value().save_version == 1);
    }

    // Неполная цепочка миграций: отказ с сообщением, а не падение (п. 4.1.9.3)
    engine::scene::SaveSystem strict(fs);
    REQUIRE(write_temp_asset(fs, path,
        R"({"$schema":"schemas/save.schema.json","schema_version":1,"save_version":1,)"
        R"("seed":7,"scene":{"round":2}})"));
    if (engine::scene::kCurrentSaveVersion > 1) {
        const auto failed = strict.load(path);
        REQUIRE_FALSE(failed.is_ok());
        REQUIRE(failed.error_message().find("миграци") != std::string::npos);
    }
    remove_temp_asset(fs, path);
}

// --- Адаптер правил и события шины (п. 4.1.11.6, FR-E110…FR-E121) ------------------
TEST_CASE("rules_adapter_publishes_events", "[integration][M6][p4116]") {
    engine::core::EventBus bus;
    engine::ecs::Registry registry;
    rules::SeededRandomProvider rng(2026);
    rules::DiceRoller roller(rng);
    const auto conditions = rules::make_standard_conditions();
    game::RulesAdapter adapter(bus, roller, registry, conditions);

    int rolls = 0, damages = 0, added = 0, removed = 0, turn_changes = 0;
    rules::RollBreakdownDTO last_breakdown;
    bus.subscribe<game::RollPerformedEvent>(
        [&](const game::RollPerformedEvent& e) { ++rolls; last_breakdown = e.breakdown; });
    bus.subscribe<game::DamageAppliedEvent>([&](const game::DamageAppliedEvent&) { ++damages; });
    bus.subscribe<game::ConditionAddedEvent>([&](const game::ConditionAddedEvent&) { ++added; });
    bus.subscribe<game::ConditionRemovedEvent>([&](const game::ConditionRemovedEvent&) { ++removed; });
    bus.subscribe<game::TurnStateChangedEvent>(
        [&](const game::TurnStateChangedEvent&) { ++turn_changes; });

    // Сущность-цель с компонентами п. 4.1.6.2
    const auto target = registry.create_entity();
    registry.add_component<game::HealthComponent>(
        target, game::HealthComponent{rules::make_health(20)});
    registry.add_component<game::ConditionComponent>(target);
    registry.add_component<game::GridPositionComponent>(target, game::GridPositionComponent{4, 5, 0});
    registry.add_component<game::TransformComponent>(target, game::TransformComponent{20.f, 25.f});
    registry.add_component<game::AbilityScoresComponent>(target);

    auto& abilities = registry.get_component<game::AbilityScoresComponent>(target);
    abilities.scores.set_score(rules::Ability::Dex, 18);
    abilities.scores.set_score(rules::Ability::Str, 8);
    REQUIRE(abilities.scores.modifier(rules::Ability::Dex) == 4);

    // AC: средняя броня + щит, DEX 18 → 13 + 2 + 2 (AC-K03)
    const int ac = adapter.compute_ac(abilities, rules::ArmorCategory::Medium, 13, true);
    REQUIRE(ac == 17);

    // Проверка: событие RollPerformed с полным разбором для UI (UI-E03)
    const auto check = adapter.perform_check("Воин", "check", /*dc=*/15, /*mod=*/3, /*prof=*/2,
                                             /*adv=*/false, /*dis=*/false);
    REQUIRE(rolls == 1);
    REQUIRE(last_breakdown.total == check.total);
    REQUIRE_FALSE(last_breakdown.modifiers.empty());

    // Атака и урон: сначала временные хиты, затем текущие (п. 4.1.11.4)
    auto& health = registry.get_component<game::HealthComponent>(target);
    rules::grant_temp_hp(health.state, 5);
    rules::DamageResistance resistance;
    const int dealt = adapter.apply_damage_to(target, "Гоблин", "Воин", 8,
                                              rules::DamageType::Slashing, resistance);
    REQUIRE(dealt == 8);
    REQUIRE(damages == 1);
    REQUIRE(health.state.temp_hp == 0);
    REQUIRE(health.state.hp_current == 17); // 20 − 3 (8 − 5 временных)

    // Сопротивление применяется до списания (FR-K42)
    resistance.resistant.set(static_cast<std::size_t>(rules::DamageType::Fire));
    const int mitigated = adapter.apply_damage_to(target, "Гоблин", "Воин", 9,
                                                  rules::DamageType::Fire, resistance);
    REQUIRE(mitigated == 4); // floor(9/2)

    // Лечение не восстанавливает временные хиты
    const int healed = adapter.heal_entity(target, 100);
    REQUIRE(healed > 0);
    REQUIRE(health.state.hp_current == 20);
    REQUIRE(health.state.temp_hp == 0);

    // Состояния: добавление/снятие публикуют события (п. 4.1.11.6)
    rules::ConditionInstance prone;
    prone.id = "Condition_Prone";
    prone.source_id = "Effect_Shove";
    REQUIRE(adapter.add_condition(target, "Воин", prone));
    REQUIRE(added == 1);
    REQUIRE_FALSE(adapter.add_condition(target, "Воин", prone)); // дубль отклоняется
    const auto summary = adapter.summarize_conditions(target);
    REQUIRE(rules::has_flag(summary.flags, rules::ConditionFlags::SpeedZero));
    REQUIRE(adapter.remove_condition(target, "Воин", "Condition_Prone", "dispelled"));
    REQUIRE(removed == 1);
    // Неизвестное состояние: отказ с записью в журнал, без падения (NFR-E14)
    rules::ConditionInstance unknown;
    unknown.id = "Condition_NonExistent";
    REQUIRE_FALSE(adapter.add_condition(target, "Воин", unknown));

    // Атака по ошеломлённой цели в ближнем бою — критический удар (DOC-01 OQ-08)
    rules::ConditionInstance stunned;
    stunned.id = "Condition_Stunned";
    stunned.source_id = "Effect_Stun";
    REQUIRE(adapter.add_condition(target, "Воин", stunned));
    rules::AttackInput attack_input;
    attack_input.ability_modifier = 3;
    attack_input.proficiency_bonus = 2;
    const auto outcome = adapter.perform_attack("Гоблин", attack_input, ac, {}, 
                                                registry.get_component<game::ConditionComponent>(target).conditions,
                                                /*melee=*/true);
    REQUIRE(outcome.hit);              // преимущество от состояния цели
    REQUIRE(outcome.critical);         // крит по ошеломлённому в ближнем бою
    adapter.remove_condition(target, "Воин", "Condition_Stunned", "expired");

    // Смена состояния автомата хода публикуется в шине (п. 4.1.7.4)
    adapter.publish_turn_state("Воин", rules::TurnState::WaitingForAction,
                               rules::TurnState::Targeting, 1);
    adapter.publish_turn_state("Воин", rules::TurnState::Targeting,
                               rules::TurnState::WaitingForAction, 1);
    REQUIRE(turn_changes == 2);

    // Инициатива строится через DiceRoller (FR-K102)
    std::vector<std::pair<engine::ecs::EntityId, int>> participants;
    participants.push_back({target, 4});
    const auto queue = adapter.build_initiative(participants);
    REQUIRE(queue.size() == 1);
}

// --- Вертикальный срез боя (Приложение Д, AC-E11) ---------------------------------
TEST_CASE("combat_scene_vertical_slice", "[integration][M7][AC-E11]") {
    engine::core::EventBus bus;
    rules::SeededRandomProvider rng(4242);
    rules::DiceRoller roller(rng);

    game::CombatScene scene(bus, roller);
    // Состав среза: PlayerFighter, PlayerRogue, Goblin1, Goblin2 (Приложение Д)
    scene.add_combatant("PlayerFighter", true, 12, 5, 2, 18, 1, 1);
    scene.add_combatant("PlayerRogue", true, 9, 4, 3, 14, 2, 1);
    scene.add_combatant("Goblin1", false, 7, 4, 2, 15, 6, 3);
    scene.add_combatant("Goblin2", false, 7, 4, 2, 15, 7, 4);

    std::vector<std::string> log_events;
    bus.subscribe<game::AttackResolvedEvent>(
        [&](const game::AttackResolvedEvent& e) { log_events.push_back(e.description); });

    scene.on_enter();
    REQUIRE(scene.combatant_count() == 4);
    for (std::size_t i = 0; i < scene.combatant_count(); ++i) {
        REQUIRE(scene.registry().is_alive(scene.combatant(i).entity));
        REQUIRE(scene.registry().has_component<game::GridPosition>(scene.combatant(i).entity));
    }

    // Сетка комнаты 20×15 не обязательна для среза: проверяем проходимость клеток
    REQUIRE(scene.grid().is_passable({1, 1}));

    // Атака: разбор броска передаётся в событии как DTO (UI-E02)
    const auto attack = scene.perform_attack(0, 2);
    REQUIRE(log_events.size() == 1);
    REQUIRE_FALSE(attack.attack_breakdown.title.empty());
    REQUIRE(attack.attack_breakdown.dice.size() >= 1);
    REQUIRE_FALSE(scene.combat_log().empty());

    // Два хода сцены: отложенные команды исполняются, сущности остаются живыми
    engine::platform::MainLoop loop;
    loop.set_update([&](double dt) { scene.update(dt); });
    loop.set_render([&](double) { scene.render(0.0); });
    for (int i = 0; i < 10; ++i) loop.tick(engine::platform::MainLoop::kDtLogic);
    for (std::size_t i = 0; i < scene.combatant_count(); ++i) {
        REQUIRE(scene.registry().is_alive(scene.combatant(i).entity));
    }
}

// --- Виджеты UI получают DTO и показывают разбор полностью (UI-E03, FR-E86) --------
TEST_CASE("ui_widgets_show_full_breakdown", "[integration][M8][UI-E03][UI-E04]") {
    // RollResult → DTO → панель разбора (правило адаптации п. Г.8)
    rules::SeededRandomProvider rng(11);
    rules::DiceRoller roller(rng);
    const auto roll = roller.roll(/*dc=*/15, /*modifier=*/5, /*advantage=*/true);
    const auto dto = rules::to_dto(roll, "Атака: Воин");

    game::DiceBreakdownPanel panel;
    panel.set_breakdown(dto);
    // UI-E03: все слагаемые отображаются, ничего не теряется
    REQUIRE(panel.modifier_row_count() >= dto.modifiers.size() + 3); // кубики + признак + итог
    bool has_total = false;
    for (const auto& row : panel.rows()) {
        if (row.find("Итого") != std::string::npos) has_total = true;
    }
    REQUIRE(has_total);
    REQUIRE(panel.subtree_size() > 1); // заголовок + контейнер строк

    // UI-E04: преимущество и помеха различимы подписью и маркером, не только цветом
    const std::string adv = game::DiceBreakdownPanel::advantage_marker(true, false, false);
    const std::string dis = game::DiceBreakdownPanel::advantage_marker(false, true, false);
    const std::string cancel = game::DiceBreakdownPanel::advantage_marker(true, true, true);
    REQUIRE(adv != dis);
    REQUIRE(adv != cancel);
    REQUIRE(adv.find("[^]") != std::string::npos);
    REQUIRE(dis.find("[v]") != std::string::npos);
    REQUIRE(cancel.find("[=]") != std::string::npos);

    // Очередь инициативы
    rules::InitiativeTrackerDTO tracker;
    tracker.round = 2;
    tracker.active_index = 0;
    tracker.entries.push_back(rules::InitiativeEntryDTO{1, "Воин", 19, true, true});
    tracker.entries.push_back(rules::InitiativeEntryDTO{2, "Гоблин", 12, false, false});
    game::InitiativeTracker tracker_widget;
    tracker_widget.set_tracker(tracker);
    REQUIRE(tracker_widget.row_count() == 3); // заголовок «Раунд» + 2 участника
    REQUIRE(tracker_widget.active_name() == "Воин");

    // Панель ресурсов хода: клетки и футы (формула 17 п. Г.13)
    rules::TurnResources resources;
    rules::reset_turn(resources, 30);
    game::TurnResourceBar bar;
    bar.set_resources(rules::to_dto(resources, "Воин"));
    REQUIRE(bar.rows().size() == 4);
    resources.action_used = true;
    resources.movement_remaining_feet = 10;
    bar.set_resources(rules::to_dto(resources, "Воин"));
    bool has_movement = false;
    for (const auto& row : bar.rows()) {
        if (row.find("10 футов") != std::string::npos && row.find("2 клеток") != std::string::npos) {
            has_movement = true;
        }
    }
    REQUIRE(has_movement);

    // Иконки состояний (UI-E04: иконка + подпись)
    const auto conditions = rules::make_standard_conditions();
    std::vector<rules::ConditionInstance> active;
    active.push_back(rules::ConditionInstance{"Condition_Prone", "Effect_Shove", -1, false});
    active.push_back(rules::ConditionInstance{"Condition_Blinded", "Effect_Darkness", -1, false});
    game::ConditionIcons icons;
    icons.set_conditions(rules::to_condition_dtos(conditions, active));
    REQUIRE(icons.icon_count() == 2);
    REQUIRE(game::ConditionIcons::icon_glyph("icon_prone") !=
            game::ConditionIcons::icon_glyph("icon_blinded"));

    // Журнал боя: прокрутка и фильтрация по типу события (FR-E87)
    game::CombatLog log;
    log.append(game::CombatLogFilter::Rolls, "Воин: d20=18 +5 = 23 против AC 15");
    log.append(game::CombatLogFilter::Damage, "Гоблин получает 8 рубящего урона");
    log.append(game::CombatLogFilter::Conditions, "Гоблин: Лежит ничком");
    log.append(game::CombatLogFilter::Turn, "Ход: Гоблин");
    REQUIRE(log.entry_count() == 4);
    REQUIRE(log.visible_count() == 4);
    log.set_filter(game::CombatLogFilter::Damage);
    REQUIRE(log.visible_count() == 1);
    REQUIRE(log.visible_entries()[0].find("рубящего урона") != std::string::npos);
    log.set_filter(game::CombatLogFilter::All);
    REQUIRE(log.visible_count() == 4);
    log.scroll_to_latest();

    // Карточка персонажа
    rules::HealthState health = rules::make_health(12);
    rules::grant_temp_hp(health, 5);
    game::CharacterSheetPanel sheet;
    sheet.set_summary(rules::make_summary("Воин", 1, 18, health, 30,
                                          rules::to_condition_dtos(conditions, active)));
    bool has_hp = false;
    for (const auto& row : sheet.rows()) {
        if (row.find("HP 12/12") != std::string::npos && row.find("+5 временные") != std::string::npos) {
            has_hp = true;
        }
    }
    REQUIRE(has_hp);

    // Экран атрибуции ассетов (UI-E05, п. 4.6.4): состав сведений из DOC-03
    game::CreditsScreen credits;
    credits.set_credits({game::CreditEntry{"tileset_dungeon.png", "Kenney", "kenney.nl", "CC0 1.0"}});
    credits.open();
    const auto rows = credits.rows();
    REQUIRE(rows.size() == 2);
    REQUIRE(rows[1].find("Kenney") != std::string::npos);
    REQUIRE(rows[1].find("CC0 1.0") != std::string::npos);
    REQUIRE(credits.is_open());

    // Отрисовка всего дерева на headless-рендерере
    engine::ui::UiRoot ui;
    ui.resize(1280, 720);
    ui.add(std::make_shared<game::DiceBreakdownPanel>(panel));
    ui.add(std::make_shared<game::InitiativeTracker>(tracker_widget));
    engine::render::NullRenderer renderer;
    ui.draw(renderer);
    REQUIRE(renderer.primitives_drawn() > 0);
    REQUIRE(renderer.text_calls() > 0); // текст рисуется через контракт (FR-E29)
}

// --- texture_loader_png (FR-E45, таблица 4.16: stb) ------------------------------
// Графика сгенерирована scripts/gen_assets.py (docs/assets.md) и декодируется
// движком в RGBA: токены 32×32, портреты 64×64, битые данные не роняют загрузчик.
TEST_CASE("texture_loader_png", "[integration][FR-E45]") {
    auto fs = make_fs();
    engine::resource::TextureLoader loader(fs);

    const auto fighter = loader.load_png("assets:/textures/tokens/fighter.png");
    REQUIRE(fighter.is_ok());
    CHECK(fighter.value().width == 32);
    CHECK(fighter.value().height == 32);
    CHECK(fighter.value().pixels.size() == 32u * 32u * 4u);
    // Угол спрайта прозрачен, центр фигуры непрозрачен (контент действительно есть)
    CHECK(fighter.value().pixels[3] == 0);
    const std::size_t center = (16u * 32u + 16u) * 4u + 3u;
    CHECK(fighter.value().pixels[center] == 255);

    const auto portrait = loader.load_png("assets:/textures/portraits/goblin.png");
    REQUIRE(portrait.is_ok());
    CHECK(portrait.value().width == 64);
    CHECK(portrait.value().height == 64);

    // Повреждённые данные: ошибка без аварийного завершения (NFR-E09)
    const std::uint8_t garbage[] = {0x89, 'P', 'N', 'G', 1, 2, 3, 4, 5};
    const auto bad = engine::resource::TextureLoader::decode_png(garbage, sizeof(garbage));
    CHECK(bad.is_ok() == false);

    // Отсутствующий файл: ошибка с сообщением
    const auto missing = loader.load_png("assets:/textures/no_such.png");
    CHECK(missing.is_ok() == false);
}
