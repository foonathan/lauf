# Copyright (C) 2022 Jonathan Müller and lauf contributors
# SPDX-License-Identifier: BSL-1.0

get_filename_component(include_dir ${CMAKE_CURRENT_SOURCE_DIR}/../include/lauf ABSOLUTE)
set(header_files
        ${include_dir}/config.h)
set(src_files
        config.cpp)

#=== Warnings ===#
add_library(lauf_warnings INTERFACE)

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    if("${CMAKE_CXX_SIMULATE_ID}" STREQUAL "MSVC")
        target_compile_options(lauf_warnings INTERFACE /WX /W3 /D _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(lauf_warnings INTERFACE -pedantic-errors -Werror -Wall -Wextra -Wconversion -Wsign-conversion)
    endif()
elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    target_compile_options(lauf_warnings INTERFACE -pedantic-errors -Werror -Wall -Wextra -Wconversion -Wsign-conversion)
elseif(MSVC)
    target_compile_options(lauf_warnings INTERFACE /WX /W3 /D _CRT_SECURE_NO_WARNINGS)
endif()

#=== Core library target ===#
add_library(lauf_core)
add_library(foonathan::lauf::core ALIAS lauf_core)
target_compile_features(lauf_core PRIVATE cxx_std_17)
target_include_directories(lauf_core PUBLIC ../include)
target_link_libraries(lauf_core PRIVATE lauf_warnings)

target_sources(lauf_core PRIVATE ${header_files} ${src_files})

