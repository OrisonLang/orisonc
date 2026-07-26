#include "llvm_emission_stage.hpp"

#include "orison/lowering/llvm_ir_emitter.hpp"
#include "orison/lowering/llvm_object_emitter.hpp"

#include "lowering_emission_options.hpp"
#include "lowering_emission_reports.hpp"

#include <utility>

namespace orison::pipeline {

auto run_llvm_emission_stage(
    CompilePipeline const& pipeline,
    std::filesystem::path const& source_path,
    CompilePipelineOptions const& options
) -> CompilePipelineResult {
    auto result = pipeline.analyze(source_path, options);
    if (result.has_errors()) {
        return result;
    }

    lowering::LlvmIrEmitter emitter;
    auto emission_options = build_lowering_emission_options(result, options, LoweringEmissionMode::full_ir);
    auto emission = emitter.emit(result.parse_result.module, result.semantic_result, emission_options);
    if (emission.has_errors()) {
        result.error_text = emission.render(result.source_file->path().string());
        if (options.test_only_collect_computed_dynamic_array_for_descriptor_renders ||
            options.test_only_collect_computed_dynamic_array_for_loop_control_renders ||
            options.test_only_collect_computed_dynamic_array_for_element_address_renders ||
            options.test_only_collect_computed_dynamic_array_for_element_load_renders ||
            options.test_only_collect_computed_dynamic_array_for_production_sequences) {
            populate_lowering_emission_reports(result, std::move(emission), options);
        }
        return result;
    }
    populate_lowering_emission_reports(result, std::move(emission), options);
    return result;
}

auto run_object_emission_stage(
    CompilePipeline const& pipeline,
    std::filesystem::path const& source_path,
    CompilePipelineOptions const& options
) -> CompilePipelineResult {
    auto result = run_llvm_emission_stage(pipeline, source_path, options);
    if (result.has_errors()) {
        return result;
    }

    lowering::LlvmObjectEmitter emitter;
    auto emission = emitter.emit(result.ir_text);
    if (emission.has_errors()) {
        result.error_text = emission.diagnostics.render(result.source_file->path().string());
        return result;
    }
    result.object_bytes = std::move(emission.object_bytes);
    return result;
}

}  // namespace orison::pipeline
