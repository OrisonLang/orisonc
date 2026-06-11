#include "orison/pipeline/compile_pipeline.hpp"

#include "orison/lowering/llvm_ir_emitter.hpp"
#include "orison/lowering/llvm_object_emitter.hpp"

#include <utility>

namespace orison::pipeline {

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
