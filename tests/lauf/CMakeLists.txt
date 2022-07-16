# Copyright (C) 2022 Jonathan Müller and lauf contributors
# SPDX-License-Identifier: BSL-1.0

set(tests
        writer.cpp
 
        support/arena.cpp)

add_executable(lauf_test ${tests})
target_link_libraries(lauf_test PRIVATE lauf_test_base)

add_test(NAME lauf_test COMMAND lauf_test)

