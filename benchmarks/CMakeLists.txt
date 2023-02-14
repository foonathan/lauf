# Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
# SPDX-License-Identifier: BSL-1.0

# Fetch nanobench.
message(STATUS "Fetching nanobench")
include(FetchContent)
FetchContent_Declare(nanobench URL https://github.com/martinus/nanobench/archive/v4.3.0.zip)
FetchContent_MakeAvailable(nanobench)

add_executable(lauf_benchmark_chunk)
target_sources(lauf_benchmark_chunk PRIVATE chunk.cpp)
target_link_libraries(lauf_benchmark_chunk PRIVATE foonathan::lauf::core nanobench)
target_compile_features(lauf_benchmark_chunk PRIVATE cxx_std_17)

