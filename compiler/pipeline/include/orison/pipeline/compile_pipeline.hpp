#pragma once

#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace orison::pipeline {

struct CompilePipelineResult {
    std::optional<source::SourceFile> source_file;
    syntax::ParseResult parse_result;
    semantics::SemanticAnalysisResult semantic_result;
    std::string ir_text;
    std::string object_bytes;
    std::vector<std::string> planned_drop_report;
    std::vector<std::string> planned_drop_action_report;
    std::vector<std::string> drop_cleanup_authorization_report;
    std::vector<std::string> link_libraries;
    std::string error_text;

    auto has_errors() const -> bool;
};

class CompilePipeline {
public:
    auto analyze(std::filesystem::path const& source_path) const -> CompilePipelineResult;
    auto emit_llvm(std::filesystem::path const& source_path) const -> CompilePipelineResult;
    auto emit_object(std::filesystem::path const& source_path) const -> CompilePipelineResult;
};

}  // namespace orison::pipeline
