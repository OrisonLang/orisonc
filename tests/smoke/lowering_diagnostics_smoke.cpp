#include "orison/lowering/lowering_diagnostics.hpp"

#include <array>
#include <cassert>
#include <string_view>
#include <utility>

int main() {
    using orison::lowering::ControlFlowLoweringFailure;
    using orison::lowering::ControlFlowLoweringFailureReason;
    using orison::lowering::ExpressionLoweringFailure;
    using orison::lowering::ExpressionLoweringFailureReason;

    assert(orison::lowering::render_expression_lowering_failure({}).empty());
    assert(orison::lowering::render_control_flow_lowering_failure({}).empty());

    auto expression_cases = std::array {
        std::pair {ExpressionLoweringFailureReason::unsupported_expression, "unsupported expression"},
        std::pair {ExpressionLoweringFailureReason::missing_string_constant, "missing lowered string constant"},
        std::pair {ExpressionLoweringFailureReason::unknown_name, "unknown lowered name"},
        std::pair {ExpressionLoweringFailureReason::type_mismatch, "expression type mismatch"},
        std::pair {ExpressionLoweringFailureReason::signedness_mismatch, "expression signedness mismatch"},
        std::pair {ExpressionLoweringFailureReason::unsupported_cast, "unsupported cast"},
        std::pair {ExpressionLoweringFailureReason::unsupported_operator, "unsupported operator"},
        std::pair {ExpressionLoweringFailureReason::cannot_infer_operand_type, "cannot infer operand type"},
        std::pair {ExpressionLoweringFailureReason::branch_type_mismatch, "branch type mismatch"},
        std::pair {ExpressionLoweringFailureReason::unknown_function, "unknown lowered function"},
        std::pair {ExpressionLoweringFailureReason::call_return_type_mismatch, "call return type mismatch"},
        std::pair {ExpressionLoweringFailureReason::call_arity_mismatch, "call arity mismatch"},
        std::pair {ExpressionLoweringFailureReason::call_argument_failure, "call argument lowering failed"},
    };
    for (auto const& [reason, expected] : expression_cases) {
        assert(
            orison::lowering::render_expression_lowering_failure(
                ExpressionLoweringFailure {.reason = reason}
            ) == expected
        );
    }
    assert(
        orison::lowering::render_expression_lowering_failure(
            ExpressionLoweringFailure {
                .reason = ExpressionLoweringFailureReason::unknown_name,
                .detail = "value",
            }
        ) == "unknown lowered name: value"
    );

    auto control_flow_cases = std::array {
        std::pair {ControlFlowLoweringFailureReason::invalid_if_shape, "invalid final if shape"},
        std::pair {ControlFlowLoweringFailureReason::if_condition_failure, "if condition lowering failed"},
        std::pair {ControlFlowLoweringFailureReason::if_then_arm_failure, "if then arm lowering failed"},
        std::pair {ControlFlowLoweringFailureReason::if_else_arm_failure, "if else arm lowering failed"},
        std::pair {ControlFlowLoweringFailureReason::if_branch_type_mismatch, "if branch type mismatch"},
        std::pair {ControlFlowLoweringFailureReason::invalid_switch_shape, "invalid final switch shape"},
        std::pair {ControlFlowLoweringFailureReason::switch_subject_type_failure, "switch subject type is not lowerable"},
        std::pair {ControlFlowLoweringFailureReason::switch_subject_failure, "switch subject lowering failed"},
        std::pair {ControlFlowLoweringFailureReason::duplicate_switch_default, "switch has multiple default cases"},
        std::pair {ControlFlowLoweringFailureReason::unsupported_switch_pattern, "switch pattern lowering failed"},
        std::pair {ControlFlowLoweringFailureReason::switch_case_failure, "switch case lowering failed"},
        std::pair {ControlFlowLoweringFailureReason::switch_case_type_mismatch, "switch case type mismatch"},
    };
    for (auto const& [reason, expected] : control_flow_cases) {
        assert(
            orison::lowering::render_control_flow_lowering_failure(
                ControlFlowLoweringFailure {.reason = reason}
            ) == expected
        );
    }
    assert(
        orison::lowering::render_control_flow_lowering_failure(
            ControlFlowLoweringFailure {
                .reason = ControlFlowLoweringFailureReason::if_condition_failure,
                .detail = "unknown lowered name: flag",
            }
        ) == "if condition lowering failed: unknown lowered name: flag"
    );
    return 0;
}
