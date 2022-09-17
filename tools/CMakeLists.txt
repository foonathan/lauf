# Copyright (C) 2022 Jonathan Müller and lauf contributors
# SPDX-License-Identifier: BSL-1.0

add_executable(lauf_tool_interpreter interpreter.cpp)
target_link_libraries(lauf_tool_interpreter PRIVATE foonathan::lauf::core foonathan::lauf::text lauf_warnings)
target_compile_features(lauf_tool_interpreter PRIVATE cxx_std_17)
set_target_properties(lauf_tool_interpreter PROPERTIES OUTPUT_NAME "lauf")

add_executable(lauf_tool_qbe qbe.cpp)
target_link_libraries(lauf_tool_qbe PRIVATE foonathan::lauf::core foonathan::lauf::text foonathan::lauf::qbe lauf_warnings)
target_compile_features(lauf_tool_qbe PRIVATE cxx_std_17)
set_target_properties(lauf_tool_qbe PROPERTIES OUTPUT_NAME "lauf-qbe")

