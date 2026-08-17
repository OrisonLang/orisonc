#pragma once

#include "orison/pipeline/runtime_indexed_cleanup_composition_failures.hpp"
#include "orison/pipeline/runtime_indexed_cleanup_emission.hpp"
#include "orison/pipeline/runtime_indexed_cleanup_module_ir_insertion.hpp"
#include "orison/pipeline/runtime_indexed_cleanup_module_ir_rewrite_candidates.hpp"
#include "orison/pipeline/runtime_indexed_cleanup_ranges.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace orison::pipeline {

enum class RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind {
    None,
    InsertionGate,
    InsertionPreview,
    Candidate,
    CandidateVerification,
    ModuleMutation,
    FunctionIntegration,
    FunctionSpliceConflict,
    IrShape,
};

struct RuntimeIndexedCleanupModuleIrProductionReadinessBlocker {
    RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind kind =
        RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::None;
    std::string stage_name;
    std::string function_symbol_name;
    RuntimeIndexedCleanupIrCompositionFailure composition_failure =
        RuntimeIndexedCleanupIrCompositionFailure::none;
    bool composition_failure_part_available = false;
    std::size_t composition_failure_part_index = 0;
    RuntimeIndexedCleanupTextSpliceRange composition_failure_splice_range;
    bool rewrite_apply_stage_available = false;
    bool branch_replacements_applied = false;
    bool cleanup_cfg_appended = false;
    bool phi_predecessors_retargeted = false;
    bool source_available = false;
    std::size_t source_line = 0;
};

struct RuntimeIndexedCleanupModuleIrProductionReadinessState {
    bool insertion_gate_ready = false;
    bool insertion_preview_ready = false;
    bool candidate_ready = false;
    bool candidate_verified = false;
    bool module_mutation_enabled = false;
    bool function_integration_ready = false;
    bool function_splice_conflict_free = false;
    bool ir_shape_ready = true;
    bool production_ready = false;
    RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind diagnostic_blocker_kind =
        RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::None;
    std::vector<RuntimeIndexedCleanupModuleIrProductionReadinessBlocker> blockers;
    std::size_t function_splice_conflict_count = 0;
    std::string diagnostic_blocker_stage_name;
    std::string diagnostic_function_symbol_name;
    RuntimeIndexedCleanupIrCompositionFailure diagnostic_composition_failure =
        RuntimeIndexedCleanupIrCompositionFailure::none;
    bool diagnostic_composition_failure_part_available = false;
    std::size_t diagnostic_composition_failure_part_index = 0;
    RuntimeIndexedCleanupTextSpliceRange diagnostic_composition_failure_splice_range;
    bool diagnostic_rewrite_apply_stage_available = false;
    bool diagnostic_branch_replacements_applied = false;
    bool diagnostic_cleanup_cfg_appended = false;
    bool diagnostic_phi_predecessors_retargeted = false;
    bool diagnostic_source_available = false;
    std::size_t diagnostic_source_line = 0;
    std::size_t diagnostic_left_candidate_index = 0;
    std::size_t diagnostic_right_candidate_index = 0;
    std::size_t diagnostic_left_source_line = 0;
    std::size_t diagnostic_right_source_line = 0;
    std::string diagnostic_text;
};

auto format_runtime_indexed_cleanup_production_readiness_diagnostic(
    RuntimeIndexedCleanupModuleIrProductionReadinessState const& state
) -> std::string;

auto runtime_indexed_cleanup_production_readiness_blocker_kind_name(
    RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind kind
) -> std::string_view;

auto format_runtime_indexed_cleanup_production_readiness_report(
    RuntimeIndexedCleanupModuleIrProductionReadinessState const& state
) -> std::string;

auto format_runtime_indexed_cleanup_production_readiness_blocker_report(
    RuntimeIndexedCleanupModuleIrProductionReadinessState const& state
) -> std::vector<std::string>;

auto runtime_indexed_cleanup_module_ir_production_readiness_state(
    RuntimeIndexedCleanupEmissionPlanState const& emission_plan_state,
    RuntimeIndexedCleanupModuleIrInsertionGateState const& insertion_gate_state,
    RuntimeIndexedCleanupModuleIrInsertionPreviewState const& preview_state,
    RuntimeIndexedCleanupModuleIrCandidateState const& candidate_state,
    RuntimeIndexedCleanupModuleIrCandidateVerificationState const& verification_state,
    RuntimeIndexedCleanupModuleIrMutationState const& mutation_state,
    RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateVerificationState const& function_verification_state,
    RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState const& function_mutation_state
) -> RuntimeIndexedCleanupModuleIrProductionReadinessState;

} // namespace orison::pipeline
