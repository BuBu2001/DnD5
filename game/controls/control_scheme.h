// game/controls/control_scheme.h — схема управления игрой (docs/controls.md).
// Игровой слой задаёт семантику действий; движок предоставляет только маппинг
// (engine/platform/input_actions.h, FR-E03) — направление зависимостей п. 3.3.2.
#pragma once

#include <string>

#include "platform/input.h"
#include "platform/input_actions.h"

namespace game {

// Имена игровых действий (идентификаторы для InputActions и тестов)
namespace actions {
inline constexpr const char* kCameraLeft = "camera_left";     // A / ←
inline constexpr const char* kCameraRight = "camera_right";   // D / →
inline constexpr const char* kCameraUp = "camera_up";         // W / ↑
inline constexpr const char* kCameraDown = "camera_down";     // S / ↓
inline constexpr const char* kZoomIn = "zoom_in";             // колесо вверх / +
inline constexpr const char* kZoomOut = "zoom_out";           // колесо вниз / −
inline constexpr const char* kSelect = "select";              // ЛКМ
inline constexpr const char* kCommand = "command";            // ЛКМ по цели/клетке
inline constexpr const char* kCancel = "cancel";              // ПКМ / Q / Esc-меню
inline constexpr const char* kConfirm = "confirm";            // Enter / Space
inline constexpr const char* kEndTurn = "end_turn";           // E
inline constexpr const char* kNextUnit = "next_unit";         // Tab
inline constexpr const char* kSelectSlot1 = "select_slot_1";  // 1
inline constexpr const char* kSelectSlot2 = "select_slot_2";  // 2
inline constexpr const char* kSelectSlot3 = "select_slot_3";  // 3
inline constexpr const char* kSelectSlot4 = "select_slot_4";  // 4
inline constexpr const char* kCharacterSheet = "character_sheet"; // C
inline constexpr const char* kInventory = "inventory";            // I
inline constexpr const char* kCombatLog = "combat_log";           // L
inline constexpr const char* kMap = "map";                        // M
inline constexpr const char* kQuickSave = "quick_save";           // F10 — V1 (слот F10)
} // namespace actions

// Сборка маппинга по схеме docs/controls.md. F1/F2/+/− обрабатывает главный
// цикл напрямую (панель отладки, музыка) — в действия не входят.
inline void build_control_scheme(engine::platform::InputActions& out) {
    using engine::platform::Key;
    using engine::platform::MouseButton;

    out.bind_key(actions::kCameraLeft, Key::A);
    out.bind_key(actions::kCameraLeft, Key::Left);
    out.bind_key(actions::kCameraRight, Key::D);
    out.bind_key(actions::kCameraRight, Key::Right);
    out.bind_key(actions::kCameraUp, Key::W);
    out.bind_key(actions::kCameraUp, Key::Up);
    out.bind_key(actions::kCameraDown, Key::S);
    out.bind_key(actions::kCameraDown, Key::Down);

    out.bind_wheel(actions::kZoomIn, +1);
    out.bind_wheel(actions::kZoomOut, -1);

    out.bind_mouse(actions::kSelect, MouseButton::Left);
    out.bind_mouse(actions::kCommand, MouseButton::Left);
    out.bind_mouse(actions::kCancel, MouseButton::Right);
    out.bind_key(actions::kCancel, Key::Q);

    out.bind_key(actions::kConfirm, Key::Enter);
    out.bind_key(actions::kConfirm, Key::Space);
    out.bind_key(actions::kEndTurn, Key::E);
    out.bind_key(actions::kNextUnit, Key::Tab);

    out.bind_key(actions::kSelectSlot1, Key::Digit1);
    out.bind_key(actions::kSelectSlot2, Key::Digit2);
    out.bind_key(actions::kSelectSlot3, Key::Digit3);
    out.bind_key(actions::kSelectSlot4, Key::Digit4);

    out.bind_key(actions::kCharacterSheet, Key::C);
    out.bind_key(actions::kInventory, Key::I);
    out.bind_key(actions::kCombatLog, Key::L);
    out.bind_key(actions::kMap, Key::M);
}

} // namespace game
