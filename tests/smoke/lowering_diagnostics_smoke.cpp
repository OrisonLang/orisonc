#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/lowering_failures.hpp"

#include <array>
#include <cassert>
#include <string_view>
#include <utility>

int main() {
    using orison::lowering::ControlFlowLoweringFailure;
    using orison::lowering::ControlFlowLoweringFailureReason;
    using orison::lowering::ExpressionLoweringFailure;
    using orison::lowering::ExpressionLoweringFailureReason;
    using orison::lowering::LoweringFailures;

    assert(orison::lowering::render_expression_lowering_failure({}).empty());
    assert(orison::lowering::render_control_flow_lowering_failure({}).empty());
    assert(orison::lowering::append_lowering_detail("lowering failed", "") == "lowering failed");
    assert(
        orison::lowering::append_lowering_detail("lowering failed", "unknown lowered name: value") ==
        "lowering failed: unknown lowered name: value"
    );

    auto expression_cases = std::array {
        std::pair {ExpressionLoweringFailureReason::unsupported_expression, "unsupported expression"},
        std::pair {ExpressionLoweringFailureReason::missing_string_constant, "missing lowered string constant"},
        std::pair {ExpressionLoweringFailureReason::unknown_name, "unknown lowered name"},
        std::pair {
            ExpressionLoweringFailureReason::unknown_member_call_receiver,
            "unknown lowered member call receiver"
        },
        std::pair {
            ExpressionLoweringFailureReason::unknown_member_call_target,
            "unknown lowered member call target"
        },
        std::pair {
            ExpressionLoweringFailureReason::ambiguous_member_call_target,
            "ambiguous lowered member call target"
        },
        std::pair {ExpressionLoweringFailureReason::type_mismatch, "expression type mismatch"},
        std::pair {ExpressionLoweringFailureReason::signedness_mismatch, "expression signedness mismatch"},
        std::pair {ExpressionLoweringFailureReason::unsupported_cast, "unsupported cast"},
        std::pair {ExpressionLoweringFailureReason::unsupported_operator, "unsupported operator"},
        std::pair {
            ExpressionLoweringFailureReason::unsupported_concurrency_expression,
            "unsupported concurrency expression"
        },
        std::pair {ExpressionLoweringFailureReason::cannot_infer_operand_type, "cannot infer operand type"},
        std::pair {ExpressionLoweringFailureReason::branch_type_mismatch, "branch type mismatch"},
        std::pair {ExpressionLoweringFailureReason::unknown_function, "unknown lowered function"},
        std::pair {ExpressionLoweringFailureReason::call_return_type_mismatch, "call return type mismatch"},
        std::pair {ExpressionLoweringFailureReason::call_arity_mismatch, "call arity mismatch"},
        std::pair {ExpressionLoweringFailureReason::call_argument_failure, "call argument lowering failed"},
    };
    for (auto const& [reason, expected] : expression_cases) {
        assert(
            orison::lowering::render_expression_lowering_failure(reason) == expected
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
    assert(
        orison::lowering::render_expression_lowering_failure(
            ExpressionLoweringFailureReason::unknown_name,
            "value"
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
    assert(
        orison::lowering::render_control_flow_lowering_failure(
            ControlFlowLoweringFailureReason::unsupported_switch_pattern,
            "case label"
        ) == "switch pattern lowering failed: case label"
    );

    auto failures = LoweringFailures {};
    assert(orison::lowering::record_expression_lowering_failure(
        failures,
        ExpressionLoweringFailureReason::unknown_name,
        "first"
    ));
    assert(!orison::lowering::record_expression_lowering_failure(
        failures,
        ExpressionLoweringFailureReason::unknown_function,
        "second"
    ));
    assert(failures.expression.reason == ExpressionLoweringFailureReason::unknown_name);
    assert(failures.expression.detail == "first");
    orison::lowering::reset_expression_lowering_failure(failures);
    assert(failures.expression.reason == ExpressionLoweringFailureReason::none);
    assert(failures.expression.detail.empty());
    assert(orison::lowering::record_expression_lowering_failure(
        failures,
        ExpressionLoweringFailureReason::unknown_function,
        "after reset"
    ));
    assert(failures.expression.reason == ExpressionLoweringFailureReason::unknown_function);
    assert(failures.expression.detail == "after reset");

    assert(orison::lowering::record_control_flow_lowering_failure(
        failures,
        ControlFlowLoweringFailureReason::if_condition_failure,
        "first control"
    ));
    assert(!orison::lowering::record_control_flow_lowering_failure(
        failures,
        ControlFlowLoweringFailureReason::switch_case_failure,
        "second control"
    ));
    assert(failures.control_flow.reason == ControlFlowLoweringFailureReason::if_condition_failure);
    assert(failures.control_flow.detail == "first control");
    orison::lowering::reset_control_flow_lowering_failure(failures);
    assert(failures.control_flow.reason == ControlFlowLoweringFailureReason::none);
    assert(failures.control_flow.detail.empty());
    assert(orison::lowering::record_control_flow_lowering_failure(
        failures,
        ControlFlowLoweringFailureReason::switch_case_failure,
        "after reset control"
    ));
    assert(failures.control_flow.reason == ControlFlowLoweringFailureReason::switch_case_failure);
    assert(failures.control_flow.detail == "after reset control");
    return 0;
}
