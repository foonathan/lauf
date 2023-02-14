# Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
# SPDX-License-Identifier: BSL-1.0

#=== Fetch doctest ===#
message(STATUS "Fetching doctest")
include(FetchContent)
FetchContent_Declare(doctest URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.8.zip)
FetchContent_MakeAvailable(doctest)

add_library(lauf_test_base ${CMAKE_CURRENT_SOURCE_DIR}/doctest_main.cpp)
target_link_libraries(lauf_test_base PUBLIC foonathan::lauf lauf_warnings doctest)
target_include_directories(lauf_test_base PUBLIC ../src)
target_compile_features(lauf_test_base PUBLIC cxx_std_17)

#=== Interface compile check target. ===#
add_library(lauf_interface_check OBJECT all.c)
target_compile_features(lauf_interface_check PRIVATE c_std_99)
target_include_directories(lauf_interface_check PRIVATE ../include)
target_link_libraries(lauf_interface_check PRIVATE lauf_warnings)

#=== tests ===#
add_subdirectory(lauf)
add_subdirectory(integration)

