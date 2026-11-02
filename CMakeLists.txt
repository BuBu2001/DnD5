# Корневой CMakeLists.txt — заготовка системы сборки (Приложение Г, п. Г.11 ТЗ DOC-04).
# Цели сборки: engine, rules, game, tests, tools (п. 4.1.1.1).
cmake_minimum_required(VERSION 3.20)
project(dnd_engine LANGUAGES CXX VERSION 0.4.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# NFR-E12: предупреждения как ошибки.
add_compile_options(-Wall -Wextra -Werror)

# --- Опции сборки ------------------------------------------------------------
# DND_WITH_SDL2=ON включает слой платформы/отрисовки/звука (этапы M1, M2, M9).
# По умолчанию SDL2 ищется в системе; при отсутствии цель game собирается в
# headless-варианте (без окна), а правила и ядро остаются полностью рабочими (E-D11).
option(DND_WITH_SDL2 "Собирать слой SDL2/OpenGL (окно, ввод, рендер, звук)" OFF)
option(DND_BUILD_TOOLS "Собирать вспомогательные инструменты tools/" ON)
option(DND_BUILD_BENCHMARKS "Собирать бенчмарки tests/benchmarks" ON)

if(DND_WITH_SDL2)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 IMPORTED_TARGET sdl2)
        pkg_check_modules(SDL2_MIXER IMPORTED_TARGET SDL2_mixer)
    endif()
    if(NOT SDL2_FOUND)
        find_package(SDL2 QUIET)
    endif()
    if(SDL2_FOUND)
        message(STATUS "SDL2 найден: слой платформы и отрисовки включён")
        if(NOT DEFINED SDL2_LIBRARIES AND TARGET SDL2::SDL2)
            set(SDL2_LIBRARIES SDL2::SDL2)
        endif()
        if(NOT DEFINED SDL2_MIXER_LIBRARIES AND TARGET SDL2_mixer::SDL2_mixer)
            set(SDL2_MIXER_LIBRARIES SDL2_mixer::SDL2_mixer)
        endif()
    else()
        message(WARNING "SDL2 не найден — DND_WITH_SDL2 принудительно выключен")
        set(DND_WITH_SDL2 OFF)
    endif()
endif()

# Сторонние библиотеки-инструменты (таблица 4.16). Подключаются по мере
# добавления в third_party/; на V0.4 используются системные SDL2 и собственный
# разбор JSON (см. docs/decisions.md, DR-01).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/CMakeLists.txt)
    add_subdirectory(third_party)
endif()

add_subdirectory(engine)
add_subdirectory(rules)
add_subdirectory(game)

enable_testing()
add_subdirectory(tests)

if(DND_BUILD_TOOLS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/CMakeLists.txt)
    add_subdirectory(tools)
endif()
