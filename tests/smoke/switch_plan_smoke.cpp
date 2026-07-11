#include "orison/lowering/switch_plan.hpp"

#include <cassert>
#include <memory>
#include <utility>
#include <vector>

int main() {
    auto empty_result = orison::lowering::plan_switch(
        {},
        orison::lowering::LoweredType {
            .type = "i1",
        },
        0
    );
    assert(!empty_result.plan.has_value());
    assert(
        empty_result.failure ==
        orison::lowering::ControlFlowLoweringFailureReason::invalid_switch_shape
    );

    auto cases = std::vector<orison::syntax::SwitchCaseSyntax> {};
    auto first = orison::syntax::SwitchCaseSyntax {};
    first.pattern.kind = orison::syntax::ExpressionKind::integer_literal;
    first.pattern.text = "1";
    cases.push_back(std::move(first));

    auto second = orison::syntax::SwitchCaseSyntax {};
    second.pattern.kind = orison::syntax::ExpressionKind::cast;
    second.pattern.text = "UInt32";
    second.pattern.left = std::make_unique<orison::syntax::ExpressionSyntax>();
    second.pattern.left->kind = orison::syntax::ExpressionKind::integer_literal;
    second.pattern.left->text = "2";
    cases.push_back(std::move(second));

    auto fallback = orison::syntax::SwitchCaseSyntax {};
    fallback.is_default = true;
    cases.push_back(std::move(fallback));

    auto result = orison::lowering::plan_switch(
        cases,
        orison::lowering::LoweredType {
            .type = "i32",
            .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
        },
        3
    );
    assert(result.failure == orison::lowering::ControlFlowLoweringFailureReason::none);
    assert(result.plan.has_value());
    assert(result.plan->merge_block == "switch.merge.3");
    assert(result.plan->fallback_block == "switch.unreachable.3");
    assert(result.plan->default_block == "switch.default.3");
    assert(result.plan->has_default);
    assert(result.plan->cases.size() == 3);
    assert(result.plan->cases[0].syntax == &cases[0]);
    assert(result.plan->cases[0].block == "switch.case.3.0");
    assert(result.plan->cases[0].pattern->value == "1");
    assert(result.plan->cases[1].block == "switch.case.3.1");
    assert(result.plan->cases[1].pattern->value == "2");
    assert(result.plan->cases[2].block == "switch.default.3");
    assert(!result.plan->cases[2].pattern.has_value());

    auto duplicate_default = std::vector<orison::syntax::SwitchCaseSyntax>(2);
    duplicate_default[0].is_default = true;
    duplicate_default[1].is_default = true;
    auto duplicate_result = orison::lowering::plan_switch(
        duplicate_default,
        orison::lowering::LoweredType {
            .type = "i1",
        },
        0
    );
    assert(!duplicate_result.plan.has_value());
    assert(
        duplicate_result.failure ==
        orison::lowering::ControlFlowLoweringFailureReason::duplicate_switch_default
    );

    auto unsupported = std::vector<orison::syntax::SwitchCaseSyntax>(1);
    unsupported.front().pattern.kind = orison::syntax::ExpressionKind::name;
    unsupported.front().pattern.text = "value";
    auto unsupported_result = orison::lowering::plan_switch(
        unsupported,
        orison::lowering::LoweredType {
            .type = "i32",
            .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
        },
        1
    );
    assert(!unsupported_result.plan.has_value());
    assert(
        unsupported_result.failure ==
        orison::lowering::ControlFlowLoweringFailureReason::unsupported_switch_pattern
    );

    auto no_default = std::vector<orison::syntax::SwitchCaseSyntax>(1);
    no_default.front().pattern.kind = orison::syntax::ExpressionKind::boolean_literal;
    no_default.front().pattern.text = "true";
    auto no_default_result = orison::lowering::plan_switch(
        no_default,
        orison::lowering::LoweredType {
            .type = "i1",
        },
        2
    );
    assert(no_default_result.plan.has_value());
    assert(!no_default_result.plan->has_default);
    assert(no_default_result.plan->default_block == "switch.unreachable.2");

    auto maybe_cases = std::vector<orison::syntax::SwitchCaseSyntax> {};
    auto maybe_some = orison::syntax::SwitchCaseSyntax {};
    maybe_some.pattern.kind = orison::syntax::ExpressionKind::call;
    maybe_some.pattern.left = std::make_unique<orison::syntax::ExpressionSyntax>();
    maybe_some.pattern.left->kind = orison::syntax::ExpressionKind::name;
    maybe_some.pattern.left->text = "Some";
    maybe_cases.push_back(std::move(maybe_some));

    auto maybe_empty = orison::syntax::SwitchCaseSyntax {};
    maybe_empty.pattern.kind = orison::syntax::ExpressionKind::name;
    maybe_empty.pattern.text = "Empty";
    maybe_cases.push_back(std::move(maybe_empty));

    auto maybe_result = orison::lowering::plan_switch(
        maybe_cases,
        orison::lowering::LoweredType {
            .type = "{ i1, i32 }",
        },
        4
    );
    assert(maybe_result.failure == orison::lowering::ControlFlowLoweringFailureReason::none);
    assert(maybe_result.plan.has_value());
    assert(maybe_result.plan->cases.size() == 2);
    assert(maybe_result.plan->cases[0].pattern->type == "i1");
    assert(maybe_result.plan->cases[0].pattern->value == "true");
    assert(maybe_result.plan->cases[1].pattern->type == "i1");
    assert(maybe_result.plan->cases[1].pattern->value == "false");
    return 0;
}
