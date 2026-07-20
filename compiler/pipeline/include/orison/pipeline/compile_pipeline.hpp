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

struct DynamicArrayCleanupProductionReadiness {
    bool descriptor_origins_available = false;
    bool descriptor_cleanup_plans_available = false;
    bool cleanup_obligations_available = false;
    bool sequence_verification_available = false;
    bool sequence_verification_passed = false;
    bool cleanup_capability_proven = false;
    bool production_signature_lowering_enabled = false;
    bool production_construction_lowering_enabled = false;
    bool production_cleanup_emission_enabled = false;
};

auto dynamic_array_cleanup_production_ready(
    DynamicArrayCleanupProductionReadiness const& readiness
) -> bool;

auto format_dynamic_array_cleanup_production_readiness(
    DynamicArrayCleanupProductionReadiness const& readiness
) -> std::string;

struct CompilePipelineOptions {
    std::vector<semantics::DropImplementation> test_only_semantic_drop_implementations;
    std::vector<semantics::DropImplementationCandidate> test_only_semantic_drop_implementation_candidates;
    std::vector<semantics::DropLoweringAuthorization> test_only_semantic_drop_lowering_authorizations;
    std::vector<lowering::TestOnlyDynamicArrayConstructionRequest> test_only_dynamic_array_construction_requests;
    bool test_only_enable_source_drop_lowering = false;
    bool source_drop_lowering_enabled = false;
    bool test_only_derive_dynamic_array_cleanup_from_semantics = false;
    bool dynamic_array_descriptor_cleanup_planning_enabled = false;
    bool test_only_enable_dynamic_array_parameter_descriptors = false;
    bool dynamic_array_parameter_descriptor_audit_bindings_enabled = false;
    bool test_only_emit_bound_dynamic_array_parameter_cleanups = false;
    bool emit_metadata_only = false;
    bool test_only_render_dynamic_array_element_drop_walks = false;
    bool dynamic_array_local_lowering_enabled = true;
    bool dynamic_array_parameter_lowering_enabled = true;
    bool dynamic_array_production_signature_lowering_enabled = false;
    bool dynamic_array_production_construction_lowering_enabled = false;
    bool dynamic_array_production_index_lowering_enabled = false;
    bool dynamic_array_production_length_lowering_enabled = false;
    bool dynamic_array_production_for_lowering_enabled = false;
    bool dynamic_array_production_append_lowering_enabled = false;
    bool dynamic_array_production_cleanup_emission_enabled = false;
};

struct CompilePipelineResult {
    std::optional<source::SourceFile> source_file;
    syntax::ParseResult parse_result;
    semantics::SemanticAnalysisResult semantic_result;
    std::string ir_text;
    std::string object_bytes;
    std::vector<std::string> semantic_dynamic_array_descriptor_origin_report;
    std::vector<std::string> semantic_planned_drop_report;
    std::vector<std::string> semantic_drop_implementation_report;
    std::vector<std::string> semantic_drop_resolution_report;
    std::vector<std::string> semantic_drop_diagnostic_report;
    std::vector<semantics::DropLoweringAuthorization> semantic_drop_lowering_authorizations;
    std::vector<std::string> semantic_drop_lowering_authorization_report;
    std::vector<std::string> semantic_drop_resolution_summary_report;
    std::vector<std::string> dynamic_array_descriptor_cleanup_plan_report;
    std::vector<std::string> dynamic_array_construction_plan_report;
    std::vector<std::string> dynamic_array_runtime_request_report;
    std::vector<std::string> dynamic_array_allocation_call_ir;
    std::vector<std::string> dynamic_array_cleanup_obligation_report;
    std::vector<std::string> dynamic_array_cleanup_sequence_plan_report;
    std::vector<std::string> dynamic_array_cleanup_sequence_verification_report;
    std::vector<std::string> dynamic_array_cleanup_emission_gate_report;
    std::vector<std::string> dynamic_array_cleanup_emission_capability_report;
    DynamicArrayCleanupProductionReadiness dynamic_array_cleanup_production_readiness;
    std::vector<std::string> dynamic_array_cleanup_production_readiness_report;
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
    auto emit_object(
        std::filesystem::path const& source_path,
        CompilePipelineOptions const& options
    ) const -> CompilePipelineResult;
};

}  // namespace orison::pipeline
