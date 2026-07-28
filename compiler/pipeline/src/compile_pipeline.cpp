#include "orison/pipeline/compile_pipeline.hpp"

#include "dynamic_array_cleanup_metadata_stage.hpp"
#include "llvm_emission_stage.hpp"
#include "semantic_analysis_stage.hpp"

namespace orison::pipeline {

auto CompilePipelineResult::has_errors() const -> bool {
    return !error_text.empty();
}

auto plan_computed_dynamic_array_for_production_readiness(
    ComputedDynamicArrayForProductionEmissionGateState const& gate_state,
    ComputedDynamicArrayForProductionSequenceState const& sequence_state
) -> ComputedDynamicArrayForProductionReadiness {
    return ComputedDynamicArrayForProductionReadiness {
        .gate_ready =
            gate_state.gate_metadata_available &&
            gate_state.all_ownership_ready &&
            gate_state.all_loop_render_ready &&
            gate_state.all_loop_cleanup_ownership_ready &&
            gate_state.all_function_cleanup_resumption_ready &&
            gate_state.all_exit_cleanup_ready &&
            gate_state.all_production_sequences_planned,
        .sequence_ready = sequence_state.sequence_metadata_available,
        .gate_sequence_counts_match =
            gate_state.gate_metadata_available &&
            sequence_state.sequence_metadata_available &&
            gate_state.gate_count == sequence_state.sequence_count,
        .gate_sequence_snippets_match =
            gate_state.gate_metadata_available &&
            sequence_state.sequence_metadata_available &&
            gate_state.rendered_ir_snippet_count == sequence_state.rendered_ir_snippet_count,
        .cleanup_owners_match =
            gate_state.gate_metadata_available &&
            sequence_state.sequence_metadata_available &&
            gate_state.cleanup_owner_names == sequence_state.cleanup_owner_names,
        .production_emission_enabled = gate_state.any_production_emission_enabled,
    };
}

auto computed_dynamic_array_for_production_ready(
    ComputedDynamicArrayForProductionReadiness const& readiness
) -> bool {
    return readiness.gate_ready &&
        readiness.sequence_ready &&
        readiness.gate_sequence_counts_match &&
        readiness.gate_sequence_snippets_match &&
        readiness.cleanup_owners_match &&
        readiness.production_emission_enabled;
}

auto CompilePipeline::analyze(std::filesystem::path const& source_path) const -> CompilePipelineResult {
    return analyze(source_path, CompilePipelineOptions {});
}

auto CompilePipeline::analyze(
    std::filesystem::path const& source_path,
    CompilePipelineOptions const& options
) const -> CompilePipelineResult {
    return run_semantic_analysis_stage(source_path, options);
}

auto CompilePipeline::emit_llvm(std::filesystem::path const& source_path) const -> CompilePipelineResult {
    return emit_llvm(source_path, CompilePipelineOptions {});
}

auto CompilePipeline::emit_llvm(
    std::filesystem::path const& source_path,
    CompilePipelineOptions const& options
) const -> CompilePipelineResult {
    return run_llvm_emission_stage(*this, source_path, options);
}

auto CompilePipeline::collect_dynamic_array_cleanup_metadata(
    std::filesystem::path const& source_path,
    CompilePipelineOptions const& options
) const -> CompilePipelineResult {
    return run_dynamic_array_cleanup_metadata_stage(*this, source_path, options);
}

auto CompilePipeline::emit_object(std::filesystem::path const& source_path) const -> CompilePipelineResult {
    return emit_object(source_path, CompilePipelineOptions {});
}

auto CompilePipeline::emit_object(
    std::filesystem::path const& source_path,
    CompilePipelineOptions const& options
) const -> CompilePipelineResult {
    return run_object_emission_stage(*this, source_path, options);
}

}  // namespace orison::pipeline
