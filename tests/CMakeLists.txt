# tests/CMakeLists.txt — модульные и интеграционные тесты (раздел 8 ТЗ DOC-04)
add_executable(tests main.cpp)
target_link_libraries(tests PRIVATE engine rules)

add_test(NAME unit_tests COMMAND tests)
