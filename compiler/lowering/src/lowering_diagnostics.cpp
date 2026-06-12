#include "orison/lowering/lowering_diagnostics.hpp"

#include <string>
#include <utility>

namespace orison::lowering {
namespace {

auto with_detail(std::string prefix, std::string const& detail) -> std::string {
    return detail.empty() ? prefix : prefix + ": " + detail;
}

}  // namespace

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
    return with_detail(std::move(prefix), failure.detail);
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
    return with_detail(std::move(prefix), failure.detail);
}

}  // namespace orison::lowering
