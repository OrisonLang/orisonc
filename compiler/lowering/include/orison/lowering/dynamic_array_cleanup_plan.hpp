#pragma once

#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowering_emission_context.hpp"

#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace orison::lowering {

struct BoundDynamicArrayParameterCleanupPlan {
    DynamicArrayDescriptorCleanupPlan descriptor_cleanup;
    std::optional<std::string> element_drop_symbol_name;
};

auto plan_bound_dynamic_array_parameter_cleanups(
    LoweringEmissionContext const& context,
    FunctionLoweringSession const& session
) -> std::optional<std::vector<BoundDynamicArrayParameterCleanupPlan>>;

auto emit_bound_dynamic_array_parameter_cleanups(
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output
) -> bool;

}  // namespace orison::lowering
