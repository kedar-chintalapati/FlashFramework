add_executable(flash_version_test unit/version_test.cpp)
target_link_libraries(flash_version_test PRIVATE flash::runtime)
flash_enable_warnings(flash_version_test)
add_test(NAME flash.version COMMAND flash_version_test)

add_executable(flash_reflection_test unit/reflection_test.cpp)
target_link_libraries(flash_reflection_test PRIVATE flash::flash)
flash_enable_warnings(flash_reflection_test)
add_test(NAME flash.reflection COMMAND flash_reflection_test)

add_executable(flash_http_types_test unit/http_types_test.cpp)
target_link_libraries(flash_http_types_test PRIVATE flash::runtime)
flash_enable_warnings(flash_http_types_test)
add_test(NAME flash.http_types COMMAND flash_http_types_test)

add_executable(flash_routing_test unit/routing_test.cpp)
target_link_libraries(flash_routing_test PRIVATE flash::meta)
flash_enable_warnings(flash_routing_test)
add_test(NAME flash.routing COMMAND flash_routing_test)

add_executable(flash_scalar_binding_test unit/scalar_binding_test.cpp)
target_link_libraries(flash_scalar_binding_test PRIVATE flash::meta)
flash_enable_warnings(flash_scalar_binding_test)
add_test(NAME flash.scalar_binding COMMAND flash_scalar_binding_test)

add_executable(flash_scalar_allocation_test unit/scalar_allocation_test.cpp)
target_link_libraries(flash_scalar_allocation_test PRIVATE flash::meta)
flash_enable_warnings(flash_scalar_allocation_test)
add_test(NAME flash.scalar_allocation COMMAND flash_scalar_allocation_test)

add_executable(flash_typed_dispatch_test unit/typed_dispatch_test.cpp)
target_link_libraries(flash_typed_dispatch_test PRIVATE flash::flash)
flash_enable_warnings(flash_typed_dispatch_test)
add_test(NAME flash.typed_dispatch COMMAND flash_typed_dispatch_test)

add_executable(flash_json_schema_test unit/json_schema_test.cpp)
target_link_libraries(flash_json_schema_test PRIVATE flash::meta)
flash_enable_warnings(flash_json_schema_test)
add_test(NAME flash.json_schema COMMAND flash_json_schema_test)

add_executable(flash_json_read_test unit/json_read_test.cpp)
target_link_libraries(flash_json_read_test PRIVATE flash::meta)
flash_enable_warnings(flash_json_read_test)
add_test(NAME flash.json_read COMMAND flash_json_read_test)

add_executable(flash_openapi_schema_test unit/openapi_schema_test.cpp)
target_link_libraries(flash_openapi_schema_test PRIVATE flash::flash)
flash_enable_warnings(flash_openapi_schema_test)
add_test(NAME flash.openapi_schema COMMAND flash_openapi_schema_test)

add_executable(flash_openapi_document_test unit/openapi_document_test.cpp)
target_link_libraries(flash_openapi_document_test PRIVATE flash::flash)
flash_enable_warnings(flash_openapi_document_test)
add_test(NAME flash.openapi_document COMMAND flash_openapi_document_test)

add_executable(flash_middleware_test unit/middleware_test.cpp)
target_link_libraries(flash_middleware_test PRIVATE flash::flash)
flash_enable_warnings(flash_middleware_test)
add_test(NAME flash.middleware COMMAND flash_middleware_test)


add_executable(flash_raw_server_test integration/raw_server_test.cpp)
target_link_libraries(flash_raw_server_test PRIVATE flash::runtime)
flash_enable_warnings(flash_raw_server_test)
add_test(NAME flash.raw_server COMMAND flash_raw_server_test)
set_tests_properties(flash.raw_server PROPERTIES TIMEOUT 15)

add_executable(flash_raw_concurrency_test integration/raw_concurrency_test.cpp)
target_link_libraries(flash_raw_concurrency_test PRIVATE flash::runtime)
flash_enable_warnings(flash_raw_concurrency_test)
add_test(NAME flash.raw_concurrency COMMAND flash_raw_concurrency_test)
set_tests_properties(flash.raw_concurrency PROPERTIES TIMEOUT 20)

add_executable(flash_shutdown_test integration/shutdown_test.cpp)
target_link_libraries(flash_shutdown_test PRIVATE flash::runtime)
flash_enable_warnings(flash_shutdown_test)
add_test(NAME flash.shutdown COMMAND flash_shutdown_test)
set_tests_properties(flash.shutdown PROPERTIES TIMEOUT 15)

add_executable(flash_timeout_test integration/timeout_test.cpp)
target_link_libraries(flash_timeout_test PRIVATE flash::runtime)
flash_enable_warnings(flash_timeout_test)
add_test(NAME flash.timeouts COMMAND flash_timeout_test)
set_tests_properties(flash.timeouts PROPERTIES TIMEOUT 15)

add_executable(flash_typed_server_test integration/typed_server_test.cpp)
target_link_libraries(flash_typed_server_test PRIVATE flash::flash)
flash_enable_warnings(flash_typed_server_test)
add_test(NAME flash.typed_server COMMAND flash_typed_server_test)
set_tests_properties(flash.typed_server PROPERTIES TIMEOUT 20)





function(flash_add_compile_fail_test name source diagnostic)
    get_target_property(
        flash_boost_include_directory
        Boost::headers
        INTERFACE_INCLUDE_DIRECTORIES
    )
    add_test(
        NAME ${name}
        COMMAND
            ${CMAKE_COMMAND}
            -DFLASH_TEST_COMPILER=${CMAKE_CXX_COMPILER}
            -DFLASH_TEST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/${source}
            -DFLASH_TEST_INCLUDE=${PROJECT_SOURCE_DIR}/include
            -DFLASH_TEST_BOOST_INCLUDE=${flash_boost_include_directory}
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
    flash.compile_fail.unbound_path
    compile_fail/unbound_path.cpp
    FLASH-E302
)

flash_add_compile_fail_test(
    flash.compile_fail.multiple_parameter_sources
    compile_fail/multiple_parameter_sources.cpp
    FLASH-E301
)


flash_add_compile_fail_test(
    flash.compile_fail.invalid_route
    compile_fail/invalid_route.cpp
    FLASH-E200
)

flash_add_compile_fail_test(
    flash.compile_fail.ambiguous_route
    compile_fail/ambiguous_route.cpp
    FLASH-E201
)


flash_add_compile_fail_test(
    flash.compile_fail.duplicate_route
    compile_fail/duplicate_route_annotation.cpp
    FLASH-E003
)

flash_add_compile_fail_test(
    flash.compile_fail.duplicate_json_alias
    compile_fail/duplicate_json_alias.cpp
    FLASH-E404
)

flash_add_compile_fail_test(
    flash.compile_fail.multiple_body_parameters
    compile_fail/multiple_body_parameters.cpp
    FLASH-E305
)

flash_add_compile_fail_test(
    flash.compile_fail.incomplete_error_mapping
    compile_fail/incomplete_error_mapping.cpp
    FLASH-E502
)

flash_add_compile_fail_test(
    flash.compile_fail.openapi_component_collision
    compile_fail/openapi_component_collision.cpp
    FLASH-E602
)

flash_add_compile_fail_test(
    flash.compile_fail.documentation_route_collision
    compile_fail/documentation_route_collision.cpp
    FLASH-E606
)

flash_add_compile_fail_test(
    flash.compile_fail.openapi_operation_collision
    compile_fail/openapi_operation_collision.cpp
    FLASH-E605
)

flash_add_compile_fail_test(
    flash.compile_fail.unregistered_state
    compile_fail/unregistered_state.cpp
    FLASH-E307
)

flash_add_compile_fail_test(
    flash.compile_fail.duplicate_state
    compile_fail/duplicate_state.cpp
    FLASH-E308
)

flash_add_compile_fail_test(
    flash.compile_fail.invalid_middleware
    compile_fail/invalid_middleware.cpp
    FLASH-E700
)
