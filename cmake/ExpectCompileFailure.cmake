if(NOT DEFINED FLASH_TEST_COMPILER)
    message(FATAL_ERROR "FLASH_TEST_COMPILER is required")
endif()

if(NOT DEFINED FLASH_TEST_SOURCE)
    message(FATAL_ERROR "FLASH_TEST_SOURCE is required")
endif()

if(NOT DEFINED FLASH_TEST_INCLUDE)
    message(FATAL_ERROR "FLASH_TEST_INCLUDE is required")
endif()

if(NOT DEFINED FLASH_EXPECTED_DIAGNOSTIC)
    message(FATAL_ERROR "FLASH_EXPECTED_DIAGNOSTIC is required")
endif()

execute_process(
    COMMAND
        "${FLASH_TEST_COMPILER}"
        -std=c++26
        -freflection
        -I${FLASH_TEST_INCLUDE}
        -fsyntax-only
        "${FLASH_TEST_SOURCE}"
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compiler_stdout
    ERROR_VARIABLE compiler_stderr
)

set(compiler_output "${compiler_stdout}\n${compiler_stderr}")

if(compile_result EQUAL 0)
    message(FATAL_ERROR
        "Expected ${FLASH_TEST_SOURCE} to fail compilation, but it succeeded")
endif()

if(NOT compiler_output MATCHES "${FLASH_EXPECTED_DIAGNOSTIC}")
    message(FATAL_ERROR
        "Compile failed without expected diagnostic '${FLASH_EXPECTED_DIAGNOSTIC}'.\n"
        "Compiler output:\n${compiler_output}")
endif()

message(STATUS
    "Observed expected compile failure '${FLASH_EXPECTED_DIAGNOSTIC}'")
