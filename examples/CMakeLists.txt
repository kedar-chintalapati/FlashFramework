add_executable(flash_version_example version/main.cpp)
target_link_libraries(flash_version_example PRIVATE flash::runtime)
flash_enable_warnings(flash_version_example)

add_executable(flash_raw_example raw/main.cpp)
target_link_libraries(flash_raw_example PRIVATE flash::runtime)
flash_enable_warnings(flash_raw_example)

add_executable(flash_hello hello/main.cpp)
target_link_libraries(flash_hello PRIVATE flash::flash)
flash_enable_warnings(flash_hello)

add_executable(flash_openapi_export openapi/main.cpp)
target_link_libraries(flash_openapi_export PRIVATE flash::flash)
flash_enable_warnings(flash_openapi_export)

add_executable(flash_blocking_example blocking/main.cpp)
target_link_libraries(flash_blocking_example PRIVATE flash::flash)
flash_enable_warnings(flash_blocking_example)

add_custom_target(
    flash_export_openapi
    COMMAND flash_openapi_export ${CMAKE_CURRENT_BINARY_DIR}/openapi.json
    DEPENDS flash_openapi_export
    COMMENT "Writing deterministic OpenAPI document"
)

if(FLASH_BUILD_TESTS)
    set(flash_exported_openapi ${CMAKE_CURRENT_BINARY_DIR}/exported-openapi.json)
    add_test(
        NAME flash.openapi_export
        COMMAND flash_openapi_export ${flash_exported_openapi}
    )
    add_test(
        NAME flash.openapi_json_validation
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/validate_openapi.py
                ${flash_exported_openapi}
                ${PROJECT_SOURCE_DIR}/tests/golden/openapi.json
    )
    add_test(
        NAME flash.openapi_external_validation
        COMMAND ${Python3_EXECUTABLE}
                ${PROJECT_SOURCE_DIR}/tests/external_openapi_validator.py
                ${flash_exported_openapi}
    )
    set_tests_properties(
        flash.openapi_json_validation flash.openapi_external_validation
        PROPERTIES DEPENDS flash.openapi_export
    )
    set_tests_properties(
        flash.openapi_external_validation
        PROPERTIES SKIP_RETURN_CODE 77
    )
endif()
