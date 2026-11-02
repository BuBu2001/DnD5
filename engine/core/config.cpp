#include "core/config.h"

#include <fstream>
#include <sstream>

namespace engine::core {

static std::string trim(const std::string& s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

Result<bool> Config::load_from_string(const std::string& text) {
    std::istringstream stream(text);
    std::string line;
    int line_no = 0;
    while (std::getline(stream, line)) {
        ++line_no;
        const auto comment = line.find('#');
        if (comment != std::string::npos) line = line.substr(0, comment);
        line = trim(line);
        if (line.empty()) continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            return Result<bool>::err("config: line " + std::to_string(line_no) +
                                     ": expected 'key = value', got '" + line + "'");
        }
        values_[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }
    return Result<bool>::ok(true);
}

Result<bool> Config::load_from_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return Result<bool>::err("config: cannot open file: " + path);
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    return load_from_string(oss.str());
}

void Config::set(const std::string& key, const std::string& value) {
    values_[key] = value;
}

std::string Config::get_string(const std::string& key, const std::string& def) const {
    auto it = values_.find(key);
    return it == values_.end() ? def : it->second;
}

int Config::get_int(const std::string& key, int def) const {
    auto it = values_.find(key);
    if (it == values_.end()) return def;
    try {
        return std::stoi(it->second);
    } catch (...) {
        return def; // некорректное значение в конфигурации — деградация (п. 4.2.3)
    }
}

double Config::get_double(const std::string& key, double def) const {
    auto it = values_.find(key);
    if (it == values_.end()) return def;
    try {
        return std::stod(it->second);
    } catch (...) {
        return def;
    }
}

bool Config::get_bool(const std::string& key, bool def) const {
    auto it = values_.find(key);
    if (it == values_.end()) return def;
    if (it->second == "true" || it->second == "1") return true;
    if (it->second == "false" || it->second == "0") return false;
    return def;
}

} // namespace engine::core
