#include "llvm_emission_stage.hpp"

#include "orison/lowering/llvm_ir_emitter.hpp"
#include "orison/lowering/llvm_object_emitter.hpp"

#include "lowering_emission_options.hpp"
#include "lowering_emission_reports.hpp"

#include <cstddef>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <utility>

namespace orison::pipeline {

namespace {

auto trim_source_line_text(std::string line) -> std::string {
    auto const first_non_space = std::find_if(
        line.begin(),
        line.end(),
        [](unsigned char character) {
            return !std::isspace(character);
        }
    );
    if (first_non_space == line.end()) {
        return {};
    }
    auto const last_non_space = std::find_if(
        line.rbegin(),
        line.rend(),
        [](unsigned char character) {
            return !std::isspace(character);
        }
    ).base();
    return std::string(first_non_space, last_non_space);
}

auto source_line_text(std::string const& source_text, std::size_t line_number) -> std::string {
    if (line_number == 0) {
        return {};
    }
    auto current_line = std::size_t {1};
    auto line_start = std::size_t {0};
    while (line_start <= source_text.size()) {
        auto line_end = source_text.find('\n', line_start);
        if (line_end == std::string::npos) {
            line_end = source_text.size();
        }
        if (current_line == line_number) {
            return trim_source_line_text(source_text.substr(line_start, line_end - line_start));
        }
        if (line_end == source_text.size()) {
            break;
        }
        line_start = line_end + 1;
        ++current_line;
    }
    return {};
}

auto runtime_indexed_member_cleanup_binding_error_text(
    CompilePipelineResult const& result,
    CompilePipelineOptions const& options
) -> std::string {
    if (!options.runtime_indexed_member_cleanup_rewrite_execution_enabled) {
        return {};
    }

    for (auto const& bindings : result.runtime_indexed_member_cleanup_helper_owned_cleanup_bindings) {
        if (bindings.all_owned_cleanup_definitions_available && bindings.helper_definition_ready) {
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
        if (bindings.source_line != 0) {
            diagnostic << " source-line " << bindings.source_line;
            auto const source_text = result.source_file ? result.source_file->content() : std::string {};
            auto const source_snippet = source_line_text(source_text, bindings.source_line);
            if (!source_snippet.empty()) {
                diagnostic << " source-text " << source_snippet;
            }
        }
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
