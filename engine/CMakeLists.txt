# engine/CMakeLists.txt — библиотека движка (цель engine, п. 4.1.1.1).
# Движок не содержит игровой логики D&D (п. 1.5, E-D01): в engine/ запрещены
# знания об AC, спасбросках, состояниях, ячейках заклинаний (проверка — CI, NFR-E10).
add_library(engine STATIC
    core/log.cpp
    core/config.cpp
    core/debug_overlay.cpp
    core/file_system.cpp
    platform/main_loop.cpp
    platform/input.cpp
    platform/window.cpp
    render/font_atlas.cpp
    render/gl_functions.cpp
    resource/json.cpp
    resource/schema_validator.cpp
    resource/schema_registry.cpp
    resource/json_loader.cpp
    scene/save_slot.cpp
    scene/save_system.cpp
    ui/control.cpp
    ui/layout.cpp
    ui/widgets.cpp
)

target_include_directories(engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(engine PUBLIC cxx_std_20)

# platform/window.cpp (формула (1)), render/font_atlas.cpp и render/gl_functions.cpp
# не требуют SDL2: они собираются всегда и тестируются headless (E-D11).
# Слои, требующие SDL2/OpenGL (этапы M1, M2, M9), подключаются опционально,
# чтобы ядро и правила собирались и тестировались в headless-среде.
if(DND_WITH_SDL2)
    target_sources(engine PRIVATE
        platform/sdl_window.cpp
        platform/sdl_input.cpp
        render/gl_backend.cpp
        render/gl_sprite_shader.cpp
        audio/sdl_audio.cpp
    )
    target_compile_definitions(engine PUBLIC DND_ENGINE_WITH_SDL2=1)
    # libGL НЕ линкуется: функции OpenGL загружаются динамически (render/gl_functions.h),
    # поэтому сборка проходит и в среде без графического драйвера.
    target_link_libraries(engine PUBLIC ${SDL2_LIBRARIES} ${SDL2_MIXER_LIBRARIES})
    target_include_directories(engine PUBLIC ${SDL2_INCLUDE_DIRS})
endif()
