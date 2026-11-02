// engine/core/config.h — конфигурация (FR-E09). Формат: ключ = значение, UTF-8 без BOM.
#pragma once

#include <string>
#include <unordered_map>

#include "core/result.h"

namespace engine::core {

class Config {
public:
    Result<bool> load_from_file(const std::string& path);
    Result<bool> load_from_string(const std::string& text);

    std::string get_string(const std::string& key, const std::string& def = {}) const;
    int get_int(const std::string& key, int def = 0) const;
    double get_double(const std::string& key, double def = 0.0) const;
    bool get_bool(const std::string& key, bool def = false) const;

    void set(const std::string& key, const std::string& value);

private:
    std::unordered_map<std::string, std::string> values_;
};

} // namespace engine::core
