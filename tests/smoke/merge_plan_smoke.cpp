#include "orison/lowering/merge_plan.hpp"

#include <array>
#include <cassert>

int main() {
    auto left = orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%left",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    };
    auto right = orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%right",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    };
    auto inputs = std::array {
        orison::lowering::BranchMergeInput {
            .value = &left,
            .block = "then",
        },
        orison::lowering::BranchMergeInput {
            .value = &right,
            .block = "else",
        },
    };
    auto result = orison::lowering::plan_branch_merge(inputs);
    assert(result.plan.has_value());
    assert(!result.mismatch.has_value());
    assert(result.plan->result_type.type == "i32");
    assert(
        result.plan->result_type.signedness ==
        orison::lowering::IntegerSignedness::unsigned_integer
    );
    assert(result.plan->incoming.size() == 2);
    assert(result.plan->incoming[0].value == "%left");
    assert(result.plan->incoming[0].block == "then");
    assert(result.plan->incoming[1].value == "%right");
    assert(result.plan->incoming[1].block == "else");

    right.type = "i64";
    auto type_mismatch = orison::lowering::plan_branch_merge(inputs);
    assert(!type_mismatch.plan.has_value());
    assert(type_mismatch.mismatch.has_value());
    assert(type_mismatch.mismatch->expected.type == "i32");
    assert(type_mismatch.mismatch->actual.type == "i64");

    right.type = "i32";
    right.signedness = orison::lowering::IntegerSignedness::signed_integer;
    auto signedness_mismatch = orison::lowering::plan_branch_merge(inputs);
    assert(!signedness_mismatch.plan.has_value());
    assert(signedness_mismatch.mismatch.has_value());
    assert(
        signedness_mismatch.mismatch->expected.signedness ==
        orison::lowering::IntegerSignedness::unsigned_integer
    );
    assert(
        signedness_mismatch.mismatch->actual.signedness ==
        orison::lowering::IntegerSignedness::signed_integer
    );

    assert(!orison::lowering::plan_branch_merge({}).plan.has_value());
    auto null_input = std::array {
        orison::lowering::BranchMergeInput {},
    };
    assert(!orison::lowering::plan_branch_merge(null_input).plan.has_value());
    return 0;
}
