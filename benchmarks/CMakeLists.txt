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

add_executable(flash_benchmark_load client/load.cpp)
target_link_libraries(flash_benchmark_load PRIVATE flash_benchmark_options)
flash_enable_warnings(flash_benchmark_load)

add_executable(flash_benchmark_typed end_to_end/flash_typed_suite.cpp)
target_link_libraries(flash_benchmark_typed PRIVATE flash::flash flash_benchmark_options)
flash_enable_warnings(flash_benchmark_typed)
