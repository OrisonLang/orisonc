#pragma once

#include "orison/pipeline/compile_pipeline.hpp"

#include <string>
#include <vector>

namespace orison::driver {

auto computed_cleanup_call_insertion_capability_report(
    pipeline::ComputedCleanupCallInsertionCapabilityState const& state
) -> std::vector<std::string>;

auto computed_cleanup_call_insertion_readiness_report(
    pipeline::ComputedCleanupCallInsertionGateState const& state
) -> std::vector<std::string>;

auto computed_inserted_cleanup_handoff_state_report(
    pipeline::ComputedInsertedCleanupHandoffState const& state
) -> std::vector<std::string>;

auto computed_cleanup_call_blocker_summary_report(
    pipeline::ComputedInsertedCleanupHandoffState const& state
) -> std::vector<std::string>;

auto computed_inserted_cleanup_call_state_report(
    pipeline::ComputedInsertedCleanupCallState const& state
) -> std::vector<std::string>;

auto computed_consumed_cleanup_descriptor_state_report(
    pipeline::ComputedConsumedCleanupDescriptorState const& state
) -> std::vector<std::string>;

auto computed_cleanup_proof_summary_state_report(
    pipeline::ComputedCleanupProofSummaryState const& state
) -> std::vector<std::string>;

}  // namespace orison::driver
