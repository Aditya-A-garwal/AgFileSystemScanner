cmake_minimum_required (VERSION 3.16)

set (CMAKE_CXX_FLAGS "")
set (CMAKE_CXX_FLAGS_RELEASE "")
set (CMAKE_CXX_FLAGS_DEBUG "")
set (CMAKE_CXX_FLAGS_RELMINSIZE "")
set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "")

add_executable (
    fss
    main.cpp
)

if (MSVC OR MSVC_IDE)
    target_compile_options (fss PRIVATE "/W4" "/WX" "/EHsc")
    # target_compile_options (fss PRIVATE "/W4" "/WX" "/EHsc" "/Ox")
else ()
    # target_compile_options (fss PRIVATE "-Wall" "-Wextra" "-Werror" "-pedantic-errors" "-O3")
    target_compile_options (fss PRIVATE "-Wall" "-Wextra" "-Werror" "-pedantic-errors")
endif ()

target_compile_definitions(fss PRIVATE "_CRT_SECURE_NO_WARNINGS")
