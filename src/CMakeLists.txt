# Copyright (C) 2022 Jonathan Müller and lauf contributors
# SPDX-License-Identifier: BSL-1.0

include(FetchContent)
FetchContent_Declare(lexy URL https://github.com/foonathan/lexy/releases/download/v2022.05.0-beta/lexy-src.zip)
FetchContent_MakeAvailable(lexy)

get_filename_component(include_dir ${CMAKE_CURRENT_SOURCE_DIR}/../include/lauf ABSOLUTE)
set(header_files
        ${include_dir}/builder.h
        ${include_dir}/config.h
        ${include_dir}/jit.h
        ${include_dir}/linker.h
        ${include_dir}/module.h
        ${include_dir}/program.h
        ${include_dir}/type.h
        ${include_dir}/value.h
        ${include_dir}/vm.h

        ${include_dir}/frontend/text.h

        ${include_dir}/lib/int.h
        ${include_dir}/lib/memory.h)
set(src_additional_files
        lauf/aarch64/assembler.hpp
        lauf/aarch64/emitter.hpp
        lauf/aarch64/jit.hpp
        lauf/aarch64/register_allocator.hpp
        lauf/bc/bytecode.hpp
        lauf/bc/bytecode_builder.hpp
        lauf/bc/literal_pool.hpp
        lauf/bc/stack_check.hpp
        lauf/bc/vm_memory.hpp
        lauf/impl/builtin.hpp
        lauf/impl/module.hpp
        lauf/impl/process.hpp
        lauf/impl/program.hpp
        lauf/impl/vm.hpp
        lauf/ir/instruction.hpp
        lauf/ir/irdump.hpp
        lauf/ir/irdump.cpp
        lauf/ir/irgen.cpp
        lauf/ir/irgen.hpp
        lauf/ir/irgen.cpp
        lauf/ir/register_allocator.hpp
        lauf/ir/register_allocator.cpp
        lauf/support/align.hpp
        lauf/support/joined_allocation.hpp
        lauf/support/stack_allocator.hpp
        lauf/support/temporary_array.hpp
        lauf/support/verify.hpp
        lauf/support/virtual_memory.hpp
        lauf/support/virtual_memory.cpp
        )
set(src_files
        builder.cpp
        linker.cpp
        module.cpp
        program.cpp
        type.cpp
        vm.cpp

        frontend/text.cpp

        lib/int.cpp
        lib/memory.cpp)

add_library(lauf_core)
add_library(foonathan::lauf::core ALIAS lauf_core)
target_compile_features(lauf_core PRIVATE cxx_std_17)
target_include_directories(lauf_core PUBLIC ../include .)
target_link_libraries(lauf_core PRIVATE foonathan::lexy::core foonathan::lexy::ext)
target_sources(lauf_core PUBLIC ${header_files})
target_sources(lauf_core PRIVATE ${src_additional_files})
target_sources(lauf_core PRIVATE lauf.c ${src_files})

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
    target_sources(lauf_core PRIVATE jit_aarch64.cpp lib/int_aarch64.cpp)
else()
    message(STATUS "lauf: JIT compiler not available on this platform or it has been disabled")
    target_sources(lauf_core PRIVATE jit_null.cpp)
endif()

