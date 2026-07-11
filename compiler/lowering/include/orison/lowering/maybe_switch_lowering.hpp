#pragma once

#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/switch_plan.hpp"

#include <iosfwd>

namespace orison::lowering {

auto is_maybe_switch_subject(LoweredType const& type) -> bool;

auto is_supported_switch_subject(
    LoweredType const& type,
    LoweringEmissionContext const& context
) -> bool;

auto switch_subject_for_emit(
    LoweredExpression subject,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output
) -> LoweredExpression;

void bind_maybe_switch_payload(
    LoweredSwitchCasePlan const& planned_case,
    LoweredExpression const& subject,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output
);

}  // namespace orison::lowering
