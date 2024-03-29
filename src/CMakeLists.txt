# Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
# SPDX-License-Identifier: BSL-1.0

get_filename_component(include_dir ${CMAKE_CURRENT_SOURCE_DIR}/../include/lauf ABSOLUTE)
get_filename_component(src_dir ${CMAKE_CURRENT_SOURCE_DIR}/lauf ABSOLUTE)

include(FetchContent)
FetchContent_Declare(lexy URL https://lexy.foonathan.net/download/lexy-src.zip)
FetchContent_MakeAvailable(lexy)

#=== Warnings ===#
add_library(lauf_warnings INTERFACE)

if(${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    if("${CMAKE_CXX_SIMULATE_ID}" STREQUAL "MSVC")
        target_compile_options(lauf_warnings INTERFACE /WX /W3 /D _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(lauf_warnings INTERFACE -pedantic-errors -Werror -Wall -Wextra -Wconversion -Wsign-conversion)
    endif()
    # We want to return incomplete types, which the warning doesn't like.
    # Since we're testing it works in C, we don't need it.
    target_compile_options(lauf_warnings INTERFACE -Wno-return-type-c-linkage)
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
target_link_libraries(lauf_core PRIVATE lauf_warnings foonathan::lexy)

if(NOT LAUF_DISPATCH_JUMP_TABLE)
    target_compile_definitions(lauf_core PUBLIC LAUF_CONFIG_DISPATCH_JUMP_TABLE=0)
    target_compile_options(lauf_core PRIVATE -fno-jump-tables)
endif()
# Since we're using tail calls for dispatching, we don't want to add frame pointers, ever.
# They would record all previously executed instructions in the call stack.
target_compile_options(lauf_core PRIVATE -fomit-frame-pointer)

target_sources(lauf_core PUBLIC
                ${include_dir}/config.h
                ${include_dir}/reader.h
                ${include_dir}/vm.h
                ${include_dir}/writer.h

                ${include_dir}/asm/builder.h
                ${include_dir}/asm/module.h
                ${include_dir}/asm/program.h
                ${include_dir}/asm/type.h

                ${include_dir}/backend/dump.h

                ${include_dir}/lib/bits.h
                ${include_dir}/lib/debug.h
                ${include_dir}/lib/fiber.h
                ${include_dir}/lib/heap.h
                ${include_dir}/lib/int.h
                ${include_dir}/lib/limits.h
                ${include_dir}/lib/memory.h
                ${include_dir}/lib/platform.h
                ${include_dir}/lib/test.h
                ${include_dir}/lib.h

                ${include_dir}/runtime/memory.h
                ${include_dir}/runtime/process.h
                ${include_dir}/runtime/stacktrace.h
                ${include_dir}/runtime/value.h)
target_sources(lauf_core PRIVATE
                ${src_dir}/reader.hpp
                ${src_dir}/vm.hpp
                ${src_dir}/writer.hpp

                ${src_dir}/asm/builder.hpp
                ${src_dir}/asm/module.hpp
                ${src_dir}/asm/program.hpp

                ${src_dir}/lib/debug.hpp

                ${src_dir}/runtime/memory.hpp
                ${src_dir}/runtime/process.hpp)
target_sources(lauf_core PRIVATE
                ${src_dir}/config.cpp
                ${src_dir}/reader.cpp
                ${src_dir}/vm.cpp
                ${src_dir}/vm_execute.cpp
                ${src_dir}/writer.cpp

                ${src_dir}/asm/builder.cpp
                ${src_dir}/asm/module.cpp
                ${src_dir}/asm/program.cpp
                ${src_dir}/asm/type.cpp

                ${src_dir}/backend/dump.cpp

                ${src_dir}/runtime/memory.cpp
                ${src_dir}/runtime/process.cpp
                ${src_dir}/runtime/stack.hpp
                ${src_dir}/runtime/stacktrace.cpp

                ${src_dir}/lib/bits.cpp
                ${src_dir}/lib/debug.cpp
                ${src_dir}/lib/fiber.cpp
                ${src_dir}/lib/heap.cpp
                ${src_dir}/lib/int.cpp
                ${src_dir}/lib/limits.cpp
                ${src_dir}/lib/memory.cpp
                ${src_dir}/lib/platform.cpp
                ${src_dir}/lib/test.cpp
                ${src_dir}/lib.cpp

                ${src_dir}/support/align.hpp
                ${src_dir}/support/array.hpp
                ${src_dir}/support/array_list.hpp
                ${src_dir}/support/arena.hpp
                ${src_dir}/support/page_allocator.hpp
                ${src_dir}/support/page_allocator.cpp)

#=== Text frontend ===#
add_library(lauf_text)
add_library(foonathan::lauf::text ALIAS lauf_text)
target_include_directories(lauf_text PRIVATE .)
target_link_libraries(lauf_text PRIVATE lauf_warnings lauf_core foonathan::lexy)

target_sources(lauf_text PUBLIC ${include_dir}/frontend/text.h)
target_sources(lauf_text PRIVATE ${src_dir}/frontend/text.cpp)

#=== QBE backend ===#
add_library(lauf_qbe)
add_library(foonathan::lauf::qbe ALIAS lauf_qbe)
target_include_directories(lauf_qbe PRIVATE .)
target_compile_features(lauf_qbe PRIVATE cxx_std_17)
target_link_libraries(lauf_qbe PRIVATE lauf_warnings lauf_core)

target_sources(lauf_qbe PUBLIC ${include_dir}/backend/qbe.h)
target_sources(lauf_qbe PRIVATE ${src_dir}/backend/qbe.hpp ${src_dir}/backend/qbe.cpp)

#=== Umbrella target ===#
add_library(lauf_umbrella INTERFACE)
add_library(foonathan::lauf ALIAS lauf_umbrella)
target_link_libraries(lauf_umbrella INTERFACE lauf_core lauf_text lauf_qbe)

