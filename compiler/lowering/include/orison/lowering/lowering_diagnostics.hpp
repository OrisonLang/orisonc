#pragma once

#include "orison/lowering/lowering_failures.hpp"

#include <string>

namespace orison::lowering {

auto append_lowering_detail(
    std::string prefix,
    std::string const& detail
) -> std::string;

auto append_expression_lowering_failure(
    std::string prefix,
    ExpressionLoweringFailure const& failure
) -> std::string;

auto append_control_flow_lowering_failure(
    std::string prefix,
    ControlFlowLoweringFailure const& failure
) -> std::string;

auto append_control_flow_lowering_failure(
    std::string prefix,
    ControlFlowLoweringFailureReason reason,
    std::string detail = {}
) -> std::string;

auto expression_lowering_failure_detail(
    ExpressionLoweringFailure const& failure
) -> std::string;

}  // namespace orison::lowering
