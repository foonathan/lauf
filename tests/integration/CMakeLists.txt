# Copyright (C) 2022 Jonathan Müller and lauf contributors
# SPDX-License-Identifier: BSL-1.0

add_executable(lauf_test_runner runner.cpp)
target_link_libraries(lauf_test_runner PRIVATE foonathan::lauf::core foonathan::lexy::core foonathan::lexy::file)

file(GLOB test_files CONFIGURE_DEPENDS "*.lauf")
foreach(file ${test_files})
    get_filename_component(name ${file} NAME)
    add_test(NAME ${name} COMMAND lauf_test_runner ${file})
endforeach()

