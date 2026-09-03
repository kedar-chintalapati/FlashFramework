add_executable(flash_version_test unit/version_test.cpp)
target_link_libraries(flash_version_test PRIVATE flash::runtime)
flash_enable_warnings(flash_version_test)
add_test(NAME flash.version COMMAND flash_version_test)

add_executable(flash_reflection_test unit/reflection_test.cpp)
target_link_libraries(flash_reflection_test PRIVATE flash::flash)
flash_enable_warnings(flash_reflection_test)
add_test(NAME flash.reflection COMMAND flash_reflection_test)

function(flash_add_compile_fail_test name source diagnostic)
    add_test(
        NAME ${name}
        COMMAND
            ${CMAKE_COMMAND}
            -DFLASH_TEST_COMPILER=${CMAKE_CXX_COMPILER}
            -DFLASH_TEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/${source}
            -DFLASH_TEST_INCLUDE=${PROJECT_SOURCE_DIR}/include
            -DFLASH_EXPECTED_DIAGNOSTIC=${diagnostic}
            -P ${PROJECT_SOURCE_DIR}/cmake/ExpectCompileFailure.cmake
    )
endfunction()

flash_add_compile_fail_test(
    flash.compile_fail.missing_route
    compile_fail/missing_route_annotation.cpp
    FLASH-E003
)

flash_add_compile_fail_test(
    flash.compile_fail.duplicate_route
    compile_fail/duplicate_route_annotation.cpp
    FLASH-E003
)

