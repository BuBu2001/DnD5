// engine/resource/texture_loader.h — загрузка растровых текстур (PNG) в RGBA.
// Декодирование — stb_image (third_party/stb, public domain, таблица 4.16).
// Движок не знает семантики изображений (E-D01): на выходе только пиксели.
#pragma once

#include <cstdint>
#include <vector>

#include "core/file_system.h"
#include "core/result.h"

namespace engine::resource {

struct ImageRgba {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels; // width*height*4, RGBA
};

class TextureLoader {
public:
    explicit TextureLoader(core::FileSystem& fs) : fs_(fs) {}

    // Читает PNG по виртуальному пути (assets:/textures/…) и декодирует в RGBA.
    // Ошибка (нет файла, не PNG) — Result с описанием, без исключений (NFR-E09).
    core::Result<ImageRgba> load_png(const std::string& virtual_path);

    // Чистая функция декодирования блока памяти (тестируется без ФС)
    static core::Result<ImageRgba> decode_png(const std::uint8_t* data, std::size_t size);

private:
    core::FileSystem& fs_;
};

} // namespace engine::resource
