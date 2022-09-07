// Copyright (C) 2022 Jonathan Müller and null contributors
// SPDX-License-Identifier: BSL-1.0

#include <lauf/asm/builder.h>

#include <doctest/doctest.h>
#include <lauf/asm/module.hpp>
#include <lauf/asm/type.h>
#include <lauf/backend/dump.h>
#include <lauf/lib/debug.h>
#include <lauf/lib/test.h>
#include <lauf/runtime/builtin.h>
#include <lauf/writer.h>
#include <vector>

namespace
{
template <typename BuilderFn>
std::vector<lauf_asm_inst> build(lauf_asm_signature sig, BuilderFn builder_fn)
{
    auto mod = lauf_asm_create_module("test");
    auto fn  = lauf_asm_add_function(mod, "test", {sig.input_count, sig.output_count});

    {
        auto builder = lauf_asm_create_builder([] {
            auto opts          = lauf_asm_default_build_options;
            opts.error_handler = [](const char*, const char* context, const char* msg) {
                FAIL(context << ": " << msg);
            };
            return opts;
        }());
        lauf_asm_build(builder, mod, fn);
        auto block = lauf_asm_declare_block(builder, sig.input_count);
        lauf_asm_build_block(builder, block);

        builder_fn(mod, builder);

        lauf_asm_inst_return(builder);
        lauf_asm_build_finish(builder);
        lauf_asm_destroy_builder(builder);
    }

    auto str = lauf_create_string_writer();
    lauf_backend_dump(str, lauf_backend_default_dump_options, mod);
    // MESSAGE(lauf_writer_get_string(str));
    lauf_destroy_writer(str);

    std::vector<lauf_asm_inst> result;

    auto start_index = 0;
    while (fn->insts[start_index].op() == lauf::asm_op::setup_local_alloc
           || fn->insts[start_index].op() == lauf::asm_op::local_alloc
           || fn->insts[start_index].op() == lauf::asm_op::local_alloc_aligned
           || fn->insts[start_index].op() == lauf::asm_op::local_storage)
        ++start_index;

    auto end_index = fn->insts_count - 1;
    for (auto i = start_index; i != end_index; ++i)
        result.push_back(fn->insts[i]);

    lauf_asm_destroy_module(mod);
    return result;
}
} // namespace

TEST_CASE("lauf_asm_inst_return")
{
    auto result = build({0, 0}, [](lauf_asm_module*, lauf_asm_builder* b) {
        lauf_asm_inst_return(b);
        lauf_asm_build_block(b, lauf_asm_declare_block(b, 0));
    });
    REQUIRE(result.size() == 1);
    CHECK(result[0].op() == lauf::asm_op::return_);
}

TEST_CASE("lauf_asm_inst_jump")
{
    auto nop = build({0, 0}, [](lauf_asm_module*, lauf_asm_builder* b) {
        auto block = lauf_asm_declare_block(b, 0);
        lauf_asm_inst_jump(b, block);
        lauf_asm_build_block(b, block);
    });
    REQUIRE(nop.size() == 1);
    CHECK(nop[0].op() == lauf::asm_op::nop);

    auto forward = build({0, 0}, [](lauf_asm_module*, lauf_asm_builder* b) {
        auto block = lauf_asm_declare_block(b, 0);
        auto dest  = lauf_asm_declare_block(b, 0);
        lauf_asm_inst_jump(b, dest);

        lauf_asm_build_block(b, block);
        lauf_asm_inst_return(b);

        lauf_asm_build_block(b, dest);
    });
    REQUIRE(forward.size() >= 1);
    CHECK(forward[0].op() == lauf::asm_op::jump);
    CHECK(forward[0].jump.offset == 2);

    auto self = build({0, 0}, [](lauf_asm_module*, lauf_asm_builder* b) {
        auto block = lauf_asm_declare_block(b, 0);
        lauf_asm_inst_jump(b, block);
        lauf_asm_build_block(b, block);
        lauf_asm_inst_jump(b, block); // This is the one we're trying to test.

        lauf_asm_build_block(b, lauf_asm_declare_block(b, 0));
    });
    REQUIRE(self.size() == 2);
    CHECK(self[1].op() == lauf::asm_op::jump);
    CHECK(self[1].jump.offset == 0);

    auto backward = build({0, 0}, [](lauf_asm_module* mod, lauf_asm_builder* b) {
        auto fn = lauf_asm_add_function(mod, "foo", {0, 1});

        auto block = lauf_asm_declare_block(b, 0);
        lauf_asm_inst_jump(b, block);
        lauf_asm_build_block(b, block);
        lauf_asm_inst_call(b, fn);
        lauf_asm_inst_pop(b, 0);
        lauf_asm_inst_jump(b, block); // This is the one we're trying to test.

        lauf_asm_build_block(b, lauf_asm_declare_block(b, 0));
    });
    REQUIRE(backward.size() == 4);
    CHECK(backward[3].op() == lauf::asm_op::jump);
    CHECK(backward[3].jump.offset == -2);
}

TEST_CASE("lauf_asm_inst_branch2")
{
    auto br_nop = build({1, 0}, [](lauf_asm_module*, lauf_asm_builder* b) {
        auto if_true  = lauf_asm_declare_block(b, 0);
        auto if_false = lauf_asm_declare_block(b, 0);
        lauf_asm_inst_branch2(b, if_true, if_false);

        lauf_asm_build_block(b, if_true);
        lauf_asm_inst_return(b);

        lauf_asm_build_block(b, if_false);
    });
    REQUIRE(br_nop.size() >= 2);
    CHECK(br_nop[0].op() == lauf::asm_op::branch_false);
    CHECK(br_nop[0].branch_false.offset == 3);
    CHECK(br_nop[1].op() == lauf::asm_op::nop);

    auto br_jump = build({1, 0}, [](lauf_asm_module*, lauf_asm_builder* b) {
        auto if_false = lauf_asm_declare_block(b, 0);
        auto if_true  = lauf_asm_declare_block(b, 0);
        lauf_asm_inst_branch2(b, if_true, if_false);

        lauf_asm_build_block(b, if_true);
        lauf_asm_inst_return(b);

        lauf_asm_build_block(b, if_false);
    });
    REQUIRE(br_jump.size() >= 2);
    CHECK(br_jump[0].op() == lauf::asm_op::branch_false);
    CHECK(br_jump[0].branch_false.offset == 2);
    CHECK(br_jump[1].op() == lauf::asm_op::jump);
    CHECK(br_jump[1].jump.offset == 2);

    auto same = build({1, 0}, [](lauf_asm_module*, lauf_asm_builder* b) {
        auto block = lauf_asm_declare_block(b, 0);
        lauf_asm_inst_branch2(b, block, block);
        lauf_asm_build_block(b, block);
    });
    REQUIRE(same.size() >= 2);
    CHECK(same[0].op() == lauf::asm_op::pop_top);
    CHECK(same[0].pop_top.idx == 0);
    CHECK(same[1].op() == lauf::asm_op::nop);
}

TEST_CASE("lauf_asm_inst_branch3")
{
    auto br_nop = build({1, 0}, [](lauf_asm_module*, lauf_asm_builder* b) {
        auto if_lt = lauf_asm_declare_block(b, 0);
        auto if_eq = lauf_asm_declare_block(b, 0);
        auto if_gt = lauf_asm_declare_block(b, 0);
        lauf_asm_inst_branch3(b, if_lt, if_eq, if_gt);

        lauf_asm_build_block(b, if_lt);
        lauf_asm_inst_return(b);

        lauf_asm_build_block(b, if_eq);
        lauf_asm_inst_return(b);

        lauf_asm_build_block(b, if_gt);
    });
    REQUIRE(br_nop.size() >= 3);
    CHECK(br_nop[0].op() == lauf::asm_op::branch_eq);
    CHECK(br_nop[0].branch_eq.offset == 4);
    CHECK(br_nop[1].op() == lauf::asm_op::branch_gt);
    CHECK(br_nop[1].branch_eq.offset == 4);
    CHECK(br_nop[2].op() == lauf::asm_op::pop_top);
    CHECK(br_nop[2].pop_top.idx == 0);

    auto br_jump = build({1, 0}, [](lauf_asm_module*, lauf_asm_builder* b) {
        auto if_eq = lauf_asm_declare_block(b, 0);
        auto if_gt = lauf_asm_declare_block(b, 0);
        auto if_lt = lauf_asm_declare_block(b, 0);
        lauf_asm_inst_branch3(b, if_lt, if_eq, if_gt);

        lauf_asm_build_block(b, if_lt);
        lauf_asm_inst_return(b);

        lauf_asm_build_block(b, if_eq);
        lauf_asm_inst_return(b);

        lauf_asm_build_block(b, if_gt);
    });
    REQUIRE(br_jump.size() >= 3);
    CHECK(br_jump[0].op() == lauf::asm_op::branch_eq);
    CHECK(br_jump[0].branch_eq.offset == 3);
    CHECK(br_jump[1].op() == lauf::asm_op::branch_gt);
    CHECK(br_jump[1].branch_gt.offset == 3);
    CHECK(br_jump[2].op() == lauf::asm_op::jump_pop);
    CHECK(br_jump[2].jump.offset == 3);

    auto cond_same = build({1, 0}, [](lauf_asm_module*, lauf_asm_builder* b) {
        auto if_lt = lauf_asm_declare_block(b, 0);
        auto if_eq = lauf_asm_declare_block(b, 0);
        lauf_asm_inst_branch3(b, if_lt, if_eq, if_eq);

        lauf_asm_build_block(b, if_lt);
        lauf_asm_inst_return(b);

        lauf_asm_build_block(b, if_eq);
    });
    REQUIRE(cond_same.size() >= 3);
    CHECK(cond_same[0].op() == lauf::asm_op::branch_eq);
    CHECK(cond_same[0].branch_eq.offset == 4);
    CHECK(cond_same[1].op() == lauf::asm_op::branch_gt);
    CHECK(cond_same[2].op() == lauf::asm_op::pop_top);
    CHECK(cond_same[2].pop_top.idx == 0);

    auto all_same = build({1, 0}, [](lauf_asm_module*, lauf_asm_builder* b) {
        auto block = lauf_asm_declare_block(b, 0);
        lauf_asm_inst_branch3(b, block, block, block);

        lauf_asm_build_block(b, block);
    });
    REQUIRE(all_same.size() >= 2);
    CHECK(all_same[0].op() == lauf::asm_op::pop_top);
    CHECK(all_same[0].pop_top.idx == 0);
    CHECK(all_same[1].op() == lauf::asm_op::nop);
}

TEST_CASE("lauf_asm_inst_panic")
{
    auto result = build({1, 0}, [](lauf_asm_module*, lauf_asm_builder* b) {
        lauf_asm_inst_panic(b);
        lauf_asm_build_block(b, lauf_asm_declare_block(b, 0));
    });
    REQUIRE(result.size() == 1);
    CHECK(result[0].op() == lauf::asm_op::panic);
}

TEST_CASE("lauf_asm_inst_sint")
{
    auto build_sint = [](lauf_sint value) {
        return build({0, 1},
                     [=](lauf_asm_module*, lauf_asm_builder* b) { lauf_asm_inst_sint(b, value); });
    };

    auto zero = build_sint(0);
    REQUIRE(zero.size() == 1);
    CHECK(zero[0].op() == lauf::asm_op::push);
    CHECK(zero[0].push.value == 0);

    auto small = build_sint(0x12'3456);
    REQUIRE(small.size() == 1);
    CHECK(small[0].op() == lauf::asm_op::push);
    CHECK(small[0].push.value == 0x12'3456);

    auto max24 = build_sint(0xFF'FFFF);
    REQUIRE(max24.size() == 1);
    CHECK(max24[0].op() == lauf::asm_op::push);
    CHECK(max24[0].push.value == 0xFF'FFFF);

    auto bigger24 = build_sint(0xABFF'FFFF);
    REQUIRE(bigger24.size() == 2);
    CHECK(bigger24[0].op() == lauf::asm_op::push);
    CHECK(bigger24[0].push.value == 0xFF'FFFF);
    CHECK(bigger24[1].op() == lauf::asm_op::push2);
    CHECK(bigger24[1].push2.value == 0xAB);

    auto max48 = build_sint(0xFFFF'FFFF'FFFF);
    REQUIRE(max48.size() == 2);
    CHECK(max48[0].op() == lauf::asm_op::push);
    CHECK(max48[0].push.value == 0xFF'FFFF);
    CHECK(max48[1].op() == lauf::asm_op::push2);
    CHECK(max48[1].push2.value == 0xFF'FFFF);

    auto bigger48 = build_sint(0x0123'4567'89AB'CDEF);
    REQUIRE(bigger48.size() == 3);
    CHECK(bigger48[0].op() == lauf::asm_op::push);
    CHECK(bigger48[0].push.value == 0xAB'CDEF);
    CHECK(bigger48[1].op() == lauf::asm_op::push2);
    CHECK(bigger48[1].push2.value == 0x45'6789);
    CHECK(bigger48[2].op() == lauf::asm_op::push3);
    CHECK(bigger48[2].push2.value == 0x0123);

    auto neg_one = build_sint(-1);
    REQUIRE(neg_one.size() == 1);
    CHECK(neg_one[0].op() == lauf::asm_op::pushn);
    CHECK(neg_one[0].push.value == 0);

    auto neg_small = build_sint(-0x12'3456);
    REQUIRE(neg_small.size() == 1);
    CHECK(neg_small[0].op() == lauf::asm_op::pushn);
    CHECK(neg_small[0].push.value == 0x12'3455);

    auto neg_max24 = build_sint(-0x100'0000);
    REQUIRE(neg_max24.size() == 1);
    CHECK(neg_max24[0].op() == lauf::asm_op::pushn);
    CHECK(neg_max24[0].push.value == 0xFF'FFFF);

    auto neg_bigger24 = build_sint(-0xFFFF'FFFFll);
    REQUIRE(neg_bigger24.size() == 3);
    CHECK(neg_bigger24[0].op() == lauf::asm_op::push);
    CHECK(neg_bigger24[0].push.value == 0x00'0001);
    CHECK(neg_bigger24[1].op() == lauf::asm_op::push2);
    CHECK(neg_bigger24[1].push2.value == 0xFF'FF00);
    CHECK(neg_bigger24[2].op() == lauf::asm_op::push3);
    CHECK(neg_bigger24[2].push3.value == 0xFFFF);
}

TEST_CASE("lauf_asm_inst_uint")
{
    auto build_uint = [](lauf_uint value) {
        return build({0, 1},
                     [=](lauf_asm_module*, lauf_asm_builder* b) { lauf_asm_inst_uint(b, value); });
    };

    auto zero = build_uint(0);
    REQUIRE(zero.size() == 1);
    CHECK(zero[0].op() == lauf::asm_op::push);
    CHECK(zero[0].push.value == 0);

    auto small = build_uint(0x12'3456);
    REQUIRE(small.size() == 1);
    CHECK(small[0].op() == lauf::asm_op::push);
    CHECK(small[0].push.value == 0x12'3456);

    auto max24 = build_uint(0xFF'FFFF);
    REQUIRE(max24.size() == 1);
    CHECK(max24[0].op() == lauf::asm_op::push);
    CHECK(max24[0].push.value == 0xFF'FFFF);

    auto bigger24 = build_uint(0xABFF'FFFF);
    REQUIRE(bigger24.size() == 2);
    CHECK(bigger24[0].op() == lauf::asm_op::push);
    CHECK(bigger24[0].push.value == 0xFF'FFFF);
    CHECK(bigger24[1].op() == lauf::asm_op::push2);
    CHECK(bigger24[1].push2.value == 0xAB);

    auto max48 = build_uint(0xFFFF'FFFF'FFFF);
    REQUIRE(max48.size() == 2);
    CHECK(max48[0].op() == lauf::asm_op::push);
    CHECK(max48[0].push.value == 0xFF'FFFF);
    CHECK(max48[1].op() == lauf::asm_op::push2);
    CHECK(max48[1].push2.value == 0xFF'FFFF);

    auto bigger48 = build_uint(0x0123'4567'89AB'CDEF);
    REQUIRE(bigger48.size() == 3);
    CHECK(bigger48[0].op() == lauf::asm_op::push);
    CHECK(bigger48[0].push.value == 0xAB'CDEF);
    CHECK(bigger48[1].op() == lauf::asm_op::push2);
    CHECK(bigger48[1].push2.value == 0x45'6789);
    CHECK(bigger48[2].op() == lauf::asm_op::push3);
    CHECK(bigger48[2].push2.value == 0x0123);

    auto neg_zero = build_uint(0xFFFF'FFFF'FF00'0000);
    REQUIRE(neg_zero.size() == 1);
    CHECK(neg_zero[0].op() == lauf::asm_op::pushn);
    CHECK(neg_zero[0].push.value == 0xFF'FFFF);

    auto neg_small = build_uint(0xFFFF'FFFF'FF12'3456);
    REQUIRE(neg_small.size() == 1);
    CHECK(neg_small[0].op() == lauf::asm_op::pushn);
    CHECK(neg_small[0].push.value == 0xED'CBA9);

    auto neg_max = build_uint(0xFFFF'FFFF'FFFF'FFFF);
    REQUIRE(neg_max.size() == 1);
    CHECK(neg_max[0].op() == lauf::asm_op::pushn);
    CHECK(neg_max[0].push.value == 0);
}

TEST_CASE("lauf_asm_inst_null")
{
    auto result
        = build({0, 1}, [](lauf_asm_module*, lauf_asm_builder* b) { lauf_asm_inst_null(b); });
    REQUIRE(result.size() == 1);
    CHECK(result[0].op() == lauf::asm_op::pushn);
    CHECK(result[0].push.value == 0);
}

TEST_CASE("lauf_asm_inst_global_addr")
{
    auto single = build({0, 1}, [](lauf_asm_module* mod, lauf_asm_builder* b) {
        auto glob = lauf_asm_add_global_zero_data(mod, lauf_asm_layout{42, 1});
        lauf_asm_inst_global_addr(b, glob);
    });
    REQUIRE(single.size() == 1);
    CHECK(single[0].op() == lauf::asm_op::global_addr);
    CHECK(single[0].global_addr.value == 0);

    auto multiple = build({0, 1}, [](lauf_asm_module* mod, lauf_asm_builder* b) {
        lauf_asm_add_global_zero_data(mod, lauf_asm_layout{11, 1});
        auto glob = lauf_asm_add_global_zero_data(mod, lauf_asm_layout{42, 1});
        lauf_asm_add_global_zero_data(mod, lauf_asm_layout{66, 1});
        lauf_asm_inst_global_addr(b, glob);
    });
    REQUIRE(multiple.size() == 1);
    CHECK(multiple[0].op() == lauf::asm_op::global_addr);
    CHECK(multiple[0].global_addr.value == 1);
}

TEST_CASE("lauf_asm_inst_local_addr")
{
    auto single = build({0, 1}, [](lauf_asm_module*, lauf_asm_builder* b) {
        auto loc = lauf_asm_build_local(b, {8, 8});
        lauf_asm_inst_local_addr(b, loc);
    });
    REQUIRE(single.size() == 1);
    CHECK(single[0].op() == lauf::asm_op::local_addr);
    CHECK(single[0].local_addr.value == 0);

    auto multiple = build({0, 1}, [](lauf_asm_module*, lauf_asm_builder* b) {
        lauf_asm_build_local(b, {8, 8});
        auto loc = lauf_asm_build_local(b, {8, 8});
        lauf_asm_inst_local_addr(b, loc);
        lauf_asm_build_local(b, {8, 8});
    });
    REQUIRE(multiple.size() == 1);
    CHECK(multiple[0].op() == lauf::asm_op::local_addr);
    CHECK(multiple[0].local_addr.value == 1);
}

TEST_CASE("lauf_asm_inst_cc")
{
    auto dynamic = build({1, 1}, [](lauf_asm_module*, lauf_asm_builder* b) {
        lauf_asm_inst_cc(b, LAUF_ASM_INST_CC_EQ);
    });
    REQUIRE(dynamic.size() == 1);
    CHECK(dynamic[0].op() == lauf::asm_op::cc);
    CHECK(dynamic[0].cc.value == LAUF_ASM_INST_CC_EQ);

    auto constant = build({0, 1}, [](lauf_asm_module*, lauf_asm_builder* b) {
        lauf_asm_inst_sint(b, -1);
        lauf_asm_inst_cc(b, LAUF_ASM_INST_CC_EQ);
    });
    REQUIRE(constant.size() == 1);
    CHECK(constant[0].op() == lauf::asm_op::push);
    CHECK(constant[0].push.value == 0);
}

TEST_CASE("lauf_asm_inst_function_addr")
{
    auto result = build({0, 1}, [](lauf_asm_module* mod, lauf_asm_builder* b) {
        auto f = lauf_asm_add_function(mod, "a", {11, 5});
        lauf_asm_inst_function_addr(b, f);
    });
    REQUIRE(result.size() == 1);
    CHECK(result[0].op() == lauf::asm_op::function_addr);
    // Cannot check offset.
}

TEST_CASE("lauf_asm_inst_pop")
{
    auto pop0
        = build({3, 2}, [](lauf_asm_module*, lauf_asm_builder* b) { lauf_asm_inst_pop(b, 0); });
    REQUIRE(pop0.size() == 1);
    CHECK(pop0[0].op() == lauf::asm_op::pop_top);
    CHECK(pop0[0].pop.idx == 0);

    auto pop2
        = build({3, 2}, [](lauf_asm_module*, lauf_asm_builder* b) { lauf_asm_inst_pop(b, 2); });
    REQUIRE(pop2.size() == 1);
    CHECK(pop2[0].op() == lauf::asm_op::pop);
    CHECK(pop2[0].pop.idx == 2);

    auto pop_push = build({0, 0}, [](lauf_asm_module*, lauf_asm_builder* b) {
        lauf_asm_inst_uint(b, 0);
        lauf_asm_inst_pop(b, 0);
    });
    REQUIRE(pop_push.empty());
    auto pop_pushn = build({0, 0}, [](lauf_asm_module*, lauf_asm_builder* b) {
        lauf_asm_inst_sint(b, -1);
        lauf_asm_inst_pop(b, 0);
    });
    REQUIRE(pop_pushn.empty());
    auto pop_global_addr = build({0, 0}, [](lauf_asm_module* mod, lauf_asm_builder* b) {
        auto glob = lauf_asm_add_global_zero_data(mod, {1, 1});
        lauf_asm_inst_global_addr(b, glob);
        lauf_asm_inst_pop(b, 0);
    });
    REQUIRE(pop_global_addr.empty());
    auto pop_function_addr = build({0, 0}, [](lauf_asm_module* mod, lauf_asm_builder* b) {
        auto fn = lauf_asm_add_function(mod, "fn", {1, 1});
        lauf_asm_inst_function_addr(b, fn);
        lauf_asm_inst_pop(b, 0);
    });
    REQUIRE(pop_function_addr.empty());
    auto pop_pick0 = build({1, 1}, [](lauf_asm_module*, lauf_asm_builder* b) {
        lauf_asm_inst_pick(b, 0);
        lauf_asm_inst_pop(b, 0);
    });
    REQUIRE(pop_pick0.empty());
    auto pop_pick1 = build({2, 2}, [](lauf_asm_module*, lauf_asm_builder* b) {
        lauf_asm_inst_pick(b, 1);
        lauf_asm_inst_pop(b, 0);
    });
    REQUIRE(pop_pick1.empty());
    auto pop_load = build({0, 0}, [](lauf_asm_module*, lauf_asm_builder* b) {
        auto loc = lauf_asm_build_local(b, lauf_asm_type_value.layout);
        lauf_asm_inst_local_addr(b, loc);
        lauf_asm_inst_load_field(b, lauf_asm_type_value, 0);
        lauf_asm_inst_pop(b, 0);
    });
    REQUIRE(pop_load.empty());

    auto pop_push2_3 = build({0, 0}, [](lauf_asm_module*, lauf_asm_builder* b) {
        lauf_asm_inst_uint(b, 0xFFFF'FFFF'FFFFF);
        lauf_asm_inst_pop(b, 0);
    });
    REQUIRE(pop_push2_3.empty());
    auto pop_aggregate_member = build({0, 0}, [](lauf_asm_module*, lauf_asm_builder* b) {
        auto loc = lauf_asm_build_local(b, lauf_asm_type_value.layout);
        lauf_asm_inst_local_addr(b, loc);
        lauf_asm_inst_aggregate_member(b, 0, &lauf_asm_type_value.layout, 1);
        lauf_asm_inst_pop(b, 0);
    });
    REQUIRE(pop_aggregate_member.empty());

    auto pop_array_element = build({0, 0}, [](lauf_asm_module*, lauf_asm_builder* b) {
        auto loc = lauf_asm_build_local(b, lauf_asm_type_value.layout);
        lauf_asm_inst_local_addr(b, loc);
        lauf_asm_inst_uint(b, 1);
        lauf_asm_inst_array_element(b, lauf_asm_type_value.layout);
        lauf_asm_inst_pop(b, 0);
    });
    REQUIRE(pop_array_element.empty());
}

TEST_CASE("lauf_asm_inst_pick")
{
    auto pick0
        = build({3, 4}, [](lauf_asm_module*, lauf_asm_builder* b) { lauf_asm_inst_pick(b, 0); });
    REQUIRE(pick0.size() == 1);
    CHECK(pick0[0].op() == lauf::asm_op::dup);
    CHECK(pick0[0].pick.idx == 0);

    auto pick2
        = build({3, 4}, [](lauf_asm_module*, lauf_asm_builder* b) { lauf_asm_inst_pick(b, 2); });
    REQUIRE(pick2.size() == 1);
    CHECK(pick2[0].op() == lauf::asm_op::pick);
    CHECK(pick2[0].pick.idx == 2);
}

TEST_CASE("lauf_asm_inst_roll")
{
    auto roll0
        = build({3, 3}, [](lauf_asm_module*, lauf_asm_builder* b) { lauf_asm_inst_roll(b, 0); });
    REQUIRE(roll0.empty());

    auto roll1
        = build({3, 3}, [](lauf_asm_module*, lauf_asm_builder* b) { lauf_asm_inst_roll(b, 1); });
    REQUIRE(roll1.size() == 1);
    CHECK(roll1[0].op() == lauf::asm_op::swap);
    CHECK(roll1[0].roll.idx == 1);

    auto roll2
        = build({3, 3}, [](lauf_asm_module*, lauf_asm_builder* b) { lauf_asm_inst_roll(b, 2); });
    REQUIRE(roll2.size() == 1);
    CHECK(roll2[0].op() == lauf::asm_op::roll);
    CHECK(roll2[0].roll.idx == 2);
}

TEST_CASE("lauf_asm_inst_call")
{
    auto regular = build({3, 5}, [](lauf_asm_module* mod, lauf_asm_builder* b) {
        auto f = lauf_asm_add_function(mod, "a", {3, 5});
        lauf_asm_inst_call(b, f);
    });
    REQUIRE(regular.size() == 1);
    CHECK(regular[0].op() == lauf::asm_op::call);
    // cannot check offset
}

TEST_CASE("lauf_asm_inst_call_indirect")
{
    auto regular = build({4, 5}, [](lauf_asm_module*, lauf_asm_builder* b) {
        lauf_asm_inst_call_indirect(b, {3, 5});
    });
    REQUIRE(regular.size() == 1);
    CHECK(regular[0].op() == lauf::asm_op::call_indirect);
    CHECK(regular[0].call_indirect.input_count == 3);
    CHECK(regular[0].call_indirect.output_count == 5);

    auto constant = build({0, 5}, [](lauf_asm_module* mod, lauf_asm_builder* b) {
        auto f = lauf_asm_add_function(mod, "a", {0, 5});
        lauf_asm_inst_function_addr(b, f);
        lauf_asm_inst_call_indirect(b, {0, 5});
    });
    REQUIRE(constant.size() == 1);
    CHECK(constant[0].op() == lauf::asm_op::call);
}

TEST_CASE("lauf_asm_inst_call_builtin")
{
    auto normal = build({1, 0}, [](lauf_asm_module*, lauf_asm_builder* b) {
        lauf_asm_inst_call_builtin(b, lauf_lib_test_assert);
    });
    REQUIRE(normal.size() == 1);
    CHECK(normal[0].op() == lauf::asm_op::call_builtin);
    // cannot check offset
}

TEST_CASE("lauf_asm_inst_array_element")
{
    auto normal = build({2, 1}, [](lauf_asm_module*, lauf_asm_builder* b) {
        lauf_asm_inst_array_element(b, {8, 8});
    });
    REQUIRE(normal.size() == 1);
    CHECK(normal[0].op() == lauf::asm_op::array_element);
    CHECK(normal[0].array_element.value == 8);

    auto alignment = build({2, 1}, [](lauf_asm_module*, lauf_asm_builder* b) {
        lauf_asm_inst_array_element(b, {4, 8});
    });
    REQUIRE(alignment.size() == 1);
    CHECK(alignment[0].op() == lauf::asm_op::array_element);
    CHECK(alignment[0].array_element.value == 8);

    auto constant_zero = build({1, 1}, [](lauf_asm_module*, lauf_asm_builder* b) {
        lauf_asm_inst_uint(b, 0);
        lauf_asm_inst_array_element(b, {8, 8});
    });
    REQUIRE(constant_zero.empty());

    auto constant = build({1, 1}, [](lauf_asm_module*, lauf_asm_builder* b) {
        lauf_asm_inst_uint(b, 2);
        lauf_asm_inst_array_element(b, {8, 8});
    });
    REQUIRE(constant.size() == 1);
    CHECK(constant[0].op() == lauf::asm_op::aggregate_member);
    CHECK(constant[0].aggregate_member.value == 16);
}

TEST_CASE("lauf_asm_inst_aggregate_member")
{
    lauf_asm_layout agg[] = {{4, 4}, {8, 8}, {4, 4}};

    auto first = build({1, 1}, [&](lauf_asm_module*, lauf_asm_builder* b) {
        lauf_asm_inst_aggregate_member(b, 0, agg, 3);
    });
    REQUIRE(first.empty());

    auto second = build({1, 1}, [&](lauf_asm_module*, lauf_asm_builder* b) {
        lauf_asm_inst_aggregate_member(b, 1, agg, 3);
    });
    REQUIRE(second.size() == 1);
    CHECK(second[0].op() == lauf::asm_op::aggregate_member);
    CHECK(second[0].aggregate_member.value == 8);

    auto third = build({1, 1}, [&](lauf_asm_module*, lauf_asm_builder* b) {
        lauf_asm_inst_aggregate_member(b, 2, agg, 3);
    });
    REQUIRE(third.size() == 1);
    CHECK(third[0].op() == lauf::asm_op::aggregate_member);
    CHECK(third[0].aggregate_member.value == 16);
}
