#pragma once

#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/drop_metadata.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowering_emission_context.hpp"

#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace orison::lowering {

struct DynamicArrayCleanupObligation {
    std::string cleanup_symbol_name;
    DynamicArrayDescriptorCleanupPlan descriptor_cleanup;
    std::vector<PlannedDropAction> actions;
    bool requires_descriptor_deallocation = true;
};

struct DynamicArrayCleanupSequencePlan {
    DynamicArrayCleanupObligation obligation;
    std::vector<std::string> phases;
};

struct BoundDynamicArrayParameterCleanupPlan {
    DynamicArrayDescriptorCleanupPlan descriptor_cleanup;
    std::optional<std::string> element_drop_symbol_name;
};

auto plan_dynamic_array_descriptor_cleanup_obligation(
    DynamicArrayDescriptorCleanupPlan const& plan,
    std::size_t ordinal
) -> DynamicArrayCleanupObligation;

auto plan_dynamic_array_descriptor_cleanup_obligations(
    std::vector<DynamicArrayDescriptorCleanupPlan> const& plans,
    std::size_t ordinal_offset = 0
) -> std::vector<DynamicArrayCleanupObligation>;

auto drop_cleanup_for_dynamic_array_cleanup_obligation(
    DynamicArrayCleanupObligation const& obligation
) -> ConcurrencyDropCleanupPlan;

auto plan_dynamic_array_cleanup_sequence(
    DynamicArrayCleanupObligation const& obligation
) -> DynamicArrayCleanupSequencePlan;

auto plan_dynamic_array_cleanup_sequences(
    std::vector<DynamicArrayCleanupObligation> const& obligations
) -> std::vector<DynamicArrayCleanupSequencePlan>;

auto format_dynamic_array_cleanup_obligation(
    DynamicArrayCleanupObligation const& obligation
) -> std::string;

auto format_dynamic_array_cleanup_obligation_report(
    std::vector<DynamicArrayCleanupObligation> const& obligations
) -> std::vector<std::string>;

auto format_dynamic_array_cleanup_sequence_plan(
    DynamicArrayCleanupSequencePlan const& plan
) -> std::string;

auto format_dynamic_array_cleanup_sequence_plan_report(
    std::vector<DynamicArrayCleanupSequencePlan> const& plans
) -> std::vector<std::string>;

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
