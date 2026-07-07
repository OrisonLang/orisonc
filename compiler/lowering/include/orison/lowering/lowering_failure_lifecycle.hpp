#pragma once

#include "orison/lowering/lowering_failures.hpp"

#include <string>
#include <utility>

namespace orison::lowering {

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
