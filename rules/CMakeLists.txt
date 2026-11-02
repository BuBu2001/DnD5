# rules/CMakeLists.txt — статическая библиотека правил, не зависит от движка (E-D11)
add_library(rules STATIC
    abilities.cpp
    random_provider.cpp
    dice_roller.cpp
)

target_include_directories(rules PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(rules PUBLIC cxx_std_20)
# Проверка E-D01 в CI: в rules/ запрещены SDL.h, GL/gl.h, engine/* (п. 4.8.2)
