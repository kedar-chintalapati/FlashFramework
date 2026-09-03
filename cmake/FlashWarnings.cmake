function(flash_enable_warnings target)
    target_compile_options(${target}
        PRIVATE
            $<$<COMPILE_LANG_AND_ID:CXX,GNU>:
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
                -Wshadow
                -Wdouble-promotion
            >
    )

    if(FLASH_WARNINGS_AS_ERRORS)
        target_compile_options(${target}
            PRIVATE $<$<COMPILE_LANG_AND_ID:CXX,GNU>:-Werror>
        )
    endif()
endfunction()

