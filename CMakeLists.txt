# Copyright (C) 2022 Jonathan Müller and lauf contributors
# SPDX-License-Identifier: BSL-1.0

cmake_minimum_required(VERSION 3.11)
project(lauf VERSION 0.0.0 LANGUAGES C CXX ASM)

option(LAUF_ENABLE_JIT "whether or not the JIT compiler is enabled" ON)

add_subdirectory(src)

add_executable(lauf_prototype main.cpp)
target_link_libraries(lauf_prototype PRIVATE lauf_core)
target_compile_features(lauf_prototype PRIVATE cxx_std_17)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.18)
    option(LAUF_BUILD_BENCHMARKS "whether or not benchmarks should be built" OFF)
    option(LAUF_BUILD_TESTS      "whether or not tests should be built" ON)

    if(LAUF_BUILD_BENCHMARKS)
        add_subdirectory(benchmarks)
    endif()
    if(LAUF_BUILD_TESTS)
        enable_testing()
        add_subdirectory(tests)
    endif()
endif()
