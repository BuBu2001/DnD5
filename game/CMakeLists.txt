# game/CMakeLists.txt — цель игры: связывает engine и rules (game → engine, game → rules)
add_executable(game main.cpp)
target_link_libraries(game PRIVATE engine rules)
