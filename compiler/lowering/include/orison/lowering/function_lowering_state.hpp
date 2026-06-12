#pragma once

#include "orison/lowering/type_lowering.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>

namespace orison::lowering {

struct LoweredExpression {
    std::string type;
    std::string value;
    IntegerSignedness signedness = IntegerSignedness::not_integer;
};

enum class ExpressionLoweringFailureReason {
    none,
    unsupported_expression,
    missing_string_constant,
    unknown_name,
    type_mismatch,
    signedness_mismatch,
    unsupported_cast,
    unsupported_operator,
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

struct FunctionLoweringState {
    std::unordered_map<std::string, LoweredExpression> immutable_bindings;
    std::unordered_map<std::string, std::size_t> local_name_counts;
    std::size_t next_temporary_index = 0;
    std::size_t next_block_index = 0;
    std::string current_block = "entry";
    ExpressionLoweringFailure expression_failure;
    ControlFlowLoweringFailure control_flow_failure;
};

}  // namespace orison::lowering
