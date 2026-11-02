# tools/map_validator — проверка карт: сетка, слои, плитки, точки появления (M11).
add_executable(map_validator map_validator.cpp)
target_include_directories(map_validator PRIVATE ${CMAKE_SOURCE_DIR}/engine)
target_link_libraries(map_validator PRIVATE engine)
