#include "orison/pipeline/compile_pipeline.hpp"

#include "orison/lowering/llvm_ir_emitter.hpp"
#include "orison/lowering/llvm_object_emitter.hpp"

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
    return result;
}

auto CompilePipeline::emit_llvm(std::filesystem::path const& source_path) const -> CompilePipelineResult {
    auto result = analyze(source_path);
    if (result.has_errors()) {
        return result;
    }

    lowering::LlvmIrEmitter emitter;
    auto emission = emitter.emit(result.parse_result.module, result.semantic_result);
    if (emission.has_errors()) {
        result.error_text = emission.render(result.source_file->path().string());
        return result;
    }
    result.ir_text = std::move(emission.ir_text);
    result.planned_drop_report = emission.planned_drop_report();
    result.planned_drop_action_report = emission.planned_drop_action_report();
    return result;
}

auto CompilePipeline::emit_object(std::filesystem::path const& source_path) const -> CompilePipelineResult {
    auto result = emit_llvm(source_path);
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
