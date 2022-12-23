// Copyright (C) 2022 Jonathan Müller and lauf contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/vm.h>

#include <doctest/doctest.h>
#include <lauf/asm/builder.h>
#include <lauf/asm/module.h>
#include <lauf/asm/program.h>
#include <lauf/asm/type.h>
#include <lauf/frontend/text.h>
#include <lauf/lib/test.h>
#include <lauf/reader.h>
#include <lauf/runtime/builtin.h>
#include <lauf/runtime/process.h>
#include <lauf/runtime/stacktrace.h>
#include <lauf/runtime/value.h>

namespace
{
lauf_asm_module* test_module()
{
    auto reader = lauf_create_cstring_reader(R"(
        module @test;

        function @noop()
        {
            return;
        }
        function @panic()
        {
            null; panic;
        }

        function @id1(1 => 1) {
            return;
        }
        function @id2(2 => 2) {
            return;
        }

        function @input(3 => 0) {
            uint 2; $lauf.test.assert_eq;
            uint 1; $lauf.test.assert_eq;
            uint 0; $lauf.test.assert_eq; 
            return;
        }
        function @output(0 => 3) {
            uint 0;
            uint 1;
            uint 2;
            return;
        }

        function @suspending(1 => 1) {
            fiber_suspend();
            fiber_suspend();
            fiber_suspend();
            return;
        }
        function @suspending_values(1 => 1) {
            uint 0; $lauf.test.assert_eq;
            uint 1; fiber_suspend(1 => 0);
            fiber_suspend(0 => 1); uint 2; $lauf.test.assert_eq;
            uint 3; return;
        }
    )");
    auto result = lauf_frontend_text(reader, lauf_frontend_default_text_options);
    lauf_destroy_reader(reader);
    return result;
}

lauf_asm_program test_program(lauf_asm_module* mod, const char* fn_name)
{
    auto fn = lauf_asm_find_function_by_name(mod, fn_name);
    return lauf_asm_create_program(mod, fn);
}
} // namespace

TEST_CASE("lauf_vm_execute_oneshot")
{
    auto mod = test_module();
    auto vm  = lauf_create_vm(lauf_default_vm_options);

    SUBCASE("noop")
    {
        auto prog   = test_program(mod, "noop");
        auto result = lauf_vm_execute_oneshot(vm, prog, nullptr, nullptr);
        CHECK(result);
    }
    SUBCASE("panic")
    {
        lauf_vm_set_panic_handler(vm, {nullptr, [](void*, lauf_runtime_process*, const char*) {}});

        auto prog   = test_program(mod, "panic");
        auto result = lauf_vm_execute_oneshot(vm, prog, nullptr, nullptr);
        CHECK(!result);
    }
    SUBCASE("id1")
    {
        auto prog = test_program(mod, "id1");

        lauf_runtime_value input = {11};
        lauf_runtime_value output;
        auto               result = lauf_vm_execute_oneshot(vm, prog, &input, &output);
        CHECK(result);
        CHECK(output.as_uint == 11);
    }
    SUBCASE("id2")
    {
        auto prog = test_program(mod, "id2");

        lauf_runtime_value input[2] = {{11}, {42}};
        lauf_runtime_value output[2];
        auto               result = lauf_vm_execute_oneshot(vm, prog, input, output);
        CHECK(result);
        CHECK(output[0].as_uint == 11);
        CHECK(output[1].as_uint == 42);
    }
    SUBCASE("input")
    {
        auto prog = test_program(mod, "input");

        lauf_runtime_value input[3] = {{0}, {1}, {2}};
        auto               result   = lauf_vm_execute_oneshot(vm, prog, input, nullptr);
        CHECK(result);
    }
    SUBCASE("output")
    {
        auto prog = test_program(mod, "output");

        lauf_runtime_value output[3];
        auto               result = lauf_vm_execute_oneshot(vm, prog, nullptr, output);
        CHECK(result);
        CHECK(output[0].as_uint == 0);
        CHECK(output[1].as_uint == 1);
        CHECK(output[2].as_uint == 2);
    }
    SUBCASE("suspending")
    {
        auto prog = test_program(mod, "suspending");

        lauf_runtime_value input = {11};
        lauf_runtime_value output;
        auto               result = lauf_vm_execute_oneshot(vm, prog, &input, &output);
        CHECK(result);
        CHECK(output.as_uint == 11);
    }
    SUBCASE("suspending_values")
    {
        lauf_vm_set_panic_handler(vm, {nullptr, [](void*, lauf_runtime_process*, const char* msg) {
                                           CHECK(msg
                                                 == doctest::String(
                                                     "mismatched signature for fiber resume"));
                                       }});

        auto prog = test_program(mod, "suspending_values");

        lauf_runtime_value input = {0};
        lauf_runtime_value output;
        auto               result = lauf_vm_execute_oneshot(vm, prog, &input, &output);
        CHECK(!result);
    }

    lauf_destroy_vm(vm);
    lauf_asm_destroy_module(mod);
}

TEST_CASE("lauf_vm_start_process")
{
    auto mod = test_module();
    auto vm  = lauf_create_vm(lauf_default_vm_options);

    SUBCASE("noop")
    {
        auto prog  = test_program(mod, "noop");
        auto proc  = lauf_vm_start_process(vm, &prog);
        auto fiber = lauf_runtime_get_current_fiber(proc);

        CHECK(lauf_runtime_resume(proc, fiber, nullptr, 0, nullptr, 0));

        lauf_runtime_destroy_process(proc);
        lauf_asm_destroy_program(prog);
    }
    SUBCASE("panic")
    {
        lauf_vm_set_panic_handler(vm, {nullptr, [](void*, lauf_runtime_process*, const char*) {}});

        auto prog  = test_program(mod, "panic");
        auto proc  = lauf_vm_start_process(vm, &prog);
        auto fiber = lauf_runtime_get_current_fiber(proc);

        CHECK(!lauf_runtime_resume(proc, fiber, nullptr, 0, nullptr, 0));

        lauf_runtime_destroy_process(proc);
        lauf_asm_destroy_program(prog);
    }
    SUBCASE("id1")
    {
        auto prog  = test_program(mod, "id1");
        auto proc  = lauf_vm_start_process(vm, &prog);
        auto fiber = lauf_runtime_get_current_fiber(proc);

        lauf_runtime_value input = {11};
        lauf_runtime_value output;
        CHECK(lauf_runtime_resume(proc, fiber, &input, 1, &output, 1));
        CHECK(output.as_uint == 11);

        lauf_runtime_destroy_process(proc);
        lauf_asm_destroy_program(prog);
    }
    SUBCASE("id2")
    {
        auto prog  = test_program(mod, "id2");
        auto proc  = lauf_vm_start_process(vm, &prog);
        auto fiber = lauf_runtime_get_current_fiber(proc);

        lauf_runtime_value input[2] = {{11}, {42}};
        lauf_runtime_value output[2];
        CHECK(lauf_runtime_resume(proc, fiber, input, 2, output, 2));
        CHECK(output[0].as_uint == 11);
        CHECK(output[1].as_uint == 42);

        lauf_runtime_destroy_process(proc);
        lauf_asm_destroy_program(prog);
    }
    SUBCASE("input")
    {
        auto prog  = test_program(mod, "input");
        auto proc  = lauf_vm_start_process(vm, &prog);
        auto fiber = lauf_runtime_get_current_fiber(proc);

        lauf_runtime_value input[3] = {{0}, {1}, {2}};
        CHECK(lauf_runtime_resume(proc, fiber, input, 3, nullptr, 0));

        lauf_runtime_destroy_process(proc);
        lauf_asm_destroy_program(prog);
    }
    SUBCASE("output")
    {
        auto prog  = test_program(mod, "output");
        auto proc  = lauf_vm_start_process(vm, &prog);
        auto fiber = lauf_runtime_get_current_fiber(proc);

        lauf_runtime_value output[3];
        CHECK(lauf_runtime_resume(proc, fiber, nullptr, 0, output, 3));
        CHECK(output[0].as_uint == 0);
        CHECK(output[1].as_uint == 1);
        CHECK(output[2].as_uint == 2);

        lauf_runtime_destroy_process(proc);
        lauf_asm_destroy_program(prog);
    }
    SUBCASE("suspending_values")
    {
        auto prog  = test_program(mod, "suspending_values");
        auto proc  = lauf_vm_start_process(vm, &prog);
        auto fiber = lauf_runtime_get_current_fiber(proc);

        lauf_runtime_value input = {0};
        lauf_runtime_value output;
        CHECK(lauf_runtime_resume(proc, fiber, &input, 1, &output, 1));
        CHECK(output.as_uint == 1);

        CHECK(lauf_runtime_resume(proc, fiber, nullptr, 0, nullptr, 0));

        input.as_uint = 2;
        CHECK(lauf_runtime_resume(proc, fiber, &input, 1, &output, 1));
        CHECK(output.as_uint == 3);

        lauf_runtime_destroy_process(proc);
        lauf_asm_destroy_program(prog);
    }

    lauf_destroy_vm(vm);
    lauf_asm_destroy_module(mod);
}

TEST_CASE("lauf_asm_link_module")
{
    auto mod       = lauf_asm_create_module("test");
    auto extern_fn = lauf_asm_add_function(mod, "extern_fn", {3, 5});
    auto fn        = lauf_asm_add_function(mod, "test", {0, 0});

    {
        auto global = lauf_asm_add_global(mod, LAUF_ASM_GLOBAL_READ_WRITE);
        lauf_asm_define_data_global(mod, global, {8, 8}, nullptr);

        auto b = lauf_asm_create_builder(lauf_asm_default_build_options);
        lauf_asm_build(b, mod, fn);

        lauf_asm_inst_uint(b, 42);
        lauf_asm_inst_global_addr(b, global);
        lauf_asm_inst_store_field(b, lauf_asm_type_value, 0);

        lauf_asm_inst_uint(b, 1);
        lauf_asm_inst_uint(b, 2);
        lauf_asm_inst_uint(b, 3);

        SUBCASE("direct")
        {
            lauf_asm_inst_call(b, extern_fn);
        }
        SUBCASE("indirect")
        {
            lauf_asm_inst_function_addr(b, extern_fn);
            lauf_asm_inst_call_indirect(b, {3, 5});
        }

        lauf_asm_inst_uint(b, 55);
        lauf_asm_inst_call_builtin(b, lauf_lib_test_assert_eq);
        lauf_asm_inst_uint(b, 44);
        lauf_asm_inst_call_builtin(b, lauf_lib_test_assert_eq);
        lauf_asm_inst_uint(b, 33);
        lauf_asm_inst_call_builtin(b, lauf_lib_test_assert_eq);
        lauf_asm_inst_uint(b, 22);
        lauf_asm_inst_call_builtin(b, lauf_lib_test_assert_eq);
        lauf_asm_inst_uint(b, 11);
        lauf_asm_inst_call_builtin(b, lauf_lib_test_assert_eq);

        lauf_asm_inst_uint(b, 42);
        lauf_asm_inst_global_addr(b, global);
        lauf_asm_inst_load_field(b, lauf_asm_type_value, 0);
        lauf_asm_inst_call_builtin(b, lauf_lib_test_assert_eq);

        lauf_asm_inst_return(b);

        lauf_asm_build_finish(b);
        lauf_asm_destroy_builder(b);
    }

    auto submod = lauf_asm_create_module("other");
    {
        auto global = lauf_asm_add_global(submod, LAUF_ASM_GLOBAL_READ_WRITE);
        lauf_asm_define_data_global(submod, global, {8, 8}, nullptr);

        auto fn_def = lauf_asm_add_function(submod, "extern_fn", {3, 5});
        auto b      = lauf_asm_create_builder(lauf_asm_default_build_options);
        lauf_asm_build(b, submod, fn_def);

        lauf_asm_inst_uint(b, 3);
        lauf_asm_inst_call_builtin(b, lauf_lib_test_assert_eq);
        lauf_asm_inst_uint(b, 2);
        lauf_asm_inst_call_builtin(b, lauf_lib_test_assert_eq);
        lauf_asm_inst_uint(b, 1);
        lauf_asm_inst_call_builtin(b, lauf_lib_test_assert_eq);

        lauf_asm_inst_uint(b, 11);
        lauf_asm_inst_global_addr(b, global);
        lauf_asm_inst_store_field(b, lauf_asm_type_value, 0);

        lauf_asm_inst_uint(b, 11);
        lauf_asm_inst_global_addr(b, global);
        lauf_asm_inst_load_field(b, lauf_asm_type_value, 0);
        lauf_asm_inst_call_builtin(b, lauf_lib_test_assert_eq);

        lauf_asm_inst_uint(b, 11);
        lauf_asm_inst_uint(b, 22);
        lauf_asm_inst_uint(b, 33);
        lauf_asm_inst_uint(b, 44);
        lauf_asm_inst_uint(b, 55);

        lauf_asm_inst_return(b);

        lauf_asm_build_finish(b);
        lauf_asm_destroy_builder(b);
    }

    auto program = lauf_asm_create_program(mod, fn);
    lauf_asm_link_module(&program, submod);

    auto vm = lauf_create_vm(lauf_default_vm_options);
    CHECK(lauf_vm_execute_oneshot(vm, program, nullptr, nullptr));
    lauf_destroy_vm(vm);

    lauf_asm_destroy_module(mod);
}

TEST_CASE("lauf_asm_define_native_global")
{
    auto mod    = lauf_asm_create_module("test");
    auto fn     = lauf_asm_add_function(mod, "test", {0, 0});
    auto global = lauf_asm_add_global(mod, LAUF_ASM_GLOBAL_READ_WRITE);

    {
        auto b = lauf_asm_create_builder(lauf_asm_default_build_options);
        lauf_asm_build(b, mod, fn);

        lauf_asm_inst_global_addr(b, global);
        lauf_asm_inst_load_field(b, lauf_asm_type_value, 0);
        lauf_asm_inst_uint(b, 11);
        lauf_asm_inst_call_builtin(b, lauf_lib_test_assert_eq);

        lauf_asm_inst_uint(b, 42);
        lauf_asm_inst_global_addr(b, global);
        lauf_asm_inst_store_field(b, lauf_asm_type_value, 0);

        lauf_asm_inst_return(b);

        lauf_asm_build_finish(b);
        lauf_asm_destroy_builder(b);
    }

    auto               program = lauf_asm_create_program(mod, fn);
    lauf_runtime_value global_val;
    lauf_asm_define_native_global(&program, global, &global_val, sizeof(global_val));

    global_val.as_uint = 11;

    auto vm = lauf_create_vm(lauf_default_vm_options);
    CHECK(lauf_vm_execute_oneshot(vm, program, nullptr, nullptr));
    lauf_destroy_vm(vm);

    CHECK(global_val.as_uint == 42);

    lauf_asm_destroy_module(mod);
}

TEST_CASE("lauf_asm_define_native_function")
{
    auto mod       = lauf_asm_create_module("test");
    auto native_fn = lauf_asm_add_function(mod, "native_fn", {3, 5});
    auto fn        = lauf_asm_add_function(mod, "test", {0, 0});

    {
        auto b = lauf_asm_create_builder(lauf_asm_default_build_options);
        lauf_asm_build(b, mod, fn);

        lauf_asm_inst_uint(b, 1);
        lauf_asm_inst_uint(b, 2);
        lauf_asm_inst_uint(b, 3);

        SUBCASE("direct")
        {
            lauf_asm_inst_call(b, native_fn);
        }
        SUBCASE("indirect")
        {
            lauf_asm_inst_function_addr(b, native_fn);
            lauf_asm_inst_call_indirect(b, {3, 5});
        }

        lauf_asm_inst_uint(b, 55);
        lauf_asm_inst_call_builtin(b, lauf_lib_test_assert_eq);
        lauf_asm_inst_uint(b, 44);
        lauf_asm_inst_call_builtin(b, lauf_lib_test_assert_eq);
        lauf_asm_inst_uint(b, 33);
        lauf_asm_inst_call_builtin(b, lauf_lib_test_assert_eq);
        lauf_asm_inst_uint(b, 22);
        lauf_asm_inst_call_builtin(b, lauf_lib_test_assert_eq);
        lauf_asm_inst_uint(b, 11);
        lauf_asm_inst_call_builtin(b, lauf_lib_test_assert_eq);

        lauf_asm_inst_return(b);

        lauf_asm_build_finish(b);
        lauf_asm_destroy_builder(b);
    }

    auto program = lauf_asm_create_program(mod, fn);
    lauf_asm_define_native_function(
        &program, native_fn,
        [](void* user_data, lauf_runtime_process* process, const lauf_runtime_value* input,
           lauf_runtime_value* output) {
            CHECK(input[0].as_uint == 1);
            CHECK(input[1].as_uint == 2);
            CHECK(input[2].as_uint == 3);

            CHECK(reinterpret_cast<std::uintptr_t>(user_data) == 42);

            auto st = lauf_runtime_get_stacktrace(process, lauf_runtime_get_current_fiber(process));
            auto fn = lauf_runtime_stacktrace_function(st);
            CHECK(lauf_asm_function_name(fn) == doctest::String("test"));
            lauf_runtime_destroy_stacktrace(st);

            output[0].as_uint = 11;
            output[1].as_uint = 22;
            output[2].as_uint = 33;
            output[3].as_uint = 44;
            output[4].as_uint = 55;

            CHECK(input[0].as_uint == 1);
            CHECK(input[1].as_uint == 2);
            CHECK(input[2].as_uint == 3);

            return true;
        },
        reinterpret_cast<void*>(42));

    auto vm = lauf_create_vm(lauf_default_vm_options);
    CHECK(lauf_vm_execute_oneshot(vm, program, nullptr, nullptr));
    lauf_destroy_vm(vm);

    lauf_asm_destroy_module(mod);
}

