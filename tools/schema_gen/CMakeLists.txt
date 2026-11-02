# tools/schema_gen — генерация справочника данных и C++-констант по схемам (M11).
add_executable(schema_gen schema_gen.cpp)
target_include_directories(schema_gen PRIVATE ${CMAKE_SOURCE_DIR}/engine)
target_link_libraries(schema_gen PRIVATE engine)
