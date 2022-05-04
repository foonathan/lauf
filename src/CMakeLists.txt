# Copyright (C) 2022 Jonathan Müller and lauf contributors
# SPDX-License-Identifier: BSL-1.0

get_filename_component(include_dir ${CMAKE_CURRENT_SOURCE_DIR}/../include/lauf ABSOLUTE)

add_library(lauf_core)
target_compile_features(lauf_core PRIVATE cxx_std_17)
target_include_directories(lauf_core PUBLIC ../include)
target_sources(lauf_core PUBLIC
                ${include_dir}/builder.h
                ${include_dir}/error.h
                ${include_dir}/function.h
                ${include_dir}/value.h
                ${include_dir}/vm.h
                )
target_sources(lauf_core PRIVATE
                detail/bytecode.hpp
                detail/function.hpp
                builder.cpp
                error.cpp
                function.cpp
                vm.cpp)
target_sources(lauf_core PRIVATE lauf.c)

