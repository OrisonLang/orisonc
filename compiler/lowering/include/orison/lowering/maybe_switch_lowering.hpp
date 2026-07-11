#pragma once

#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/switch_plan.hpp"

#include <iosfwd>
#include <optional>
#include <string_view>

namespace orison::lowering {

auto is_maybe_switch_subject(LoweredType const& type) -> bool;

auto is_supported_switch_subject(
    LoweredType const& type,
    LoweringEmissionContext const& context,
    std::optional<std::string_view> subject_source_type_name = std::nullopt
) -> bool;

auto switch_subject_for_emit(
    LoweredExpression subject,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output,
    std::optional<std::string_view> subject_source_type_name = std::nullopt
) -> LoweredExpression;

void bind_switch_payload(
    LoweredSwitchCasePlan const& planned_case,
    LoweredExpression const& subject,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output,
    std::optional<std::string_view> subject_source_type_name = std::nullopt
);

}  // namespace orison::lowering
