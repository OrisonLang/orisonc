#pragma once

#include <string>

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
    use_after_move,
    cannot_infer_operand_type,
    branch_type_mismatch,
    generic_record_constructor_inference_failed,
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

}  // namespace orison::lowering
