#include "orison/pipeline/compile_pipeline.hpp"

#include "orison/lowering/llvm_ir_emitter.hpp"
#include "orison/lowering/llvm_object_emitter.hpp"
#include "orison/pipeline/dynamic_array_cleanup_metadata.hpp"

#include "lowering_emission_reports.hpp"
#include "lowering_emission_options.hpp"
#include "semantic_drop_reports.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

namespace orison::pipeline {
namespace {

auto unquoted_text(std::string_view text) -> std::string {
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return std::string(text.substr(1, text.size() - 2));
    }
    return std::string(text);
}

void collect_link_libraries(
    syntax::ModuleSyntax const& module,
    std::vector<std::string>& libraries
) {
    for (auto const& foreign_import : module.foreign_imports) {
        if (unquoted_text(foreign_import.abi) != "c" || foreign_import.library_name.empty()) {
            continue;
        }
        auto library = unquoted_text(foreign_import.library_name);
        if (std::find(libraries.begin(), libraries.end(), library) == libraries.end()) {
            libraries.push_back(std::move(library));
        }
    }
}

}  // namespace

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
    auto result = CompilePipelineResult {};
    result.source_file = source::SourceFile::read(source_path);
    if (!result.source_file.has_value()) {
        result.error_text = "error: unable to read source file\n";
        return result;
    }

    syntax::ModuleParser parser;
    result.parse_result = parser.parse(*result.source_file);
    if (result.parse_result.diagnostics.has_errors()) {
        result.error_text = result.parse_result.diagnostics.render(result.source_file->path().string());
        return result;
    }
    collect_link_libraries(result.parse_result.module, result.link_libraries);

    semantics::ModuleSemanticAnalyzer semantic_analyzer;
    result.semantic_result = semantic_analyzer.analyze(result.parse_result.module);
    if (result.semantic_result.has_errors()) {
        result.error_text = result.semantic_result.render(result.source_file->path().string());
    }
    populate_semantic_drop_reports(result, options);
    return result;
}

auto CompilePipeline::emit_llvm(std::filesystem::path const& source_path) const -> CompilePipelineResult {
    return emit_llvm(source_path, CompilePipelineOptions {});
}

auto CompilePipeline::emit_llvm(
    std::filesystem::path const& source_path,
    CompilePipelineOptions const& options
) const -> CompilePipelineResult {
    auto result = analyze(source_path, options);
    if (result.has_errors()) {
        return result;
    }

    lowering::LlvmIrEmitter emitter;
    auto emission_options = build_lowering_emission_options(result, options, LoweringEmissionMode::full_ir);
    auto emission = emitter.emit(result.parse_result.module, result.semantic_result, emission_options);
    if (emission.has_errors()) {
        result.error_text = emission.render(result.source_file->path().string());
        return result;
    }
    populate_lowering_emission_reports(result, std::move(emission), options);
    return result;
}

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
    auto result = emit_llvm(source_path, options);
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
