// engine/resource/resource_manager.h — интерфейс менеджера ресурсов (FR-E45…FR-E54).
// Заглушка до этапа M2/M3: кэш, загрузка JSON/TTF/PNG/OGG, валидация по схемам.
// Ошибка одного ресурса не приводит к аварийному завершению (п. 4.1.4.4).
#pragma once

#include <string>

#include "core/result.h"

namespace engine::resource {

class IResourceManager {
public:
    virtual ~IResourceManager() = default;
    // Возвращает содержимое текстового ресурса по виртуальному пути assets:/...
    virtual Result<std::string> load_text(const std::string& virtual_path) = 0;
    virtual void invalidate(const std::string& virtual_path) = 0; // «горячая» перезагрузка — V1
};

} // namespace engine::resource
