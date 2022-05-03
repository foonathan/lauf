# Copyright (C) 2022 Jonathan Müller and lauf contributors
# SPDX-License-Identifier: BSL-1.0

cmake_minimum_required(VERSION 3.8)
project(lauf VERSION 0.0.0 LANGUAGES C CXX)

add_subdirectory(src)

add_executable(lauf_prototype main.cpp)
target_link_libraries(lauf_prototype PRIVATE lauf_core)

