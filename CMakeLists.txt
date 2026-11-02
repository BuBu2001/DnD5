# Заготовка системы сборки (Приложение Г, п. Г.11 ТЗ DOC-04)
cmake_minimum_required(VERSION 3.20)
project(dnd_engine LANGUAGES CXX VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Werror)

# third_party подключается по мере добавления библиотек (таблица 4.16)
# add_subdirectory(third_party)

add_subdirectory(engine)
add_subdirectory(rules)
add_subdirectory(game)

enable_testing()
add_subdirectory(tests)
