// engine/core/file_system.h — виртуальные пути assets:/<каталог>/<файл>
// (FR-E07, DATA-E05: абсолютные пути и пути, зависящие от ОС, в данных запрещены).
#pragma once

#include <string>
#include <vector>

#include "core/result.h"

namespace engine::core {

class FileSystem {
public:
    // root — физический каталог, соответствующий префиксу "assets:/"
    explicit FileSystem(std::string root);

    // Преобразование виртуального пути в физический. Возвращает ошибку,
    // если путь не начинается с "assets:/" или содержит "..".
    Result<std::string> resolve(const std::string& virtual_path) const;

    Result<std::string> read_text(const std::string& virtual_path) const;
    Result<std::vector<unsigned char>> read_bytes(const std::string& virtual_path) const;
    bool exists(const std::string& virtual_path) const;

    // Перечисление каталога: возвращает виртуальные пути assets:/… в алфавитном
    // порядке (детерминизм, NFR-E09). Пустой список — каталог отсутствует или пуст.
    // Используется реестром схем (FR-E47) и упаковщиком ассетов (tools/asset_packer).
    std::vector<std::string> list_directory(const std::string& virtual_dir) const;

    const std::string& root() const { return root_; }

private:
    std::string root_;
};

} // namespace engine::core
