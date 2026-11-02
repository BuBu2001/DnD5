// engine/resource/texture_loader.cpp — декодирование PNG через stb_image.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
// Сторонний код (public domain) собирается без учёта наших -Werror-правил
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "stb_image.h"
#pragma GCC diagnostic pop

#include "resource/texture_loader.h"

namespace engine::resource {

core::Result<ImageRgba> TextureLoader::decode_png(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size == 0) return core::Result<ImageRgba>::err("пустой блок данных");
    int w = 0, h = 0, channels = 0;
    unsigned char* pixels = stbi_load_from_memory(data, static_cast<int>(size), &w, &h,
                                                  &channels, /*desired=*/4);
    if (pixels == nullptr) {
        return core::Result<ImageRgba>::err(std::string("stb_image: ") +
                                            stbi_failure_reason());
    }
    ImageRgba image;
    image.width = w;
    image.height = h;
    image.pixels.assign(pixels, pixels + static_cast<std::size_t>(w) * h * 4);
    stbi_image_free(pixels);
    return core::Result<ImageRgba>::ok(std::move(image));
}

core::Result<ImageRgba> TextureLoader::load_png(const std::string& virtual_path) {
    const auto bytes = fs_.read_bytes(virtual_path);
    if (!bytes.is_ok()) return core::Result<ImageRgba>::err(bytes.error_message());
    return decode_png(bytes.value().data(), bytes.value().size());
}

} // namespace engine::resource
