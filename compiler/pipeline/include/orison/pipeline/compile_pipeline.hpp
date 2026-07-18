#pragma once

#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/lowering_options.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace orison::pipeline {

struct CompilePipelineOptions {
    std::vector<semantics::DropImplementation> test_only_semantic_drop_implementations;
    std::vector<semantics::DropImplementationCandidate> test_only_semantic_drop_implementation_candidates;
    std::vector<lowering::TestOnlyDynamicArrayConstructionRequest> test_only_dynamic_array_construction_requests;
    bool test_only_render_dynamic_array_element_drop_walks = false;
};

struct CompilePipelineResult {
    std::optional<source::SourceFile> source_file;
    syntax::ParseResult parse_result;
    semantics::SemanticAnalysisResult semantic_result;
    std::string ir_text;
    std::string object_bytes;
    std::vector<std::string> semantic_planned_drop_report;
    std::vector<std::string> semantic_drop_implementation_report;
    std::vector<std::string> semantic_drop_resolution_report;
    std::vector<std::string> semantic_drop_diagnostic_report;
    std::vector<semantics::DropLoweringAuthorization> semantic_drop_lowering_authorizations;
    std::vector<std::string> semantic_drop_lowering_authorization_report;
    std::vector<std::string> semantic_drop_resolution_summary_report;
    std::vector<std::string> planned_drop_report;
    std::vector<std::string> emitted_drop_declaration_report;
    std::vector<std::string> planned_drop_action_report;
    std::vector<std::string> drop_cleanup_authorization_report;
    lowering::DropReadinessSnapshot drop_readiness_snapshot;
    std::vector<std::string> drop_readiness_snapshot_report;
    lowering::DropReadinessSummary drop_readiness_summary;
    std::vector<std::string> drop_readiness_summary_report;
    std::vector<std::string> drop_readiness_relation_report;
    lowering::DropReadinessBlockerSummary drop_readiness_blocker_summary;
    std::vector<std::string> drop_readiness_blocker_report;
    std::vector<std::string> drop_readiness_source_correlation_report;
    std::vector<std::string> link_libraries;
    std::string error_text;

    auto has_errors() const -> bool;
};

class CompilePipeline {
public:
    auto analyze(std::filesystem::path const& source_path) const -> CompilePipelineResult;
    auto analyze(
        std::filesystem::path const& source_path,
        CompilePipelineOptions const& options
    ) const -> CompilePipelineResult;
    auto emit_llvm(std::filesystem::path const& source_path) const -> CompilePipelineResult;
    auto emit_llvm(
        std::filesystem::path const& source_path,
        CompilePipelineOptions const& options
    ) const -> CompilePipelineResult;
    auto emit_object(std::filesystem::path const& source_path) const -> CompilePipelineResult;
};

}  // namespace orison::pipeline
