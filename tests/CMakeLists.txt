# tests/CMakeLists.txt — модульные, интеграционные и golden-тесты (раздел 8 ТЗ DOC-04).
# Цели:
#   tests_rules        — модульные испытания правил (таблица 8.2), покрытие ≥ 90 % (NFR-E08)
#   tests_engine       — интеграционные испытания подсистем движка (таблица 8.3)
#   tests_integration  — сквозные испытания: данные, сохранения, адаптеры, вертикальный срез
#   tests_golden       — golden-тесты с фиксированным seed (подраздел 8.4)
#   tests (устаревшая единая цель) — сохранена до завершения переноса кейсов

set(DND_TESTS_INCLUDES
    ${CMAKE_SOURCE_DIR}/engine
    ${CMAKE_SOURCE_DIR}/game
    ${CMAKE_SOURCE_DIR}/game/combat
    ${CMAKE_SOURCE_DIR}/tests
)

# Каталог assets/ для испытаний ресурсов и сохранений (DATA-E01)
set(DND_ASSETS_DIR ${CMAKE_SOURCE_DIR}/assets)
set(DND_GOLDEN_PATH ${CMAKE_SOURCE_DIR}/tests/golden/golden_values.txt)

add_executable(tests_rules test_main.cpp rules_tests/rules_tests.cpp)
target_include_directories(tests_rules PRIVATE ${DND_TESTS_INCLUDES})
target_link_libraries(tests_rules PRIVATE rules)
add_test(NAME rules_unit_tests COMMAND tests_rules)

add_executable(tests_engine test_main.cpp engine_tests/engine_tests.cpp)
target_include_directories(tests_engine PRIVATE ${DND_TESTS_INCLUDES})
target_compile_definitions(tests_engine PRIVATE DND_ASSETS_ROOT="${DND_ASSETS_DIR}")
target_link_libraries(tests_engine PRIVATE engine)
add_test(NAME engine_tests COMMAND tests_engine)

# Испытания слоя платформы и отрисовки без окна и драйвера (E-D11): формула (1),
# кадр ввода, адаптация в UI, шрифтовой атлас, загрузчик функций OpenGL, пакеты спрайтов.
add_executable(tests_platform test_main.cpp engine_tests/platform_tests.cpp)
target_include_directories(tests_platform PRIVATE ${DND_TESTS_INCLUDES})
target_link_libraries(tests_platform PRIVATE engine)
add_test(NAME platform_tests COMMAND tests_platform)

add_executable(tests_integration test_main.cpp integration_tests/integration_tests.cpp)
target_include_directories(tests_integration PRIVATE ${DND_TESTS_INCLUDES})
target_compile_definitions(tests_integration PRIVATE DND_ASSETS_ROOT="${DND_ASSETS_DIR}")
target_link_libraries(tests_integration PRIVATE game_logic)
add_test(NAME integration_tests COMMAND tests_integration)

add_executable(tests_golden test_main.cpp golden/golden_tests.cpp)
target_include_directories(tests_golden PRIVATE ${DND_TESTS_INCLUDES})
target_compile_definitions(tests_golden PRIVATE DND_GOLDEN_FILE="${DND_GOLDEN_PATH}")
target_link_libraries(tests_golden PRIVATE rules)
add_test(NAME golden_tests COMMAND tests_golden)

# Единая цель-обёртка: прогон всех тестов одной командой (удобство разработчика)
add_custom_target(check_all
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS tests_rules tests_engine tests_platform tests_integration tests_golden
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# --- Контрольные замерa производительности (таблица 8.4, NFR-E02…NFR-E06) ------
# Цель не регистрируется в ctest: замер зависит от нагрузки машины CI.
# Запуск вручную: ./build/tests/benchmarks/benchmarks --assets ../assets [--strict]
if(DND_BUILD_BENCHMARKS)
    add_executable(benchmarks benchmarks/benchmarks.cpp)
    target_include_directories(benchmarks PRIVATE ${DND_TESTS_INCLUDES})
    target_link_libraries(benchmarks PRIVATE engine rules)
endif()
