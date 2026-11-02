# tools/CMakeLists.txt — вспомогательные инструменты (этап M11, DOC-04 п. 4.1.1.1).
#   json_validator    — проверка данных по схемам (FR-E47, FR-E48, DATA-E02)
#   schema_gen        — генерация справочника данных и C++-констант из схем (DR-02)
#   map_validator     — проверка карт: размеры слоёв, ключи плиток, точки появления
#   asset_packer      — упаковщик атласов спрайтов (E-D05, FR-E21)
add_subdirectory(json_validator)
add_subdirectory(schema_gen)
add_subdirectory(map_validator)
add_subdirectory(asset_packer)
