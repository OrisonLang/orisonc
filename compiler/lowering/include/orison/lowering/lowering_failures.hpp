#pragma once

#include <string>
#include <utility>

namespace orison::lowering {

enum class ExpressionLoweringFailureReason {
    none,
    unsupported_expression,
    missing_string_constant,
    unknown_name,
    unknown_member_call_receiver,
    unknown_member_call_target,
    ambiguous_member_call_target,
    type_mismatch,
    signedness_mismatch,
    unsupported_cast,
    unsupported_operator,
    unsupported_concurrency_expression,
    cannot_infer_operand_type,
    branch_type_mismatch,
    unknown_function,
    call_return_type_mismatch,
    call_arity_mismatch,
    call_argument_failure,
};

struct ExpressionLoweringFailure {
    ExpressionLoweringFailureReason reason = ExpressionLoweringFailureReason::none;
    std::string detail;
};

enum class ControlFlowLoweringFailureReason {
    none,
    invalid_if_shape,
    if_condition_failure,
    if_then_arm_failure,
    if_else_arm_failure,
    if_branch_type_mismatch,
    invalid_switch_shape,
    switch_subject_type_failure,
    switch_subject_failure,
    duplicate_switch_default,
    unsupported_switch_pattern,
    switch_case_failure,
    switch_case_type_mismatch,
};

struct ControlFlowLoweringFailure {
    ControlFlowLoweringFailureReason reason = ControlFlowLoweringFailureReason::none;
    std::string detail;
};

struct LoweringFailures {
    ExpressionLoweringFailure expression;
    ControlFlowLoweringFailure control_flow;
};

inline auto reset_expression_lowering_failure(LoweringFailures& failures) -> void {
    failures.expression = {};
}

inline auto reset_control_flow_lowering_failure(LoweringFailures& failures) -> void {
    failures.control_flow = {};
}

inline auto record_expression_lowering_failure(
    LoweringFailures& failures,
    ExpressionLoweringFailureReason reason,
    std::string detail = {}
) -> bool {
    if (failures.expression.reason != ExpressionLoweringFailureReason::none) {
        return false;
    }
    failures.expression = ExpressionLoweringFailure {
        .reason = reason,
        .detail = std::move(detail),
    };
    return true;
}

inline auto record_control_flow_lowering_failure(
    LoweringFailures& failures,
    ControlFlowLoweringFailureReason reason,
    std::string detail = {}
) -> bool {
    if (failures.control_flow.reason != ControlFlowLoweringFailureReason::none) {
        return false;
    }
    failures.control_flow = ControlFlowLoweringFailure {
        .reason = reason,
        .detail = std::move(detail),
    };
    return true;
}

}  // namespace orison::lowering
