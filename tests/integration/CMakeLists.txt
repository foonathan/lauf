# Copyright (C) 2022-2023 Jonathan Müller and lauf contributors
# SPDX-License-Identifier: BSL-1.0

add_custom_target(lauf_test_qbe ALL)

file(GLOB test_files CONFIGURE_DEPENDS "*.lauf")
foreach(file ${test_files})
    get_filename_component(name ${file} NAME)
    add_test(NAME ${name} COMMAND lauf_tool_interpreter ${file})

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${name}.qbe COMMAND lauf_tool_qbe ${file} > ${name}.qbe DEPENDS ${file} lauf_tool_qbe)
    add_custom_command(OUTPUT ${name}.s   COMMAND qbe ${CMAKE_CURRENT_BINARY_DIR}/${name}.qbe -o ${name}.s DEPENDS ${name}.qbe)
    add_custom_command(OUTPUT ${name}.exe COMMAND clang ${CMAKE_CURRENT_SOURCE_DIR}/runtime.c ${name}.s  -o ${name}.exe
                                          DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/runtime.c ${name}.s)

    add_custom_target(lauf_test_qbe_${name} DEPENDS ${name}.exe)
    add_dependencies(lauf_test_qbe lauf_test_qbe_${name})

    add_test(NAME ${name}.qbe COMMAND ${name}.exe)
endforeach()

