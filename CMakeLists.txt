cmake_minimum_required(VERSION 4.4)

project(
    flash
    VERSION 0.1.0
    DESCRIPTION "Experimental C++26 compile-time REST framework"
    LANGUAGES CXX
)

option(FLASH_BUILD_TESTS "Build Flash tests" ON)
option(FLASH_BUILD_EXAMPLES "Build Flash examples" ON)
option(FLASH_BUILD_BENCHMARKS "Build Flash benchmarks" OFF)
option(FLASH_WARNINGS_AS_ERRORS "Treat Flash warnings as errors" OFF)
option(FLASH_NATIVE_OPTIMIZATION "Use local CPU tuning for benchmark targets" OFF)
option(FLASH_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)

if(FLASH_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR WIN32)
        message(FATAL_ERROR "Flash sanitizer builds require GCC on Linux")
    endif()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

find_package(Boost 1.91 CONFIG REQUIRED)

add_library(flash_runtime STATIC
    src/runtime/errors.cpp
    src/runtime/version.cpp
)
add_library(flash::runtime ALIAS flash_runtime)

target_compile_features(flash_runtime PUBLIC cxx_std_26)
target_include_directories(flash_runtime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(flash_runtime PUBLIC boost::boost)
target_compile_definitions(flash_runtime PUBLIC BOOST_ERROR_CODE_HEADER_ONLY)

if(WIN32)
    target_link_libraries(flash_runtime PUBLIC ws2_32 mswsock)
endif()

add_library(flash_meta INTERFACE)
add_library(flash::meta ALIAS flash_meta)
target_compile_features(flash_meta INTERFACE cxx_std_26)
target_include_directories(flash_meta INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_options(flash_meta INTERFACE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-freflection>
)

add_library(flash INTERFACE)
add_library(flash::flash ALIAS flash)
target_link_libraries(flash INTERFACE flash::meta flash::runtime)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
set_target_properties(flash_runtime PROPERTIES EXPORT_NAME runtime)
set_target_properties(flash_meta PROPERTIES EXPORT_NAME meta)
install(TARGETS flash flash_meta flash_runtime
    EXPORT flashTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT flashTargets
    NAMESPACE flash::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/flash
)
configure_package_config_file(
    cmake/flashConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/flashConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/flash
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/flashConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/flashConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/flashConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/flash
)

include(cmake/FlashWarnings.cmake)
flash_enable_warnings(flash_runtime)

if(FLASH_BUILD_TESTS)
    find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)
    enable_testing()
    add_subdirectory(tests)
endif()

if(FLASH_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(FLASH_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
