# engine/CMakeLists.txt — библиотека движка (цель engine, п. 4.1.1.1)
add_library(engine STATIC
    core/log.cpp
    core/config.cpp
    core/file_system.cpp
    platform/main_loop.cpp
)

target_include_directories(engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(engine PUBLIC cxx_std_20)
