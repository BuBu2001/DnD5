# tools/json_validator — проверка данных по схемам в CI (этап M11, FR-E47, FR-E48).
add_executable(json_validator json_validator.cpp)
target_include_directories(json_validator PRIVATE ${CMAKE_SOURCE_DIR}/engine)
target_link_libraries(json_validator PRIVATE engine)
