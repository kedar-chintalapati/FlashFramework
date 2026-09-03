add_executable(flash_version_example version/main.cpp)
target_link_libraries(flash_version_example PRIVATE flash::runtime)
flash_enable_warnings(flash_version_example)

add_executable(flash_raw_example raw/main.cpp)
target_link_libraries(flash_raw_example PRIVATE flash::runtime)
flash_enable_warnings(flash_raw_example)

