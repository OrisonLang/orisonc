#include "llvm_emission_stage.hpp"

#include "orison/lowering/llvm_ir_emitter.hpp"
#include "orison/lowering/llvm_object_emitter.hpp"

#include "lowering_emission_options.hpp"
#include "lowering_emission_reports.hpp"

#include <cstddef>
#include <sstream>
#include <utility>

namespace orison::pipeline {

namespace {

auto runtime_indexed_member_cleanup_binding_error_text(
    CompilePipelineResult const& result,
    CompilePipelineOptions const& options
) -> std::string {
    if (!options.runtime_indexed_member_cleanup_rewrite_execution_enabled) {
        return {};
    }

    for (auto const& bindings : result.runtime_indexed_member_cleanup_helper_drop_bindings) {
        if (bindings.all_drop_definitions_available && bindings.helper_definition_ready) {
            continue;
        }
        auto diagnostic = std::ostringstream {};
        diagnostic << "runtime-index member cleanup blocked: member cleanup helper Drop bindings are missing"
                   << " owner " << bindings.owner_name
                   << " index " << bindings.index_expression_text
                   << " element " << bindings.element_source_type_name
                   << " moved " << bindings.moved_source_type_name
                   << " member-path ";
        for (auto index = std::size_t {0}; index < bindings.moved_member_path.size(); ++index) {
            if (index != 0) {
                diagnostic << ".";
            }
            diagnostic << bindings.moved_member_path[index];
        }
        diagnostic << " helper " << bindings.helper_symbol_name;
        return diagnostic.str();
    }

    return {};
}

}  // namespace

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
        if (options.collect_computed_dynamic_array_for_descriptor_renders ||
            options.collect_computed_dynamic_array_for_loop_control_renders ||
            options.collect_computed_dynamic_array_for_element_address_renders ||
            options.collect_computed_dynamic_array_for_element_load_renders ||
            options.collect_computed_dynamic_array_for_loop_continue_renders ||
            options.collect_computed_dynamic_array_for_loop_render_sequences ||
            options.collect_computed_dynamic_array_for_loop_exit_cleanups ||
            options.collect_computed_dynamic_array_for_cleanup_transitions ||
            options.collect_computed_dynamic_array_for_production_emission_gates ||
            options.collect_computed_dynamic_array_for_production_sequences ||
            options.emit_computed_dynamic_array_for_production_sequence_comments ||
            options.collect_aggregate_projection_access_metadata ||
            options.collect_runtime_indexed_cleanup_audit) {
            populate_lowering_emission_reports(result, std::move(emission), options);
        }
        return result;
    }
    populate_lowering_emission_reports(result, std::move(emission), options);
    if (auto error_text = runtime_indexed_member_cleanup_binding_error_text(result, options);
        !error_text.empty()) {
        result.error_text = std::move(error_text);
    }
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
