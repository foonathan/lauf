# Copyright (C) 2022 Jonathan Müller and lauf contributors
# SPDX-License-Identifier: BSL-1.0

get_filename_component(include_dir ${CMAKE_CURRENT_SOURCE_DIR}/../include/lauf ABSOLUTE)
get_filename_component(src_dir ${CMAKE_CURRENT_SOURCE_DIR}/lauf ABSOLUTE)
set(header_files
        ${include_dir}/config.h
        ${include_dir}/vm.h
        ${include_dir}/writer.h

        ${include_dir}/asm/builder.h
        ${include_dir}/asm/module.h
        ${include_dir}/asm/program.h

        ${include_dir}/backend/dump.h

        ${include_dir}/runtime/process.h
        ${include_dir}/runtime/stacktrace.h
        ${include_dir}/runtime/value.h)
set(impl_header_files
        ${src_dir}/vm.hpp
        ${src_dir}/writer.hpp

        ${src_dir}/asm/builder.hpp
        ${src_dir}/asm/module.hpp
        ${src_dir}/asm/program.hpp

        ${src_dir}/runtime/process.hpp)
set(src_files
        ${src_dir}/config.cpp
        ${src_dir}/vm.cpp
        ${src_dir}/vm_execute.cpp
        ${src_dir}/writer.cpp

        ${src_dir}/asm/builder.cpp
        ${src_dir}/asm/module.cpp
        ${src_dir}/asm/program.cpp

        ${src_dir}/backend/dump.cpp

        ${src_dir}/runtime/process.cpp
        ${src_dir}/runtime/stacktrace.cpp

        ${src_dir}/support/align.hpp
        ${src_dir}/support/array_list.hpp
        ${src_dir}/support/arena.hpp)

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
target_include_directories(lauf_core SYSTEM INTERFACE ../include)
target_include_directories(lauf_core PRIVATE ../include .)
target_link_libraries(lauf_core PRIVATE lauf_warnings)

target_sources(lauf_core PRIVATE ${header_files} ${impl_header_files} ${src_files})

