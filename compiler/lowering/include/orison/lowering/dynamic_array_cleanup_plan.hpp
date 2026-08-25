#pragma once

#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/dynamic_array_cleanup_capability.hpp"
#include "orison/lowering/dynamic_array_cleanup_metadata.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/drop_metadata.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowering_emission_context.hpp"

#include <ostream>
#include <string>
#include <vector>

namespace orison::lowering {

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

auto verify_dynamic_array_cleanup_sequence_plan(
    DynamicArrayCleanupSequencePlan const& plan
) -> DynamicArrayCleanupSequenceVerification;

auto verify_dynamic_array_cleanup_sequence_plans(
    std::vector<DynamicArrayCleanupSequencePlan> const& plans
) -> std::vector<DynamicArrayCleanupSequenceVerification>;

auto dynamic_array_cleanup_sequence_verification_passed(
    DynamicArrayCleanupSequenceVerification const& verification
) -> bool;

auto dynamic_array_cleanup_sequence_verification_report_passed(
    std::vector<DynamicArrayCleanupSequenceVerification> const& verifications
) -> bool;

auto format_dynamic_array_cleanup_sequence_verification(
    DynamicArrayCleanupSequenceVerification const& verification
) -> std::string;

auto format_dynamic_array_cleanup_sequence_verification_report(
    std::vector<DynamicArrayCleanupSequenceVerification> const& verifications
) -> std::vector<std::string>;

auto format_dynamic_array_cleanup_emission_gate(
    DynamicArrayCleanupSequenceVerification const& verification
) -> std::string;

auto format_dynamic_array_cleanup_emission_gate_report(
    std::vector<DynamicArrayCleanupSequenceVerification> const& verifications
) -> std::vector<std::string>;

auto plan_bound_dynamic_array_parameter_cleanups(
    LoweringEmissionContext const& context,
    FunctionLoweringSession const& session
) -> std::optional<std::vector<BoundDynamicArrayParameterCleanupPlan>>;

auto prove_bound_dynamic_array_parameter_cleanup_emission_capability(
    LoweringEmissionContext const& context,
    std::vector<BoundDynamicArrayParameterCleanupPlan> const& plans
) -> DynamicArrayCleanupEmissionCapability;

auto plan_local_dynamic_array_cleanups(
    LoweringEmissionContext const& context,
    FunctionLoweringSession const& session
) -> std::optional<std::vector<LocalDynamicArrayCleanupPlan>>;

auto prove_local_dynamic_array_cleanup_emission_capability(
    LoweringEmissionContext const& context,
    std::vector<LocalDynamicArrayCleanupPlan> const& plans
) -> DynamicArrayCleanupEmissionCapability;

auto prove_dynamic_array_cleanup_emission_capability(
    bool emission_enabled,
    std::vector<DynamicArrayDescriptorCleanupPlan> const& descriptor_cleanup_plans,
    std::vector<DynamicArrayCleanupSequenceVerification> const& sequence_verifications,
    std::vector<DynamicArrayCleanupObligation> const& obligations,
    std::vector<semantics::DropLoweringAuthorization> const& semantic_drop_lowering_authorizations
) -> DynamicArrayCleanupEmissionCapability;

auto dynamic_array_cleanup_emission_capability_proven(
    DynamicArrayCleanupEmissionCapability const& capability
) -> bool;

auto format_dynamic_array_cleanup_emission_capability(
    DynamicArrayCleanupEmissionCapability const& capability
) -> std::string;

auto emit_bound_dynamic_array_parameter_cleanups(
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output
) -> bool;

auto emit_bound_dynamic_array_parameter_cleanups_for_names(
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output,
    std::vector<std::string> const& owner_names
) -> bool;

auto emit_local_dynamic_array_cleanups(
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output
) -> bool;

auto emit_choice_dynamic_array_payload_cleanups(
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output
) -> bool;

auto emit_choice_dynamic_array_payload_cleanups_for_names(
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostream& output,
    std::vector<std::string> const& owner_names
) -> bool;

auto emit_bound_dynamic_array_parameter_cleanup_plans(
    DynamicArrayCleanupEmissionCapability const& capability,
    std::vector<BoundDynamicArrayParameterCleanupPlan> const& plans,
    FunctionLoweringSession& session,
    std::ostream& output
) -> bool;

}  // namespace orison::lowering
