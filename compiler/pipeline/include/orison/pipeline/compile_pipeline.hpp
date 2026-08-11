#pragma once

#include "orison/pipeline/computed_dynamic_array_cleanup_states.hpp"
#include "orison/pipeline/computed_dynamic_array_production_sequence.hpp"
#include "orison/pipeline/compile_pipeline_options.hpp"
#include "orison/pipeline/compile_pipeline_result.hpp"
#include "orison/pipeline/dynamic_array_pipeline_states.hpp"

#include <filesystem>
#include <string>

namespace orison::pipeline {

auto dynamic_array_cleanup_production_ready(
    DynamicArrayCleanupProductionReadiness const& readiness
) -> bool;

auto format_dynamic_array_cleanup_production_readiness(
    DynamicArrayCleanupProductionReadiness const& readiness
) -> std::string;

auto plan_computed_dynamic_array_for_production_readiness(
    ComputedDynamicArrayForProductionEmissionGateState const& gate_state,
    ComputedDynamicArrayForProductionSequenceState const& sequence_state,
    ComputedInsertedCleanupTransitionState const& inserted_transition_state,
    ComputedInsertedCleanupStateVerificationState const& inserted_verification_state,
    ComputedCleanupCallInsertionCapabilityState const& insertion_capability_state
) -> ComputedDynamicArrayForProductionReadiness;

auto computed_dynamic_array_for_production_ready(
    ComputedDynamicArrayForProductionReadiness const& readiness
) -> bool;

class CompilePipeline {
public:
    auto analyze(std::filesystem::path const& source_path) const -> CompilePipelineResult;
    auto analyze(
        std::filesystem::path const& source_path,
        CompilePipelineOptions const& options
    ) const -> CompilePipelineResult;
    auto emit_llvm(std::filesystem::path const& source_path) const -> CompilePipelineResult;
    auto emit_llvm(
        std::filesystem::path const& source_path,
        CompilePipelineOptions const& options
    ) const -> CompilePipelineResult;
    auto emit_object(std::filesystem::path const& source_path) const -> CompilePipelineResult;
    auto emit_object(
        std::filesystem::path const& source_path,
        CompilePipelineOptions const& options
    ) const -> CompilePipelineResult;
    auto collect_dynamic_array_cleanup_metadata(
        std::filesystem::path const& source_path,
        CompilePipelineOptions const& options
    ) const -> CompilePipelineResult;
};

}  // namespace orison::pipeline
