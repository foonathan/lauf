# Copyright (C) 2022 Jonathan Müller and lauf contributors
# SPDX-License-Identifier: BSL-1.0

include(FetchContent)
FetchContent_Declare(lexy URL https://github.com/foonathan/lexy/releases/download/v2022.05.0-beta/lexy-src.zip)
FetchContent_MakeAvailable(lexy)

get_filename_component(include_dir ${CMAKE_CURRENT_SOURCE_DIR}/../include/lauf ABSOLUTE)
set(header_files
        ${include_dir}/builder.h
        ${include_dir}/config.h
        ${include_dir}/linker.h
        ${include_dir}/module.h
        ${include_dir}/program.h
        ${include_dir}/type.h
        ${include_dir}/value.h
        ${include_dir}/vm.h

        ${include_dir}/frontend/text.h

        ${include_dir}/lib/int.h)
set(src_header_files
        lauf/support/joined_allocation.hpp
        lauf/support/stack_allocator.hpp
        lauf/support/temporary_array.hpp
        lauf/impl/module.hpp
        lauf/impl/process.hpp
        lauf/impl/program.hpp
        lauf/impl/vm.hpp
        lauf/bytecode.hpp
        lauf/bytecode_builder.hpp
        lauf/literal_pool.hpp
        lauf/stack_check.hpp
        lauf/verify.hpp)
set(src_files
        builder.cpp
        linker.cpp
        module.cpp
        program.cpp
        type.cpp
        vm.cpp

        frontend/text.cpp

        lib/int.cpp)

add_library(lauf_core)
add_library(foonathan::lauf::core ALIAS lauf_core)
target_compile_features(lauf_core PRIVATE cxx_std_17)
target_include_directories(lauf_core PUBLIC ../include .)
target_link_libraries(lauf_core PRIVATE foonathan::lexy::core foonathan::lexy::ext)
target_sources(lauf_core PUBLIC ${header_files})
target_sources(lauf_core PRIVATE ${src_header_files})
target_sources(lauf_core PRIVATE lauf.c ${src_files})

