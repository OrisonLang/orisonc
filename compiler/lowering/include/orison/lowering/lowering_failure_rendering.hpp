#pragma once

#include "orison/lowering/lowering_failures.hpp"

#include <string>

namespace orison::lowering {

auto render_expression_lowering_failure(
    ExpressionLoweringFailure const& failure
) -> std::string;

auto render_expression_lowering_failure(
    ExpressionLoweringFailureReason reason,
    std::string detail = {}
) -> std::string;

auto render_control_flow_lowering_failure(
    ControlFlowLoweringFailure const& failure
) -> std::string;

auto render_control_flow_lowering_failure(
    ControlFlowLoweringFailureReason reason,
    std::string detail = {}
) -> std::string;

}  // namespace orison::lowering
