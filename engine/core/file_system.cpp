#include "core/file_system.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

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
    // Абсолютные пути и пути, зависящие от ОС, в данных запрещены (DATA-E05):
    // ведущий разделитель или имя диска Windows делают путь недействительным.
    if (!rel.empty() && (rel.front() == '/' || rel.front() == '\\')) {
        return Result<std::string>::err("fs: absolute path inside virtual root is forbidden: " +
                                        virtual_path);
    }
    if (rel.size() > 1 && rel[1] == ':') {
        return Result<std::string>::err("fs: drive letter is forbidden: " + virtual_path);
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

std::vector<std::string> FileSystem::list_directory(const std::string& virtual_dir) const {
    std::vector<std::string> out;
    const auto physical = resolve(virtual_dir);
    if (!physical) return out;
    const std::string base = physical.value();

#ifdef _WIN32
    WIN32_FIND_DATAA entry;
    HANDLE handle = FindFirstA((base + "\\*").c_str(), &entry);
    if (handle == INVALID_HANDLE_VALUE) return out;
    do {
        const std::string name = entry.cFileName;
        if (name == "." || name == "..") continue;
        const bool is_dir = (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        out.push_back(normalize_child(virtual_dir, name, is_dir));
    } while (FindNextA(handle, &entry));
    FindClose(handle);
#else
    DIR* dir = opendir(base.c_str());
    if (dir == nullptr) return out;
    while (const dirent* entry = readdir(dir)) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        struct stat st {};
        const std::string child = base + "/" + name;
        bool is_dir = false;
        if (stat(child.c_str(), &st) == 0) is_dir = S_ISDIR(st.st_mode);
        std::string vpath = virtual_dir;
        if (!vpath.empty() && vpath.back() != '/') vpath += "/";
        vpath += name;
        if (is_dir) vpath += "/";
        out.push_back(vpath);
    }
    closedir(dir);
#endif

    std::sort(out.begin(), out.end());
    return out;
}

} // namespace engine::core
