#include "orison/lowering/lowering_diagnostics.hpp"

#include <string>
#include <utility>

namespace orison::lowering {
auto append_lowering_detail(std::string prefix, std::string const& detail) -> std::string {
    return detail.empty() ? prefix : prefix + ": " + detail;
}

auto append_expression_lowering_failure(
    std::string prefix,
    ExpressionLoweringFailure const& failure
) -> std::string {
    return append_lowering_detail(std::move(prefix), render_expression_lowering_failure(failure));
}

auto append_control_flow_lowering_failure(
    std::string prefix,
    ControlFlowLoweringFailure const& failure
) -> std::string {
    return append_lowering_detail(std::move(prefix), render_control_flow_lowering_failure(failure));
}

auto append_control_flow_lowering_failure(
    std::string prefix,
    ControlFlowLoweringFailureReason reason,
    std::string detail
) -> std::string {
    return append_lowering_detail(
        std::move(prefix),
        render_control_flow_lowering_failure(reason, std::move(detail))
    );
}

auto expression_lowering_failure_detail(
    ExpressionLoweringFailure const& failure
) -> std::string {
    return render_expression_lowering_failure(failure);
}

auto render_expression_lowering_failure(
    ExpressionLoweringFailure const& failure
) -> std::string {
    auto prefix = std::string {};
    switch (failure.reason) {
    case ExpressionLoweringFailureReason::none:
        return {};
    case ExpressionLoweringFailureReason::unsupported_expression:
        prefix = "unsupported expression";
        break;
    case ExpressionLoweringFailureReason::missing_string_constant:
        prefix = "missing lowered string constant";
        break;
    case ExpressionLoweringFailureReason::unknown_name:
        prefix = "unknown lowered name";
        break;
    case ExpressionLoweringFailureReason::unknown_member_call_receiver:
        prefix = "unknown lowered member call receiver";
        break;
    case ExpressionLoweringFailureReason::unknown_member_call_target:
        prefix = "unknown lowered member call target";
        break;
    case ExpressionLoweringFailureReason::ambiguous_member_call_target:
        prefix = "ambiguous lowered member call target";
        break;
    case ExpressionLoweringFailureReason::type_mismatch:
        prefix = "expression type mismatch";
        break;
    case ExpressionLoweringFailureReason::signedness_mismatch:
        prefix = "expression signedness mismatch";
        break;
    case ExpressionLoweringFailureReason::unsupported_cast:
        prefix = "unsupported cast";
        break;
    case ExpressionLoweringFailureReason::unsupported_operator:
        prefix = "unsupported operator";
        break;
    case ExpressionLoweringFailureReason::unsupported_concurrency_expression:
        prefix = "unsupported concurrency expression";
        break;
    case ExpressionLoweringFailureReason::cannot_infer_operand_type:
        prefix = "cannot infer operand type";
        break;
    case ExpressionLoweringFailureReason::branch_type_mismatch:
        prefix = "branch type mismatch";
        break;
    case ExpressionLoweringFailureReason::unknown_function:
        prefix = "unknown lowered function";
        break;
    case ExpressionLoweringFailureReason::call_return_type_mismatch:
        prefix = "call return type mismatch";
        break;
    case ExpressionLoweringFailureReason::call_arity_mismatch:
        prefix = "call arity mismatch";
        break;
    case ExpressionLoweringFailureReason::call_argument_failure:
        prefix = "call argument lowering failed";
        break;
    }
    return append_lowering_detail(std::move(prefix), failure.detail);
}

auto render_expression_lowering_failure(
    ExpressionLoweringFailureReason reason,
    std::string detail
) -> std::string {
    return render_expression_lowering_failure(ExpressionLoweringFailure {
        .reason = reason,
        .detail = std::move(detail),
    });
}

auto render_control_flow_lowering_failure(
    ControlFlowLoweringFailure const& failure
) -> std::string {
    auto prefix = std::string {};
    switch (failure.reason) {
    case ControlFlowLoweringFailureReason::none:
        return {};
    case ControlFlowLoweringFailureReason::invalid_if_shape:
        prefix = "invalid final if shape";
        break;
    case ControlFlowLoweringFailureReason::if_condition_failure:
        prefix = "if condition lowering failed";
        break;
    case ControlFlowLoweringFailureReason::if_then_arm_failure:
        prefix = "if then arm lowering failed";
        break;
    case ControlFlowLoweringFailureReason::if_else_arm_failure:
        prefix = "if else arm lowering failed";
        break;
    case ControlFlowLoweringFailureReason::if_branch_type_mismatch:
        prefix = "if branch type mismatch";
        break;
    case ControlFlowLoweringFailureReason::invalid_switch_shape:
        prefix = "invalid final switch shape";
        break;
    case ControlFlowLoweringFailureReason::switch_subject_type_failure:
        prefix = "switch subject type is not lowerable";
        break;
    case ControlFlowLoweringFailureReason::switch_subject_failure:
        prefix = "switch subject lowering failed";
        break;
    case ControlFlowLoweringFailureReason::duplicate_switch_default:
        prefix = "switch has multiple default cases";
        break;
    case ControlFlowLoweringFailureReason::unsupported_switch_pattern:
        prefix = "switch pattern lowering failed";
        break;
    case ControlFlowLoweringFailureReason::switch_case_failure:
        prefix = "switch case lowering failed";
        break;
    case ControlFlowLoweringFailureReason::switch_case_type_mismatch:
        prefix = "switch case type mismatch";
        break;
    }
    return append_lowering_detail(std::move(prefix), failure.detail);
}

auto render_control_flow_lowering_failure(
    ControlFlowLoweringFailureReason reason,
    std::string detail
) -> std::string {
    return render_control_flow_lowering_failure(ControlFlowLoweringFailure {
        .reason = reason,
        .detail = std::move(detail),
    });
}

}  // namespace orison::lowering
