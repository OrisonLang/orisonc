#include "orison/pipeline/compile_pipeline.hpp"

#include "orison/pipeline/dynamic_array_cleanup_metadata.hpp"

#include "llvm_emission_stage.hpp"
#include "semantic_analysis_stage.hpp"

namespace orison::pipeline {

auto CompilePipelineResult::has_errors() const -> bool {
    return !error_text.empty();
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
    return DynamicArrayCleanupMetadataCollector(*this).collect(source_path, options);
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
