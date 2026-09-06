add_library(flash_benchmark_options INTERFACE)
target_compile_features(flash_benchmark_options INTERFACE cxx_std_26)
target_link_libraries(flash_benchmark_options INTERFACE boost::boost)
target_compile_definitions(flash_benchmark_options INTERFACE BOOST_ERROR_CODE_HEADER_ONLY)

if(WIN32)
    target_link_libraries(flash_benchmark_options INTERFACE ws2_32 mswsock)
endif()

if(FLASH_NATIVE_OPTIMIZATION)
    target_compile_options(flash_benchmark_options INTERFACE
        $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-O3;-march=native>
    )
endif()

add_executable(flash_benchmark_raw end_to_end/flash_raw_health.cpp)
target_link_libraries(flash_benchmark_raw PRIVATE flash::runtime flash_benchmark_options)
flash_enable_warnings(flash_benchmark_raw)

add_executable(flash_benchmark_beast baselines/beast/health_server.cpp)
target_link_libraries(flash_benchmark_beast PRIVATE flash_benchmark_options)
flash_enable_warnings(flash_benchmark_beast)

add_executable(flash_benchmark_manual baselines/beast/suite_server.cpp)
target_link_libraries(flash_benchmark_manual PRIVATE flash::flash flash_benchmark_options)
flash_enable_warnings(flash_benchmark_manual)

add_executable(flash_benchmark_request_view micro/request_view.cpp)
target_link_libraries(flash_benchmark_request_view PRIVATE flash::runtime flash_benchmark_options)
flash_enable_warnings(flash_benchmark_request_view)

add_executable(flash_benchmark_core micro/core.cpp)
target_link_libraries(flash_benchmark_core PRIVATE flash::flash flash_benchmark_options)
flash_enable_warnings(flash_benchmark_core)

add_executable(flash_benchmark_allocations micro/allocations.cpp)
target_link_libraries(flash_benchmark_allocations PRIVATE flash::flash flash_benchmark_options)
flash_enable_warnings(flash_benchmark_allocations)

add_executable(flash_benchmark_load client/load.cpp)
target_link_libraries(flash_benchmark_load PRIVATE flash_benchmark_options)
flash_enable_warnings(flash_benchmark_load)

add_executable(flash_benchmark_typed end_to_end/flash_typed_suite.cpp)
target_link_libraries(flash_benchmark_typed PRIVATE flash::flash flash_benchmark_options)
flash_enable_warnings(flash_benchmark_typed)

function(flash_add_synthetic_routes route_count)
    set(generated_directory ${CMAKE_CURRENT_BINARY_DIR}/generated)
    file(MAKE_DIRECTORY ${generated_directory})
    set(source ${generated_directory}/routes_${route_count}.cpp)
    file(WRITE ${source}
        "#include <flash/routing/matcher.hpp>\n\n"
        "#include <string_view>\n\n"
        "namespace synthetic_api {\n\n"
    )
    math(EXPR last_route "${route_count} - 1")
    foreach(route_index RANGE 0 ${last_route})
        file(APPEND ${source}
            "[[=flash::get(\"/synthetic/${route_index}\")]] "
            "int route_${route_index}();\n"
        )
    endforeach()
    file(APPEND ${source}
        "\n} // namespace synthetic_api\n\n"
        "int main(int argument_count, char** arguments) {\n"
        "    const std::string_view target{\n"
        "        argument_count > 1 ? arguments[1] : \"/synthetic/${last_route}\"};\n"
        "    const auto result = flash::routing::match_api<^^synthetic_api>(\n"
        "        flash::http_method::get, target);\n"
        "    return result.outcome == flash::routing::match_outcome::found &&\n"
        "                   result.endpoint_index == ${last_route}\n"
        "               ? 0\n"
        "               : 1;\n"
        "}\n"
    )

    set(target flash_synthetic_routes_${route_count})
    add_executable(${target} EXCLUDE_FROM_ALL ${source})
    target_link_libraries(${target} PRIVATE flash::meta)
    target_compile_options(${target} PRIVATE
        $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-ftemplate-depth=2048>
    )
    flash_enable_warnings(${target})
endfunction()

flash_add_synthetic_routes(1)
flash_add_synthetic_routes(10)
flash_add_synthetic_routes(100)
flash_add_synthetic_routes(1000)
