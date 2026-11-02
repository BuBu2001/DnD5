# tools/asset_packer — упаковщик атласов спрайтов (M11, E-D05, NFR-E01).
add_executable(asset_packer asset_packer.cpp)
target_include_directories(asset_packer PRIVATE ${CMAKE_SOURCE_DIR}/engine)
target_link_libraries(asset_packer PRIVATE engine)
