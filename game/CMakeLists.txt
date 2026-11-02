# game/CMakeLists.txt — цель игры: связывает engine и rules (game → engine, game → rules).
# Адаптеры имеют право знать оба слоя (п. 3.3.1); расчётные формулы правил в game
# запрещены — только вызовы rules::* (п. 3.3.2, «Запрещено» для Game Layer).
add_library(game_logic STATIC
    adapters/rules_adapter.cpp
    save/combat_save_state.cpp
    ui_widgets/combat_widgets.cpp
    ui_widgets/dice_breakdown_panel.cpp
)
target_include_directories(game_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(game_logic PUBLIC engine rules)

add_executable(game main.cpp)
target_link_libraries(game PRIVATE game_logic)
