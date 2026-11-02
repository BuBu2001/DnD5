#include "core/file_system.h"

#include <fstream>
#include <sstream>

namespace engine::core {

static constexpr const char* kVirtualPrefix = "assets:/";

FileSystem::FileSystem(std::string root) : root_(std::move(root)) {}

Result<std::string> FileSystem::resolve(const std::string& virtual_path) const {
    const std::string prefix = kVirtualPrefix;
    if (virtual_path.rfind(prefix, 0) != 0) {
        return Result<std::string>::err(
            "fs: not a virtual path (must start with 'assets:/'): " + virtual_path);
    }
    const std::string rel = virtual_path.substr(prefix.size());
    if (rel.find("..") != std::string::npos) {
        return Result<std::string>::err("fs: path traversal is forbidden: " + virtual_path);
    }
#ifdef _WIN32
    std::string sep = "\\";
#else
    std::string sep = "/";
#endif
    std::string out = root_;
    if (!out.empty() && out.back() != '/' && out.back() != '\\') out += sep;
    // нормализация разделителей только на границе движка/ОС (DATA-E05)
    std::string normalized = rel;
    for (char& c : normalized) {
        if (c == '/') c = sep[0];
    }
    return Result<std::string>::ok(out + normalized);
}

Result<std::string> FileSystem::read_text(const std::string& virtual_path) const {
    auto physical = resolve(virtual_path);
    if (!physical) return Result<std::string>::err(physical.error_message());
    std::ifstream file(physical.value(), std::ios::binary);
    if (!file.is_open()) {
        return Result<std::string>::err("fs: file not found: " + virtual_path);
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    return Result<std::string>::ok(oss.str());
}

Result<std::vector<unsigned char>> FileSystem::read_bytes(const std::string& virtual_path) const {
    auto physical = resolve(virtual_path);
    if (!physical) return Result<std::vector<unsigned char>>::err(physical.error_message());
    std::ifstream file(physical.value(), std::ios::binary);
    if (!file.is_open()) {
        return Result<std::vector<unsigned char>>::err("fs: file not found: " + virtual_path);
    }
    std::vector<unsigned char> data{std::istreambuf_iterator<char>(file),
                                    std::istreambuf_iterator<char>()};
    return Result<std::vector<unsigned char>>::ok(std::move(data));
}

bool FileSystem::exists(const std::string& virtual_path) const {
    auto physical = resolve(virtual_path);
    if (!physical) return false;
    std::ifstream file(physical.value());
    return file.is_open();
}

} // namespace engine::core
