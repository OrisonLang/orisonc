#include "orison/lowering/switch_plan.hpp"

#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/type_lowering.hpp"

#include <string>
#include <utility>

namespace orison::lowering {
namespace {

auto lower_switch_pattern(
    syntax::ExpressionSyntax const& pattern,
    LoweredType const& subject_type,
    LoweringContext const& context
) -> std::optional<LoweredExpression> {
    auto const maybe_subject = subject_type.type.starts_with("{ i1,");
    if (maybe_subject && pattern.kind == syntax::ExpressionKind::name && pattern.text == "Empty") {
        return LoweredExpression {
            .type = "i1",
            .value = "false",
            .signedness = IntegerSignedness::not_integer,
        };
    }
    if (maybe_subject &&
        pattern.kind == syntax::ExpressionKind::call &&
        pattern.left != nullptr &&
        pattern.left->kind == syntax::ExpressionKind::name &&
        pattern.left->text == "Some") {
        return LoweredExpression {
            .type = "i1",
            .value = "true",
            .signedness = IntegerSignedness::not_integer,
        };
    }

    if (auto const* choice = find_lowered_choice_layout_by_llvm_type(context, subject_type.type)) {
        auto const* variant_name = static_cast<std::string const*>(nullptr);
        if (pattern.kind == syntax::ExpressionKind::name) {
            variant_name = &pattern.text;
        } else if (pattern.kind == syntax::ExpressionKind::call &&
                   pattern.left != nullptr &&
                   pattern.left->kind == syntax::ExpressionKind::name) {
            variant_name = &pattern.left->text;
        }
        if (variant_name != nullptr) {
            for (auto const& variant : choice->variants) {
                if (variant.name == *variant_name) {
                    return LoweredExpression {
                        .type = "i32",
                        .value = std::to_string(variant.tag),
                        .signedness = IntegerSignedness::unsigned_integer,
                    };
                }
            }
        }
    }

    if (auto literal = lower_integer_literal(pattern, subject_type.type, subject_type.signedness)) {
        return literal;
    }
    if (auto literal = lower_boolean_literal(pattern, subject_type.type)) {
        return literal;
    }
    if (pattern.kind != syntax::ExpressionKind::cast || pattern.left == nullptr) {
        return std::nullopt;
    }

    auto target_type = syntax::TypeSyntax {
        .name = pattern.text,
    };
    auto cast_type = llvm_type_for(target_type);
    if (!cast_type.has_value() || *cast_type != subject_type.type) {
        return std::nullopt;
    }
    return lower_integer_literal(*pattern.left, subject_type.type, subject_type.signedness);
}

}  // namespace

auto plan_switch(
    std::vector<syntax::SwitchCaseSyntax> const& cases,
    LoweredType const& subject_type,
    LoweringContext const& context,
    std::size_t block_index
) -> SwitchPlanningResult {
    if (cases.empty()) {
        return {
            .failure = ControlFlowLoweringFailureReason::invalid_switch_shape,
        };
    }

    auto plan = SwitchPlan {
        .merge_block = llvm_block_name("switch.merge", block_index),
        .fallback_block = llvm_block_name("switch.unreachable", block_index),
    };
    plan.cases.reserve(cases.size());
    auto value_case_index = std::size_t {0};

    for (auto const& switch_case : cases) {
        auto planned_case = LoweredSwitchCasePlan {
            .syntax = &switch_case,
        };
        if (switch_case.is_default) {
            if (plan.has_default) {
                return {
                    .failure = ControlFlowLoweringFailureReason::duplicate_switch_default,
                };
            }
            planned_case.block = llvm_block_name("switch.default", block_index);
            plan.default_block = planned_case.block;
            plan.has_default = true;
        } else {
            planned_case.block = llvm_block_name("switch.case", block_index, value_case_index++);
            planned_case.pattern = lower_switch_pattern(switch_case.pattern, subject_type, context);
            if (!planned_case.pattern.has_value()) {
                return {
                    .failure = ControlFlowLoweringFailureReason::unsupported_switch_pattern,
                };
            }
        }
        plan.cases.push_back(std::move(planned_case));
    }

    if (!plan.has_default) {
        plan.default_block = plan.fallback_block;
    }
    return {
        .plan = std::move(plan),
    };
}

}  // namespace orison::lowering
