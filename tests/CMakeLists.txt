add_executable(flash_version_test unit/version_test.cpp)
target_link_libraries(flash_version_test PRIVATE flash::runtime)
flash_enable_warnings(flash_version_test)
add_test(NAME flash.version COMMAND flash_version_test)

add_executable(flash_reflection_test unit/reflection_test.cpp)
target_link_libraries(flash_reflection_test PRIVATE flash::flash)
flash_enable_warnings(flash_reflection_test)
add_test(NAME flash.reflection COMMAND flash_reflection_test)

