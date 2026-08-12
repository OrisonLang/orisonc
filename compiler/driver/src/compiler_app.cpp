#include "orison/driver/compiler_app.hpp"

#include "computed_cleanup_reports.hpp"

#include "orison/driver/runtime_indexed_cleanup_reports.hpp"
#include "orison/lowering/drop_metadata.hpp"
#include "orison/lowering/concurrency_plan.hpp"
#include "orison/link/host_linker.hpp"
#include "orison/link/host_runner.hpp"
#include "orison/pipeline/compile_pipeline.hpp"
#include "orison/pipeline/drop_readiness_source_correlation_report.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace orison::driver {
namespace {

auto render_expression(orison::syntax::ExpressionSyntax const& expression) -> std::string;

auto render_report_lines(std::vector<std::string> const& lines) -> std::string {
    auto output = std::ostringstream {};
    for (auto const& line : lines) {
        output << line << '\n';
    }
    return output.str();
}

auto planned_drop_declaration_state_report(
    pipeline::PlannedDropDeclarationState const& state
) -> std::vector<std::string> {
    return lowering::format_planned_drop_report(state.declarations);
}

auto emitted_drop_declaration_state_report(
    pipeline::PlannedDropDeclarationState const& state
) -> std::vector<std::string> {
    return lowering::format_emitted_drop_declaration_report(state.declarations);
}

auto planned_drop_action_state_report(
    pipeline::PlannedDropActionState const& state
) -> std::vector<std::string> {
    return lowering::format_planned_drop_action_report(state.actions);
}

auto drop_cleanup_authorization_state_report(
    pipeline::DropCleanupAuthorizationState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    for (auto index = std::size_t {0}; index < state.cleanups.size(); ++index) {
        if (index >= state.authorizations.size()) {
            break;
        }
        auto const& authorization = state.authorizations[index];
        if (
            authorization.authorized ||
            (authorization.semantic_lowering_blockers.empty() && authorization.missing_declarations.empty())
        ) {
            continue;
        }
        auto cleanup_lines = lowering::format_drop_cleanup_authorization_report(
            state.cleanups[index],
            authorization
        );
        lines.insert(lines.end(), cleanup_lines.begin(), cleanup_lines.end());
    }
    return lines;
}

auto drop_readiness_summary_state_report(
    lowering::DropReadinessSummary const& summary
) -> std::vector<std::string> {
    return {lowering::format_drop_readiness_summary(summary)};
}

auto drop_readiness_snapshot_state_report(
    lowering::DropReadinessSnapshot const& snapshot
) -> std::vector<std::string> {
    return lowering::format_drop_readiness_snapshot_report(snapshot);
}

auto drop_readiness_relation_state_report(
    lowering::DropReadinessSnapshot const& snapshot
) -> std::vector<std::string> {
    return lowering::format_drop_readiness_relation_report(snapshot);
}

auto drop_readiness_blocker_state_report(
    lowering::DropReadinessBlockerSummary const& summary
) -> std::vector<std::string> {
    return lowering::format_drop_readiness_blocker_report(summary);
}

auto drop_readiness_source_correlation_state_report(
    lowering::DropReadinessSnapshot const& snapshot
) -> std::vector<std::string> {
    return pipeline::format_drop_readiness_source_correlation_report(snapshot);
}

auto semantic_dynamic_array_descriptor_origin_state_report(
    semantics::SemanticAnalysisResult const& result
) -> std::vector<std::string> {
    return semantics::format_dynamic_array_descriptor_origin_report(result.dynamic_array_descriptor_origins);
}

auto semantic_drop_implementations(
    pipeline::SemanticDropState const& state
) -> std::vector<semantics::DropImplementation> {
    auto implementations = std::vector<semantics::DropImplementation> {};
    implementations.reserve(state.discovered_implementations.size());
    for (auto const& discovered : state.discovered_implementations) {
        implementations.push_back(discovered.implementation);
    }
    return implementations;
}

auto semantic_drop_resolution_state_report(
    pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    return semantics::format_drop_implementation_resolution_report(
        result.semantic_result.planned_drop_sites,
        semantic_drop_implementations(result.semantic_drop_state)
    );
}

auto semantic_drop_diagnostic_state_report(
    pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    return semantics::format_drop_implementation_diagnostic_report(
        result.semantic_result.planned_drop_sites,
        semantic_drop_implementations(result.semantic_drop_state)
    );
}

auto semantic_drop_resolution_summary_state_report(
    pipeline::SemanticDropState const& state
) -> std::vector<std::string> {
    return semantics::format_drop_implementation_resolution_summary_report(state.resolution_summaries);
}

auto usage_text() -> std::string {
    return "usage: orisonc --version | run <file> | --parse <file> | --emit-llvm <file> | "
           "--semantic-planned-drops <file> | --semantic-drop-resolution <file> | "
           "--semantic-drop-diagnostics <file> | --semantic-drop-lowering-authorization <file> | "
           "--semantic-drop-summary <file> | --semantic-dynamic-array-descriptor-origins <file> | "
           "--planned-drops <file> | --planned-drop-actions <file> | --emitted-drops <file> | "
           "--drop-cleanup-authorization <file> | --drop-readiness <file> | "
           "--drop-readiness-summary <file> | --drop-readiness-relations <file> | "
           "--drop-readiness-blockers <file> | --drop-readiness-source-correlations <file> | "
           "--dynamic-array-descriptor-cleanup-plan <file> | "
           "--dynamic-array-cleanup-obligations <file> | "
           "--dynamic-array-cleanup-sequence-plan <file> | "
           "--dynamic-array-cleanup-sequence-verification <file> | "
           "--dynamic-array-cleanup-emission-gate <file> | "
           "--dynamic-array-cleanup-capability <file> | "
           "--computed-dynamic-array-cleanup-call-insertion-capability <file> | "
           "--test-only-computed-dynamic-array-cleanup-call-insertion-capability <file> | "
           "--computed-dynamic-array-cleanup-call-insertion-readiness <file> | "
           "--test-only-computed-dynamic-array-cleanup-call-insertion-readiness <file> | "
           "--computed-dynamic-array-inserted-cleanup-handoffs <file> | "
           "--test-only-computed-dynamic-array-inserted-cleanup-handoffs <file> | "
           "--computed-dynamic-array-inserted-cleanup-calls <file> | "
           "--test-only-computed-dynamic-array-inserted-cleanup-calls <file> | "
           "--computed-dynamic-array-consumed-cleanup-descriptors <file> | "
           "--test-only-computed-dynamic-array-consumed-cleanup-descriptors <file> | "
           "--computed-dynamic-array-cleanup-proof-summary <file> | "
           "--test-only-computed-dynamic-array-cleanup-proof-summary <file> | "
           "--test-only-aggregate-projection-access-plans <file> | "
           "--dynamic-array-cleanup-production-readiness <file> | --dynamic-array-cleanup-audit <file> | "
           "--runtime-indexed-cleanup-audit <file> | "
           "--runtime-indexed-cleanup-emit-llvm <file> | "
           "--runtime-indexed-constructor-move-production-readiness <file> | "
           "--test-only-runtime-indexed-cleanup-production-readiness <file> | "
           "--test-only-runtime-indexed-constructor-move-run <file> | "
           "--emit-object <file> -o <output> | --build <file> -o <executable>";
}

auto emit_llvm_report(
    std::filesystem::path const& source_path,
    pipeline::CompilePipelineOptions const& options,
    auto report_selector
) -> CompileResult {
    pipeline::CompilePipeline pipeline;
    auto result = pipeline.emit_llvm(source_path, options);
    if (result.has_errors()) {
        return CompileResult {
            .exit_code = 1,
            .stderr_text = std::move(result.error_text),
        };
    }

    return CompileResult {
        .exit_code = 0,
        .stdout_text = render_report_lines(report_selector(result)),
    };
}

auto emit_llvm_report(std::filesystem::path const& source_path, auto report_selector) -> CompileResult {
    return emit_llvm_report(source_path, pipeline::CompilePipelineOptions {}, report_selector);
}

auto emit_llvm_report_with_failure_output(
    std::filesystem::path const& source_path,
    pipeline::CompilePipelineOptions const& options,
    auto report_selector
) -> CompileResult {
    pipeline::CompilePipeline pipeline;
    auto result = pipeline.emit_llvm(source_path, options);
    auto report_lines = render_report_lines(report_selector(result));
    if (result.has_errors()) {
        return CompileResult {
            .exit_code = 1,
            .stdout_text = std::move(report_lines),
            .stderr_text = std::move(result.error_text),
        };
    }

    return CompileResult {
        .exit_code = 0,
        .stdout_text = std::move(report_lines),
    };
}

auto dynamic_array_cleanup_report_options() -> pipeline::CompilePipelineOptions {
    return pipeline::CompilePipelineOptions {
        .source_drop_lowering_enabled = true,
        .dynamic_array_descriptor_cleanup_planning_enabled = true,
        .dynamic_array_parameter_descriptor_audit_bindings_enabled = true,
        .collect_computed_dynamic_array_for_descriptor_renders = true,
        .collect_computed_dynamic_array_for_loop_control_renders = true,
        .collect_computed_dynamic_array_for_element_address_renders = true,
        .collect_computed_dynamic_array_for_element_load_renders = true,
        .collect_computed_dynamic_array_for_loop_continue_renders = true,
        .collect_computed_dynamic_array_for_loop_render_sequences = true,
        .collect_computed_dynamic_array_for_loop_exit_cleanups = true,
        .collect_computed_dynamic_array_for_cleanup_transitions = true,
        .collect_computed_dynamic_array_for_production_emission_gates = true,
        .collect_computed_dynamic_array_for_production_sequences = true,
        .dynamic_array_production_cleanup_emission_enabled = true,
    };
}

auto test_only_computed_cleanup_insertion_seam_options() -> pipeline::CompilePipelineOptions {
    auto options = dynamic_array_cleanup_report_options();
    options.fixture_authorize_computed_dynamic_array_cleanup_calls = true;
    options.fixture_insert_computed_dynamic_array_cleanup_calls = true;
    options.computed_dynamic_array_local_cleanup_call_insertion_enabled = false;
    options.dynamic_array_production_construction_lowering_enabled = true;
    options.dynamic_array_production_for_lowering_enabled = true;
    return options;
}

auto computed_cleanup_call_insertion_readiness_options() -> pipeline::CompilePipelineOptions {
    auto options = dynamic_array_cleanup_report_options();
    options.dynamic_array_production_construction_lowering_enabled = true;
    options.dynamic_array_production_for_lowering_enabled = true;
    return options;
}

auto runtime_indexed_cleanup_audit_options() -> pipeline::CompilePipelineOptions {
    auto options = pipeline::CompilePipelineOptions {};
    options.source_drop_lowering_enabled = true;
    options.collect_runtime_indexed_cleanup_audit = true;
    options.runtime_indexed_cleanup_emission_enabled = true;
    options.runtime_indexed_cleanup_module_ir_insertion_enabled = true;
    options.runtime_indexed_cleanup_module_ir_mutation_enabled = true;
    options.runtime_indexed_cleanup_function_ir_module_rewrite_enabled = true;
    options.runtime_indexed_constructor_move_enabled = true;
    options.dynamic_array_production_construction_lowering_enabled = true;
    options.dynamic_array_production_append_lowering_enabled = true;
    options.dynamic_array_production_index_lowering_enabled = true;
    return options;
}

auto runtime_indexed_constructor_move_run_options() -> pipeline::CompilePipelineOptions {
    auto options = pipeline::CompilePipelineOptions {};
    options.source_drop_lowering_enabled = true;
    options.collect_runtime_indexed_cleanup_audit = true;
    options.runtime_indexed_cleanup_emission_enabled = true;
    options.runtime_indexed_cleanup_source_drop_emission_enabled = true;
    options.runtime_indexed_constructor_move_enabled = true;
    return options;
}

auto runtime_indexed_constructor_move_production_readiness_options() -> pipeline::CompilePipelineOptions {
    auto options = runtime_indexed_constructor_move_run_options();
    options.runtime_indexed_constructor_move_enabled = false;
    options.dynamic_array_production_construction_lowering_enabled = true;
    options.dynamic_array_production_append_lowering_enabled = true;
    options.dynamic_array_production_index_lowering_enabled = true;
    return options;
}

auto runtime_indexed_cleanup_module_ir_production_readiness_report(
    pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessState const& state
) -> std::string {
    return pipeline::format_runtime_indexed_cleanup_production_readiness_report(state);
}

auto runtime_indexed_constructor_move_production_readiness_report(
    pipeline::CompilePipelineResult const& result
) -> std::string {
    auto const has_constructor_move_gate_diagnostic =
        result.error_text.find("constructor-move disabled") != std::string::npos;
    auto const has_partial_ownership_diagnostic =
        result.error_text.find("indexed constructor ownership move requires explicit partial ownership support") !=
        std::string::npos;

    auto report = std::ostringstream {};
    report << "runtime-index cleanup constructor-move production-readiness "
           << "constructor-move " << (has_constructor_move_gate_diagnostic ? "blocked" : "enabled")
           << " partial-ownership " << (has_partial_ownership_diagnostic ? "required" : "accepted")
           << " cleanup-proof "
           << (result.runtime_indexed_cleanup_capability_state.all_prerequisites_ready ? "ready" : "blocked")
           << " cleanup-production "
           << (result.runtime_indexed_cleanup_capability_state.any_production_enabled ? "enabled" : "disabled")
           << " capability-count " << result.runtime_indexed_cleanup_capability_state.capability_count
           << " ordinary-emit " << (result.has_errors() ? "rejected" : "accepted")
           << " diagnostic "
           << (has_constructor_move_gate_diagnostic ? "runtime-index constructor move gate disabled" : "none");
    return report.str();
}

auto runtime_indexed_cleanup_function_module_verification_report(
    pipeline::RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateVerificationState const& state
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index cleanup function-module verification "
           << "metadata " << (state.verification_metadata_available ? "available" : "missing")
           << " verifications " << state.verification_count
           << " candidate-functions " << (state.all_candidate_functions_found ? "found" : "blocked")
           << " candidate-match " << (state.all_candidate_functions_match_verified_candidates ? "true" : "false")
           << " replacement-targets " << (state.all_replacement_targets_unique ? "unique" : "blocked")
           << " module-changed " << (state.all_module_ir_changed ? "true" : "false")
           << " separate-module " << (state.all_candidates_separate_from_module_ir ? "true" : "false")
           << " splice-conflicts " << state.splice_conflict_count
           << " composition-failures " << state.composition_failure_count
           << " first-composition-failure "
           << pipeline::runtime_indexed_cleanup_ir_composition_failure_token(state.first_composition_failure)
           << " llvm-ran " << (state.any_llvm_verifier_ran ? "true" : "false")
           << " llvm-passed " << (state.all_llvm_verifier_passed ? "true" : "false")
           << " verified " << (state.all_verified ? "true" : "false")
           << " verified-count " << state.verified_count
           << " llvm-verified-count " << state.llvm_verified_count
           << " diagnostics " << state.llvm_verifier_diagnostic_count;
    return report.str();
}

auto runtime_indexed_cleanup_function_module_splice_conflict_report(
    pipeline::RuntimeIndexedCleanupFunctionIrModuleRewriteSpliceConflict const& conflict
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index cleanup function-module splice-conflict "
           << "function " << conflict.function_symbol_name
           << " left-candidate " << conflict.left_candidate_index
           << " left-line " << conflict.left_source_line
           << " left-range " << conflict.left_splice_range.start_offset
           << ".." << conflict.left_splice_range.end_offset
           << " right-candidate " << conflict.right_candidate_index
           << " right-line " << conflict.right_source_line
           << " right-range " << conflict.right_splice_range.start_offset
           << ".." << conflict.right_splice_range.end_offset;
    return report.str();
}

auto runtime_indexed_cleanup_function_module_verification_detail_report(
    pipeline::RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateVerification const& verification
) -> std::string {
    auto diagnostic_text = verification.llvm_verifier_diagnostic_text;
    for (auto& character : diagnostic_text) {
        if (character == '\n' || character == '\r' || character == '\t') {
            character = ' ';
        }
    }
    auto report = std::ostringstream {};
    report << "runtime-index cleanup function-module verification detail "
           << "function " << verification.function_symbol_name
           << " llvm-passed " << (verification.llvm_verifier_passed ? "true" : "false")
           << " diagnostics " << verification.llvm_verifier_diagnostic_count
           << " text " << diagnostic_text;
    return report.str();
}

auto dynamic_array_cleanup_report(
    std::filesystem::path const& source_path,
    pipeline::CompilePipelineOptions const& options,
    auto report_selector
) -> CompileResult {
    pipeline::CompilePipeline pipeline;
    auto result = pipeline.collect_dynamic_array_cleanup_metadata(source_path, options);
    if (result.has_errors()) {
        return CompileResult {
            .exit_code = 1,
            .stderr_text = std::move(result.error_text),
        };
    }

    return CompileResult {
        .exit_code = 0,
        .stdout_text = render_report_lines(report_selector(result)),
    };
}

auto try_dynamic_array_cleanup_report_command(
    std::span<char const* const> args,
    std::string_view command,
    pipeline::CompilePipelineOptions const& options,
    auto report_selector
) -> std::optional<CompileResult> {
    if (args.size() != 3 || std::string_view(args[1]) != command) {
        return std::nullopt;
    }
    return dynamic_array_cleanup_report(std::filesystem::path(args[2]), options, report_selector);
}

auto try_emit_llvm_report_command(
    std::span<char const* const> args,
    std::string_view command,
    pipeline::CompilePipelineOptions const& options,
    auto report_selector
) -> std::optional<CompileResult> {
    if (args.size() != 3 || std::string_view(args[1]) != command) {
        return std::nullopt;
    }
    return emit_llvm_report(std::filesystem::path(args[2]), options, report_selector);
}

void append_report_lines(std::vector<std::string>& output, std::vector<std::string> const& lines) {
    output.insert(output.end(), lines.begin(), lines.end());
}

void prefer_report_lines(std::vector<std::string>& result, std::vector<std::string>&& emitted_result) {
    if (!emitted_result.empty()) {
        result = std::move(emitted_result);
    }
}

void prefer_emitted_dynamic_array_cleanup_reports(
    pipeline::CompilePipelineResult& result,
    pipeline::CompilePipelineResult&& emitted_result
) {
    if (!emitted_result.emitted_dynamic_array_cleanup_obligation_state.obligations.empty()) {
        result.dynamic_array_cleanup_obligation_state =
            std::move(emitted_result.emitted_dynamic_array_cleanup_obligation_state);
    }
    if (!emitted_result.emitted_dynamic_array_cleanup_sequence_plan_state.plans.empty()) {
        result.dynamic_array_cleanup_sequence_plan_state =
            std::move(emitted_result.emitted_dynamic_array_cleanup_sequence_plan_state);
    }
    if (!emitted_result.emitted_dynamic_array_cleanup_sequence_verification_state.verifications.empty()) {
        result.dynamic_array_cleanup_sequence_verification_state =
            std::move(emitted_result.emitted_dynamic_array_cleanup_sequence_verification_state);
    }
    if (emitted_result.dynamic_array_cleanup_emission_capability_state.capability_metadata_available) {
        result.dynamic_array_cleanup_emission_capability_state =
            std::move(emitted_result.dynamic_array_cleanup_emission_capability_state);
    }
    if (emitted_result.computed_dynamic_array_for_descriptor_render_state.render_count > 0) {
        result.computed_dynamic_array_for_descriptor_render_state =
            std::move(emitted_result.computed_dynamic_array_for_descriptor_render_state);
    }
    if (emitted_result.computed_dynamic_array_for_loop_control_render_state.render_count > 0) {
        result.computed_dynamic_array_for_loop_control_render_state =
            std::move(emitted_result.computed_dynamic_array_for_loop_control_render_state);
    }
    if (emitted_result.computed_dynamic_array_for_element_address_render_state.render_count > 0) {
        result.computed_dynamic_array_for_element_address_render_state =
            std::move(emitted_result.computed_dynamic_array_for_element_address_render_state);
    }
    if (emitted_result.computed_dynamic_array_for_element_load_render_state.render_count > 0) {
        result.computed_dynamic_array_for_element_load_render_state =
            std::move(emitted_result.computed_dynamic_array_for_element_load_render_state);
    }
    if (emitted_result.computed_dynamic_array_for_loop_continue_render_state.render_count > 0) {
        result.computed_dynamic_array_for_loop_continue_render_state =
            std::move(emitted_result.computed_dynamic_array_for_loop_continue_render_state);
    }
    if (emitted_result.computed_dynamic_array_for_loop_render_sequence_state.sequence_count > 0) {
        result.computed_dynamic_array_for_loop_render_sequence_state =
            std::move(emitted_result.computed_dynamic_array_for_loop_render_sequence_state);
    }
    if (emitted_result.computed_dynamic_array_for_loop_exit_cleanup_state.cleanup_count > 0) {
        result.computed_dynamic_array_for_loop_exit_cleanup_state =
            std::move(emitted_result.computed_dynamic_array_for_loop_exit_cleanup_state);
    }
    if (emitted_result.computed_dynamic_array_for_cleanup_transition_state.transition_count > 0) {
        result.computed_dynamic_array_for_cleanup_transition_state =
            std::move(emitted_result.computed_dynamic_array_for_cleanup_transition_state);
    }
    if (emitted_result.computed_dynamic_array_for_inserted_cleanup_handoff_state.transition_count > 0) {
        result.computed_dynamic_array_for_inserted_cleanup_handoff_state =
            std::move(emitted_result.computed_dynamic_array_for_inserted_cleanup_handoff_state);
    }
    if (emitted_result.computed_dynamic_array_for_inserted_cleanup_state_verification_state.verification_count > 0) {
        result.computed_dynamic_array_for_inserted_cleanup_state_verification_state =
            std::move(emitted_result.computed_dynamic_array_for_inserted_cleanup_state_verification_state);
    }
    if (emitted_result.computed_dynamic_array_for_cleanup_call_emission_gate_state.gate_count > 0) {
        result.computed_dynamic_array_for_cleanup_call_emission_gate_state =
            std::move(emitted_result.computed_dynamic_array_for_cleanup_call_emission_gate_state);
    }
    if (emitted_result.computed_dynamic_array_for_cleanup_call_plan_render_state.plan_count > 0) {
        result.computed_dynamic_array_for_cleanup_call_plan_render_state =
            std::move(emitted_result.computed_dynamic_array_for_cleanup_call_plan_render_state);
    }
    if (emitted_result.consumed_descriptor_finalization_state.ready_plan_count > 0 ||
        emitted_result.consumed_descriptor_finalization_state.blocked_plan_count > 0) {
        result.consumed_descriptor_finalization_state =
            std::move(emitted_result.consumed_descriptor_finalization_state);
    }
    if (emitted_result.computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.descriptor_model_count > 0) {
        result.computed_dynamic_array_for_consumed_cleanup_descriptor_model_state =
            std::move(emitted_result.computed_dynamic_array_for_consumed_cleanup_descriptor_model_state);
    }
    if (emitted_result.computed_dynamic_array_for_production_emission_gate_state.gate_count > 0) {
        result.computed_dynamic_array_for_production_emission_gate_state =
            std::move(emitted_result.computed_dynamic_array_for_production_emission_gate_state);
    }
    if (emitted_result.computed_dynamic_array_for_production_sequence_state.sequence_count > 0) {
        result.computed_dynamic_array_for_production_sequence_state =
            std::move(emitted_result.computed_dynamic_array_for_production_sequence_state);
    }
}

auto dynamic_array_cleanup_audit_report(pipeline::CompilePipelineResult const& result) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    append_report_lines(report, semantic_dynamic_array_descriptor_origin_state_report(result.semantic_result));
    append_report_lines(
        report,
        dynamic_array_descriptor_cleanup_plan_state_report(result.dynamic_array_descriptor_cleanup_plan_state)
    );
    append_report_lines(report, dynamic_array_cleanup_obligation_state_report(
        result.dynamic_array_cleanup_obligation_state
    ));
    append_report_lines(
        report,
        dynamic_array_cleanup_sequence_plan_state_report(result.dynamic_array_cleanup_sequence_plan_state)
    );
    append_report_lines(
        report,
        dynamic_array_cleanup_sequence_verification_state_report(result.dynamic_array_cleanup_sequence_verification_state)
    );
    append_report_lines(
        report,
        dynamic_array_cleanup_emission_gate_state_report(result.dynamic_array_cleanup_sequence_verification_state)
    );
    append_report_lines(
        report,
        dynamic_array_cleanup_emission_capability_state_report(
            result.dynamic_array_cleanup_emission_capability_state
        )
    );
    append_report_lines(
        report,
        computed_dynamic_array_for_descriptor_render_state_report(
            result.computed_dynamic_array_for_descriptor_render_state
        )
    );
    append_report_lines(
        report,
        computed_dynamic_array_for_loop_control_render_state_report(
            result.computed_dynamic_array_for_loop_control_render_state
        )
    );
    append_report_lines(
        report,
        computed_dynamic_array_for_element_address_render_state_report(
            result.computed_dynamic_array_for_element_address_render_state
        )
    );
    append_report_lines(
        report,
        computed_dynamic_array_for_element_load_render_state_report(
            result.computed_dynamic_array_for_element_load_render_state
        )
    );
    append_report_lines(
        report,
        computed_dynamic_array_for_loop_continue_render_state_report(
            result.computed_dynamic_array_for_loop_continue_render_state
        )
    );
    append_report_lines(
        report,
        computed_dynamic_array_for_loop_render_sequence_state_report(
            result.computed_dynamic_array_for_loop_render_sequence_state
        )
    );
    append_report_lines(
        report,
        computed_dynamic_array_for_loop_exit_cleanup_state_report(
            result.computed_dynamic_array_for_loop_exit_cleanup_state
        )
    );
    append_report_lines(
        report,
        computed_dynamic_array_for_cleanup_transition_state_report(
            result.computed_dynamic_array_for_cleanup_transition_state
        )
    );
    append_report_lines(
        report,
        computed_inserted_cleanup_handoff_state_report(
            result.computed_dynamic_array_for_inserted_cleanup_handoff_state
        )
    );
    append_report_lines(
        report,
        computed_cleanup_call_blocker_summary_report(
            result.computed_dynamic_array_for_inserted_cleanup_handoff_state
        )
    );
    append_report_lines(
        report,
        computed_cleanup_proof_summary_state_report(
            result.computed_dynamic_array_for_cleanup_proof_summary_state
        )
    );
    append_report_lines(
        report,
        computed_inserted_cleanup_state_verification_report(
            result.computed_dynamic_array_for_inserted_cleanup_state_verification_state
        )
    );
    append_report_lines(
        report,
        computed_cleanup_call_emission_gate_state_report(
            result.computed_dynamic_array_for_cleanup_call_emission_gate_state
        )
    );
    append_report_lines(
        report,
        computed_cleanup_call_plan_state_report(
            result.computed_dynamic_array_for_cleanup_call_plan_render_state
        )
    );
    append_report_lines(
        report,
        computed_cleanup_call_render_state_report(
            result.computed_dynamic_array_for_cleanup_call_plan_render_state
        )
    );
    append_report_lines(
        report,
        computed_cleanup_call_insertion_capability_report(
            result.computed_dynamic_array_for_cleanup_call_insertion_capability_state
        )
    );
    append_report_lines(
        report,
        computed_cleanup_call_insertion_readiness_report(
            result.computed_dynamic_array_for_cleanup_call_insertion_gate_state
        )
    );
    append_report_lines(
        report,
        computed_inserted_cleanup_call_state_report(
            result.computed_dynamic_array_for_inserted_cleanup_call_state
        )
    );
    append_report_lines(
        report,
        consumed_descriptor_finalization_state_report(
            result.consumed_descriptor_finalization_state
        )
    );
    append_report_lines(
        report,
        computed_consumed_cleanup_descriptor_model_state_report(
            result.computed_dynamic_array_for_consumed_cleanup_descriptor_model_state
        )
    );
    append_report_lines(
        report,
        computed_consumed_cleanup_descriptor_state_report(
            result.computed_dynamic_array_for_consumed_cleanup_descriptor_state
        )
    );
    append_report_lines(
        report,
        computed_dynamic_array_for_production_emission_gate_state_report(
            result.computed_dynamic_array_for_production_emission_gate_state
        )
    );
    append_report_lines(
        report,
        computed_dynamic_array_for_production_sequence_state_report(
            result.computed_dynamic_array_for_production_sequence_state
        )
    );
    append_report_lines(
        report,
        dynamic_array_cleanup_production_readiness_state_report(
            result.dynamic_array_cleanup_production_readiness
        )
    );
    return report;
}

auto dynamic_array_cleanup_audit(
    std::filesystem::path const& source_path,
    pipeline::CompilePipelineOptions const& options
) -> CompileResult {
    pipeline::CompilePipeline pipeline;
    auto result = pipeline.collect_dynamic_array_cleanup_metadata(source_path, options);
    if (result.has_errors()) {
        return CompileResult {
            .exit_code = 1,
            .stderr_text = std::move(result.error_text),
        };
    }

    auto emitted_result = pipeline.emit_llvm(source_path, options);
    if (!emitted_result.has_errors()) {
        prefer_emitted_dynamic_array_cleanup_reports(result, std::move(emitted_result));
    }

    return CompileResult {
        .exit_code = 0,
        .stdout_text = render_report_lines(dynamic_array_cleanup_audit_report(result)),
    };
}

auto runtime_indexed_cleanup_audit(std::filesystem::path const& source_path) -> CompileResult {
    pipeline::CompilePipeline pipeline;
    auto result = pipeline.emit_llvm(source_path, runtime_indexed_cleanup_audit_options());
    auto lines = result.runtime_indexed_cleanup_audit_lines;
    if (lines.empty()) {
        lines.push_back("runtime-index cleanup audit: no runtime-index cleanup metadata");
    } else {
        lines.push_back(runtime_indexed_cleanup_function_module_verification_report(
            result.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
        ));
        for (auto const& conflict :
             result.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state.splice_conflicts) {
            lines.push_back(runtime_indexed_cleanup_function_module_splice_conflict_report(conflict));
        }
        for (auto const& verification :
             result.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state.verifications) {
            if (!verification.llvm_verifier_passed && verification.llvm_verifier_diagnostic_count > 0) {
                lines.push_back(runtime_indexed_cleanup_function_module_verification_detail_report(verification));
            }
        }
        lines.push_back(runtime_indexed_cleanup_function_module_mutation_report(
            result.runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
        ));
        lines.push_back(runtime_indexed_cleanup_module_ir_production_readiness_report(
            result.runtime_indexed_cleanup_module_ir_production_readiness_state
        ));
        auto readiness_blocker_lines =
            pipeline::format_runtime_indexed_cleanup_production_readiness_blocker_report(
                result.runtime_indexed_cleanup_module_ir_production_readiness_state
            );
        lines.insert(lines.end(), readiness_blocker_lines.begin(), readiness_blocker_lines.end());
    }
    return CompileResult {
        .exit_code = 0,
        .stdout_text = render_report_lines(lines),
    };
}

auto runtime_indexed_cleanup_emit_llvm(std::filesystem::path const& source_path) -> CompileResult {
    pipeline::CompilePipeline pipeline;
    auto result = pipeline.emit_llvm(source_path, runtime_indexed_cleanup_audit_options());
    auto readiness_report = runtime_indexed_cleanup_module_ir_production_readiness_report(
        result.runtime_indexed_cleanup_module_ir_production_readiness_state
    ) + "\n";
    if (result.has_errors()) {
        return CompileResult {
            .exit_code = 1,
            .stderr_text = std::move(readiness_report) + result.error_text,
        };
    }
    if (!result.runtime_indexed_cleanup_module_ir_production_readiness_state.production_ready) {
        return CompileResult {
            .exit_code = 1,
            .stderr_text = std::move(readiness_report),
        };
    }
    if (!result.ir_text.empty() && result.ir_text.back() != '\n') {
        result.ir_text.push_back('\n');
    }
    return CompileResult {
        .exit_code = 0,
        .stdout_text = std::move(result.ir_text),
    };
}

auto test_only_runtime_indexed_cleanup_production_readiness(std::filesystem::path const& source_path) -> CompileResult {
    pipeline::CompilePipeline pipeline;
    auto result = pipeline.emit_llvm(source_path, runtime_indexed_cleanup_audit_options());
    auto readiness_report = runtime_indexed_cleanup_module_ir_production_readiness_report(
        result.runtime_indexed_cleanup_module_ir_production_readiness_state
    ) + "\n";
    if (result.has_errors()) {
        return CompileResult {
            .exit_code = 1,
            .stdout_text = std::move(readiness_report),
            .stderr_text = std::move(result.error_text),
        };
    }
    if (!result.runtime_indexed_cleanup_module_ir_production_readiness_state.production_ready) {
        return CompileResult {
            .exit_code = 1,
            .stdout_text = std::move(readiness_report),
        };
    }
    return CompileResult {
        .exit_code = 0,
        .stdout_text = std::move(readiness_report),
    };
}

auto runtime_indexed_constructor_move_production_readiness(std::filesystem::path const& source_path) -> CompileResult {
    pipeline::CompilePipeline pipeline;
    auto result = pipeline.emit_llvm(source_path, runtime_indexed_constructor_move_production_readiness_options());
    auto readiness_report = runtime_indexed_constructor_move_production_readiness_report(result) + "\n";
    if (
        result.has_errors() &&
        result.error_text.find("indexed constructor ownership move requires explicit partial ownership support") ==
            std::string::npos
    ) {
        return CompileResult {
            .exit_code = 1,
            .stdout_text = std::move(readiness_report),
            .stderr_text = std::move(result.error_text),
        };
    }
    return CompileResult {
        .exit_code = 0,
        .stdout_text = std::move(readiness_report),
    };
}

auto test_only_runtime_indexed_constructor_move_run(std::filesystem::path const& source_path) -> CompileResult {
    pipeline::CompilePipeline pipeline;
    auto result = pipeline.emit_object(source_path, runtime_indexed_constructor_move_run_options());
    if (result.has_errors()) {
        return CompileResult {
            .exit_code = 1,
            .stderr_text = std::move(result.error_text),
        };
    }

    link::HostRunner runner;
    auto run_result = runner.run(result.object_bytes, result.link_libraries);
    if (run_result.has_errors()) {
        return CompileResult {
            .exit_code = 1,
            .stderr_text = run_result.diagnostics.render(result.source_file->path().string()),
        };
    }
    return CompileResult {.exit_code = run_result.exit_code};
}

auto analyze_report(std::filesystem::path const& source_path, auto report_selector) -> CompileResult {
    pipeline::CompilePipeline pipeline;
    auto result = pipeline.analyze(source_path);
    if (result.has_errors()) {
        return CompileResult {
            .exit_code = 1,
            .stderr_text = std::move(result.error_text),
        };
    }

    return CompileResult {
        .exit_code = 0,
        .stdout_text = render_report_lines(report_selector(result)),
    };
}

auto render_type(orison::syntax::TypeSyntax const& type) -> std::string {
    std::string rendered = type.name;
    if (type.generic_arguments.empty()) {
        return rendered;
    }

    rendered += "<";
    for (std::size_t i = 0; i < type.generic_arguments.size(); ++i) {
        if (i > 0) {
            rendered += ", ";
        }
        rendered += render_type(type.generic_arguments[i]);
    }
    rendered += ">";
    return rendered;
}

auto render_visibility(orison::syntax::Visibility visibility) -> std::string_view {
    using orison::syntax::Visibility;
    switch (visibility) {
    case Visibility::public_visibility:
        return "public";
    case Visibility::package_visibility:
        return "package";
    case Visibility::private_visibility:
        return "private";
    }
    return "unknown";
}

auto render_where_constraint(orison::syntax::WhereConstraintSyntax const& constraint) -> std::string {
    std::string rendered = constraint.parameter_name + ": ";
    for (std::size_t i = 0; i < constraint.requirements.size(); ++i) {
        if (i > 0) {
            rendered += " + ";
        }
        rendered += render_type(constraint.requirements[i]);
    }
    return rendered;
}

auto render_statement_kind(orison::syntax::StatementKind kind) -> std::string_view {
    using orison::syntax::StatementKind;
    switch (kind) {
    case StatementKind::let_binding:
        return "let";
    case StatementKind::var_binding:
        return "var";
    case StatementKind::assignment_statement:
        return "assignment";
    case StatementKind::return_statement:
        return "return";
    case StatementKind::break_statement:
        return "break";
    case StatementKind::continue_statement:
        return "continue";
    case StatementKind::switch_statement:
        return "switch";
    case StatementKind::guard_statement:
        return "guard";
    case StatementKind::if_statement:
        return "if";
    case StatementKind::while_statement:
        return "while";
    case StatementKind::repeat_statement:
        return "repeat";
    case StatementKind::for_statement:
        return "for";
    case StatementKind::unsafe_statement:
        return "unsafe";
    case StatementKind::defer_statement:
        return "defer";
    case StatementKind::expression_statement:
        return "expression";
    }
    return "unknown";
}

auto render_inline_statement(orison::syntax::StatementSyntax const& statement) -> std::string {
    switch (statement.kind) {
    case orison::syntax::StatementKind::let_binding:
        return "let " + statement.name + " = " + render_expression(statement.expression);
    case orison::syntax::StatementKind::var_binding:
        return "var " + statement.name + " = " + render_expression(statement.expression);
    case orison::syntax::StatementKind::assignment_statement:
        return render_expression(statement.assignment_target) + " = " + render_expression(statement.expression);
    case orison::syntax::StatementKind::return_statement:
        if (statement.expression.text.empty() && !statement.expression.left && !statement.expression.right &&
            !statement.expression.alternate && statement.expression.arguments.empty() &&
            statement.expression.nested_statements.empty()) {
            return "return";
        }
        return "return " + render_expression(statement.expression);
    case orison::syntax::StatementKind::expression_statement:
        return render_expression(statement.expression);
    default:
        return std::string(render_statement_kind(statement.kind));
    }
}

auto render_expression(orison::syntax::ExpressionSyntax const& expression) -> std::string {
    using orison::syntax::ExpressionKind;
    switch (expression.kind) {
    case ExpressionKind::name:
    case ExpressionKind::integer_literal:
    case ExpressionKind::float_literal:
    case ExpressionKind::string_literal:
    case ExpressionKind::boolean_literal:
        return expression.text;
    case ExpressionKind::array_literal: {
        std::string rendered = "[";
        for (std::size_t i = 0; i < expression.arguments.size(); ++i) {
            if (i > 0) {
                rendered += ", ";
            }
            rendered += render_expression(expression.arguments[i]);
        }
        rendered += "]";
        return rendered;
    }
    case ExpressionKind::task:
    case ExpressionKind::thread: {
        std::string rendered = expression.text + " { ";
        for (std::size_t i = 0; i < expression.nested_statements.size(); ++i) {
            if (i > 0) {
                rendered += "; ";
            }
            rendered += render_inline_statement(*expression.nested_statements[i]);
        }
        rendered += " }";
        return rendered;
    }
    case ExpressionKind::unary:
        if (expression.text == "not" || expression.text == "bit_not" || expression.text == "await") {
            return expression.text + " " + render_expression(*expression.left);
        }
        return expression.text + render_expression(*expression.left);
    case ExpressionKind::cast:
        return render_expression(*expression.left) + " as " + expression.text;
    case ExpressionKind::call: {
        std::string rendered = render_expression(*expression.left);
        rendered += "(";
        for (std::size_t i = 0; i < expression.arguments.size(); ++i) {
            if (i > 0) {
                rendered += ", ";
            }
            rendered += render_expression(expression.arguments[i]);
        }
        rendered += ")";
        return rendered;
    }
    case ExpressionKind::member_access:
        return render_expression(*expression.left) + "." + expression.text;
    case ExpressionKind::null_safe_member_access:
        return render_expression(*expression.left) + "?." + expression.text;
    case ExpressionKind::index_access:
        return render_expression(*expression.left) + "[" + render_expression(expression.arguments.front()) + "]";
    case ExpressionKind::binary:
        return "(" + render_expression(*expression.left) + " " + expression.text + " " +
               render_expression(*expression.right) + ")";
    case ExpressionKind::ternary:
        return "(" + render_expression(*expression.left) + " ? " + render_expression(*expression.right) + " : " +
               render_expression(*expression.alternate) + ")";
    }
    return "";
}

}  // namespace

auto CompilerApp::run(std::span<char const* const> args) const -> CompileResult {
    auto command_index = std::size_t {1};
    auto options = pipeline::CompilePipelineOptions {};

    if (args.size() > command_index && std::string_view(args[command_index]) == "--version") {
        return CompileResult {.exit_code = 0, .stdout_text = "orisonc 0.1.0-dev\n"};
    }

    if (args.size() == command_index + 2 && std::string_view(args[command_index]) == "run") {
        pipeline::CompilePipeline pipeline;
        auto result = pipeline.emit_object(std::filesystem::path(args[command_index + 1]), options);
        if (result.has_errors()) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = std::move(result.error_text),
            };
        }

        link::HostRunner runner;
        auto run_result = runner.run(result.object_bytes, result.link_libraries);
        if (run_result.has_errors()) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = run_result.diagnostics.render(result.source_file->path().string()),
            };
        }
        return CompileResult {.exit_code = run_result.exit_code};
    }

    if (args.size() == command_index + 2 && std::string_view(args[command_index]) == "--emit-llvm") {
        pipeline::CompilePipeline pipeline;
        auto result = pipeline.emit_llvm(std::filesystem::path(args[command_index + 1]), options);
        if (result.has_errors()) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = std::move(result.error_text),
            };
        }
        return CompileResult {
            .exit_code = 0,
            .stdout_text = std::move(result.ir_text),
        };
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--planned-drops") {
        return emit_llvm_report(std::filesystem::path(args[2]), [](auto const& result) {
            return planned_drop_declaration_state_report(result.planned_drop_declaration_state);
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--semantic-planned-drops") {
        return analyze_report(std::filesystem::path(args[2]), [](auto const& result) {
            return semantics::format_planned_drop_site_report(result.semantic_result.planned_drop_sites);
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--semantic-drop-resolution") {
        return analyze_report(std::filesystem::path(args[2]), [](auto const& result) {
            return semantic_drop_resolution_state_report(result);
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--semantic-drop-diagnostics") {
        return analyze_report(std::filesystem::path(args[2]), [](auto const& result) {
            return semantic_drop_diagnostic_state_report(result);
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--semantic-drop-lowering-authorization") {
        return analyze_report(std::filesystem::path(args[2]), [](auto const& result) {
            return semantics::format_drop_lowering_authorization_report(
                result.semantic_drop_lowering_authorizations
            );
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--semantic-drop-summary") {
        return analyze_report(std::filesystem::path(args[2]), [](auto const& result) {
            return semantic_drop_resolution_summary_state_report(result.semantic_drop_state);
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--semantic-dynamic-array-descriptor-origins") {
        return analyze_report(std::filesystem::path(args[2]), [](auto const& result) {
            return semantic_dynamic_array_descriptor_origin_state_report(result.semantic_result);
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--planned-drop-actions") {
        return emit_llvm_report(std::filesystem::path(args[2]), [](auto const& result) {
            return planned_drop_action_state_report(result.planned_drop_action_state);
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--emitted-drops") {
        return emit_llvm_report(std::filesystem::path(args[2]), [](auto const& result) {
            return emitted_drop_declaration_state_report(result.planned_drop_declaration_state);
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--drop-cleanup-authorization") {
        return emit_llvm_report(std::filesystem::path(args[2]), [](auto const& result) {
            return drop_cleanup_authorization_state_report(result.drop_cleanup_authorization_state);
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--drop-readiness") {
        return emit_llvm_report(std::filesystem::path(args[2]), [](auto const& result) {
            return drop_readiness_snapshot_state_report(result.drop_readiness_snapshot);
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--drop-readiness-summary") {
        return emit_llvm_report(std::filesystem::path(args[2]), [](auto const& result) {
            return drop_readiness_summary_state_report(result.drop_readiness_summary);
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--drop-readiness-relations") {
        return emit_llvm_report(std::filesystem::path(args[2]), [](auto const& result) {
            return drop_readiness_relation_state_report(result.drop_readiness_snapshot);
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--drop-readiness-blockers") {
        return emit_llvm_report(std::filesystem::path(args[2]), [](auto const& result) {
            return drop_readiness_blocker_state_report(result.drop_readiness_blocker_summary);
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--drop-readiness-source-correlations") {
        return emit_llvm_report(std::filesystem::path(args[2]), [](auto const& result) {
            return drop_readiness_source_correlation_state_report(result.drop_readiness_snapshot);
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--dynamic-array-descriptor-cleanup-plan") {
        return dynamic_array_cleanup_report(
            std::filesystem::path(args[2]),
            dynamic_array_cleanup_report_options(),
            [](auto const& result) {
                return dynamic_array_descriptor_cleanup_plan_state_report(
                    result.dynamic_array_descriptor_cleanup_plan_state
                );
            }
        );
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--dynamic-array-cleanup-obligations") {
        return dynamic_array_cleanup_report(
            std::filesystem::path(args[2]),
            dynamic_array_cleanup_report_options(),
            [](auto const& result) {
                return dynamic_array_cleanup_obligation_state_report(result.dynamic_array_cleanup_obligation_state);
            }
        );
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--dynamic-array-cleanup-sequence-plan") {
        return dynamic_array_cleanup_report(
            std::filesystem::path(args[2]),
            dynamic_array_cleanup_report_options(),
            [](auto const& result) {
                return dynamic_array_cleanup_sequence_plan_state_report(result.dynamic_array_cleanup_sequence_plan_state);
            }
        );
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--dynamic-array-cleanup-sequence-verification") {
        return dynamic_array_cleanup_report(
            std::filesystem::path(args[2]),
            dynamic_array_cleanup_report_options(),
            [](auto const& result) {
                return dynamic_array_cleanup_sequence_verification_state_report(
                    result.dynamic_array_cleanup_sequence_verification_state
                );
            }
        );
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--dynamic-array-cleanup-emission-gate") {
        return dynamic_array_cleanup_report(
            std::filesystem::path(args[2]),
            dynamic_array_cleanup_report_options(),
            [](auto const& result) {
                return dynamic_array_cleanup_emission_gate_state_report(
                    result.dynamic_array_cleanup_sequence_verification_state
                );
            }
        );
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--dynamic-array-cleanup-capability") {
        return dynamic_array_cleanup_report(
            std::filesystem::path(args[2]),
            dynamic_array_cleanup_report_options(),
            [](auto const& result) {
                return dynamic_array_cleanup_emission_capability_state_report(
                    result.dynamic_array_cleanup_emission_capability_state
                );
            }
        );
    }

    if (auto result = try_dynamic_array_cleanup_report_command(
            args,
            "--computed-dynamic-array-cleanup-call-insertion-capability",
            dynamic_array_cleanup_report_options(),
            [](auto const& result) {
                return computed_cleanup_call_insertion_capability_report(
                    result.computed_dynamic_array_for_cleanup_call_insertion_capability_state
                );
            }
        )) {
        return std::move(*result);
    }

    if (auto result = try_dynamic_array_cleanup_report_command(
            args,
            "--test-only-computed-dynamic-array-cleanup-call-insertion-capability",
            test_only_computed_cleanup_insertion_seam_options(),
            [](auto const& result) {
                return computed_cleanup_call_insertion_capability_report(
                    result.computed_dynamic_array_for_cleanup_call_insertion_capability_state
                );
            }
        )) {
        return std::move(*result);
    }

    if (auto result = try_emit_llvm_report_command(
            args,
            "--computed-dynamic-array-cleanup-call-insertion-readiness",
            computed_cleanup_call_insertion_readiness_options(),
            [](auto const& result) {
                return computed_cleanup_call_insertion_readiness_report(
                    result.computed_dynamic_array_for_cleanup_call_insertion_gate_state
                );
            }
        )) {
        return std::move(*result);
    }

    if (auto result = try_emit_llvm_report_command(
            args,
            "--test-only-computed-dynamic-array-cleanup-call-insertion-readiness",
            test_only_computed_cleanup_insertion_seam_options(),
            [](auto const& result) {
                return computed_cleanup_call_insertion_readiness_report(
                    result.computed_dynamic_array_for_cleanup_call_insertion_gate_state
                );
            }
        )) {
        return std::move(*result);
    }

    if (auto result = try_emit_llvm_report_command(
            args,
            "--computed-dynamic-array-inserted-cleanup-handoffs",
            computed_cleanup_call_insertion_readiness_options(),
            [](auto const& result) {
                return computed_inserted_cleanup_handoff_state_report(
                    result.computed_dynamic_array_for_inserted_cleanup_handoff_state
                );
            }
        )) {
        return std::move(*result);
    }

    if (auto result = try_emit_llvm_report_command(
            args,
            "--test-only-computed-dynamic-array-inserted-cleanup-handoffs",
            test_only_computed_cleanup_insertion_seam_options(),
            [](auto const& result) {
                return computed_inserted_cleanup_handoff_state_report(
                    result.computed_dynamic_array_for_inserted_cleanup_handoff_state
                );
            }
        )) {
        return std::move(*result);
    }

    if (auto result = try_emit_llvm_report_command(
            args,
            "--computed-dynamic-array-inserted-cleanup-calls",
            computed_cleanup_call_insertion_readiness_options(),
            [](auto const& result) {
                return computed_inserted_cleanup_call_state_report(
                    result.computed_dynamic_array_for_inserted_cleanup_call_state
                );
            }
        )) {
        return std::move(*result);
    }

    if (auto result = try_emit_llvm_report_command(
            args,
            "--test-only-computed-dynamic-array-inserted-cleanup-calls",
            test_only_computed_cleanup_insertion_seam_options(),
            [](auto const& result) {
                return computed_inserted_cleanup_call_state_report(
                    result.computed_dynamic_array_for_inserted_cleanup_call_state
                );
            }
        )) {
        return std::move(*result);
    }

    if (auto result = try_emit_llvm_report_command(
            args,
            "--computed-dynamic-array-consumed-cleanup-descriptors",
            computed_cleanup_call_insertion_readiness_options(),
            [](auto const& result) {
                return computed_consumed_cleanup_descriptor_state_report(
                    result.computed_dynamic_array_for_consumed_cleanup_descriptor_state
                );
            }
        )) {
        return std::move(*result);
    }

    if (auto result = try_emit_llvm_report_command(
            args,
            "--test-only-computed-dynamic-array-consumed-cleanup-descriptors",
            test_only_computed_cleanup_insertion_seam_options(),
            [](auto const& result) {
                return computed_consumed_cleanup_descriptor_state_report(
                    result.computed_dynamic_array_for_consumed_cleanup_descriptor_state
                );
            }
        )) {
        return std::move(*result);
    }

    if (auto result = try_emit_llvm_report_command(
            args,
            "--computed-dynamic-array-cleanup-proof-summary",
            computed_cleanup_call_insertion_readiness_options(),
            [](auto const& result) {
                return computed_cleanup_proof_summary_state_report(
                    result.computed_dynamic_array_for_cleanup_proof_summary_state
                );
            }
        )) {
        return std::move(*result);
    }

    if (auto result = try_emit_llvm_report_command(
            args,
            "--test-only-computed-dynamic-array-cleanup-proof-summary",
            test_only_computed_cleanup_insertion_seam_options(),
            [](auto const& result) {
                return computed_cleanup_proof_summary_state_report(
                    result.computed_dynamic_array_for_cleanup_proof_summary_state
                );
            }
        )) {
        return std::move(*result);
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--test-only-aggregate-projection-access-plans") {
        return emit_llvm_report_with_failure_output(
            std::filesystem::path(args[2]),
            pipeline::CompilePipelineOptions {
                .collect_aggregate_projection_access_metadata = true,
            },
            [](auto const& result) {
                return aggregate_projection_access_plan_state_report(
                    result.aggregate_projection_access_plan_state
                );
            }
        );
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--dynamic-array-cleanup-production-readiness") {
        return dynamic_array_cleanup_report(
            std::filesystem::path(args[2]),
            dynamic_array_cleanup_report_options(),
            [](auto const& result) {
                return dynamic_array_cleanup_production_readiness_state_report(
                    result.dynamic_array_cleanup_production_readiness
                );
            }
        );
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--dynamic-array-cleanup-audit") {
        return dynamic_array_cleanup_audit(
            std::filesystem::path(args[2]),
            computed_cleanup_call_insertion_readiness_options()
        );
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--runtime-indexed-cleanup-audit") {
        return runtime_indexed_cleanup_audit(std::filesystem::path(args[2]));
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--runtime-indexed-cleanup-emit-llvm") {
        return runtime_indexed_cleanup_emit_llvm(std::filesystem::path(args[2]));
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--runtime-indexed-constructor-move-production-readiness") {
        return runtime_indexed_constructor_move_production_readiness(std::filesystem::path(args[2]));
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--test-only-runtime-indexed-cleanup-production-readiness") {
        return test_only_runtime_indexed_cleanup_production_readiness(std::filesystem::path(args[2]));
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--test-only-runtime-indexed-constructor-move-run") {
        return test_only_runtime_indexed_constructor_move_run(std::filesystem::path(args[2]));
    }

    if (args.size() == command_index + 4 && std::string_view(args[command_index]) == "--emit-object" &&
        std::string_view(args[command_index + 2]) == "-o") {
        pipeline::CompilePipeline pipeline;
        auto result = pipeline.emit_object(std::filesystem::path(args[command_index + 1]), options);
        if (result.has_errors()) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = std::move(result.error_text),
            };
        }

        auto output_path = std::filesystem::path(args[command_index + 3]);
        auto output = std::ofstream(output_path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!output) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = "error: unable to write object file\n",
            };
        }
        output.write(
            result.object_bytes.data(),
            static_cast<std::streamsize>(result.object_bytes.size())
        );
        output.close();
        if (!output) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = "error: unable to write object file\n",
            };
        }
        return CompileResult {.exit_code = 0};
    }

    if (args.size() == command_index + 4 && std::string_view(args[command_index]) == "--build" &&
        std::string_view(args[command_index + 2]) == "-o") {
        pipeline::CompilePipeline pipeline;
        auto result = pipeline.emit_object(std::filesystem::path(args[command_index + 1]), options);
        if (result.has_errors()) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = std::move(result.error_text),
            };
        }

        link::HostLinker linker;
        auto link_result = linker.link(
            result.object_bytes,
            std::filesystem::path(args[command_index + 3]),
            result.link_libraries
        );
        if (link_result.has_errors()) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = link_result.diagnostics.render(result.source_file->path().string()),
            };
        }
        return CompileResult {.exit_code = 0};
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--parse") {
        pipeline::CompilePipeline pipeline;
        auto result = pipeline.analyze(std::filesystem::path(args[2]));
        if (result.has_errors()) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = std::move(result.error_text),
            };
        }
        auto const& source_file = *result.source_file;
        auto const& parse_result = result.parse_result;

        std::ostringstream output;
        output << "parsed " << source_file.path().string() << '\n';
        output << "package " << parse_result.module.package_name << '\n';
        output << "top-level declarations: " << parse_result.module.top_level_declaration_count << '\n';
        output << "imports: " << parse_result.module.imports.size() << '\n';
        output << "foreign imports: " << parse_result.module.foreign_imports.size() << '\n';
        output << "foreign exports: " << parse_result.module.foreign_exports.size() << '\n';
        output << "constants: " << parse_result.module.constants.size() << '\n';
        output << "type aliases: " << parse_result.module.type_aliases.size() << '\n';
        output << "records: " << parse_result.module.records.size() << '\n';
        output << "choices: " << parse_result.module.choices.size() << '\n';
        output << "interfaces: " << parse_result.module.interfaces.size() << '\n';
        output << "implementations: " << parse_result.module.implementations.size() << '\n';
        output << "extensions: " << parse_result.module.extensions.size() << '\n';
        output << "functions: " << parse_result.module.functions.size() << '\n';
        if (!parse_result.module.imports.empty()) {
            output << "first import from: " << parse_result.module.imports.front().from_package << '\n';
        }
        if (!parse_result.module.foreign_imports.empty()) {
            output << "first foreign import abi: " << parse_result.module.foreign_imports.front().abi << '\n';
            output << "first foreign import library: "
                   << (parse_result.module.foreign_imports.front().library_name.empty()
                           ? "<none>"
                           : parse_result.module.foreign_imports.front().library_name)
                   << '\n';
            output << "first foreign import functions: " << parse_result.module.foreign_imports.front().functions.size()
                   << '\n';
        }
        if (!parse_result.module.foreign_exports.empty()) {
            output << "first foreign export abi: " << parse_result.module.foreign_exports.front().abi << '\n';
            output << "first foreign export symbol: "
                   << (parse_result.module.foreign_exports.front().external_name.empty()
                           ? "<none>"
                           : parse_result.module.foreign_exports.front().external_name)
                   << '\n';
            output << "first foreign export function: "
                   << parse_result.module.foreign_exports.front().function.name << '\n';
        }
        if (!parse_result.module.constants.empty()) {
            output << "first constant type: " << render_type(parse_result.module.constants.front().type) << '\n';
            output << "first constant initializer: "
                   << render_expression(parse_result.module.constants.front().initializer) << '\n';
        }
        if (!parse_result.module.type_aliases.empty()) {
            output << "first type alias visibility: "
                   << render_visibility(parse_result.module.type_aliases.front().visibility) << '\n';
            output << "first type alias target: "
                   << render_type(parse_result.module.type_aliases.front().aliased_type) << '\n';
        }
        if (!parse_result.module.records.empty()) {
            output << "first record visibility: " << render_visibility(parse_result.module.records.front().visibility)
                   << '\n';
            output << "record fields: " << parse_result.module.records.front().fields.size() << '\n';
            if (!parse_result.module.records.front().fields.empty()) {
                output << "first field visibility: "
                       << render_visibility(parse_result.module.records.front().fields.front().visibility) << '\n';
                output << "first field type: "
                       << render_type(parse_result.module.records.front().fields.front().type) << '\n';
            }
        }
        if (!parse_result.module.choices.empty()) {
            output << "first choice visibility: " << render_visibility(parse_result.module.choices.front().visibility)
                   << '\n';
            output << "first choice variants: " << parse_result.module.choices.front().variants.size() << '\n';
            if (!parse_result.module.choices.front().variants.empty()) {
                output << "first choice payloads: "
                       << parse_result.module.choices.front().variants.front().payloads.size() << '\n';
            }
        }
        if (!parse_result.module.interfaces.empty()) {
            output << "first interface visibility: "
                   << render_visibility(parse_result.module.interfaces.front().visibility) << '\n';
            output << "first interface methods: " << parse_result.module.interfaces.front().methods.size() << '\n';
        }
        if (!parse_result.module.implementations.empty()) {
            output << "first implementation interface: "
                   << render_type(parse_result.module.implementations.front().interface_type) << '\n';
            output << "first implementation receiver: "
                   << render_type(parse_result.module.implementations.front().receiver_type) << '\n';
            output << "first implementation methods: " << parse_result.module.implementations.front().methods.size()
                   << '\n';
        }
        if (!parse_result.module.extensions.empty()) {
            output << "first extension receiver: " << render_type(parse_result.module.extensions.front().receiver_type)
                   << '\n';
            output << "first extension methods: " << parse_result.module.extensions.front().methods.size() << '\n';
            if (!parse_result.module.extensions.front().methods.empty()) {
                output << "first extension method visibility: "
                       << render_visibility(parse_result.module.extensions.front().methods.front().visibility) << '\n';
            }
        }
        if (!parse_result.module.functions.empty()) {
            output << "first function visibility: "
                   << render_visibility(parse_result.module.functions.front().visibility) << '\n';
            output << "first function async: "
                   << (parse_result.module.functions.front().is_async ? "true" : "false") << '\n';
            output << "first function unsafe: "
                   << (parse_result.module.functions.front().is_unsafe ? "true" : "false") << '\n';
            output << "function parameters: " << parse_result.module.functions.front().parameters.size() << '\n';
            output << "function return type: " << render_type(parse_result.module.functions.front().return_type)
                   << '\n';
            output << "function where constraints: "
                   << parse_result.module.functions.front().where_constraints.size() << '\n';
            if (!parse_result.module.functions.front().where_constraints.empty()) {
                output << "first function where constraint: "
                       << render_where_constraint(parse_result.module.functions.front().where_constraints.front())
                       << '\n';
            }
            output << "function body statements: " << parse_result.module.functions.front().body_statements.size()
                   << '\n';
            if (!parse_result.module.functions.front().body_statements.empty()) {
                auto const& first_statement = parse_result.module.functions.front().body_statements.front();
                output << "first statement kind: " << render_statement_kind(first_statement.kind) << '\n';
                output << "first statement expression: " << render_expression(first_statement.expression)
                       << '\n';
                output << "first statement nested count: " << first_statement.nested_statements.size() << '\n';
                output << "first statement alternate count: " << first_statement.alternate_statements.size() << '\n';
                output << "first statement switch cases: " << first_statement.switch_cases.size() << '\n';
                if (!first_statement.switch_cases.empty()) {
                    output << "first switch case pattern: "
                           << render_expression(first_statement.switch_cases.front().pattern) << '\n';
                    output << "first switch case statements: " << first_statement.switch_cases.front().statements.size()
                           << '\n';
                }
            }
        }
        return CompileResult {.exit_code = 0, .stdout_text = output.str()};
    }

    return CompileResult {.exit_code = 1, .stderr_text = usage_text() + "\n"};
}

}  // namespace orison::driver
