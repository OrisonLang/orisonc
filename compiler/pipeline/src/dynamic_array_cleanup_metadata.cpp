#include "orison/pipeline/dynamic_array_cleanup_metadata.hpp"

#include "orison/lowering/llvm_ir_emitter.hpp"

#include "lowering_emission_reports.hpp"
#include "lowering_emission_options.hpp"

#include <utility>

namespace orison::pipeline {

DynamicArrayCleanupMetadataCollector::DynamicArrayCleanupMetadataCollector(CompilePipeline const& pipeline)
    : pipeline_(pipeline) {}

auto DynamicArrayCleanupMetadataCollector::collect(
    std::filesystem::path const& source_path,
    CompilePipelineOptions const& options
) const -> CompilePipelineResult {
    auto result = pipeline_.analyze(source_path, options);
    if (result.has_errors()) {
        return result;
    }

    lowering::LlvmIrEmitter emitter;
    auto emission_options =
        build_lowering_emission_options(result, options, LoweringEmissionMode::dynamic_array_cleanup_metadata);
    auto emission = emitter.emit_metadata(result.parse_result.module, result.semantic_result, emission_options);
    if (emission.has_errors()) {
        result.error_text = emission.render(result.source_file->path().string());
        return result;
    }
    populate_lowering_emission_reports(result, std::move(emission), options);
    return result;
}

}  // namespace orison::pipeline
