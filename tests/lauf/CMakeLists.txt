# Copyright (C) 2022 Jonathan Müller and lauf contributors
# SPDX-License-Identifier: BSL-1.0

set(tests
        reader.cpp
        writer.cpp

        support/arena.cpp
        support/array.cpp
        support/array_list.cpp)

add_executable(lauf_test ${tests})
target_link_libraries(lauf_test PRIVATE lauf_test_base foonathan::lexy)

add_test(NAME lauf_test COMMAND lauf_test)

