#pragma once

#include "orison/lowering/function_lowering_state.hpp"

#include <string>

namespace orison::lowering {

auto render_expression_lowering_failure(
    ExpressionLoweringFailure const& failure
) -> std::string;

auto render_control_flow_lowering_failure(
    ControlFlowLoweringFailure const& failure
) -> std::string;

}  // namespace orison::lowering
