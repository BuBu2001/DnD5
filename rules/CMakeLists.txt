# rules/CMakeLists.txt — статическая библиотека правил, не зависит от движка (E-D11).
# Контроль изоляции: scripts/check_includes.py (E-D01, NFR-E10, п. 4.8.2).
add_library(rules STATIC
    abilities.cpp
    conditions.cpp
    dice_roller.cpp
    features.cpp
    items.cpp
    random_provider.cpp
)

target_include_directories(rules PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(rules PUBLIC cxx_std_20)
# Библиотека rules собирается и тестируется без SDL и OpenGL (E-D11):
# намеренно НЕ линкуется с целью engine.
