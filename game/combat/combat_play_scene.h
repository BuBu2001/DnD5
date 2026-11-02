// game/combat/combat_play_scene.h — играбельная боевая сцена (docs/controls.md):
// карта из assets/maps/goblin_ambush.json, токены/плитки из PNG (docs/assets.md),
// управление WASD + мышь + горячие клавиши, ИИ гоблинов, подсветка достижимых
// клеток. Сцена связывает engine (рендер, ввод) и rules (через CombatScene) —
// это игровой слой, направление зависимостей п. 3.3.2 соблюдается.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "combat/combat_scene.h"
#include "controls/camera_controller.h"
#include "controls/cell_picking.h"
#include "controls/control_scheme.h"
#include "core/file_system.h"
#include "platform/input_actions.h"
#include "render/camera.h"
#include "render/renderer.h"
#include "render/sprite_batch.h"
#include "resource/json_loader.h"
#include "resource/schema_registry.h"
#include "resource/schema_validator.h"
#include "resource/texture_loader.h"
#include "rules/dto_convert.h"
#include "rules/grid.h"

namespace game {

// Создание GL-текстуры из RGBA-пикселей; kInvalidTexture — headless-режим
using TextureProvider = std::function<engine::render::TextureId(
    int w, int h, const std::uint8_t* pixels)>;

class CombatPlayScene : public engine::scene::Scene {
public:
    static constexpr float kTileSize = 32.f;

    CombatPlayScene(engine::render::IRenderer& renderer, engine::core::EventBus& bus,
                    rules::DiceRoller& roller, engine::core::FileSystem& fs)
        : renderer_(renderer), fs_(fs), combat_(bus, roller), batch_(renderer),
          camera_controller_(camera_, 640.f, 480.f, 1280.f, 720.f) {}

    void set_texture_provider(TextureProvider provider) { texture_provider_ = std::move(provider); }
    void set_map_path(std::string path) { map_path_ = std::move(path); }

    // Загрузка карты/спавнов/текстур; при ошибке данных — карта 20×15 по умолчанию
    // (вертикальный срез Приложения Д) и процедурные цвета вместо спрайтов.
    void on_enter() override {
        load_map();
        load_textures();
        combat_.on_enter();
        // В начале боя запас движения есть у всех участников; begin_turn позже
        // пополняет его для активного бойца (п. Г.6, reset_turn).
        for (std::size_t i = 0; i < combat_.combatant_count(); ++i) {
            combat_.combatant_mut(i).movement_remaining_feet = combat_.combatant(i).speed_feet;
        }
        // Мир = сетка клеток; камера сразу ограничивается картой (центрирование,
        // если карта меньше вьюпорта 1280×720)
        camera_controller_.set_world(grid_columns_ * kTileSize, grid_rows_ * kTileSize);
        build_turn_order();
        select_first_player_unit();
    }

    void update(double dt_logic) override {
        combat_.update(dt_logic);
        if (enemy_turn_pending_) run_enemy_turn();
        refresh_reachable();
    }

    // --- ввод (docs/controls.md) ------------------------------------------------
    void set_mouse_logical(float x, float y) { mouse_x_ = x; mouse_y_ = y; }

    void handle_actions(const engine::platform::InputActions& actions, bool ui_captured) {
        if (actions.held(game::actions::kCameraLeft) || actions.held(game::actions::kCameraRight) ||
            actions.held(game::actions::kCameraUp) || actions.held(game::actions::kCameraDown) ||
            actions.wheel(game::actions::kZoomIn) || actions.wheel(game::actions::kZoomOut)) {
            camera_controller_.update(actions, static_cast<float>(1.0 / 60.0));
        }
        if (ui_captured) return; // курсор над панелью UI — поле не трогает

        // Наведение: клетка под курсором (FR-E33)
        const auto pick = pick_cell(camera_, mouse_x_, mouse_y_, combat_.grid(), kTileSize);
        hover_ = pick.has_value() ? std::optional<rules::Cell>(pick->cell) : std::nullopt;

        if (actions.pressed(game::actions::kCancel)) {
            selected_ = kNone;
            status_ = "Выбор отменён (ПКМ/Q)";
        }
        if (actions.pressed(game::actions::kSelect) && hover_.has_value()) {
            request_cell_action(*hover_);
        }
        if (actions.pressed(game::actions::kEndTurn)) end_turn();
        if (actions.pressed(game::actions::kNextUnit)) select_next_player_unit();
        // Цифры 1…4 — выбор бойца по слоту инициативы
        const char* slots[] = {game::actions::kSelectSlot1, game::actions::kSelectSlot2,
                               game::actions::kSelectSlot3, game::actions::kSelectSlot4};
        for (int i = 0; i < 4; ++i) {
            if (actions.pressed(slots[i])) select_player_slot(i);
        }
    }

    void request_cell_action(const rules::Cell& cell) {
        if (battle_over_) { status_ = "Бой завершён"; return; }
        const std::size_t occupant = combat_.combatant_at(cell);
        if (selected_ == kNone) {
            // Выбор своего бойца ЛКМ
            if (occupant != CombatScene::kNoIndex &&
                combat_.combatant(occupant).player_side &&
                combat_.combatant(occupant).health.hp_current > 0) {
                selected_ = occupant;
                status_ = "Выбран: " + combat_.combatant(occupant).name;
            } else {
                status_ = "ЛКМ — выбрать своего бойца";
            }
            return;
        }
        if (selected_ >= combat_.combatant_count()) { selected_ = kNone; return; }
        auto& unit = combat_.combatant_mut(selected_);

        // Атака: смежная клетка с противником и действие не потрачено
        if (occupant != CombatScene::kNoIndex && !combat_.combatant(occupant).player_side) {
            if (rules::TacticalGrid::manhattan(unit.cell, cell) == 1) {
                if (unit.action_used) {
                    status_ = "Действие атаки уже потрачено (E — конец хода)";
                    return;
                }
                const auto ev = combat_.perform_attack(selected_, occupant);
                last_breakdown_ = ev.attack_breakdown;
                has_breakdown_ = true;
                status_ = ev.description;
                check_battle_end();
            } else {
                status_ = "Цель слишком далеко: нужна смежная клетка";
            }
            return;
        }
        // Перемещение в свободную клетку
        if (occupant == CombatScene::kNoIndex) {
            const auto result = combat_.move_unit(selected_, cell);
            if (result.ok) {
                status_ = unit.name + ": перемещение (−" + std::to_string(result.cost_feet) +
                          " футов, остаток " + std::to_string(unit.movement_remaining_feet) + ")";
            } else {
                status_ = "Нельзя: " + result.reason;
            }
        }
    }

    void end_turn() {
        if (battle_over_) return;
        status_ = "Конец хода: " + current_name();
        advance_turn();
    }

    void select_next_player_unit() {
        for (std::size_t step = 1; step <= combat_.combatant_count(); ++step) {
            const std::size_t idx = (player_slot(selected_ == kNone ? 0 : selected_) + step) %
                                    std::max<std::size_t>(1, player_count());
            const std::size_t candidate = player_unit_by_slot(idx);
            if (candidate != kNone && candidate != selected_) {
                selected_ = candidate;
                status_ = "Выбран: " + combat_.combatant(candidate).name;
                return;
            }
        }
    }

    void select_player_slot(int slot) {
        const std::size_t candidate = player_unit_by_slot(static_cast<std::size_t>(slot));
        if (candidate != kNone) {
            selected_ = candidate;
            status_ = "Выбран: " + combat_.combatant(candidate).name;
        } else {
            status_ = "В слоте " + std::to_string(slot + 1) + " нет бойца";
        }
    }

    // --- отрисовка ---------------------------------------------------------------
    void render(double /*alpha*/) override {
        using engine::render::Color;
        using engine::render::Rect;
        using engine::render::Vec2f;
        const float zoom = camera_.zoom();
        const auto visible = camera_.visible_rect(1280.f, 720.f);
        batch_.begin();

        // Плитки местности (отсечение по видимой области, NFR-E02)
        const int c0 = std::max(0, static_cast<int>(visible.x / kTileSize));
        const int r0 = std::max(0, static_cast<int>(visible.y / kTileSize));
        const int c1 = std::min(grid_columns_ - 1,
                                static_cast<int>((visible.x + visible.w) / kTileSize));
        const int r1 = std::min(grid_rows_ - 1,
                                static_cast<int>((visible.y + visible.h) / kTileSize));
        for (int r = r0; r <= r1; ++r) {
            for (int c = c0; c <= c1; ++c) {
                const auto screen = camera_.world_to_screen(
                    {c * kTileSize, r * kTileSize});
                const Rect dst{screen.x, screen.y, kTileSize * zoom, kTileSize * zoom};
                const engine::render::TextureId tex = tile_texture({c, r});
                if (tex != engine::render::kInvalidTexture) {
                    batch_.draw(tex, {0.f, 0.f, kTileSize, kTileSize}, dst,
                                Color{1.f, 1.f, 1.f, 1.f});
                } else {
                    batch_.draw(placeholder_tile_, {0.f, 0.f, 4.f, 4.f}, dst,
                                tile_color({c, r}));
                }
            }
        }
        // Токены бойцов
        for (std::size_t i = 0; i < combat_.combatant_count(); ++i) {
            const auto& unit = combat_.combatant(i);
            if (unit.health.hp_current <= 0) continue; // павшие не отрисовываются
            const auto screen = camera_.world_to_screen(
                {unit.cell.x * kTileSize, unit.cell.y * kTileSize});
            const Rect dst{screen.x, screen.y, kTileSize * zoom, kTileSize * zoom};
            const engine::render::TextureId tex = token_texture(i);
            if (tex != engine::render::kInvalidTexture) {
                batch_.draw(tex, {0.f, 0.f, kTileSize, kTileSize}, dst,
                            Color{1.f, 1.f, 1.f, 1.f});
            } else {
                batch_.draw(placeholder_tile_, {0.f, 0.f, 4.f, 4.f}, dst,
                            unit.player_side ? Color{0.3f, 0.55f, 0.9f, 1.f}
                                             : Color{0.75f, 0.3f, 0.25f, 1.f});
            }
        }
        batch_.end();
        batch_.flush();

        // Подсветка достижимых клеток выбранного бойца (основа FR-E35)
        if (selected_ != kNone) {
            const Color reach{0.30f, 0.62f, 1.00f, 0.22f};
            for (const auto& cell : reachable_cache_) {
                const auto screen = camera_.world_to_screen(
                    {cell.x * kTileSize, cell.y * kTileSize});
                renderer_.draw_rect({screen.x, screen.y, kTileSize * zoom, kTileSize * zoom},
                                    reach);
            }
        }
        // Клетка под курсором и выбор
        if (hover_.has_value()) {
            const auto screen = camera_.world_to_screen(
                {hover_->x * kTileSize, hover_->y * kTileSize});
            renderer_.draw_rect_outline({screen.x, screen.y, kTileSize * zoom, kTileSize * zoom},
                                        Color{1.f, 1.f, 1.f, 0.85f}, 2.f);
        }
        if (selected_ != kNone && selected_ < combat_.combatant_count()) {
            const auto& unit = combat_.combatant(selected_);
            const auto screen = camera_.world_to_screen(
                {unit.cell.x * kTileSize, unit.cell.y * kTileSize});
            renderer_.draw_rect_outline({screen.x - 2.f, screen.y - 2.f,
                                         kTileSize * zoom + 4.f, kTileSize * zoom + 4.f},
                                        Color{0.35f, 0.9f, 0.45f, 0.95f}, 2.f);
            // Полоса хитов над токеном (HP-бар, UI-E04: значение и подписью)
            const float frac = static_cast<float>(unit.health.hp_current) /
                               static_cast<float>(std::max(1, unit.health.hp_max));
            const float bar_w = kTileSize * zoom;
            renderer_.draw_rect({screen.x, screen.y - 7.f, bar_w, 4.f},
                                Color{0.1f, 0.1f, 0.12f, 0.9f});
            renderer_.draw_rect({screen.x, screen.y - 7.f, bar_w * frac, 4.f},
                                frac > 0.5f ? Color{0.35f, 0.85f, 0.4f, 1.f}
                                            : Color{0.9f, 0.4f, 0.3f, 1.f});
            char hp[24];
            std::snprintf(hp, sizeof(hp), "%d/%d", unit.health.hp_current, unit.health.hp_max);
            renderer_.draw_text(screen.x, screen.y - 20.f, hp, 12.f, Color{1.f, 1.f, 1.f, 1.f});
            renderer_.draw_text(screen.x, screen.y + kTileSize * zoom + 2.f, unit.name, 13.f,
                                Color{0.95f, 0.92f, 0.8f, 1.f});
        }
        // Статусная строка сцены
        renderer_.draw_text(16.f, 700.f - 118.f, status_, 15.f,
                            engine::render::Color{1.f, 0.95f, 0.75f, 1.f});
        ++renders_;
    }

    // --- состояние для UI (только DTO, E-D12) ------------------------------------
    rules::InitiativeTrackerDTO initiative_dto() const {
        return rules::to_dto(combat_.initiative(), round_, active_index_);
    }
    rules::TurnResourceBarDTO turn_resources_dto() const {
        rules::TurnResources res;
        std::string name = "—";
        const std::size_t idx = active_unit_index();
        if (idx != kNone) {
            const auto& unit = combat_.combatant(idx);
            res.movement_remaining_feet = unit.movement_remaining_feet;
            res.action_used = unit.action_used;
            name = unit.name;
        }
        return rules::to_dto(res, name);
    }
    std::size_t selected() const { return selected_; }
    void set_selected(std::size_t idx) {
        selected_ = idx;
        refresh_reachable();
    }
    bool has_breakdown() const { return has_breakdown_; }
    const rules::RollBreakdownDTO& last_breakdown() const { return last_breakdown_; }
    const std::string& status() const { return status_; }
    bool battle_over() const { return battle_over_; }
    std::size_t renders() const { return renders_; }
    CombatScene& combat() { return combat_; }
    engine::render::Camera2D& camera() { return camera_; }
    CameraController& camera_controller() { return camera_controller_; }
    int grid_columns() const { return grid_columns_; }
    int grid_rows() const { return grid_rows_; }

    // Ходы: активный боец в порядке инициативы
    std::size_t active_unit_index() const {
        if (turn_order_.empty()) return kNone;
        return turn_order_[active_index_ % turn_order_.size()];
    }
    std::string current_name() const {
        const std::size_t idx = active_unit_index();
        return idx == kNone ? std::string("—") : combat_.combatant(idx).name;
    }

private:
    static constexpr std::size_t kNone = CombatScene::kNoIndex;

    // --- загрузка карты (assets/maps/goblin_ambush.json, DATA-E02) ----------------
    void load_map() {
        engine::resource::SchemaValidator validator;
        engine::resource::SchemaRegistry registry(fs_, validator);
        registry.load_directory("assets:/schemas");
        engine::resource::JsonLoader loader(fs_, validator);
        const auto doc = loader.load(map_path_);
        if (!doc.is_ok()) {
            status_ = "Карта не загружена (" + doc.error_message() + ") — срез по умолчанию";
            spawn_default_slice();
            return;
        }
        const auto& map = doc.value().value;
        grid_columns_ = static_cast<int>(map.at("grid").at("columns").as_int(20));
        grid_rows_ = static_cast<int>(map.at("grid").at("rows").as_int(15));

        // Ключи плиток: проходимость и тип местности
        std::unordered_map<std::string, char> key_passable;   // '.' проходимо
        std::unordered_map<std::string, bool> key_difficult;
        std::unordered_map<std::string, std::string> key_texture;
        for (const auto& tile : map.at("tiles").as_array()) {
            const std::string key = tile.at("key").as_string_or("?");
            key_passable[key] = tile.at("passable").as_bool(true) ? '.' : '#';
            key_difficult[key] = tile.at("difficult_terrain").as_bool(false);
            key_texture[key] = tile.at("texture_key").as_string_or("");
        }
        // Слой местности → сетка правил и таблица текстур
        combat_.reset_grid(grid_columns_, grid_rows_);
        for (const auto& layer : map.at("layers").as_array()) {
            if (layer.at("kind").as_string_or("") != "terrain") continue;
            int row = 0;
            for (const auto& line : layer.at("rows").as_array()) {
                const std::string text = line.as_string_or("");
                for (std::size_t col = 0; col < text.size() && col < static_cast<std::size_t>(grid_columns_); ++col) {
                    const std::string ch(1, text[col]);
                    const rules::Cell cell{static_cast<int>(col), row};
                    tile_texture_key_[cell_key(cell)] = key_texture.count(ch) ? key_texture[ch] : "";
                    if (key_passable.count(ch) && key_passable[ch] == '#') {
                        combat_.grid().set_terrain(cell, rules::TerrainType::Impassable);
                    } else if (key_difficult.count(ch) && key_difficult[ch]) {
                        combat_.grid().set_terrain(cell, rules::TerrainType::Difficult);
                    }
                }
                ++row;
                if (row >= grid_rows_) break;
            }
        }
        // Спавны: состав вертикального среза (Приложение Д)
        int goblin_seq = 0;
        for (const auto& spawn : map.at("spawns").as_array()) {
            const std::string id = spawn.at("entity_id").as_string_or("");
            const int x = static_cast<int>(spawn.at("column").as_int(0));
            const int y = static_cast<int>(spawn.at("row").as_int(0));
            if (id == "Character_PlayerFighter") {
                combat_.add_combatant("Воин", true, 12, 5, 2, 18, x, y);
                combat_.combatant_mut(combat_.combatant_count() - 1).speed_feet = 30;
                token_kind_.push_back(TokenKind::Fighter);
            } else if (id == "Character_PlayerRogue") {
                combat_.add_combatant("Плут", true, 9, 4, 3, 14, x, y);
                combat_.combatant_mut(combat_.combatant_count() - 1).speed_feet = 30;
                token_kind_.push_back(TokenKind::Rogue);
            } else if (id == "Enemy_Goblin") {
                ++goblin_seq;
                combat_.add_combatant("Гоблин " + std::to_string(goblin_seq), false, 7, 4, 2, 15,
                                      x, y);
                combat_.combatant_mut(combat_.combatant_count() - 1).speed_feet = 30;
                token_kind_.push_back(TokenKind::Goblin);
            }
        }
        if (combat_.combatant_count() == 0) spawn_default_slice();
    }

    void spawn_default_slice() {
        grid_columns_ = 20;
        grid_rows_ = 15;
        combat_.reset_grid(grid_columns_, grid_rows_);
        combat_.add_combatant("Воин", true, 12, 5, 2, 18, 2, 6);
        combat_.add_combatant("Плут", true, 9, 4, 3, 14, 3, 6);
        combat_.add_combatant("Гоблин 1", false, 7, 4, 2, 15, 16, 5);
        combat_.add_combatant("Гоблин 2", false, 7, 4, 2, 15, 17, 9);
        for (std::size_t i = 0; i < combat_.combatant_count(); ++i) {
            combat_.combatant_mut(i).speed_feet = 30;
        }
        token_kind_ = {TokenKind::Fighter, TokenKind::Rogue, TokenKind::Goblin, TokenKind::Goblin};
    }

    // --- текстуры (docs/assets.md) -------------------------------------------------
    enum class TokenKind : std::uint8_t { Fighter, Rogue, Goblin };

    void load_textures() {
        engine::resource::TextureLoader loader(fs_);
        placeholder_tile_ = make_placeholder();
        const std::pair<const char*, std::string> files[] = {
            {"floor_stone", "assets:/textures/tiles/floor_stone.png"},
            {"wall_stone", "assets:/textures/tiles/wall_stone.png"},
            {"rubble", "assets:/textures/tiles/rubble.png"},
            {"token_fighter", "assets:/textures/tokens/fighter.png"},
            {"token_rogue", "assets:/textures/tokens/rogue.png"},
            {"token_goblin", "assets:/textures/tokens/goblin.png"},
        };
        for (const auto& [name, path] : files) {
            const auto image = loader.load_png(path);
            if (!image.is_ok()) continue; // нет файла — рисуем цветной заглушкой
            if (texture_provider_) {
                const auto tex = texture_provider_(image.value().width, image.value().height,
                                                   image.value().pixels.data());
                if (tex != engine::render::kInvalidTexture) textures_[name] = tex;
            }
        }
    }

    engine::render::TextureId make_placeholder() {
        // Белая текстура 4×4 для отрисовки плиток/токенов цветом (headless и
        // случай отсутствия PNG)
        static const std::uint8_t white[4 * 4 * 4] = {
            255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255,
            255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255,
            255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255,
            255,255,255,255, 255,255,255,255, 255,255,255,255, 255,255,255,255};
        if (texture_provider_) return texture_provider_(4, 4, white);
        return engine::render::kInvalidTexture;
    }

    engine::render::TextureId tile_texture(const rules::Cell& cell) const {
        const auto it = tile_texture_key_.find(cell_key(cell));
        if (it == tile_texture_key_.end()) return engine::render::kInvalidTexture;
        const auto tex = textures_.find(it->second);
        return tex == textures_.end() ? engine::render::kInvalidTexture : tex->second;
    }

    engine::render::Color tile_color(const rules::Cell& cell) const {
        const auto terrain = combat_.grid().terrain(cell);
        switch (terrain) {
            case rules::TerrainType::Impassable: return {0.22f, 0.22f, 0.28f, 1.f};
            case rules::TerrainType::Difficult:  return {0.42f, 0.40f, 0.34f, 1.f};
            default:                             return {0.36f, 0.34f, 0.40f, 1.f};
        }
    }

    engine::render::TextureId token_texture(std::size_t idx) const {
        if (idx >= token_kind_.size()) return engine::render::kInvalidTexture;
        const char* name = "token_goblin";
        if (token_kind_[idx] == TokenKind::Fighter) name = "token_fighter";
        else if (token_kind_[idx] == TokenKind::Rogue) name = "token_rogue";
        const auto tex = textures_.find(name);
        return tex == textures_.end() ? engine::render::kInvalidTexture : tex->second;
    }

    static long long cell_key(const rules::Cell& c) {
        return (static_cast<long long>(c.x) << 16) | static_cast<unsigned short>(c.y);
    }

    // --- ходы и ИИ -----------------------------------------------------------------
    void build_turn_order() {
        // Порядок — по инициативе (CombatScene строит очередь в on_enter)
        turn_order_.clear();
        const auto& queue = combat_.initiative();
        for (std::size_t i = 0; i < queue.size(); ++i) {
            turn_order_.push_back(static_cast<std::size_t>(queue.at(i).entity));
        }
        if (turn_order_.empty()) {
            for (std::size_t i = 0; i < combat_.combatant_count(); ++i) turn_order_.push_back(i);
        }
        active_index_ = 0;
        round_ = 1;
        combat_.begin_turn(active_unit_index());
        enemy_turn_pending_ = !combat_.combatant(active_unit_index()).player_side;
    }

    void advance_turn() {
        if (turn_order_.empty()) return;
        for (std::size_t step = 0; step < turn_order_.size(); ++step) {
            active_index_ = (active_index_ + 1) % turn_order_.size();
            if (active_index_ == 0) ++round_;
            const std::size_t idx = turn_order_[active_index_];
            if (combat_.combatant(idx).health.hp_current <= 0) continue; // павшие пропускаются
            combat_.begin_turn(idx);
            enemy_turn_pending_ = !combat_.combatant(idx).player_side;
            if (!enemy_turn_pending_) {
                selected_ = idx; // свой ход: боец сразу выбран
                refresh_reachable();
            }
            status_ = "Ход: " + combat_.combatant(idx).name + " (раунд " +
                      std::to_string(round_) + ")";
            return;
        }
    }

    void select_first_player_unit() {
        for (std::size_t i = 0; i < combat_.combatant_count(); ++i) {
            if (combat_.combatant(i).player_side && combat_.combatant(i).health.hp_current > 0) {
                selected_ = i;
                refresh_reachable();
                return;
            }
        }
    }

    std::size_t player_count() const {
        std::size_t n = 0;
        for (std::size_t i = 0; i < combat_.combatant_count(); ++i) {
            if (combat_.combatant(i).player_side) ++n;
        }
        return n;
    }
    std::size_t player_slot(std::size_t unit_idx) const {
        std::size_t slot = 0;
        for (std::size_t i = 0; i < unit_idx && i < combat_.combatant_count(); ++i) {
            if (combat_.combatant(i).player_side) ++slot;
        }
        return slot;
    }
    std::size_t player_unit_by_slot(std::size_t slot) const {
        std::size_t seen = 0;
        for (std::size_t i = 0; i < combat_.combatant_count(); ++i) {
            if (!combat_.combatant(i).player_side) continue;
            if (combat_.combatant(i).health.hp_current <= 0) continue;
            if (seen == slot) return i;
            ++seen;
        }
        return kNone;
    }

    // ИИ гоблина (фаза M4, упрощённо): к ближайшему игроку, смежно — атака.
    // Все расчёты — через правила (движение — CombatScene::move_unit, атака —
    // perform_attack): игровой слой формул не содержит (п. 3.3.1).
    void run_enemy_turn() {
        enemy_turn_pending_ = false;
        if (battle_over_) return;
        const std::size_t idx = active_unit_index();
        if (idx == kNone || combat_.combatant(idx).player_side) return;

        const std::size_t target = nearest_player(idx);
        if (target != kNone) {
            // Шаги к цели по одной клетке (стоимость учитывается move_unit)
            for (int guard = 0; guard < 12; ++guard) {
                const auto& me = combat_.combatant(idx);
                const auto& foe = combat_.combatant(target);
                if (rules::TacticalGrid::manhattan(me.cell, foe.cell) <= 1) break;
                const int dx = (foe.cell.x > me.cell.x) - (foe.cell.x < me.cell.x);
                const int dy = (foe.cell.y > me.cell.y) - (foe.cell.y < me.cell.y);
                const rules::Cell tries[2] = {{me.cell.x + dx, me.cell.y + dy},
                                              {dx != 0 ? me.cell.x + dx : me.cell.x,
                                               dx != 0 ? me.cell.y : me.cell.y + dy}};
                bool moved = false;
                for (const auto& next : tries) {
                    if (combat_.move_unit(idx, next).ok) { moved = true; break; }
                }
                if (!moved) break;
            }
            const auto& me = combat_.combatant(idx);
            const auto& foe = combat_.combatant(target);
            if (rules::TacticalGrid::manhattan(me.cell, foe.cell) == 1 && !me.action_used &&
                foe.health.hp_current > 0) {
                const auto ev = combat_.perform_attack(idx, target);
                last_breakdown_ = ev.attack_breakdown;
                has_breakdown_ = true;
                status_ = ev.description;
                check_battle_end();
            }
        }
        advance_turn();
    }

    std::size_t nearest_player(std::size_t from) const {
        std::size_t best = kNone;
        int best_dist = INT32_MAX;
        for (std::size_t i = 0; i < combat_.combatant_count(); ++i) {
            const auto& unit = combat_.combatant(i);
            if (!unit.player_side || unit.health.hp_current <= 0) continue;
            const int dist = rules::TacticalGrid::manhattan(combat_.combatant(from).cell,
                                                            unit.cell);
            if (dist < best_dist) { best_dist = dist; best = i; }
        }
        return best;
    }

    void check_battle_end() {
        bool players_alive = false;
        bool enemies_alive = false;
        for (std::size_t i = 0; i < combat_.combatant_count(); ++i) {
            if (combat_.combatant(i).health.hp_current <= 0) continue;
            if (combat_.combatant(i).player_side) players_alive = true;
            else enemies_alive = true;
        }
        if (!enemies_alive) {
            battle_over_ = true;
            status_ = "Победа! Все противники повержены";
        } else if (!players_alive) {
            battle_over_ = true;
            status_ = "Поражение: отряд пал";
        }
    }

    void refresh_reachable() {
        reachable_cache_.clear();
        if (selected_ != kNone && selected_ < combat_.combatant_count()) {
            reachable_cache_ = combat_.reachable_cells(selected_);
        }
    }

    engine::render::IRenderer& renderer_;
    engine::core::FileSystem& fs_;
    CombatScene combat_;
    engine::render::SpriteBatch batch_;
    engine::render::Camera2D camera_;
    CameraController camera_controller_;
    TextureProvider texture_provider_;
    std::string map_path_ = "assets:/maps/goblin_ambush.json";

    std::unordered_map<std::string, engine::render::TextureId> textures_;
    std::unordered_map<long long, std::string> tile_texture_key_;
    std::vector<TokenKind> token_kind_;
    engine::render::TextureId placeholder_tile_ = engine::render::kInvalidTexture;

    int grid_columns_ = 20;
    int grid_rows_ = 15;
    std::vector<std::size_t> turn_order_;
    std::size_t active_index_ = 0;
    int round_ = 1;
    std::size_t selected_ = kNone;
    std::optional<rules::Cell> hover_;
    std::vector<rules::Cell> reachable_cache_;
    bool enemy_turn_pending_ = false;
    bool battle_over_ = false;
    bool has_breakdown_ = false;
    rules::RollBreakdownDTO last_breakdown_;
    std::string status_ = "ЛКМ — выбор/ход, ПКМ — отмена, E — конец хода, WASD — камера";
    float mouse_x_ = 0.f;
    float mouse_y_ = 0.f;
    std::size_t renders_ = 0;
};

} // namespace game
