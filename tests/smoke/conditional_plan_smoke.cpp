#include "orison/lowering/conditional_plan.hpp"

#include <cassert>

int main() {
    auto ternary = orison::lowering::plan_conditional(
        orison::lowering::ConditionalPlanKind::ternary,
        2
    );
    assert(ternary.then_block == "ternary.then.2");
    assert(ternary.else_block == "ternary.else.2");
    assert(ternary.merge_block == "ternary.merge.2");

    auto if_statement = orison::lowering::plan_conditional(
        orison::lowering::ConditionalPlanKind::if_statement,
        7
    );
    assert(if_statement.then_block == "if.then.7");
    assert(if_statement.else_block == "if.else.7");
    assert(if_statement.merge_block == "if.merge.7");
    return 0;
}
