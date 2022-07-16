# Copyright (C) 2022 Jonathan Müller and lauf contributors
# SPDX-License-Identifier: BSL-1.0

cmake_minimum_required(VERSION 3.8)
project(lauf VERSION 0.0.0 LANGUAGES C CXX)

add_subdirectory(src)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.18)
    option(LAUF_BUILD_TESTS "whether or not tests should be built" ON)

    if(LAUF_BUILD_TESTS)
        enable_testing()
        add_subdirectory(tests)
    endif()
endif()

add_executable(lauf_prototype main.cpp)
target_link_libraries(lauf_prototype PRIVATE foonathan::lauf::core)

