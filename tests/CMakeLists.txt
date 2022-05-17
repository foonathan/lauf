# Copyright (C) 2022 Jonathan Müller and lauf contributors
# SPDX-License-Identifier: BSL-1.0

# Fetch doctest.
message(STATUS "Fetching doctest")
include(FetchContent)
FetchContent_Declare(doctest URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.8.zip)
FetchContent_MakeAvailable(doctest)

# A generic test target.
add_library(lauf_test_base ${CMAKE_CURRENT_SOURCE_DIR}/doctest_main.cpp)
target_link_libraries(lauf_test_base PUBLIC foonathan::lauf::core doctest)

add_subdirectory(integration)

