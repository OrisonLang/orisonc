#include "computed_dynamic_array_audit_expectations.hpp"

#include "computed_cleanup_proof_model.hpp"

#include "orison/lowering/consumed_descriptor_finalization.hpp"
#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/drop_metadata.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/llvm_object_emitter.hpp"
#include "orison/link/host_linker.hpp"
#include "orison/pipeline/compile_pipeline.hpp"
#include "orison/pipeline/drop_readiness_source_correlation_report.hpp"
#include "orison/pipeline/dynamic_array_cleanup_metadata.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <vector>
#include <unistd.h>

namespace {

namespace smoke = orison::tests::smoke;

auto logical_line_count(std::string const& text) -> std::size_t {
    if (text.empty()) {
        return 0;
    }

    auto line_count = static_cast<std::size_t>(
        std::count(text.begin(), text.end(), '\n')
    );
    if (text.back() != '\n') {
        ++line_count;
    }
    return line_count;
}

auto occurrence_count(
    std::string const& text,
    std::string const& needle
) -> std::size_t {
    if (text.empty() || needle.empty()) {
        return 0;
    }

    auto count = std::size_t {0};
    auto position = std::string::size_type {0};
    while ((position = text.find(needle, position)) != std::string::npos) {
        ++count;
        position += needle.size();
    }
    return count;
}

void assert_line_contains(
    std::vector<std::string> const& lines,
    std::size_t index,
    std::string_view expected_fragment
) {
    assert(index < lines.size());
    if (lines[index].find(expected_fragment) == std::string::npos) {
        std::cerr << "expected report line " << index << " to contain '" << expected_fragment
                  << "', actual line: '" << lines[index] << "'\n";
    }
    assert(lines[index].find(expected_fragment) != std::string::npos);
}

void assert_any_line_contains(
    std::vector<std::string> const& lines,
    std::string_view expected_fragment
) {
    for (auto const& line : lines) {
        if (line.find(expected_fragment) != std::string::npos) {
            return;
        }
    }
    std::cerr << "expected any report line to contain '" << expected_fragment << "'\n";
    for (auto const& line : lines) {
        std::cerr << "actual line: '" << line << "'\n";
    }
    assert(false);
}

auto line_index_containing(
    std::vector<std::string> const& lines,
    std::string_view expected_fragment
) -> std::size_t {
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (lines[index].find(expected_fragment) != std::string::npos) {
            return index;
        }
    }
    std::cerr << "expected any report line to contain '" << expected_fragment << "'\n";
    for (auto const& line : lines) {
        std::cerr << "actual line: '" << line << "'\n";
    }
    assert(false);
    return lines.size();
}

auto formatted_dynamic_array_cleanup_production_readiness_report(
    orison::pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    return {
        orison::pipeline::format_dynamic_array_cleanup_production_readiness(
            result.dynamic_array_cleanup_production_readiness
        ),
    };
}

auto semantic_dynamic_array_descriptor_origin_report(
    orison::pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    return orison::semantics::format_dynamic_array_descriptor_origin_report(
        result.semantic_result.dynamic_array_descriptor_origins
    );
}

auto semantic_drop_implementations(
    orison::pipeline::SemanticDropState const& state
) -> std::vector<orison::semantics::DropImplementation> {
    auto implementations = std::vector<orison::semantics::DropImplementation> {};
    implementations.reserve(state.discovered_implementations.size());
    for (auto const& discovered : state.discovered_implementations) {
        implementations.push_back(discovered.implementation);
    }
    return implementations;
}

auto semantic_drop_implementation_discovery_report(
    orison::pipeline::SemanticDropState const& state
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(state.discovered_implementations.size());
    for (auto const& discovered : state.discovered_implementations) {
        report.push_back(
            orison::semantics::format_drop_implementation(discovered.implementation) +
            " discovery " + discovered.discovery_name
        );
    }
    return report;
}

auto semantic_planned_drop_report(
    orison::pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    return orison::semantics::format_planned_drop_site_report(result.semantic_result.planned_drop_sites);
}

auto semantic_drop_resolution_report(
    orison::pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    return orison::semantics::format_drop_implementation_resolution_report(
        result.semantic_result.planned_drop_sites,
        semantic_drop_implementations(result.semantic_drop_state)
    );
}

auto semantic_drop_diagnostic_report(
    orison::pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    return orison::semantics::format_drop_implementation_diagnostic_report(
        result.semantic_result.planned_drop_sites,
        semantic_drop_implementations(result.semantic_drop_state)
    );
}

auto semantic_drop_lowering_authorization_report(
    orison::pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    return orison::semantics::format_drop_lowering_authorization_report(
        result.semantic_drop_lowering_authorizations
    );
}

auto semantic_drop_resolution_summary_report(
    orison::pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    return orison::semantics::format_drop_implementation_resolution_summary_report(
        result.semantic_drop_state.resolution_summaries
    );
}

auto dynamic_array_construction_plan_report(
    orison::pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    return orison::lowering::format_dynamic_array_construction_plan_report(
        result.dynamic_array_construction_plan_state.plans
    );
}

auto dynamic_array_runtime_request_report(
    orison::pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    return orison::lowering::format_dynamic_array_runtime_request_report(
        result.dynamic_array_runtime_request_state.operations
    );
}

auto planned_drop_declaration_report(
    orison::pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    return orison::lowering::format_planned_drop_report(
        result.planned_drop_declaration_state.declarations
    );
}

auto emitted_drop_declaration_report(
    orison::pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    return orison::lowering::format_emitted_drop_declaration_report(
        result.planned_drop_declaration_state.declarations
    );
}

auto planned_drop_action_report(
    orison::pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    return orison::lowering::format_planned_drop_action_report(
        result.planned_drop_action_state.actions
    );
}

auto drop_cleanup_authorization_report(
    orison::pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto const& state = result.drop_cleanup_authorization_state;
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
        auto cleanup_lines = orison::lowering::format_drop_cleanup_authorization_report(
            state.cleanups[index],
            authorization
        );
        lines.insert(lines.end(), cleanup_lines.begin(), cleanup_lines.end());
    }
    return lines;
}

auto drop_readiness_summary_report(
    orison::pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    if (result.has_errors()) {
        return {};
    }
    return {orison::lowering::format_drop_readiness_summary(result.drop_readiness_summary)};
}

auto drop_readiness_snapshot_report(
    orison::pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    if (result.has_errors()) {
        return {};
    }
    return orison::lowering::format_drop_readiness_snapshot_report(result.drop_readiness_snapshot);
}

auto drop_readiness_relation_report(
    orison::pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    if (result.has_errors()) {
        return {};
    }
    return orison::lowering::format_drop_readiness_relation_report(result.drop_readiness_snapshot);
}

auto drop_readiness_blocker_report(
    orison::pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    if (result.has_errors()) {
        return {};
    }
    return orison::lowering::format_drop_readiness_blocker_report(result.drop_readiness_blocker_summary);
}

auto drop_readiness_source_correlation_report(
    orison::pipeline::CompilePipelineResult const& result
) -> std::vector<std::string> {
    if (result.has_errors()) {
        return {};
    }
    return orison::pipeline::format_drop_readiness_source_correlation_report(result.drop_readiness_snapshot);
}

void assert_computed_cleanup_proof_model_reusable_without_reports() {
    auto handoffs = std::vector<orison::lowering::ComputedDynamicArrayCleanupStateHandoff> {
        {
            .kind = orison::lowering::ComputedDynamicArrayCleanupStateHandoffKind::acquire,
            .operation_name = "items.computed_for.cleanup.acquire",
            .source_owner_name = "items",
            .target_owner_name = "items.loop.entry",
            .cleanup_calls_enabled = true,
        },
        {
            .kind = orison::lowering::ComputedDynamicArrayCleanupStateHandoffKind::resume,
            .operation_name = "items.computed_for.cleanup.resume",
            .source_owner_name = "items.loop.entry",
            .target_owner_name = "items",
            .cleanup_calls_enabled = true,
        },
    };
    auto operands = std::vector<orison::lowering::ComputedDynamicArrayCleanupCallOperands> {
        {
            .cleanup_operation_name = "items.computed_for.cleanup.resume",
            .data_pointer_name = "%items.data",
            .element_size_bytes = 4,
            .capacity_name = "%items.capacity",
            .descriptor_storage_name = "%items.addr",
            .cleanup_call_inserted = true,
            .descriptor_finalized = true,
        },
    };

    auto proof_model = orison::pipeline::build_computed_cleanup_proof_model("", handoffs, operands);
    assert(proof_model.inserted_cleanup_state.from_metadata);
    assert(proof_model.inserted_cleanup_state.verified_pairs.size() == 1);
    assert(proof_model.inserted_cleanup_state.transition_events.size() == 1);
    assert(proof_model.inserted_cleanup_state.verification_events.size() == 1);
    assert(
        proof_model.inserted_cleanup_state.verification_events.front().kind ==
        orison::pipeline::InsertedCleanupStateVerificationKind::paired
    );
    assert(proof_model.reports.inserted_cleanup_transition_report.size() == 1);
    assert(proof_model.reports.inserted_cleanup_state_verification_report.size() == 1);
    assert(
        proof_model.reports.inserted_cleanup_transition_report.front().find(
            "computed DynamicArray for inserted cleanup transition"
        ) != std::string::npos
    );
    assert(
        proof_model.reports.inserted_cleanup_state_verification_report.front().find("[handoff paired]") !=
        std::string::npos
    );
    assert(proof_model.verified_cleanup_calls.size() == 1);
    assert(proof_model.summary.cleanup_proof_model_count == 1);
    assert(proof_model.summary.verified_inserted_cleanup_pair_count == 1);
    assert(proof_model.summary.structured_inserted_cleanup_handoff_count == 2);
    assert(proof_model.summary.structured_inserted_cleanup_handoff_use_count == 2);
    assert(proof_model.summary.structured_cleanup_operand_count == 1);
    assert(proof_model.summary.structured_cleanup_operand_use_count == 1);
    assert(proof_model.summary.structured_inserted_cleanup_call_count == 1);
    assert(proof_model.summary.structured_consumed_cleanup_descriptor_count == 1);
    assert(proof_model.reports.cleanup_call_emission_gate_report.size() == 1);
    assert(proof_model.reports.cleanup_call_plan_report.size() == 1);
    assert(proof_model.reports.cleanup_call_render_report.size() == 1);
    assert(proof_model.reports.cleanup_call_insertion_gate_report.size() == 1);
    assert(proof_model.reports.inserted_cleanup_call_report.size() == 1);
    assert(proof_model.reports.consumed_cleanup_descriptor_report.size() == 1);
    assert(proof_model.cleanup_call_report_events.emission_gate_events.size() == 1);
    assert(proof_model.cleanup_call_report_events.plan_events.size() == 1);
    assert(proof_model.cleanup_call_report_events.render_events.size() == 1);
    assert(proof_model.cleanup_call_report_events.insertion_gate_events.size() == 1);
    assert(proof_model.cleanup_call_report_events.inserted_call_events.size() == 1);
    assert(proof_model.cleanup_call_report_events.consumed_descriptor_events.size() == 1);
    assert(
        proof_model.cleanup_call_report_events.plan_events.front().operands.capacity_name ==
        "%items.capacity"
    );
    assert(proof_model.cleanup_call_report_events.insertion_gate_events.front().decision.insertion_ready);
    assert(
        proof_model.cleanup_call_report_events.consumed_descriptor_events.front().descriptor_storage_name ==
        "%items.addr"
    );
    assert(
        proof_model.reports.cleanup_call_insertion_gate_report.front().find(
            "[cleanup call insertion ready]"
        ) != std::string::npos
    );

    auto const& call = proof_model.verified_cleanup_calls.front();
    auto const insertion_decision = orison::pipeline::computed_cleanup_call_insertion_decision(call);
    auto const inserted_call_decision = orison::pipeline::computed_inserted_cleanup_call_decision("", call);
    auto const consumed_descriptor_decision =
        orison::pipeline::computed_consumed_cleanup_descriptor_decision("", call);
    assert(orison::pipeline::computed_cleanup_call_operands_complete(call.operands));
    assert(call.operands.from_metadata);
    assert(call.insertion_decision.insertion_ready);
    assert(call.inserted_call_decision.inserted);
    assert(call.consumed_descriptor_decision.finalized);
    assert(insertion_decision.state_verified);
    assert(insertion_decision.operands_proven);
    assert(insertion_decision.cleanup_calls_authorized);
    assert(insertion_decision.insertion_ready);
    assert(inserted_call_decision.operands_proven);
    assert(inserted_call_decision.proven_by_metadata);
    assert(!inserted_call_decision.proven_by_ir);
    assert(inserted_call_decision.inserted);
    assert(consumed_descriptor_decision.operands_proven);
    assert(consumed_descriptor_decision.finalized_by_metadata);
    assert(!consumed_descriptor_decision.finalized_by_ir);
    assert(consumed_descriptor_decision.finalized);
    assert(consumed_descriptor_decision.descriptor_storage_name == "%items.addr");
    assert(orison::pipeline::computed_cleanup_call_inserted_by_metadata(call));
    assert(!orison::pipeline::computed_cleanup_call_inserted_by_ir("", call));
    assert(orison::pipeline::computed_consumed_cleanup_descriptor_by_metadata(call));
    assert(!orison::pipeline::computed_consumed_cleanup_descriptor_by_ir("", call).has_value());
    assert(
        orison::pipeline::rendered_computed_cleanup_call_text(call.operands) ==
        "  call void @__orison_dynamic_array_deallocate(ptr %items.data, i64 4, i64 %items.capacity)\n"
    );
}

void assert_consumed_descriptor_finalization_readiness_typed() {
    auto ready_plan = orison::lowering::plan_consumed_descriptor_finalization(
        "items",
        "%items.addr",
        "items.computed_for.cleanup.resume"
    );
    auto ready_state = orison::lowering::plan_consumed_descriptor_finalization_readiness(ready_plan);
    assert(ready_state.cleanup_owner_consumed);
    assert(ready_state.descriptor_finalization_planned);
    assert(ready_state.ready);
    assert(orison::lowering::consumed_descriptor_finalization_plan_ready(ready_plan));

    auto blocked_plan = orison::lowering::plan_consumed_descriptor_finalization(
        "items",
        "",
        "items.computed_for.cleanup.resume"
    );
    auto blocked_state = orison::lowering::plan_consumed_descriptor_finalization_readiness(blocked_plan);
    assert(blocked_state.cleanup_owner_consumed);
    assert(!blocked_state.descriptor_finalization_planned);
    assert(!blocked_state.ready);
    assert(!orison::lowering::consumed_descriptor_finalization_plan_ready(blocked_plan));
}

void assert_aggregate_projection_access_plan_state(
    orison::pipeline::CompilePipeline& pipeline,
    std::filesystem::path const& smoke_temp_root
) {
    auto source_path = smoke_temp_root / "aggregate_projection_access_plan_state.or";
    {
        auto output = std::ofstream(source_path);
        output <<
            "package smoke.aggregate_access_plan_state\n"
            "\n"
            "record Payload\n"
            "    public value: UInt32\n"
            "\n"
            "record Box\n"
            "    public payload: Payload\n"
            "    public count: UInt32\n"
            "\n"
            "function consume_payload(payload: Payload) -> UInt32\n"
            "    payload.value\n"
            "\n"
            "function main() -> UInt32\n"
            "    let box: Box = Box(Payload(13 as UInt32), 7 as UInt32)\n"
            "    let count: UInt32 = box.count\n"
            "    consume_payload(box.payload) + count\n";
    }

    auto result = pipeline.emit_llvm(
        source_path,
        orison::pipeline::CompilePipelineOptions {
            .collect_aggregate_projection_access_metadata = true,
        }
    );
    assert(!result.has_errors());
    auto const& state = result.aggregate_projection_access_plan_state;
    assert(state.access_plans_available);
    assert(state.plan_count == 3);
    assert(state.allowed_count == 3);
    assert(state.blocked_count == 0);
    assert(state.receiver_projection_count == 0);
    assert(state.function_symbol_names.size() == 3);
    assert(state.intents.size() == 3);
    assert(state.statuses.size() == 3);
    assert(state.binding_names.size() == 3);
    assert(state.source_type_names.size() == 3);
    assert(state.diagnostics.size() == 3);
    assert(state.function_symbol_names[0] == "consume_payload");
    assert(state.intents[0] == orison::lowering::AggregateProjectionAccessIntent::value_read);
    assert(state.statuses[0] == orison::lowering::AggregateProjectionAccessStatus::non_owned_projection);
    assert(state.binding_names[0] == "payload.value");
    assert(state.source_type_names[0] == "UInt32");
    assert(state.diagnostics[0].empty());
    assert(!state.receiver_projections[0]);
    assert(state.function_symbol_names[2] == "main");
    assert(state.intents[2] == orison::lowering::AggregateProjectionAccessIntent::explicit_transfer);
    assert(state.statuses[2] == orison::lowering::AggregateProjectionAccessStatus::allowed);
    assert(state.binding_names[2] == "box.payload");
    assert(state.source_type_names[2] == "Payload");
    assert(state.diagnostics[2].empty());
    assert(!state.receiver_projections[2]);
}

}  // namespace

auto main() -> int {
    assert_computed_cleanup_proof_model_reusable_without_reports();
    assert_consumed_descriptor_finalization_readiness_typed();

    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_pipeline_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    orison::pipeline::CompilePipeline pipeline;
    assert_aggregate_projection_access_plan_state(pipeline, smoke_temp_root);

    auto source_path = std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "minimal.or";

    auto analysis = pipeline.analyze(source_path);
    assert(!analysis.has_errors());
    assert(analysis.source_file.has_value());
    assert(analysis.parse_result.module.package_name == "demo.minimal");
    assert(analysis.parse_result.module.functions.size() == 1);
    assert(semantic_planned_drop_report(analysis).empty());
    assert(semantic_drop_implementation_discovery_report(analysis.semantic_drop_state).empty());
    assert(semantic_drop_resolution_report(analysis).empty());
    assert(semantic_drop_diagnostic_report(analysis).empty());
    assert(analysis.semantic_drop_lowering_authorizations.empty());
    assert(semantic_drop_lowering_authorization_report(analysis).empty());
    assert(semantic_drop_resolution_summary_report(analysis).empty());
    assert(analysis.dynamic_array_descriptor_cleanup_plan_state.plans.empty());

    auto ir = pipeline.emit_llvm(source_path);
    assert(!ir.has_errors());
    assert(ir.ir_text.find("define i32 @main()") != std::string::npos);
    assert(ir.ir_text.find("ret i32 0") != std::string::npos);
    assert(ir.semantic_drop_lowering_authorizations.empty());
    assert(ir.planned_drop_declaration_state.declarations.empty());
    assert(planned_drop_declaration_report(ir).empty());
    assert(emitted_drop_declaration_report(ir).empty());
    assert(ir.drop_readiness_snapshot.semantic_authorizations.empty());
    assert(ir.drop_readiness_snapshot.emitted_declarations.empty());
    assert(ir.drop_readiness_snapshot.cleanup_authorizations.empty());
    auto ir_drop_readiness_snapshot_report = drop_readiness_snapshot_report(ir);
    assert(ir_drop_readiness_snapshot_report.size() == 1);
    assert(
        ir_drop_readiness_snapshot_report.front().find("semantic authorizations 0") != std::string::npos
    );
    assert(ir.drop_readiness_summary.semantic_authorized == 0);
    assert(ir.drop_readiness_summary.semantic_blocked == 0);
    assert(ir.drop_readiness_summary.emitted_declarations == 0);
    assert(ir.drop_readiness_summary.cleanup_authorized == 0);
    assert(ir.drop_readiness_summary.cleanup_blocked == 0);
    auto ir_drop_readiness_summary_report = drop_readiness_summary_report(ir);
    assert(ir_drop_readiness_summary_report.size() == 1);
    assert(
        ir_drop_readiness_summary_report.front().find("semantic authorized 0 blocked 0") != std::string::npos
    );
    assert(drop_readiness_relation_report(ir).empty());
    assert(ir.drop_readiness_blocker_summary.blocked_cleanups == 0);
    assert(ir.drop_readiness_blocker_summary.semantic_lowering_blockers.empty());
    assert(ir.drop_readiness_blocker_summary.semantic_unresolved_blockers.empty());
    assert(ir.drop_readiness_blocker_summary.source_drop_lowering_blockers.empty());
    assert(ir.drop_readiness_blocker_summary.missing_declarations.empty());
    auto ir_drop_readiness_blocker_report = drop_readiness_blocker_report(ir);
    assert(ir_drop_readiness_blocker_report.size() == 1);
    assert(
        ir_drop_readiness_blocker_report.front() ==
        "drop readiness blockers cleanups 0 semantic blockers 0 semantic unresolved 0 "
        "source lowering blocked 0 missing declarations 0"
    );
    auto ir_drop_readiness_source_correlation_report = drop_readiness_source_correlation_report(ir);
    assert(ir_drop_readiness_source_correlation_report.size() == 1);
    assert(
        ir_drop_readiness_source_correlation_report.front() ==
        "drop readiness source correlations actions 0 semantic sites 0"
    );
    assert(ir.dynamic_array_descriptor_cleanup_plan_state.plans.empty());
    assert(!ir.dynamic_array_cleanup_availability.descriptor_origins_available);
    assert(!ir.dynamic_array_cleanup_availability.descriptor_cleanup_plans_available);
    assert(!ir.dynamic_array_cleanup_availability.cleanup_obligations_available);
    assert(!ir.dynamic_array_cleanup_availability.sequence_verification_available);
    assert(!ir.dynamic_array_cleanup_availability.sequence_verification_passed);
    assert(!ir.dynamic_array_cleanup_availability.cleanup_capability_proven);
    assert(!orison::pipeline::dynamic_array_cleanup_production_ready(
        ir.dynamic_array_cleanup_production_readiness
    ));
    auto ir_production_readiness_report =
        formatted_dynamic_array_cleanup_production_readiness_report(ir);
    assert(ir_production_readiness_report.size() == 1);
    assert_line_contains(
        ir_production_readiness_report,
        0,
        "production readiness blocked"
    );
    assert_line_contains(
        ir_production_readiness_report,
        0,
        "[descriptor origins missing]"
    );

    auto drop_readiness_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" / "drop_readiness.or";
    auto drop_readiness = pipeline.emit_llvm(drop_readiness_path);
    assert(!drop_readiness.has_errors());
    assert(drop_readiness.drop_readiness_snapshot.semantic_authorizations.size() == 1);
    assert(drop_readiness.drop_readiness_snapshot.emitted_declarations.empty());
    assert(drop_readiness.drop_readiness_snapshot.cleanup_authorizations.size() == 1);
    auto drop_readiness_snapshot_report_lines = drop_readiness_snapshot_report(drop_readiness);
    assert(drop_readiness_snapshot_report_lines.size() == 3);
    assert(
        drop_readiness_snapshot_report_lines[0].find("semantic authorizations 1") !=
        std::string::npos
    );
    assert(
        drop_readiness_snapshot_report_lines[1].find("__orison_drop.Payload") !=
        std::string::npos
    );
    assert(
        drop_readiness_snapshot_report_lines[2].find("__orison_thread_cleanup.launch.12.0 blocked") !=
        std::string::npos
    );
    assert(drop_readiness.drop_readiness_summary.semantic_authorized == 0);
    assert(drop_readiness.drop_readiness_summary.semantic_blocked == 1);
    assert(drop_readiness.drop_readiness_summary.emitted_declarations == 0);
    assert(drop_readiness.drop_readiness_summary.cleanup_authorized == 0);
    assert(drop_readiness.drop_readiness_summary.cleanup_blocked == 1);
    auto drop_readiness_summary_report_lines = drop_readiness_summary_report(drop_readiness);
    assert(drop_readiness_summary_report_lines.size() == 1);
    assert(
        drop_readiness_summary_report_lines.front().find("semantic authorized 0 blocked 1") !=
        std::string::npos
    );
    auto drop_readiness_relation_report_lines = drop_readiness_relation_report(drop_readiness);
    assert(drop_readiness_relation_report_lines.size() == 3);
    assert(
        drop_readiness_relation_report_lines[0].find(
            "__orison_thread_cleanup.launch.12.0 blocked"
        ) != std::string::npos
    );
    assert(
        drop_readiness_relation_report_lines[1].find("__orison_drop.Payload") !=
        std::string::npos
    );
    assert(
        drop_readiness_relation_report_lines[2].find("missing declaration __orison_drop.Payload") !=
        std::string::npos
    );
    assert(drop_readiness.drop_readiness_blocker_summary.blocked_cleanups == 1);
    assert(drop_readiness.drop_readiness_blocker_summary.semantic_lowering_blockers.size() == 1);
    assert(drop_readiness.drop_readiness_blocker_summary.semantic_unresolved_blockers.size() == 1);
    assert(drop_readiness.drop_readiness_blocker_summary.source_drop_lowering_blockers.empty());
    assert(drop_readiness.drop_readiness_blocker_summary.missing_declarations.size() == 1);
    auto drop_readiness_blocker_report_lines = drop_readiness_blocker_report(drop_readiness);
    assert(drop_readiness_blocker_report_lines.size() == 4);
    assert(
        drop_readiness_blocker_report_lines[0] ==
        "drop readiness blockers cleanups 1 semantic blockers 1 semantic unresolved 1 "
        "source lowering blocked 0 missing declarations 1"
    );
    assert(
        drop_readiness_blocker_report_lines[1].find("__orison_drop.Payload") != std::string::npos
    );
    auto drop_readiness_source_correlation_report_lines =
        drop_readiness_source_correlation_report(drop_readiness);
    assert(drop_readiness_source_correlation_report_lines.size() == 2);
    assert(
        drop_readiness_source_correlation_report_lines[0] ==
        "drop readiness source correlations actions 1 semantic sites 1"
    );
    assert(
        drop_readiness_source_correlation_report_lines[1].find(
            "__orison_thread_cleanup.launch.12.0 __orison_drop.Payload"
        ) != std::string::npos
    );

    auto dynamic_array_drop_report_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_drop_report.or";
    {
        auto dynamic_array_drop_report_source = std::ofstream(dynamic_array_drop_report_path);
        dynamic_array_drop_report_source
            << "package demo.pipeline.dynamicarray\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    1 as UInt32\n";
    }
    auto dynamic_array_drop_readiness = pipeline.emit_llvm(
        dynamic_array_drop_report_path,
        orison::pipeline::CompilePipelineOptions {
            .fixture_dynamic_array_construction_requests = {
                orison::lowering::FixtureDynamicArrayConstructionRequest {
                    .source_type_name = "DynamicArray<Payload>",
                    .initial_capacity = 2,
                },
            },
            .test_only_render_dynamic_array_element_drop_walks = true,
        }
    );
    assert(!dynamic_array_drop_readiness.has_errors());
    auto dynamic_array_drop_readiness_action_report =
        planned_drop_action_report(dynamic_array_drop_readiness);
    assert(dynamic_array_drop_readiness.planned_drop_action_state.actions.size() == 1);
    assert_line_contains(
        dynamic_array_drop_readiness_action_report,
        0,
        "dynamic_array0.element: Payload"
    );
    auto dynamic_array_drop_readiness_authorization_report =
        drop_cleanup_authorization_report(dynamic_array_drop_readiness);
    assert(dynamic_array_drop_readiness_authorization_report.size() == 4);
    assert_line_contains(
        dynamic_array_drop_readiness_authorization_report,
        0,
        "drop cleanup authorization __orison_dynamic_array_cleanup.0 blocked"
    );
    assert_line_contains(
        dynamic_array_drop_readiness_authorization_report,
        1,
        "semantic drop lowering blocked __orison_drop.Payload"
    );
    assert_line_contains(
        dynamic_array_drop_readiness_authorization_report,
        2,
        "semantic drop unresolved __orison_drop.Payload"
    );
    assert_line_contains(
        dynamic_array_drop_readiness_authorization_report,
        3,
        "missing drop declaration __orison_drop.Payload"
    );
    assert(dynamic_array_drop_readiness.drop_readiness_snapshot.cleanup_authorizations.size() == 1);
    assert(dynamic_array_drop_readiness.drop_readiness_blocker_summary.blocked_cleanups == 1);
    assert(dynamic_array_drop_readiness.drop_readiness_blocker_summary.semantic_lowering_blockers.size() == 1);
    assert(dynamic_array_drop_readiness.drop_readiness_blocker_summary.semantic_unresolved_blockers.size() == 1);
    assert(dynamic_array_drop_readiness.drop_readiness_blocker_summary.missing_declarations.size() == 1);
    auto dynamic_array_drop_readiness_relation_report =
        drop_readiness_relation_report(dynamic_array_drop_readiness);
    assert(dynamic_array_drop_readiness_relation_report.size() == 3);
    assert_line_contains(
        dynamic_array_drop_readiness_relation_report,
        0,
        "__orison_dynamic_array_cleanup.0 blocked"
    );
    assert_line_contains(
        dynamic_array_drop_readiness_relation_report,
        1,
        "semantic blocker __orison_drop.Payload"
    );
    assert_line_contains(
        dynamic_array_drop_readiness_relation_report,
        2,
        "missing declaration __orison_drop.Payload"
    );
    auto dynamic_array_drop_readiness_source_correlation_report =
        drop_readiness_source_correlation_report(dynamic_array_drop_readiness);
    assert(dynamic_array_drop_readiness_source_correlation_report.size() == 2);
    assert(
        dynamic_array_drop_readiness_source_correlation_report[0] ==
        "drop readiness source correlations actions 1 semantic sites 0"
    );
    assert_line_contains(
        dynamic_array_drop_readiness_source_correlation_report,
        1,
        "__orison_dynamic_array_cleanup.0 __orison_drop.Payload"
    );
    assert_line_contains(
        dynamic_array_drop_readiness_source_correlation_report,
        1,
        "semantic absent source lowering absent declaration missing"
    );
    assert(dynamic_array_drop_readiness.ir_text.find("call void @__orison_drop.Payload") == std::string::npos);

    auto dynamic_array_source_owner_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_source_owner.or";
    {
        auto dynamic_array_source_owner_source = std::ofstream(dynamic_array_source_owner_path);
        dynamic_array_source_owner_source
            << "package demo.pipeline.dynamicarraysource\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    1 as UInt32\n";
    }
    auto dynamic_array_source_owner = pipeline.analyze(dynamic_array_source_owner_path);
    assert(!dynamic_array_source_owner.has_errors());
    assert(dynamic_array_source_owner.semantic_result.dynamic_array_descriptor_origins.size() == 1);
    auto dynamic_array_source_owner_descriptor_origin_report =
        semantic_dynamic_array_descriptor_origin_report(dynamic_array_source_owner);
    assert(dynamic_array_source_owner_descriptor_origin_report.size() == 1);
    assert(
        dynamic_array_source_owner_descriptor_origin_report.front() ==
        "dynamic array descriptor origin DynamicArray<Payload> owner items element Payload at line 6 (metadata only)"
    );
    auto dynamic_array_source_owner_semantic_planned_drop_report =
        semantic_planned_drop_report(dynamic_array_source_owner);
    assert(dynamic_array_source_owner_semantic_planned_drop_report.size() == 2);
    assert_line_contains(
        dynamic_array_source_owner_semantic_planned_drop_report,
        0,
        "DynamicArray<Payload> owner items"
    );
    assert_line_contains(
        dynamic_array_source_owner_semantic_planned_drop_report,
        1,
        "Payload owner items.element"
    );

    auto dynamic_array_bound_descriptor = pipeline.emit_llvm(
        dynamic_array_source_owner_path,
        orison::pipeline::CompilePipelineOptions {
            .fixture_derive_dynamic_array_cleanup_from_semantics = true,
            .fixture_enable_dynamic_array_parameter_descriptors = true,
            .dynamic_array_parameter_lowering_enabled = false,
        }
    );
    assert(!dynamic_array_bound_descriptor.has_errors());
    assert(dynamic_array_bound_descriptor.dynamic_array_descriptor_cleanup_plan_state.plans.size() == 1);
    assert(dynamic_array_bound_descriptor.dynamic_array_descriptor_cleanup_plan_state.plans.front().owner_name == "items");
    assert(
        dynamic_array_bound_descriptor.dynamic_array_descriptor_cleanup_plan_state.plans.front().descriptor_storage_name ==
        "%items.addr"
    );
    assert(dynamic_array_bound_descriptor.dynamic_array_cleanup_obligation_state.obligations.size() == 1);
    assert(
        dynamic_array_bound_descriptor.dynamic_array_cleanup_obligation_state.obligations.front()
            .descriptor_cleanup.owner_name == "items"
    );
    assert(
        dynamic_array_bound_descriptor.dynamic_array_cleanup_obligation_state.obligations.front()
            .descriptor_cleanup.source_type_name == "DynamicArray<Payload>"
    );
    assert(dynamic_array_bound_descriptor.dynamic_array_cleanup_obligation_state.obligations.front().actions.size() == 1);
    assert(dynamic_array_bound_descriptor.dynamic_array_cleanup_sequence_plan_state.plans.size() == 1);
    assert(
        dynamic_array_bound_descriptor.dynamic_array_cleanup_sequence_plan_state.plans.front().phases.size() == 3
    );
    assert(
        dynamic_array_bound_descriptor.dynamic_array_cleanup_sequence_plan_state.plans.front().phases[1] ==
        "drop initialized elements"
    );
    assert(dynamic_array_bound_descriptor.dynamic_array_cleanup_sequence_verification_state.verifications.size() == 1);
    assert(
        dynamic_array_bound_descriptor.dynamic_array_cleanup_sequence_verification_state.verifications.front()
            .cleanup_symbol_name == "__orison_dynamic_array_cleanup.0"
    );
    assert(
        dynamic_array_bound_descriptor.dynamic_array_cleanup_sequence_verification_state.verifications.front()
            .errors.empty()
    );
    assert(dynamic_array_bound_descriptor.dynamic_array_cleanup_availability.descriptor_origins_available);
    assert(dynamic_array_bound_descriptor.dynamic_array_cleanup_availability.descriptor_cleanup_plans_available);
    assert(dynamic_array_bound_descriptor.dynamic_array_cleanup_availability.cleanup_obligations_available);
    assert(dynamic_array_bound_descriptor.dynamic_array_cleanup_availability.sequence_verification_available);
    assert(dynamic_array_bound_descriptor.dynamic_array_cleanup_availability.sequence_verification_passed);
    assert(
        dynamic_array_bound_descriptor.ir_text.find("define i32 @use_items({ ptr, i64, i64 } %items)") !=
        std::string::npos
    );
    assert(
        dynamic_array_bound_descriptor.ir_text.find("store { ptr, i64, i64 } %items, ptr %items.addr") !=
        std::string::npos
    );

    auto dynamic_array_owned_production_signature_descriptor = pipeline.emit_llvm(
        dynamic_array_source_owner_path,
        orison::pipeline::CompilePipelineOptions {
            .fixture_derive_dynamic_array_cleanup_from_semantics = true,
            .dynamic_array_parameter_lowering_enabled = true,
        }
    );
    assert(dynamic_array_owned_production_signature_descriptor.has_errors());
    assert(
        dynamic_array_owned_production_signature_descriptor.error_text.find(
            "lowering DynamicArray parameter 'items' with owned element type Payload requires ownership/drop proof "
            "before production lowering"
        ) != std::string::npos
    );

    auto dynamic_array_source_correlated_cleanup = pipeline.emit_llvm(
        dynamic_array_source_owner_path,
        orison::pipeline::CompilePipelineOptions {
            .fixture_derive_dynamic_array_cleanup_from_semantics = true,
            .fixture_enable_dynamic_array_parameter_descriptors = true,
            .test_only_render_dynamic_array_element_drop_walks = true,
            .dynamic_array_parameter_lowering_enabled = false,
        }
    );
    assert(!dynamic_array_source_correlated_cleanup.has_errors());
    auto dynamic_array_source_correlated_cleanup_source_correlation_report =
        drop_readiness_source_correlation_report(dynamic_array_source_correlated_cleanup);
    assert(dynamic_array_source_correlated_cleanup_source_correlation_report.size() == 2);
    assert_line_contains(
        dynamic_array_source_correlated_cleanup_source_correlation_report,
        0,
        "drop readiness source correlations actions 1 semantic sites"
    );
    assert_line_contains(
        dynamic_array_source_correlated_cleanup_source_correlation_report,
        1,
        "__orison_dynamic_array_cleanup.0 __orison_drop.Payload for Payload capture items.element field 0 action line 6"
    );
    assert_line_contains(
        dynamic_array_source_correlated_cleanup_source_correlation_report,
        1,
        "semantic owner items.element site line 6"
    );
    assert_line_contains(
        dynamic_array_source_correlated_cleanup_source_correlation_report,
        1,
        "declaration missing"
    );

    auto cleanup_metadata_options = orison::pipeline::CompilePipelineOptions {
        .source_drop_lowering_enabled = true,
        .dynamic_array_descriptor_cleanup_planning_enabled = true,
        .dynamic_array_parameter_descriptor_audit_bindings_enabled = true,
        .dynamic_array_production_cleanup_emission_enabled = true,
    };
    auto cleanup_metadata_facade = pipeline.collect_dynamic_array_cleanup_metadata(
        dynamic_array_source_owner_path,
        cleanup_metadata_options
    );
    auto cleanup_metadata_collector =
        orison::pipeline::DynamicArrayCleanupMetadataCollector(pipeline).collect(
            dynamic_array_source_owner_path,
            cleanup_metadata_options
        );
    assert(!cleanup_metadata_facade.has_errors());
    assert(!cleanup_metadata_collector.has_errors());
    assert(
        cleanup_metadata_collector.dynamic_array_cleanup_obligation_state.obligations.size() ==
        cleanup_metadata_facade.dynamic_array_cleanup_obligation_state.obligations.size()
    );
    assert(
        cleanup_metadata_collector.dynamic_array_cleanup_sequence_verification_state.verifications.size() ==
        cleanup_metadata_facade.dynamic_array_cleanup_sequence_verification_state.verifications.size()
    );
    assert(
        cleanup_metadata_collector.dynamic_array_cleanup_emission_capability_state.cleanup_pairs ==
        cleanup_metadata_facade.dynamic_array_cleanup_emission_capability_state.cleanup_pairs
    );
    assert(
        cleanup_metadata_collector.dynamic_array_cleanup_emission_capability_state.cleanup_owner_names ==
        cleanup_metadata_facade.dynamic_array_cleanup_emission_capability_state.cleanup_owner_names
    );
    assert(
        cleanup_metadata_collector.dynamic_array_cleanup_emission_capability_state.proven ==
        cleanup_metadata_facade.dynamic_array_cleanup_emission_capability_state.proven
    );
    assert(
        orison::pipeline::format_dynamic_array_cleanup_production_readiness(
            cleanup_metadata_collector.dynamic_array_cleanup_production_readiness
        ) ==
        orison::pipeline::format_dynamic_array_cleanup_production_readiness(
            cleanup_metadata_facade.dynamic_array_cleanup_production_readiness
        )
    );

    auto scalar_dynamic_array_path =
        smoke_temp_root / "orison_pipeline_scalar_dynamic_array_parameter_descriptor.or";
    {
        auto scalar_dynamic_array_source = std::ofstream(scalar_dynamic_array_path);
        scalar_dynamic_array_source
            << "package demo.pipeline.dynamicarrayscalar\n"
            << "\n"
            << "function use_words(words: DynamicArray<UInt32>) -> UInt32\n"
            << "    1 as UInt32\n";
    }
    auto scalar_dynamic_array_cleanup = pipeline.emit_llvm(
        scalar_dynamic_array_path,
        orison::pipeline::CompilePipelineOptions {
            .fixture_derive_dynamic_array_cleanup_from_semantics = true,
        }
    );
    assert(!scalar_dynamic_array_cleanup.has_errors());
    assert(scalar_dynamic_array_cleanup.dynamic_array_descriptor_cleanup_plan_state.plans.size() == 1);
    assert(
        scalar_dynamic_array_cleanup.dynamic_array_descriptor_cleanup_plan_state.plans.front().descriptor_storage_name ==
        "%words.addr"
    );
    assert(scalar_dynamic_array_cleanup.dynamic_array_cleanup_sequence_plan_state.plans.size() == 1);
    assert(scalar_dynamic_array_cleanup.dynamic_array_cleanup_sequence_plan_state.plans.front().phases.size() == 2);
    assert(
        scalar_dynamic_array_cleanup.dynamic_array_cleanup_sequence_plan_state.plans.front().phases.back() ==
        "deallocate descriptor storage"
    );
    assert(scalar_dynamic_array_cleanup.dynamic_array_cleanup_sequence_verification_state.verifications.size() == 1);
    assert(
        scalar_dynamic_array_cleanup.dynamic_array_cleanup_sequence_verification_state.verifications.front()
            .errors.empty()
    );
    assert(scalar_dynamic_array_cleanup.dynamic_array_cleanup_emission_capability_state.capability_metadata_available);
    assert(scalar_dynamic_array_cleanup.dynamic_array_cleanup_emission_capability_state.proven);
    assert(scalar_dynamic_array_cleanup.dynamic_array_cleanup_emission_capability_state.emission_enabled);
    assert(scalar_dynamic_array_cleanup.dynamic_array_cleanup_emission_capability_state.descriptor_storage_bound);
    assert(scalar_dynamic_array_cleanup.dynamic_array_cleanup_emission_capability_state.sequence_verified);
    assert(
        scalar_dynamic_array_cleanup.dynamic_array_cleanup_emission_capability_state
            .element_cleanup_authorized_or_not_required
    );
    assert(
        scalar_dynamic_array_cleanup.dynamic_array_cleanup_emission_capability_state
            .descriptor_deallocation_authorized
    );
    assert(scalar_dynamic_array_cleanup.dynamic_array_cleanup_emission_capability_state.cleanup_pairs.size() == 1);
    assert(
        scalar_dynamic_array_cleanup.dynamic_array_cleanup_emission_capability_state.cleanup_pairs.front() ==
        "words:__orison_dynamic_array_cleanup.0"
    );
    assert(scalar_dynamic_array_cleanup.dynamic_array_cleanup_emission_capability_state.cleanup_owner_names.size() == 1);
    assert(scalar_dynamic_array_cleanup.dynamic_array_cleanup_emission_capability_state.cleanup_owner_names.front() == "words");
    assert(
        scalar_dynamic_array_cleanup.dynamic_array_cleanup_emission_capability_state.function_symbol_names.size() == 1
    );
    assert(
        scalar_dynamic_array_cleanup.dynamic_array_cleanup_emission_capability_state.function_symbol_names.front() ==
        "use_words"
    );
    assert(
        scalar_dynamic_array_cleanup.ir_text.find("declare void @__orison_dynamic_array_deallocate(ptr, i64, i64)") !=
        std::string::npos
    );
    assert(
        scalar_dynamic_array_cleanup.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %words.dynamic_array_cleanup0.cleanup.data, i64 4, "
            "i64 %words.dynamic_array_cleanup0.cleanup.capacity)"
        ) != std::string::npos
    );

    auto scalar_dynamic_array_production_signature = pipeline.emit_llvm(
        scalar_dynamic_array_path,
        orison::pipeline::CompilePipelineOptions {
            .fixture_derive_dynamic_array_cleanup_from_semantics = true,
        }
    );
    assert(!scalar_dynamic_array_production_signature.has_errors());
    assert(
        scalar_dynamic_array_production_signature.ir_text.find(
            "define i32 @use_words({ ptr, i64, i64 } %words)"
        ) != std::string::npos
    );
    assert(
        scalar_dynamic_array_production_signature.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %words.dynamic_array_cleanup0.cleanup.data, i64 4, "
            "i64 %words.dynamic_array_cleanup0.cleanup.capacity)"
        ) != std::string::npos
    );

    auto scalar_dynamic_array_parameter_length_path =
        smoke_temp_root / "orison_pipeline_scalar_dynamic_array_parameter_length.or";
    {
        auto parameter_length_source = std::ofstream(scalar_dynamic_array_parameter_length_path);
        parameter_length_source
            << "package demo.pipeline.dynamicarrayparameterlength\n"
            << "\n"
            << "function use_words(words: DynamicArray<UInt32>) -> IntSize\n"
            << "    words.length()\n";
    }
    auto scalar_dynamic_array_parameter_length = pipeline.emit_llvm(
        scalar_dynamic_array_parameter_length_path,
        orison::pipeline::CompilePipelineOptions {
            .fixture_derive_dynamic_array_cleanup_from_semantics = true,
        }
    );
    assert(!scalar_dynamic_array_parameter_length.has_errors());
    assert(
        scalar_dynamic_array_parameter_length.ir_text.find(
            "define i64 @use_words({ ptr, i64, i64 } %words)"
        ) != std::string::npos
    );
    assert(
        scalar_dynamic_array_parameter_length.ir_text.find(
            "%words.dynamic_array_length0.value = extractvalue { ptr, i64, i64 } "
            "%words.dynamic_array_length0.descriptor, 1"
        ) != std::string::npos
    );
    assert(
        scalar_dynamic_array_parameter_length.ir_text.find(
            "ret i64 %words.dynamic_array_length0.value"
        ) != std::string::npos
    );

    auto scalar_dynamic_array_parameter_index_path =
        smoke_temp_root / "orison_pipeline_scalar_dynamic_array_parameter_index.or";
    {
        auto parameter_index_source = std::ofstream(scalar_dynamic_array_parameter_index_path);
        parameter_index_source
            << "package demo.pipeline.dynamicarrayparameterindex\n"
            << "\n"
            << "function use_words(words: DynamicArray<UInt32>) -> UInt32\n"
            << "    words[0]\n";
    }
    auto scalar_dynamic_array_parameter_index = pipeline.emit_llvm(
        scalar_dynamic_array_parameter_index_path,
        orison::pipeline::CompilePipelineOptions {
            .fixture_derive_dynamic_array_cleanup_from_semantics = true,
        }
    );
    assert(!scalar_dynamic_array_parameter_index.has_errors());
    auto scalar_dynamic_array_parameter_index_runtime_report =
        dynamic_array_runtime_request_report(scalar_dynamic_array_parameter_index);
    assert(scalar_dynamic_array_parameter_index.dynamic_array_runtime_request_state.operations.size() == 2);
    assert_line_contains(
        scalar_dynamic_array_parameter_index_runtime_report,
        0,
        "__orison_dynamic_array_bounds_failed"
    );
    assert(
        scalar_dynamic_array_parameter_index.ir_text.find(
            "define i32 @use_words({ ptr, i64, i64 } %words)"
        ) != std::string::npos
    );
    assert(
        scalar_dynamic_array_parameter_index.ir_text.find(
            "%words.dynamic_array_index0.in_bounds = icmp ult i64 0, %words.dynamic_array_index0.length"
        ) != std::string::npos
    );
    assert(
        scalar_dynamic_array_parameter_index.ir_text.find(
            "ret i32 %words.dynamic_array_index0.value"
        ) != std::string::npos
    );

    auto scalar_dynamic_array_parameter_for_path =
        smoke_temp_root / "orison_pipeline_scalar_dynamic_array_parameter_for.or";
    {
        auto parameter_for_source = std::ofstream(scalar_dynamic_array_parameter_for_path);
        parameter_for_source
            << "package demo.pipeline.dynamicarrayparameterfor\n"
            << "\n"
            << "function sum_words(words: DynamicArray<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for word in words\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto scalar_dynamic_array_parameter_for_without_gate = pipeline.emit_llvm(
        scalar_dynamic_array_parameter_for_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_local_lowering_enabled = false,
            .dynamic_array_parameter_lowering_enabled = false,
            .dynamic_array_production_signature_lowering_enabled = true,
        }
    );
    assert(scalar_dynamic_array_parameter_for_without_gate.has_errors());
    assert(
        scalar_dynamic_array_parameter_for_without_gate.error_text.find(
            "lowering DynamicArray for statements currently requires explicit production enablement"
        ) != std::string::npos
    );

    auto scalar_dynamic_array_parameter_for = pipeline.emit_llvm(
        scalar_dynamic_array_parameter_for_path,
        orison::pipeline::CompilePipelineOptions {
            .fixture_derive_dynamic_array_cleanup_from_semantics = true,
        }
    );
    assert(!scalar_dynamic_array_parameter_for.has_errors());
    auto scalar_dynamic_array_parameter_for_runtime_report =
        dynamic_array_runtime_request_report(scalar_dynamic_array_parameter_for);
    assert(scalar_dynamic_array_parameter_for.dynamic_array_runtime_request_state.operations.size() == 1);
    assert_line_contains(
        scalar_dynamic_array_parameter_for_runtime_report,
        0,
        "__orison_dynamic_array_deallocate"
    );
    assert(
        scalar_dynamic_array_parameter_for.ir_text.find(
            "define i32 @sum_words({ ptr, i64, i64 } %words)"
        ) != std::string::npos
    );
    assert(
        scalar_dynamic_array_parameter_for.ir_text.find(
            "%words.sequence_for0.value = load i32, ptr %words.sequence_for0.element.addr"
        ) != std::string::npos
    );
    assert(
        scalar_dynamic_array_parameter_for.ir_text.find(
            "ret i32 %tmp"
        ) != std::string::npos
    );
    auto scalar_dynamic_array_parameter_for_object =
        orison::lowering::LlvmObjectEmitter {}.emit(scalar_dynamic_array_parameter_for.ir_text);
    assert(!scalar_dynamic_array_parameter_for_object.has_errors());

    auto computed_dynamic_array_parameter_for_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_parameter_for_rejected.or";
    {
        auto computed_parameter_for_source = std::ofstream(computed_dynamic_array_parameter_for_path);
        computed_parameter_for_source
            << "package demo.pipeline.computeddynamicarrayparameterfor\n"
            << "\n"
            << "function sum_words(flag: Bool, left: DynamicArray<UInt32>, right: DynamicArray<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for word in flag ? left : right\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto computed_dynamic_array_parameter_for = pipeline.emit_llvm(
        computed_dynamic_array_parameter_for_path,
        orison::pipeline::CompilePipelineOptions {
            .fixture_derive_dynamic_array_cleanup_from_semantics = true,
        }
    );
    assert(computed_dynamic_array_parameter_for.has_errors());
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "lowering DynamicArray for statements currently requires a named descriptor iterable"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray iterable of type 'DynamicArray<UInt32>' requires a proven single descriptor owner"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "cleanup owner proof missing"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray ownership plan ternary branch owner mismatch source DynamicArray<UInt32> "
            "element UInt32 owners left right [ownership join blocked] [cleanup owner blocked] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray descriptor handoff plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [descriptor storage blocked] [cleanup owner blocked] "
            "[lowering disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray cleanup sequence plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [loop cleanup blocked] [function cleanup blocked] "
            "[cleanup sequence disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray descriptor render plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [descriptor load blocked] [data projection blocked] "
            "[length projection blocked] [capacity projection blocked] [render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray loop control render plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [entry branch blocked] [index phi blocked] [bounds check blocked] "
            "[conditional branch blocked] [render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray element address render plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [data pointer blocked] [index blocked] [element address blocked] "
            "[render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray element load render plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [element address blocked] [item value blocked] [render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray loop continue render plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [continue block blocked] [next index blocked] [backedge branch blocked] "
            "[render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray loop render sequence plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [descriptor render blocked] [loop control blocked] [body block blocked] "
            "[element address blocked] [element load blocked] [loop continue blocked] "
            "[render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray loop exit cleanup plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [exit block blocked] [cleanup blocked] [cleanup sequence disabled] "
            "[render disabled] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_parameter_for.error_text.find(
            "computed DynamicArray production emission gate plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [ownership blocked] [loop render blocked] [loop cleanup ownership blocked] "
            "[function cleanup resumption blocked] [exit cleanup blocked] "
            "[production sequence blocked] [production emission disabled] (metadata only)"
        ) != std::string::npos
    );

    auto computed_dynamic_array_same_owner_for_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_same_owner_for.or";
    {
        auto same_owner_for_source = std::ofstream(computed_dynamic_array_same_owner_for_path);
        same_owner_for_source
            << "package demo.pipeline.computeddynamicarraysameownerfor\n"
            << "\n"
            << "function sum_words(flag: Bool, items: DynamicArray<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for word in flag ? items : items\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto computed_dynamic_array_same_owner_for = pipeline.emit_llvm(computed_dynamic_array_same_owner_for_path);
    assert(!computed_dynamic_array_same_owner_for.has_errors());
    assert(
        computed_dynamic_array_same_owner_for.ir_text.find(
            "define i32 @sum_words(i1 %flag, { ptr, i64, i64 } %items)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_same_owner_for.ir_text.find(
            "items.computed_for.0.condition:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_same_owner_for.ir_text.find(
            "items.computed_for.0.body:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_same_owner_for.ir_text.find(
            "items.computed_for.0.exit:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_same_owner_for.ir_text.find(
            "items.computed_for.0.cleanup.resume.call"
        ) == std::string::npos
    );
    auto same_owner_first_deallocate = computed_dynamic_array_same_owner_for.ir_text.find(
        "call void @__orison_dynamic_array_deallocate"
    );
    assert(same_owner_first_deallocate != std::string::npos);
    assert(
        computed_dynamic_array_same_owner_for.ir_text.find(
            "call void @__orison_dynamic_array_deallocate",
            same_owner_first_deallocate + 1
        ) == std::string::npos
    );
    assert(
        computed_dynamic_array_same_owner_for.ir_text.find(
            "store { ptr, i64, i64 } zeroinitializer, ptr %items.addr"
        ) != std::string::npos
    );

    auto computed_dynamic_array_nested_same_owner_for_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_nested_same_owner_for.or";
    {
        auto nested_same_owner_for_source = std::ofstream(computed_dynamic_array_nested_same_owner_for_path);
        nested_same_owner_for_source
            << "package demo.pipeline.computeddynamicarraynestedsameownerfor\n"
            << "\n"
            << "function sum_words(flag: Bool, other_flag: Bool, items: DynamicArray<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for word in flag ? items : other_flag ? items : items\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto computed_dynamic_array_nested_same_owner_for =
        pipeline.emit_llvm(computed_dynamic_array_nested_same_owner_for_path);
    assert(!computed_dynamic_array_nested_same_owner_for.has_errors());
    assert(
        computed_dynamic_array_nested_same_owner_for.ir_text.find(
            "define i32 @sum_words(i1 %flag, i1 %other_flag, { ptr, i64, i64 } %items)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_nested_same_owner_for.ir_text.find(
            "items.computed_for.0.condition:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_nested_same_owner_for.ir_text.find(
            "items.computed_for.0.body:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_nested_same_owner_for.ir_text.find(
            "items.computed_for.0.exit:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_nested_same_owner_for.ir_text.find(
            "items.computed_for.0.cleanup.resume.call"
        ) == std::string::npos
    );
    auto nested_same_owner_first_deallocate =
        computed_dynamic_array_nested_same_owner_for.ir_text.find(
            "call void @__orison_dynamic_array_deallocate"
        );
    assert(nested_same_owner_first_deallocate != std::string::npos);
    assert(
        computed_dynamic_array_nested_same_owner_for.ir_text.find(
            "call void @__orison_dynamic_array_deallocate",
            nested_same_owner_first_deallocate + 1
        ) == std::string::npos
    );
    assert(
        computed_dynamic_array_nested_same_owner_for.ir_text.find(
            "store { ptr, i64, i64 } zeroinitializer, ptr %items.addr"
        ) != std::string::npos
    );

    auto computed_dynamic_array_nested_owner_mismatch_for_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_nested_owner_mismatch_for_rejected.or";
    {
        auto nested_owner_mismatch_for_source =
            std::ofstream(computed_dynamic_array_nested_owner_mismatch_for_path);
        nested_owner_mismatch_for_source
            << "package demo.pipeline.computeddynamicarraynestedownermismatchfor\n"
            << "\n"
            << "function sum_words(flag: Bool, other_flag: Bool, "
            << "items: DynamicArray<UInt32>, other: DynamicArray<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for word in flag ? items : other_flag ? items : other\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto computed_dynamic_array_nested_owner_mismatch_for =
        pipeline.emit_llvm(computed_dynamic_array_nested_owner_mismatch_for_path);
    assert(computed_dynamic_array_nested_owner_mismatch_for.has_errors());
    assert(
        computed_dynamic_array_nested_owner_mismatch_for.error_text.find(
            "computed DynamicArray ownership plan ternary branch owner mismatch source DynamicArray<UInt32> "
            "element UInt32 owners items items other [ownership join blocked] [cleanup owner blocked] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_nested_owner_mismatch_for.error_text.find(
            "computed DynamicArray descriptor handoff plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [descriptor storage blocked] [cleanup owner blocked] "
            "[lowering disabled] (metadata only)"
        ) != std::string::npos
    );

    auto computed_dynamic_array_local_nested_owner_mismatch_for_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_local_nested_owner_mismatch_for_rejected.or";
    {
        auto local_nested_owner_mismatch_for_source =
            std::ofstream(computed_dynamic_array_local_nested_owner_mismatch_for_path);
        local_nested_owner_mismatch_for_source
            << "package demo.pipeline.computeddynamicarraylocalnestedownermismatchfor\n"
            << "\n"
            << "function sum_words(flag: Bool, other_flag: Bool) -> UInt32\n"
            << "    let items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    let other: DynamicArray<UInt32> = DynamicArray()\n"
            << "    var total = 0 as UInt32\n"
            << "    for word in flag ? items : other_flag ? items : other\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto computed_dynamic_array_local_nested_owner_mismatch_for = pipeline.emit_llvm(
        computed_dynamic_array_local_nested_owner_mismatch_for_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
        }
    );
    assert(computed_dynamic_array_local_nested_owner_mismatch_for.has_errors());
    assert(
        computed_dynamic_array_local_nested_owner_mismatch_for.error_text.find(
            "computed DynamicArray ownership plan ternary branch owner mismatch source DynamicArray<UInt32> "
            "element UInt32 owners items items other [ownership join blocked] [cleanup owner blocked] (metadata only)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_nested_owner_mismatch_for.error_text.find(
            "computed DynamicArray descriptor handoff plan ownership join blocked source DynamicArray<UInt32> "
            "element UInt32 [descriptor storage blocked] [cleanup owner blocked] "
            "[lowering disabled] (metadata only)"
        ) != std::string::npos
    );

    auto computed_dynamic_array_local_nested_same_owner_for_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_local_nested_same_owner_for.or";
    {
        auto local_nested_same_owner_for_source =
            std::ofstream(computed_dynamic_array_local_nested_same_owner_for_path);
        local_nested_same_owner_for_source
            << "package demo.pipeline.computeddynamicarraylocalnestedsameownerfor\n"
            << "\n"
            << "function sum_words(flag: Bool, other_flag: Bool) -> UInt32\n"
            << "    let items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    var total = 0 as UInt32\n"
            << "    for word in flag ? items : other_flag ? items : items\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto computed_dynamic_array_local_nested_same_owner_for = pipeline.emit_llvm(
        computed_dynamic_array_local_nested_same_owner_for_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_nested_same_owner_for.has_errors());
    assert(
        computed_dynamic_array_local_nested_same_owner_for.ir_text.find(
            "define i32 @sum_words(i1 %flag, i1 %other_flag)"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_nested_same_owner_for.ir_text.find(
            "items.computed_for.0.condition:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_nested_same_owner_for.ir_text.find(
            "items.computed_for.0.body:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_nested_same_owner_for.ir_text.find(
            "items.computed_for.0.exit:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_nested_same_owner_for.ir_text.find(
            "items.computed_for.0.cleanup.resume.call"
        ) == std::string::npos
    );

    auto computed_dynamic_array_local_same_owner_for_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_local_same_owner_for_rejected.or";
    {
        auto local_same_owner_for_source = std::ofstream(computed_dynamic_array_local_same_owner_for_path);
        local_same_owner_for_source
            << "package demo.pipeline.computeddynamicarraylocalsameownerfor\n"
            << "\n"
            << "function sum_words(flag: Bool) -> UInt32\n"
            << "    let items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    var total = 0 as UInt32\n"
            << "    for word in flag ? items : items\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto computed_dynamic_array_local_same_owner_for = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_for_path,
        orison::pipeline::CompilePipelineOptions {
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
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
            .computed_dynamic_array_local_cleanup_call_insertion_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_for.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_descriptor_render_state.render_metadata_available
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_descriptor_render_state.render_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_descriptor_render_state.rendered_ir_snippet_count == 4
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_descriptor_render_state.all_descriptor_projections_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_descriptor_render_state.enclosing_function_names.front() == "sum_words"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_descriptor_render_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_descriptor_render_state.source_type_names.front() ==
        "DynamicArray<UInt32>"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_descriptor_render_state.element_source_type_names.front() == "UInt32"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_descriptor_render_state.descriptor_storage_names.front() == "%items.addr"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_descriptor_render_state.descriptor_value_names.front() ==
        "%items.computed_for.descriptor"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_descriptor_render_state.data_pointer_names.front() ==
        "%items.computed_for.data"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_descriptor_render_state.length_names.front() ==
        "%items.computed_for.length"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_descriptor_render_state.capacity_names.front() ==
        "%items.computed_for.capacity"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_control_render_state.render_metadata_available
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_control_render_state.render_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_control_render_state.rendered_ir_snippet_count == 5
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_control_render_state.all_control_flow_names_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_control_render_state.enclosing_function_names.front() == "sum_words"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_control_render_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_control_render_state.source_type_names.front() ==
        "DynamicArray<UInt32>"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_control_render_state.element_source_type_names.front() == "UInt32"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_control_render_state.condition_block_names.front() ==
        "items.computed_for.condition"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_control_render_state.body_block_names.front() ==
        "items.computed_for.body"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_control_render_state.continue_block_names.front() ==
        "items.computed_for.continue"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_control_render_state.exit_block_names.front() ==
        "items.computed_for.exit"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_control_render_state.index_names.front() ==
        "%items.computed_for.index"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_control_render_state.next_index_names.front() ==
        "%items.computed_for.next.index"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_control_render_state.bounds_check_names.front() ==
        "%items.computed_for.more"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_address_render_state.render_metadata_available
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_address_render_state.render_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_address_render_state.rendered_ir_snippet_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_address_render_state.all_element_address_inputs_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_address_render_state.enclosing_function_names.front() == "sum_words"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_address_render_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_address_render_state.source_type_names.front() ==
        "DynamicArray<UInt32>"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_address_render_state.element_source_type_names.front() == "UInt32"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_address_render_state.element_llvm_type_names.front() == "i32"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_address_render_state.data_pointer_names.front() ==
        "%items.computed_for.data"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_address_render_state.index_names.front() ==
        "%items.computed_for.index"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_address_render_state.element_address_names.front() ==
        "%items.computed_for.element.addr"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_load_render_state.render_metadata_available
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_load_render_state.render_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_load_render_state.rendered_ir_snippet_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_load_render_state.all_element_load_inputs_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_load_render_state.enclosing_function_names.front() == "sum_words"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_load_render_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_load_render_state.source_type_names.front() ==
        "DynamicArray<UInt32>"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_load_render_state.element_source_type_names.front() == "UInt32"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_load_render_state.element_llvm_type_names.front() == "i32"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_load_render_state.element_address_names.front() ==
        "%items.computed_for.element.addr"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_element_load_render_state.item_value_names.front() ==
        "%items.computed_for.item"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_continue_render_state.render_metadata_available
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_continue_render_state.render_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_continue_render_state.rendered_ir_snippet_count == 3
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_continue_render_state.all_loop_continue_inputs_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_continue_render_state.enclosing_function_names.front() == "sum_words"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_continue_render_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_continue_render_state.source_type_names.front() ==
        "DynamicArray<UInt32>"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_continue_render_state.element_source_type_names.front() == "UInt32"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_continue_render_state.continue_block_names.front() ==
        "items.computed_for.continue"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_continue_render_state.condition_block_names.front() ==
        "items.computed_for.condition"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_continue_render_state.index_names.front() ==
        "%items.computed_for.index"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_continue_render_state.next_index_names.front() ==
        "%items.computed_for.next.index"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_render_sequence_state.sequence_metadata_available
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_render_sequence_state.sequence_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_render_sequence_state.rendered_ir_snippet_count == 15
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_render_sequence_state.all_body_blocks_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_render_sequence_state.enclosing_function_names.front() == "sum_words"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_render_sequence_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_render_sequence_state.source_type_names.front() ==
        "DynamicArray<UInt32>"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_render_sequence_state.element_source_type_names.front() == "UInt32"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_render_sequence_state.body_block_names.front() ==
        "items.computed_for.body"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_exit_cleanup_state.cleanup_metadata_available
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_exit_cleanup_state.cleanup_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_exit_cleanup_state.rendered_ir_snippet_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_exit_cleanup_state.all_cleanup_resumptions_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_exit_cleanup_state.enclosing_function_names.front() == "sum_words"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_exit_cleanup_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_exit_cleanup_state.source_type_names.front() ==
        "DynamicArray<UInt32>"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_exit_cleanup_state.element_source_type_names.front() == "UInt32"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_exit_cleanup_state.exit_block_names.front() ==
        "items.computed_for.exit"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_exit_cleanup_state.loop_entry_cleanup_owner_names.front() ==
        "items.loop.entry"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_exit_cleanup_state.loop_exit_cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_loop_exit_cleanup_state.cleanup_resumption_operation_names.front() ==
        "items.computed_for.cleanup.resume"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_transition_state.transition_metadata_available
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_transition_state.transition_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_transition_state.all_transitions_paired
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_transition_state.enclosing_function_names.front() == "sum_words"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_transition_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_transition_state.source_type_names.front() ==
        "DynamicArray<UInt32>"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_transition_state.element_source_type_names.front() == "UInt32"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_transition_state.acquisition_source_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_transition_state.acquisition_target_owner_names.front() ==
        "items.loop.entry"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_transition_state.acquisition_operation_names.front() ==
        "items.computed_for.cleanup.acquire"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_transition_state.resumption_source_owner_names.front() ==
        "items.loop.entry"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_transition_state.resumption_target_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_transition_state.resumption_operation_names.front() ==
        "items.computed_for.cleanup.resume"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_inserted_cleanup_transition_state.transition_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_inserted_cleanup_transition_state.transitions_available
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_inserted_cleanup_state_verification_state.verification_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_inserted_cleanup_state_verification_state.all_paired
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.transition_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.verification_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.all_paired
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.all_cleanup_calls_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.verified_inserted_cleanup_pair_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.cleanup_proof_model_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_cleanup_operand_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.gate_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.all_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.plan_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.render_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.gate_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.all_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_inserted_cleanup_call_state.call_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_inserted_cleanup_call_state.all_inserted
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .consumed_descriptor_finalization_state.computed_descriptor_plan_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .consumed_descriptor_finalization_state.emitted_finalization_plan_count == 0
    );
    assert(computed_dynamic_array_local_same_owner_for.consumed_descriptor_finalization_state.ready_plan_count == 1);
    assert(computed_dynamic_array_local_same_owner_for.consumed_descriptor_finalization_state.blocked_plan_count == 0);
    assert(computed_dynamic_array_local_same_owner_for.consumed_descriptor_finalization_state.all_ready);
    assert(
        computed_dynamic_array_local_same_owner_for
            .consumed_descriptor_finalization_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .consumed_descriptor_finalization_state.descriptor_storage_names.front() == "%items.addr"
    );
    assert(
        computed_dynamic_array_local_same_owner_for.ir_text.find("items.dynamic_array_cleanup") ==
        std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.descriptor_model_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.ready_model_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
             .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.all_finalization_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_state.descriptor_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_state.all_finalized
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_emission_gate_state.gate_metadata_available
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_emission_gate_state.gate_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_emission_gate_state.rendered_ir_snippet_count == 17
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_emission_gate_state.all_ownership_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_emission_gate_state.all_loop_render_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_emission_gate_state.all_loop_cleanup_ownership_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_emission_gate_state.all_function_cleanup_resumption_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_emission_gate_state.all_exit_cleanup_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_emission_gate_state.all_production_sequences_planned
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_emission_gate_state.any_production_emission_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_emission_gate_state.cleanup_owner_names.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_emission_gate_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_sequence_state.sequence_metadata_available
    );
    assert(
        !computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_sequence_state.module_comments_emitted
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_sequence_state.sequence_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_sequence_state.rendered_ir_snippet_count == 17
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_sequence_state.module_comment_line_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_sequence_state.cleanup_owner_names.size() == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_sequence_state.cleanup_owner_names.front() == "items"
    );
    assert(computed_dynamic_array_local_same_owner_for.computed_dynamic_array_for_production_readiness.gate_ready);
    assert(computed_dynamic_array_local_same_owner_for.computed_dynamic_array_for_production_readiness.sequence_ready);
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_readiness.inserted_cleanup_transition_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_readiness.inserted_cleanup_state_verification_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_readiness.gate_sequence_counts_match
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_readiness.gate_sequence_snippets_match
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_readiness.cleanup_owners_match
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_readiness.sequence_transition_counts_match
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_readiness.transition_verification_counts_match
    );
    assert(
        computed_dynamic_array_local_same_owner_for
            .computed_dynamic_array_for_production_readiness.production_emission_enabled
    );
    assert(orison::pipeline::computed_dynamic_array_for_production_ready(
        computed_dynamic_array_local_same_owner_for.computed_dynamic_array_for_production_readiness
    ));
    auto dynamic_array_metadata_collector =
        orison::pipeline::DynamicArrayCleanupMetadataCollector {pipeline};
    auto computed_dynamic_array_local_same_owner_metadata_without_comments =
        dynamic_array_metadata_collector.collect(
            computed_dynamic_array_local_same_owner_for_path,
            orison::pipeline::CompilePipelineOptions {
                .collect_computed_dynamic_array_for_production_sequences = true,
                .dynamic_array_production_construction_lowering_enabled = true,
                .dynamic_array_production_for_lowering_enabled = true,
            }
        );
    assert(!computed_dynamic_array_local_same_owner_metadata_without_comments.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_metadata_without_comments
            .computed_dynamic_array_for_production_sequence_module_ir_artifact_state.comment_ir_lines.empty()
    );
    assert(
        computed_dynamic_array_local_same_owner_metadata_without_comments
            .computed_dynamic_array_for_production_sequence_state.sequence_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_metadata_without_comments
            .computed_dynamic_array_for_production_sequence_state.rendered_ir_snippet_count == 17
    );
    assert(
        !computed_dynamic_array_local_same_owner_metadata_without_comments
            .computed_dynamic_array_for_production_sequence_state.module_comments_emitted
    );
    assert(
        !computed_dynamic_array_local_same_owner_metadata_without_comments
            .computed_dynamic_array_for_production_readiness.gate_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_metadata_without_comments
            .computed_dynamic_array_for_production_readiness.sequence_ready
    );
    assert(
        !computed_dynamic_array_local_same_owner_metadata_without_comments
             .computed_dynamic_array_for_production_readiness.inserted_cleanup_transition_ready
    );
    assert(
        !computed_dynamic_array_local_same_owner_metadata_without_comments
             .computed_dynamic_array_for_production_readiness.inserted_cleanup_state_verification_ready
    );
    assert(
        !computed_dynamic_array_local_same_owner_metadata_without_comments
            .computed_dynamic_array_for_production_readiness.gate_sequence_counts_match
    );
    assert(!orison::pipeline::computed_dynamic_array_for_production_ready(
        computed_dynamic_array_local_same_owner_metadata_without_comments.computed_dynamic_array_for_production_readiness
    ));
    assert(
        computed_dynamic_array_local_same_owner_metadata_without_comments.ir_text.find(
            "; computed DynamicArray for production sequence"
        ) == std::string::npos
    );
    auto computed_dynamic_array_local_same_owner_metadata_with_comments =
        dynamic_array_metadata_collector.collect(
            computed_dynamic_array_local_same_owner_for_path,
            orison::pipeline::CompilePipelineOptions {
                .emit_computed_dynamic_array_for_production_sequence_comments = true,
                .dynamic_array_production_construction_lowering_enabled = true,
                .dynamic_array_production_for_lowering_enabled = true,
            }
        );
    assert(!computed_dynamic_array_local_same_owner_metadata_with_comments.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_metadata_with_comments
            .computed_dynamic_array_for_production_sequence_module_ir_artifact_state.comment_ir_lines.size() == 18
    );
    assert(
        computed_dynamic_array_local_same_owner_metadata_with_comments
            .computed_dynamic_array_for_production_sequence_state.sequence_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_metadata_with_comments
            .computed_dynamic_array_for_production_sequence_state.rendered_ir_snippet_count == 17
    );
    assert(
        computed_dynamic_array_local_same_owner_metadata_with_comments
            .computed_dynamic_array_for_production_sequence_state.module_comments_emitted
    );
    assert(
        computed_dynamic_array_local_same_owner_metadata_with_comments
            .computed_dynamic_array_for_production_sequence_state.module_comment_line_count == 18
    );
    assert(
        !computed_dynamic_array_local_same_owner_metadata_with_comments
            .computed_dynamic_array_for_production_readiness.gate_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_metadata_with_comments
            .computed_dynamic_array_for_production_readiness.sequence_ready
    );
    assert(
        !computed_dynamic_array_local_same_owner_metadata_with_comments
             .computed_dynamic_array_for_production_readiness.inserted_cleanup_transition_ready
    );
    assert(
        !computed_dynamic_array_local_same_owner_metadata_with_comments
             .computed_dynamic_array_for_production_readiness.inserted_cleanup_state_verification_ready
    );
    assert(!orison::pipeline::computed_dynamic_array_for_production_ready(
        computed_dynamic_array_local_same_owner_metadata_with_comments.computed_dynamic_array_for_production_readiness
    ));
    assert(
        computed_dynamic_array_local_same_owner_metadata_with_comments.ir_text.find(
            "; computed DynamicArray for production sequence function sum_words line 6 "
            "source DynamicArray<UInt32> element UInt32 owner items snippets 17 (metadata only)\n"
        ) != std::string::npos
    );
    auto computed_dynamic_array_local_same_owner_lowered_for = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_for_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
            .computed_dynamic_array_local_cleanup_call_insertion_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_lowered_for.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_transition_state.transition_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_transition_state.transitions_available
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_transition_state.from_metadata
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_transition_state.all_cleanup_calls_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_transition_state.acquire_source_owner_names.front() ==
        "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_transition_state.acquire_target_owner_names.front() ==
        "items.loop.entry"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_transition_state.resume_source_owner_names.front() ==
        "items.loop.entry"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_transition_state.resume_target_owner_names.front() ==
        "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_transition_state.acquire_operation_names.front() ==
        "items.computed_for.0.cleanup.acquire"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_transition_state.resume_operation_names.front() ==
        "items.computed_for.0.cleanup.resume"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_state_verification_state.verification_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_state_verification_state.paired_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_state_verification_state.blocked_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_state_verification_state.blocked_reasons.empty()
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_state_verification_state.from_metadata
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_state_verification_state.all_paired
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_state_verification_state.all_cleanup_calls_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_state_verification_state.acquire_operation_names.front() ==
        "items.computed_for.0.cleanup.acquire"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_state_verification_state.resume_operation_names.front() ==
        "items.computed_for.0.cleanup.resume"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_production_readiness.inserted_cleanup_transition_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_production_readiness.inserted_cleanup_state_verification_ready
    );
    assert(
        !computed_dynamic_array_local_same_owner_lowered_for
             .computed_dynamic_array_for_production_readiness.sequence_transition_counts_match
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_production_readiness.transition_verification_counts_match
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.transition_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.verification_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.paired_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.blocked_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.from_metadata
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.all_paired
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.all_cleanup_calls_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.acquire_operation_names.front() ==
        "items.computed_for.0.cleanup.acquire"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.resume_operation_names.front() ==
        "items.computed_for.0.cleanup.resume"
    );
    auto computed_dynamic_array_local_same_owner_lowered_for_with_sequence = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_for_path,
        orison::pipeline::CompilePipelineOptions {
            .collect_computed_dynamic_array_for_production_emission_gates = true,
            .collect_computed_dynamic_array_for_production_sequences = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
            .computed_dynamic_array_local_cleanup_call_insertion_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_lowered_for_with_sequence.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_lowered_for_with_sequence
            .computed_dynamic_array_for_production_sequence_state.sequence_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for_with_sequence
            .computed_dynamic_array_for_inserted_cleanup_transition_state.transition_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for_with_sequence
            .computed_dynamic_array_for_inserted_cleanup_handoff_state
            .production_cleanup_call_authorization_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for_with_sequence
            .computed_dynamic_array_for_inserted_cleanup_handoff_state
            .explicit_test_seam_cleanup_call_authorization_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for_with_sequence
            .computed_dynamic_array_for_inserted_cleanup_state_verification_state.verification_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for_with_sequence
            .computed_dynamic_array_for_production_readiness.gate_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for_with_sequence
            .computed_dynamic_array_for_production_readiness.sequence_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for_with_sequence
            .computed_dynamic_array_for_production_readiness.inserted_cleanup_transition_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for_with_sequence
            .computed_dynamic_array_for_production_readiness.inserted_cleanup_state_verification_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for_with_sequence
            .computed_dynamic_array_for_production_readiness.sequence_transition_counts_match
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for_with_sequence
            .computed_dynamic_array_for_production_readiness.transition_verification_counts_match
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for_with_sequence
            .computed_dynamic_array_for_production_readiness.production_emission_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for_with_sequence
            .computed_dynamic_array_for_cleanup_call_insertion_capability_state
            .cleanup_call_authorization_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for_with_sequence
            .computed_dynamic_array_for_cleanup_call_insertion_capability_state
            .cleanup_call_insertion_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for_with_sequence
            .computed_dynamic_array_for_cleanup_call_insertion_capability_state.enabled
    );
    assert(orison::pipeline::computed_dynamic_array_for_production_ready(
        computed_dynamic_array_local_same_owner_lowered_for_with_sequence
            .computed_dynamic_array_for_production_readiness
    ));
    auto computed_dynamic_array_local_same_owner_production_ready_for = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_for_path,
        orison::pipeline::CompilePipelineOptions {
            .collect_computed_dynamic_array_for_production_emission_gates = true,
            .collect_computed_dynamic_array_for_production_sequences = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_production_ready_for.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_production_ready_for
            .computed_dynamic_array_for_production_emission_gate_state.any_production_emission_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_production_ready_for
            .computed_dynamic_array_for_production_readiness.gate_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_production_ready_for
            .computed_dynamic_array_for_production_readiness.sequence_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_production_ready_for
            .computed_dynamic_array_for_production_readiness.inserted_cleanup_transition_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_production_ready_for
            .computed_dynamic_array_for_production_readiness.inserted_cleanup_state_verification_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_production_ready_for
            .computed_dynamic_array_for_production_readiness.gate_sequence_counts_match
    );
    assert(
        computed_dynamic_array_local_same_owner_production_ready_for
            .computed_dynamic_array_for_production_readiness.gate_sequence_snippets_match
    );
    assert(
        computed_dynamic_array_local_same_owner_production_ready_for
            .computed_dynamic_array_for_production_readiness.sequence_transition_counts_match
    );
    assert(
        computed_dynamic_array_local_same_owner_production_ready_for
            .computed_dynamic_array_for_production_readiness.transition_verification_counts_match
    );
    assert(
        computed_dynamic_array_local_same_owner_production_ready_for
            .computed_dynamic_array_for_production_readiness.cleanup_owners_match
    );
    assert(
        computed_dynamic_array_local_same_owner_production_ready_for
            .computed_dynamic_array_for_production_readiness.production_emission_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_production_ready_for
            .computed_dynamic_array_for_cleanup_call_insertion_capability_state
            .cleanup_call_authorization_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_production_ready_for
            .computed_dynamic_array_for_cleanup_call_insertion_capability_state
            .cleanup_call_insertion_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_production_ready_for
            .computed_dynamic_array_for_cleanup_call_insertion_capability_state.enabled
    );
    assert(orison::pipeline::computed_dynamic_array_for_production_ready(
        computed_dynamic_array_local_same_owner_production_ready_for
            .computed_dynamic_array_for_production_readiness
    ));
    assert(
        computed_dynamic_array_local_same_owner_production_ready_for
            .computed_dynamic_array_for_inserted_cleanup_call_state.call_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.verified_inserted_cleanup_pair_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.cleanup_proof_model_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_inserted_cleanup_handoff_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_inserted_cleanup_handoff_use_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_inserted_cleanup_handoff_fallback_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_cleanup_operand_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_cleanup_operand_use_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_cleanup_operand_fallback_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_inserted_cleanup_call_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_inserted_cleanup_call_fallback_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_inserted_cleanup_call_state.call_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_consumed_cleanup_descriptor_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_consumed_cleanup_descriptor_fallback_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.gate_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.ready_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.blocked_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.all_state_verified
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.all_cleanup_calls_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.all_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.acquire_operation_names.front() ==
        "items.computed_for.0.cleanup.acquire"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.resume_operation_names.front() ==
        "items.computed_for.0.cleanup.resume"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.plan_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.render_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.planned_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.renderable_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.all_state_verified
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.all_operands_proven
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.all_cleanup_calls_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.all_renderable
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.cleanup_operation_names.front() ==
        "items.computed_for.0.cleanup.resume.call"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.data_pointer_names.front() ==
        "%items.computed_for.0.data"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.element_size_bytes.front() == "4"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.capacity_names.front() ==
        "%items.computed_for.0.capacity"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %items.computed_for.0.data"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.gate_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.ready_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.blocked_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.all_state_verified
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.all_operands_proven
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.all_cleanup_calls_authorized
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.all_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.cleanup_operation_names.front() ==
        "items.computed_for.0.cleanup.resume.call"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .consumed_descriptor_finalization_state.computed_descriptor_plan_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .consumed_descriptor_finalization_state.emitted_finalization_plan_count == 0
    );
    assert(computed_dynamic_array_local_same_owner_lowered_for.consumed_descriptor_finalization_state.ready_plan_count == 1);
    assert(computed_dynamic_array_local_same_owner_lowered_for.consumed_descriptor_finalization_state.blocked_plan_count == 0);
    assert(computed_dynamic_array_local_same_owner_lowered_for.consumed_descriptor_finalization_state.all_ready);
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .consumed_descriptor_finalization_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .consumed_descriptor_finalization_state.descriptor_storage_names.front() == "%items.addr"
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for.ir_text.find("items.dynamic_array_cleanup") ==
        std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.descriptor_model_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_state.descriptor_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for.ir_text.find(
            "  ; cleanup state handoff acquire operation items.computed_for.0.cleanup.acquire "
            "from items to items.loop.entry [cleanup calls enabled]\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for.ir_text.find(
            "items.computed_for.0.condition:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for.ir_text.find(
            "items.computed_for.0.body:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for.ir_text.find(
            "  %items.computed_for.0.item = load i32, ptr %items.computed_for.0.element.addr\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_lowered_for.ir_text.find(
            "items.computed_for.0.exit:\n"
        ) != std::string::npos
    );
    auto computed_dynamic_array_local_owned_same_owner_for_without_drop_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_local_owned_same_owner_for_without_drop.or";
    {
        auto local_owned_same_owner_for_without_drop_source =
            std::ofstream(computed_dynamic_array_local_owned_same_owner_for_without_drop_path);
        local_owned_same_owner_for_without_drop_source
            << "package demo.pipeline.dynamicarraylocalownedsameownerforwithoutdrop\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    var total = 0 as Int64\n"
            << "    for item in true ? items : items\n"
            << "        total = total + item.value\n"
            << "    0 as UInt32\n";
    }
    auto computed_dynamic_array_local_owned_same_owner_for_without_drop =
        pipeline.emit_llvm(computed_dynamic_array_local_owned_same_owner_for_without_drop_path);
    assert(computed_dynamic_array_local_owned_same_owner_for_without_drop.has_errors());
    assert(
        computed_dynamic_array_local_owned_same_owner_for_without_drop.error_text.find(
            "lowering computed DynamicArray cleanup for owned element type Payload requires authorized element drop"
        ) != std::string::npos
    );

    auto computed_dynamic_array_local_owned_same_owner_for_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_local_owned_same_owner_for.or";
    {
        auto local_owned_same_owner_for_source =
            std::ofstream(computed_dynamic_array_local_owned_same_owner_for_path);
        local_owned_same_owner_for_source
            << "package demo.pipeline.dynamicarraylocalownedsameownerfor\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function main() -> Int64\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    var total = 0 as Int64\n"
            << "    items.push(Payload(5))\n"
            << "    items.push(Payload(7))\n"
            << "    for item in true ? items : items\n"
            << "        total = total + item.value\n"
            << "    total\n";
    }
    auto computed_dynamic_array_local_owned_same_owner_for =
        pipeline.emit_llvm(computed_dynamic_array_local_owned_same_owner_for_path);
    assert(!computed_dynamic_array_local_owned_same_owner_for.has_errors());
    assert(
        computed_dynamic_array_local_owned_same_owner_for.ir_text.find(
            "define void @__orison_drop.Payload(ptr %value)"
        ) != std::string::npos
    );
    auto computed_owned_drop = computed_dynamic_array_local_owned_same_owner_for.ir_text.find(
        "call void @__orison_drop.Payload(ptr %items.computed_dynamic_array_cleanup"
    );
    auto computed_owned_deallocate = computed_dynamic_array_local_owned_same_owner_for.ir_text.find(
        "call void @__orison_dynamic_array_deallocate(ptr %items.computed_for."
    );
    auto computed_owned_finalize = computed_dynamic_array_local_owned_same_owner_for.ir_text.find(
        "store { ptr, i64, i64 } zeroinitializer, ptr %items.addr"
    );
    assert(
        computed_dynamic_array_local_owned_same_owner_for.ir_text.find(
            ", i64 8, i64 %items.computed_for.",
            computed_owned_deallocate
        ) != std::string::npos
    );
    auto computed_owned_return = computed_dynamic_array_local_owned_same_owner_for.ir_text.find("ret i64 %tmp");
    assert(computed_owned_drop != std::string::npos);
    assert(computed_owned_deallocate != std::string::npos);
    assert(computed_owned_finalize != std::string::npos);
    assert(computed_owned_return != std::string::npos);
    assert(computed_owned_drop < computed_owned_deallocate);
    assert(computed_owned_deallocate < computed_owned_finalize);
    assert(computed_owned_finalize < computed_owned_return);
    assert(
        computed_dynamic_array_local_owned_same_owner_for.ir_text.find("items.dynamic_array_cleanup") ==
        std::string::npos
    );
    auto computed_dynamic_array_local_owned_same_owner_object =
        orison::lowering::LlvmObjectEmitter {}.emit(computed_dynamic_array_local_owned_same_owner_for.ir_text);
    assert(!computed_dynamic_array_local_owned_same_owner_object.has_errors());
    auto computed_dynamic_array_local_owned_same_owner_executable =
        smoke_temp_root / "dynamic_array_local_owned_same_owner_for";
    auto computed_dynamic_array_local_owned_same_owner_link = orison::link::HostLinker {}.link(
        computed_dynamic_array_local_owned_same_owner_object.object_bytes,
        computed_dynamic_array_local_owned_same_owner_executable
    );
    assert(!computed_dynamic_array_local_owned_same_owner_link.has_errors());
    auto computed_dynamic_array_local_owned_same_owner_status =
        std::system(computed_dynamic_array_local_owned_same_owner_executable.string().c_str());
    assert(WIFEXITED(computed_dynamic_array_local_owned_same_owner_status));
    assert(WEXITSTATUS(computed_dynamic_array_local_owned_same_owner_status) == 12);

    auto computed_dynamic_array_local_same_owner_operand_fallback_for = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_for_path,
        orison::pipeline::CompilePipelineOptions {
            .suppress_computed_dynamic_array_cleanup_operand_metadata = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
            .computed_dynamic_array_local_cleanup_call_insertion_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_operand_fallback_for.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_operand_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.verified_inserted_cleanup_pair_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_operand_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.cleanup_proof_model_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_operand_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_inserted_cleanup_handoff_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_operand_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_inserted_cleanup_handoff_use_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_operand_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_inserted_cleanup_handoff_fallback_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_operand_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_cleanup_operand_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_operand_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_cleanup_operand_use_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_operand_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_cleanup_operand_fallback_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_operand_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_inserted_cleanup_call_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_operand_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_inserted_cleanup_call_fallback_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_operand_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_consumed_cleanup_descriptor_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_operand_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_consumed_cleanup_descriptor_fallback_count == 1
    );
    auto computed_dynamic_array_local_same_owner_authorized_cleanup_for = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_for_path,
        orison::pipeline::CompilePipelineOptions {
            .fixture_authorize_computed_dynamic_array_cleanup_calls = true,
            .dynamic_array_local_lowering_enabled = false,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
            .computed_dynamic_array_local_cleanup_call_insertion_enabled = false,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_authorized_cleanup_for.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for.ir_text.find(
            "  ; cleanup state handoff acquire operation items.computed_for.0.cleanup.acquire "
            "from items to items.loop.entry [cleanup calls enabled]\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for.ir_text.find(
            "  ; cleanup state handoff resume operation items.computed_for.0.cleanup.resume "
            "from items.loop.entry to items [cleanup calls enabled]\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %items.computed_for.0.data"
        ) == std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_insertion_capability_state
            .cleanup_call_authorization_enabled
    );
    assert(
        !computed_dynamic_array_local_same_owner_authorized_cleanup_for
             .computed_dynamic_array_for_cleanup_call_insertion_capability_state
             .cleanup_call_insertion_enabled
    );
    assert(
        !computed_dynamic_array_local_same_owner_authorized_cleanup_for
             .computed_dynamic_array_for_cleanup_call_insertion_capability_state.enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.transition_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.verification_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.paired_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.blocked_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.from_metadata
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.all_paired
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.all_cleanup_calls_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state
            .production_cleanup_call_authorization_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state
            .explicit_test_seam_cleanup_call_authorization_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.verified_inserted_cleanup_pair_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.cleanup_proof_model_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_inserted_cleanup_handoff_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_inserted_cleanup_handoff_use_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_inserted_cleanup_handoff_fallback_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_cleanup_operand_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_cleanup_operand_use_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_cleanup_operand_fallback_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_inserted_cleanup_call_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_inserted_cleanup_call_fallback_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_consumed_cleanup_descriptor_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_consumed_cleanup_descriptor_fallback_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.gate_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.ready_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.blocked_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.all_state_verified
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.all_cleanup_calls_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.all_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.plan_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.all_cleanup_calls_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.all_renderable
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.gate_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.ready_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.blocked_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.all_state_verified
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.all_operands_proven
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.all_cleanup_calls_authorized
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.all_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .consumed_descriptor_finalization_state.computed_descriptor_plan_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.descriptor_model_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_authorized_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_state.descriptor_count == 0
    );
    auto computed_dynamic_array_local_same_owner_inserted_cleanup_for = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_for_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_inserted_cleanup_for.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for.ir_text.find(
            "  call void @__orison_dynamic_array_deallocate(ptr %items.computed_for.0.data, "
            "i64 4, i64 %items.computed_for.0.capacity)\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for.ir_text.find(
            "  store { ptr, i64, i64 } zeroinitializer, ptr %items.addr\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for.ir_text.find(
            "declare void @__orison_dynamic_array_deallocate(ptr, i64, i64)\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .dynamic_array_runtime_request_state.operations.size() == 2
    );
    auto computed_dynamic_array_local_same_owner_inserted_cleanup_for_runtime_report =
        dynamic_array_runtime_request_report(computed_dynamic_array_local_same_owner_inserted_cleanup_for);
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_call_insertion_capability_state.enabled
    );
    assert(
        !computed_dynamic_array_local_same_owner_inserted_cleanup_for
             .computed_dynamic_array_for_production_readiness.gate_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_production_readiness.production_emission_enabled
    );
    assert(!orison::pipeline::computed_dynamic_array_for_production_ready(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_production_readiness
    ));
    assert_line_contains(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for_runtime_report,
        0,
        "__orison_dynamic_array_allocate"
    );
    assert_line_contains(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for_runtime_report,
        1,
        "__orison_dynamic_array_deallocate"
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.plan_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.render_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.all_cleanup_calls_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_call_plan_render_state.all_renderable
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.gate_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.ready_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.blocked_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.all_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.verified_inserted_cleanup_pair_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.cleanup_proof_model_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_inserted_cleanup_handoff_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_inserted_cleanup_handoff_use_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_inserted_cleanup_handoff_fallback_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_cleanup_operand_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_cleanup_operand_use_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_cleanup_operand_fallback_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_inserted_cleanup_call_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_inserted_cleanup_call_fallback_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_consumed_cleanup_descriptor_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_consumed_cleanup_descriptor_fallback_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_call_state.call_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_call_state.structured_proof_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_call_state.ir_fallback_proof_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_call_state.all_inserted
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_call_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_call_state.data_pointer_names.front() ==
        "%items.computed_for.0.data"
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_inserted_cleanup_call_state.capacity_names.front() ==
        "%items.computed_for.0.capacity"
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .consumed_descriptor_finalization_state.computed_descriptor_plan_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .consumed_descriptor_finalization_state.emitted_finalization_plan_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .consumed_descriptor_finalization_state.ready_plan_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .consumed_descriptor_finalization_state.blocked_plan_count == 0
    );
    assert(computed_dynamic_array_local_same_owner_inserted_cleanup_for.consumed_descriptor_finalization_state.all_ready);
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for.ir_text.find("items.dynamic_array_cleanup") ==
        std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.descriptor_model_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.ready_model_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.blocked_model_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.all_finalization_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.enclosing_function_names.front() ==
        "sum_words"
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.descriptor_storage_names.front() ==
        "%items.addr"
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.cleanup_operation_names.front() ==
        "items.computed_for.cleanup.resume"
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.source_type_names.front() ==
        "DynamicArray<UInt32>"
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.element_source_type_names.front() ==
        "UInt32"
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_state.descriptor_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_state.structured_proof_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_state.ir_fallback_proof_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_state.all_finalized
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_state.descriptor_storage_names.front() ==
        "%items.addr"
    );
    auto computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_for_path,
        orison::pipeline::CompilePipelineOptions {
            .suppress_computed_dynamic_array_cleanup_operand_metadata = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_cleanup_operand_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_inserted_cleanup_handoff_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_inserted_cleanup_handoff_use_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_inserted_cleanup_handoff_fallback_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_cleanup_operand_use_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_cleanup_operand_fallback_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_inserted_cleanup_call_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_inserted_cleanup_call_fallback_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_consumed_cleanup_descriptor_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_consumed_cleanup_descriptor_fallback_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for
            .computed_dynamic_array_for_inserted_cleanup_call_state.call_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for
            .computed_dynamic_array_for_inserted_cleanup_call_state.structured_proof_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for
            .computed_dynamic_array_for_inserted_cleanup_call_state.ir_fallback_proof_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for
            .computed_dynamic_array_for_inserted_cleanup_call_state.all_inserted
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_state.descriptor_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_state.structured_proof_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_state.ir_fallback_proof_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_fallback_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_state.all_finalized
    );
    auto computed_dynamic_array_local_same_owner_handoff_fallback_for = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_for_path,
        orison::pipeline::CompilePipelineOptions {
            .suppress_computed_dynamic_array_cleanup_handoff_metadata = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
            .computed_dynamic_array_local_cleanup_call_insertion_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_handoff_fallback_for.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_handoff_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.verified_inserted_cleanup_pair_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_handoff_fallback_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.transition_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_handoff_fallback_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.verification_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_handoff_fallback_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.paired_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_handoff_fallback_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.blocked_count == 0
    );
    assert(
        !computed_dynamic_array_local_same_owner_handoff_fallback_for
             .computed_dynamic_array_for_inserted_cleanup_handoff_state.from_metadata
    );
    assert(
        computed_dynamic_array_local_same_owner_handoff_fallback_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.all_paired
    );
    assert(
        computed_dynamic_array_local_same_owner_handoff_fallback_for
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.all_cleanup_calls_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_handoff_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.cleanup_proof_model_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_handoff_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_inserted_cleanup_handoff_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_handoff_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_inserted_cleanup_handoff_use_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_handoff_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_inserted_cleanup_handoff_fallback_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_handoff_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_cleanup_operand_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_handoff_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_cleanup_operand_use_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_handoff_fallback_for
            .computed_dynamic_array_for_cleanup_proof_summary_state.ir_cleanup_operand_fallback_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .consumed_descriptor_finalization_state.computed_descriptor_plan_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.descriptor_model_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_state.descriptor_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.cleanup_owner_names.front() ==
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .consumed_descriptor_finalization_state.cleanup_owner_names.front()
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .computed_dynamic_array_for_consumed_cleanup_descriptor_state.descriptor_storage_names.front() ==
        computed_dynamic_array_local_same_owner_inserted_cleanup_for
            .consumed_descriptor_finalization_state.descriptor_storage_names.front()
    );
    auto computed_dynamic_array_local_same_owner_inserted_cleanup_object =
        orison::lowering::LlvmObjectEmitter {}.emit(
            computed_dynamic_array_local_same_owner_inserted_cleanup_for.ir_text
        );
    assert(!computed_dynamic_array_local_same_owner_inserted_cleanup_object.has_errors());
    auto computed_dynamic_array_local_same_owner_inserted_cleanup_run_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_local_same_owner_inserted_cleanup_run.or";
    {
        auto inserted_cleanup_run_source =
            std::ofstream(computed_dynamic_array_local_same_owner_inserted_cleanup_run_path);
        inserted_cleanup_run_source
            << "package demo.pipeline.computeddynamicarraylocalsameownerinsertedcleanuprun\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    var total = 0 as UInt32\n"
            << "    items.push(7 as UInt32)\n"
            << "    for word in true ? items : items\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto computed_dynamic_array_local_same_owner_inserted_cleanup_run = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_inserted_cleanup_run.has_errors());
    assert(computed_dynamic_array_local_same_owner_inserted_cleanup_run.dynamic_array_runtime_request_state.operations.size() == 3);
    auto computed_dynamic_array_local_same_owner_inserted_cleanup_run_runtime_report =
        dynamic_array_runtime_request_report(computed_dynamic_array_local_same_owner_inserted_cleanup_run);
    assert_line_contains(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run_runtime_report,
        1,
        "__orison_dynamic_array_grow"
    );
    assert_line_contains(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run_runtime_report,
        2,
        "__orison_dynamic_array_deallocate"
    );
    auto computed_inserted_deallocate =
        computed_dynamic_array_local_same_owner_inserted_cleanup_run.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %items.computed_for."
        );
    auto computed_inserted_clear =
        computed_dynamic_array_local_same_owner_inserted_cleanup_run.ir_text.find(
            "store { ptr, i64, i64 } zeroinitializer, ptr %items.addr"
        );
    auto computed_return =
        computed_dynamic_array_local_same_owner_inserted_cleanup_run.ir_text.find("ret i32 %tmp");
    assert(computed_inserted_deallocate != std::string::npos);
    assert(computed_inserted_clear != std::string::npos);
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup"
        ) == std::string::npos
    );
    assert(computed_return != std::string::npos);
    assert(computed_inserted_deallocate < computed_inserted_clear);
    assert(computed_inserted_clear < computed_return);
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run
            .consumed_descriptor_finalization_state.computed_descriptor_plan_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run
            .consumed_descriptor_finalization_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.descriptor_model_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.ready_model_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.all_finalization_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.enclosing_function_names.front() ==
        "main"
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.cleanup_operation_names.front() ==
        "items.computed_for.cleanup.resume"
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run
            .computed_dynamic_array_for_consumed_cleanup_descriptor_model_state.descriptor_storage_names.front() ==
        "%items.addr"
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run
            .computed_dynamic_array_for_consumed_cleanup_descriptor_state.descriptor_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run
            .computed_dynamic_array_for_consumed_cleanup_descriptor_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run
            .computed_dynamic_array_for_consumed_cleanup_descriptor_state.descriptor_storage_names.front() ==
        "%items.addr"
    );
    auto computed_dynamic_array_local_same_owner_inserted_cleanup_run_object =
        orison::lowering::LlvmObjectEmitter {}.emit(
            computed_dynamic_array_local_same_owner_inserted_cleanup_run.ir_text
        );
    assert(!computed_dynamic_array_local_same_owner_inserted_cleanup_run_object.has_errors());
    auto computed_dynamic_array_local_same_owner_inserted_cleanup_run_executable =
        smoke_temp_root / "computed_dynamic_array_local_same_owner_inserted_cleanup_run";
    auto computed_dynamic_array_local_same_owner_inserted_cleanup_run_link = orison::link::HostLinker {}.link(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run_object.object_bytes,
        computed_dynamic_array_local_same_owner_inserted_cleanup_run_executable
    );
    assert(!computed_dynamic_array_local_same_owner_inserted_cleanup_run_link.has_errors());
    auto computed_dynamic_array_local_same_owner_inserted_cleanup_run_status = std::system(
        computed_dynamic_array_local_same_owner_inserted_cleanup_run_executable.string().c_str()
    );
    assert(WIFEXITED(computed_dynamic_array_local_same_owner_inserted_cleanup_run_status));
    assert(WEXITSTATUS(computed_dynamic_array_local_same_owner_inserted_cleanup_run_status) == 7);
    auto computed_dynamic_array_local_same_owner_two_loops_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_local_same_owner_two_loops.or";
    {
        auto local_same_owner_two_loops_source = std::ofstream(computed_dynamic_array_local_same_owner_two_loops_path);
        local_same_owner_two_loops_source
            << "package demo.pipeline.computeddynamicarraylocalsameownertwoloops\n"
            << "\n"
            << "function sum_words(flag: Bool) -> UInt32\n"
            << "    let items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    var total = 0 as UInt32\n"
            << "    for word in flag ? items : items\n"
            << "        total = total + word\n"
            << "    for word in flag ? items : items\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto computed_dynamic_array_local_same_owner_two_loops = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_two_loops_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
            .dynamic_array_production_cleanup_emission_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_two_loops.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.transition_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.verification_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.paired_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.blocked_count == 0
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.from_metadata
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.all_paired
    );
    assert(
        !computed_dynamic_array_local_same_owner_two_loops
             .computed_dynamic_array_for_inserted_cleanup_handoff_state.all_cleanup_calls_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.cleanup_owner_names.size() == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.cleanup_owner_names.front() == "items"
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.acquire_operation_names.front() ==
        "items.computed_for.0.cleanup.acquire"
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.acquire_operation_names.back() ==
        "items.computed_for.1.cleanup.acquire"
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_inserted_cleanup_handoff_state.resume_operation_names.back() ==
        "items.computed_for.1.cleanup.resume"
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_proof_summary_state.verified_inserted_cleanup_pair_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_proof_summary_state.cleanup_proof_model_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_proof_summary_state.structured_cleanup_operand_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.gate_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.ready_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.blocked_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.all_state_verified
    );
    assert(
        !computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.all_cleanup_calls_enabled
    );
    assert(
        !computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_emission_gate_state.all_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_plan_render_state.plan_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_plan_render_state.render_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_plan_render_state.planned_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_plan_render_state.renderable_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_plan_render_state.all_state_verified
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_plan_render_state.all_operands_proven
    );
    assert(
        !computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_plan_render_state.all_cleanup_calls_enabled
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_plan_render_state.all_renderable
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.gate_count == 2
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.ready_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.blocked_count == 1
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.all_state_verified
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.all_operands_proven
    );
    assert(
        !computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.all_cleanup_calls_authorized
    );
    assert(
        !computed_dynamic_array_local_same_owner_two_loops
            .computed_dynamic_array_for_cleanup_call_insertion_gate_state.all_ready
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops.ir_text.find(
            "items.computed_for.0.condition:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops.ir_text.find(
            "items.computed_for.0.cleanup.acquire"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops.ir_text.find(
            "items.computed_for.1.condition:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops.ir_text.find(
            "items.computed_for.1.cleanup.acquire"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops.ir_text.find(
            "  ; cleanup state handoff acquire operation items.computed_for.0.cleanup.acquire "
            "from items to items.loop.entry [cleanup calls disabled] [cleanup blocked: later owner use]\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops.ir_text.find(
            "  ; cleanup state handoff acquire operation items.computed_for.1.cleanup.acquire "
            "from items to items.loop.entry [cleanup calls enabled]\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops.ir_text.find(
            "  call void @__orison_dynamic_array_deallocate(ptr %items.computed_for.0.data, "
            "i64 4, i64 %items.computed_for.0.capacity)\n"
        ) == std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops.ir_text.find(
            "  call void @__orison_dynamic_array_deallocate(ptr %items.computed_for.1.data, "
            "i64 4, i64 %items.computed_for.1.capacity)\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_two_loops.ir_text.find(
            "  %items.computed_for.1.index = phi i64 [ 0, %items.computed_for.0.exit ], "
            "[ %items.computed_for.1.next.index, %items.computed_for.1.continue ]\n"
        ) != std::string::npos
    );
    auto computed_dynamic_array_local_same_owner_if_then_later_loop_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_local_same_owner_if_then_later_loop.or";
    {
        auto local_same_owner_if_then_later_loop_source =
            std::ofstream(computed_dynamic_array_local_same_owner_if_then_later_loop_path);
        local_same_owner_if_then_later_loop_source
            << "package demo.pipeline.computeddynamicarraylocalsameownerifthenlaterloop\n"
            << "\n"
            << "function sum_words(flag: Bool) -> UInt32\n"
            << "    let items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    var total = 0 as UInt32\n"
            << "    if flag\n"
            << "        for word in flag ? items : items\n"
            << "            total = total + word\n"
            << "    for word in flag ? items : items\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto computed_dynamic_array_local_same_owner_if_then_later_loop = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_if_then_later_loop_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
            .dynamic_array_production_cleanup_emission_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_if_then_later_loop.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_if_then_later_loop.ir_text.find(
            "  ; cleanup state handoff acquire operation items.computed_for.1.cleanup.acquire "
            "from items to items.loop.entry [cleanup calls disabled] [cleanup blocked: later owner use]\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_if_then_later_loop.ir_text.find(
            "  ; cleanup state handoff acquire operation items.computed_for.2.cleanup.acquire "
            "from items to items.loop.entry [cleanup calls enabled]\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_if_then_later_loop.ir_text.find(
            "  call void @__orison_dynamic_array_deallocate(ptr %items.computed_for.1.data, "
            "i64 4, i64 %items.computed_for.1.capacity)\n"
        ) == std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_if_then_later_loop.ir_text.find(
            "  call void @__orison_dynamic_array_deallocate(ptr %items.computed_for.2.data, "
            "i64 4, i64 %items.computed_for.2.capacity)\n"
        ) != std::string::npos
    );
    auto computed_dynamic_array_local_same_owner_switch_case_later_loop_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_local_same_owner_switch_case_later_loop.or";
    {
        auto local_same_owner_switch_case_later_loop_source =
            std::ofstream(computed_dynamic_array_local_same_owner_switch_case_later_loop_path);
        local_same_owner_switch_case_later_loop_source
            << "package demo.pipeline.computeddynamicarraylocalsameownerswitchcaselaterloop\n"
            << "\n"
            << "function sum_words(flag: Bool) -> UInt32\n"
            << "    let items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    var total = 0 as UInt32\n"
            << "    switch flag\n"
            << "        true =>\n"
            << "            for word in flag ? items : items\n"
            << "                total = total + word\n"
            << "        default => total = total + 1 as UInt32\n"
            << "    for word in flag ? items : items\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto computed_dynamic_array_local_same_owner_switch_case_later_loop = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_switch_case_later_loop_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
            .dynamic_array_production_cleanup_emission_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_switch_case_later_loop.has_errors());
    auto switch_disabled_handoff_position =
        computed_dynamic_array_local_same_owner_switch_case_later_loop.ir_text.find(
            "[cleanup calls disabled] [cleanup blocked: later owner use]"
        );
    auto switch_enabled_handoff_position =
        computed_dynamic_array_local_same_owner_switch_case_later_loop.ir_text.find("[cleanup calls enabled]");
    assert(switch_disabled_handoff_position != std::string::npos);
    assert(switch_enabled_handoff_position != std::string::npos);
    assert(switch_disabled_handoff_position < switch_enabled_handoff_position);

    auto computed_dynamic_array_local_same_owner_while_body_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_local_same_owner_while_body.or";
    {
        auto local_same_owner_while_body_source =
            std::ofstream(computed_dynamic_array_local_same_owner_while_body_path);
        local_same_owner_while_body_source
            << "package demo.pipeline.computeddynamicarraylocalsameownerwhilebody\n"
            << "\n"
            << "function sum_words(flag: Bool) -> UInt32\n"
            << "    let items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    var total = 0 as UInt32\n"
            << "    while flag\n"
            << "        for word in flag ? items : items\n"
            << "            total = total + word\n"
            << "        break\n"
            << "    total\n";
    }
    auto computed_dynamic_array_local_same_owner_while_body = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_while_body_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
            .dynamic_array_production_cleanup_emission_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_while_body.has_errors());
    assert(computed_dynamic_array_local_same_owner_while_body.ir_text.find("[cleanup calls disabled]") != std::string::npos);
    assert(
        computed_dynamic_array_local_same_owner_while_body.ir_text.find(
            "[cleanup calls disabled] [cleanup blocked: active loop body]"
        ) != std::string::npos
    );
    assert(computed_dynamic_array_local_same_owner_while_body.ir_text.find("[cleanup calls enabled]") == std::string::npos);
    assert(
        computed_dynamic_array_local_same_owner_while_body.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %items.computed_for."
        ) == std::string::npos
    );
    auto computed_dynamic_array_local_same_owner_after_while_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_local_same_owner_after_while.or";
    {
        auto local_same_owner_after_while_source =
            std::ofstream(computed_dynamic_array_local_same_owner_after_while_path);
        local_same_owner_after_while_source
            << "package demo.pipeline.computeddynamicarraylocalsameownerafterwhile\n"
            << "\n"
            << "function sum_words(flag: Bool) -> UInt32\n"
            << "    let items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    var total = 0 as UInt32\n"
            << "    while flag\n"
            << "        total = total + 1 as UInt32\n"
            << "        break\n"
            << "    for word in flag ? items : items\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto computed_dynamic_array_local_same_owner_after_while = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_after_while_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
            .dynamic_array_production_cleanup_emission_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_after_while.has_errors());
    assert(computed_dynamic_array_local_same_owner_after_while.ir_text.find("[cleanup calls disabled]") == std::string::npos);
    assert(computed_dynamic_array_local_same_owner_after_while.ir_text.find("[cleanup calls enabled]") != std::string::npos);
    assert(
        computed_dynamic_array_local_same_owner_after_while.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %items.computed_for."
        ) != std::string::npos
    );
    auto computed_dynamic_array_local_same_owner_after_if_path =
        smoke_temp_root / "orison_pipeline_computed_dynamic_array_local_same_owner_after_if.or";
    {
        auto local_same_owner_after_if_source = std::ofstream(computed_dynamic_array_local_same_owner_after_if_path);
        local_same_owner_after_if_source
            << "package demo.pipeline.computeddynamicarraylocalsameownerafterif\n"
            << "\n"
            << "function sum_words(flag: Bool) -> UInt32\n"
            << "    let items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    var total = 0 as UInt32\n"
            << "    if flag\n"
            << "        total = 1 as UInt32\n"
            << "    else\n"
            << "        total = 2 as UInt32\n"
            << "    for word in flag ? items : items\n"
            << "        total = total + word\n"
            << "    total\n";
    }
    auto computed_dynamic_array_local_same_owner_after_if = pipeline.emit_llvm(
        computed_dynamic_array_local_same_owner_after_if_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_for_lowering_enabled = true,
        }
    );
    assert(!computed_dynamic_array_local_same_owner_after_if.has_errors());
    assert(
        computed_dynamic_array_local_same_owner_after_if.ir_text.find(
            "  %items.computed_for.1.index = phi i64 [ 0, %if.merge."
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_after_if.ir_text.find(
            "items.computed_for.1.body:\n"
        ) != std::string::npos
    );
    assert(
        computed_dynamic_array_local_same_owner_after_if.ir_text.find(
            "items.computed_for.1.exit:\n"
        ) != std::string::npos
    );

    auto view_parameter_length_path = smoke_temp_root / "orison_pipeline_view_parameter_length.or";
    {
        auto view_length_source = std::ofstream(view_parameter_length_path);
        view_length_source
            << "package demo.pipeline.viewlength\n"
            << "\n"
            << "function count(values: View<UInt32>) -> IntSize\n"
            << "    values.length()\n";
    }
    auto view_parameter_length = pipeline.emit_llvm(view_parameter_length_path);
    assert(!view_parameter_length.has_errors());
    assert(view_parameter_length.dynamic_array_runtime_request_state.operations.empty());
    assert(
        view_parameter_length.ir_text.find("define i64 @count({ ptr, i64 } %values)") !=
        std::string::npos
    );
    assert(
        view_parameter_length.ir_text.find("%view_length0.value = extractvalue { ptr, i64 } %values, 1") !=
        std::string::npos
    );

    auto view_parameter_index_path = smoke_temp_root / "orison_pipeline_view_parameter_index.or";
    {
        auto view_index_source = std::ofstream(view_parameter_index_path);
        view_index_source
            << "package demo.pipeline.viewindex\n"
            << "\n"
            << "function first(values: View<UInt32>) -> UInt32\n"
            << "    values[0]\n";
    }
    auto view_parameter_index = pipeline.emit_llvm(view_parameter_index_path);
    assert(!view_parameter_index.has_errors());
    assert(view_parameter_index.dynamic_array_runtime_request_state.operations.size() == 1);
    assert_line_contains(
        dynamic_array_runtime_request_report(view_parameter_index),
        0,
        "__orison_dynamic_array_bounds_failed"
    );
    assert(
        view_parameter_index.ir_text.find("define i32 @first({ ptr, i64 } %values)") !=
        std::string::npos
    );
    assert(
        view_parameter_index.ir_text.find("%view_index1.in_bounds = icmp ult i64 0, %view_index1.length") !=
        std::string::npos
    );
    auto view_parameter_index_object =
        orison::lowering::LlvmObjectEmitter {}.emit(view_parameter_index.ir_text);
    assert(!view_parameter_index_object.has_errors());

    auto view_parameter_for_path = smoke_temp_root / "orison_pipeline_view_parameter_for.or";
    {
        auto view_for_source = std::ofstream(view_parameter_for_path);
        view_for_source
            << "package demo.pipeline.viewfor\n"
            << "\n"
            << "function sum(values: View<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for item in values\n"
            << "        total = total + item\n"
            << "    total\n";
    }
    auto view_parameter_for = pipeline.emit_llvm(view_parameter_for_path);
    assert(!view_parameter_for.has_errors());
    assert(view_parameter_for.dynamic_array_runtime_request_state.operations.empty());
    assert(
        view_parameter_for.ir_text.find("define i32 @sum({ ptr, i64 } %values)") != std::string::npos
    );
    assert(
        view_parameter_for.ir_text.find(
            "%values.sequence_for0.value = load i32, ptr %values.sequence_for0.element.addr"
        ) != std::string::npos
    );
    auto view_parameter_for_object =
        orison::lowering::LlvmObjectEmitter {}.emit(view_parameter_for.ir_text);
    assert(!view_parameter_for_object.has_errors());

    auto shared_view_parameter_length_path =
        smoke_temp_root / "orison_pipeline_shared_view_parameter_length.or";
    {
        auto shared_view_length_source = std::ofstream(shared_view_parameter_length_path);
        shared_view_length_source
            << "package demo.pipeline.sharedviewlength\n"
            << "\n"
            << "function count(values: shared.View<UInt32>) -> IntSize\n"
            << "    values.length()\n";
    }
    auto shared_view_parameter_length = pipeline.emit_llvm(shared_view_parameter_length_path);
    assert(!shared_view_parameter_length.has_errors());
    assert(shared_view_parameter_length.dynamic_array_runtime_request_state.operations.empty());
    assert(
        shared_view_parameter_length.ir_text.find("define i64 @count({ ptr, i64 } %values)") !=
        std::string::npos
    );
    assert(
        shared_view_parameter_length.ir_text.find(
            "%view_length0.value = extractvalue { ptr, i64 } %values, 1"
        ) != std::string::npos
    );

    auto exclusive_view_parameter_index_path =
        smoke_temp_root / "orison_pipeline_exclusive_view_parameter_index.or";
    {
        auto exclusive_view_index_source = std::ofstream(exclusive_view_parameter_index_path);
        exclusive_view_index_source
            << "package demo.pipeline.exclusiveviewindex\n"
            << "\n"
            << "function first(values: exclusive.View<UInt32>) -> UInt32\n"
            << "    values[0]\n";
    }
    auto exclusive_view_parameter_index = pipeline.emit_llvm(exclusive_view_parameter_index_path);
    assert(!exclusive_view_parameter_index.has_errors());
    assert(exclusive_view_parameter_index.dynamic_array_runtime_request_state.operations.size() == 1);
    assert_line_contains(
        dynamic_array_runtime_request_report(exclusive_view_parameter_index),
        0,
        "__orison_dynamic_array_bounds_failed"
    );
    assert(
        exclusive_view_parameter_index.ir_text.find("define i32 @first({ ptr, i64 } %values)") !=
        std::string::npos
    );
    assert(
        exclusive_view_parameter_index.ir_text.find(
            "%view_index1.value = load i32, ptr %view_index1.element.addr"
        ) != std::string::npos
    );

    auto exclusive_view_parameter_assignment_path =
        smoke_temp_root / "orison_pipeline_exclusive_view_parameter_assignment.or";
    {
        auto exclusive_view_assignment_source = std::ofstream(exclusive_view_parameter_assignment_path);
        exclusive_view_assignment_source
            << "package demo.pipeline.exclusiveviewassign\n"
            << "\n"
            << "function write_first(values: exclusive.View<UInt32>) -> Unit\n"
            << "    values[0] = 7 as UInt32\n";
    }
    auto exclusive_view_parameter_assignment = pipeline.emit_llvm(exclusive_view_parameter_assignment_path);
    assert(!exclusive_view_parameter_assignment.has_errors());
    assert(exclusive_view_parameter_assignment.dynamic_array_runtime_request_state.operations.size() == 1);
    assert_line_contains(
        dynamic_array_runtime_request_report(exclusive_view_parameter_assignment),
        0,
        "__orison_dynamic_array_bounds_failed"
    );
    assert(
        exclusive_view_parameter_assignment.ir_text.find(
            "define void @write_first({ ptr, i64 } %values)"
        ) != std::string::npos
    );
    assert(
        exclusive_view_parameter_assignment.ir_text.find(
            "%values.view_assign0.in_bounds = icmp ult i64 0, %values.view_assign0.length"
        ) != std::string::npos
    );
    assert(
        exclusive_view_parameter_assignment.ir_text.find(
            "store i32 7, ptr %values.view_assign0.element.addr"
        ) != std::string::npos
    );
    auto exclusive_view_parameter_assignment_object =
        orison::lowering::LlvmObjectEmitter {}.emit(exclusive_view_parameter_assignment.ir_text);
    assert(!exclusive_view_parameter_assignment_object.has_errors());

    auto shared_view_parameter_for_path =
        smoke_temp_root / "orison_pipeline_shared_view_parameter_for.or";
    {
        auto shared_view_for_source = std::ofstream(shared_view_parameter_for_path);
        shared_view_for_source
            << "package demo.pipeline.sharedviewfor\n"
            << "\n"
            << "function sum(values: shared.View<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for item in values\n"
            << "        total = total + item\n"
            << "    total\n";
    }
    auto shared_view_parameter_for = pipeline.emit_llvm(shared_view_parameter_for_path);
    assert(!shared_view_parameter_for.has_errors());
    assert(shared_view_parameter_for.dynamic_array_runtime_request_state.operations.empty());
    assert(
        shared_view_parameter_for.ir_text.find(
            "%values.sequence_for0.value = load i32, ptr %values.sequence_for0.element.addr"
        ) != std::string::npos
    );

    auto computed_shared_view_for_path =
        smoke_temp_root / "orison_pipeline_computed_shared_view_for.or";
    {
        auto computed_shared_view_for_source = std::ofstream(computed_shared_view_for_path);
        computed_shared_view_for_source
            << "package demo.pipeline.computedsharedviewfor\n"
            << "\n"
            << "function forward(values: shared.View<UInt32>) -> shared.View<UInt32>\n"
            << "    values\n"
            << "\n"
            << "function sum(values: shared.View<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for item in forward(values)\n"
            << "        total = total + item\n"
            << "    total\n";
    }
    auto computed_shared_view_for = pipeline.emit_llvm(computed_shared_view_for_path);
    assert(!computed_shared_view_for.has_errors());
    assert(computed_shared_view_for.dynamic_array_runtime_request_state.operations.empty());
    assert(
        computed_shared_view_for.ir_text.find(
            " = call { ptr, i64 } @forward({ ptr, i64 } %values)"
        ) != std::string::npos
    );
    assert(
        computed_shared_view_for.ir_text.find("%computed.sequence_for") != std::string::npos
    );
    auto computed_shared_view_for_object =
        orison::lowering::LlvmObjectEmitter {}.emit(computed_shared_view_for.ir_text);
    assert(!computed_shared_view_for_object.has_errors());

    auto method_returned_shared_view_for_path =
        smoke_temp_root / "orison_pipeline_method_returned_shared_view_for.or";
    {
        auto method_returned_shared_view_for_source =
            std::ofstream(method_returned_shared_view_for_path);
        method_returned_shared_view_for_source
            << "package demo.pipeline.methodreturnedsharedviewfor\n"
            << "\n"
            << "extend UInt32\n"
            << "    public function forward_view(this: shared This, values: shared.View<UInt32>) -> shared.View<UInt32>\n"
            << "        values\n"
            << "\n"
            << "function sum(seed: UInt32, values: shared.View<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for item in seed.forward_view(values)\n"
            << "        total = total + item\n"
            << "    total\n";
    }
    auto method_returned_shared_view_for = pipeline.emit_llvm(method_returned_shared_view_for_path);
    assert(!method_returned_shared_view_for.has_errors());
    assert(method_returned_shared_view_for.dynamic_array_runtime_request_state.operations.empty());
    assert(
        method_returned_shared_view_for.ir_text.find(
            " = call { ptr, i64 } @method.UInt32.forward_view(i32 %seed, { ptr, i64 } %values)"
        ) != std::string::npos
    );
    assert(
        method_returned_shared_view_for.ir_text.find("%computed.sequence_for") != std::string::npos
    );
    auto method_returned_shared_view_for_object =
        orison::lowering::LlvmObjectEmitter {}.emit(method_returned_shared_view_for.ir_text);
    assert(!method_returned_shared_view_for_object.has_errors());

    auto member_receiver_method_returned_shared_view_for_path =
        smoke_temp_root / "orison_pipeline_member_receiver_method_returned_shared_view_for.or";
    {
        auto member_receiver_method_returned_shared_view_for_source =
            std::ofstream(member_receiver_method_returned_shared_view_for_path);
        member_receiver_method_returned_shared_view_for_source
            << "package demo.pipeline.memberreceiverreturnedsharedviewfor\n"
            << "\n"
            << "record Bucket\n"
            << "    marker: UInt32\n"
            << "\n"
            << "record Wrapper\n"
            << "    bucket: Bucket\n"
            << "\n"
            << "record Shelf\n"
            << "    buckets: Array<Bucket, 2>\n"
            << "\n"
            << "extend Bucket\n"
            << "    public function forward_view(this: shared This, values: shared.View<UInt32>) -> shared.View<UInt32>\n"
            << "        values\n"
            << "\n"
            << "function sum_wrapper(wrapper: Wrapper, values: shared.View<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for item in wrapper.bucket.forward_view(values)\n"
            << "        total = total + item\n"
            << "    total\n"
            << "\n"
            << "function sum_shelf(shelf: Shelf, values: shared.View<UInt32>) -> UInt32\n"
            << "    var total = 0 as UInt32\n"
            << "    for item in shelf.buckets[0].forward_view(values)\n"
            << "        total = total + item\n"
            << "    total\n";
    }
    auto member_receiver_method_returned_shared_view_for =
        pipeline.emit_llvm(member_receiver_method_returned_shared_view_for_path);
    assert(!member_receiver_method_returned_shared_view_for.has_errors());
    assert(member_receiver_method_returned_shared_view_for.dynamic_array_runtime_request_state.operations.empty());
    assert(
        member_receiver_method_returned_shared_view_for.ir_text.find(
            " = call { ptr, i64 } @method.Bucket.forward_view(%record.Bucket"
        ) != std::string::npos
    );
    assert(
        member_receiver_method_returned_shared_view_for.ir_text.find(
            " = getelementptr %record.Wrapper, ptr %wrapper.addr"
        ) != std::string::npos
    );
    assert(
        member_receiver_method_returned_shared_view_for.ir_text.find(
            " = getelementptr [2 x %record.Bucket], ptr"
        ) != std::string::npos
    );
    assert(
        member_receiver_method_returned_shared_view_for.ir_text.find("%computed.sequence_for") !=
        std::string::npos
    );
    auto member_receiver_method_returned_shared_view_for_object =
        orison::lowering::LlvmObjectEmitter {}.emit(
            member_receiver_method_returned_shared_view_for.ir_text
        );
    assert(!member_receiver_method_returned_shared_view_for_object.has_errors());

    auto dynamic_array_blocked_owned_cleanup = pipeline.emit_llvm(
        dynamic_array_source_owner_path,
        orison::pipeline::CompilePipelineOptions {
            .fixture_derive_dynamic_array_cleanup_from_semantics = true,
            .fixture_enable_dynamic_array_parameter_descriptors = true,
            .fixture_emit_bound_dynamic_array_parameter_cleanups = true,
            .dynamic_array_parameter_lowering_enabled = false,
        }
    );
    assert(!dynamic_array_blocked_owned_cleanup.has_errors());
    assert(dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_sequence_verification_passed);
    assert(!dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_capability_proven);
    assert(dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_emission_capability_state.capability_metadata_available);
    assert(!dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_emission_capability_state.proven);
    assert(
        !dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_emission_capability_state
            .element_cleanup_authorized_or_not_required
    );
    assert(
        dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_emission_capability_state
            .missing_element_drop_pairs.size() == 1
    );
    assert(
        dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_emission_capability_state
            .missing_element_drop_pairs.front() ==
        "items:items.element:__orison_drop.Payload"
    );
    assert(dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_availability.descriptor_origins_available);
    assert(dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_availability.descriptor_cleanup_plans_available);
    assert(dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_availability.cleanup_obligations_available);
    assert(dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_availability.sequence_verification_available);
    assert(dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_availability.sequence_verification_passed);
    assert(!dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_availability.cleanup_capability_proven);
    assert(dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_availability.missing_element_drop_pairs.size() == 1);
    assert(
        dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_availability
            .missing_element_drop_pairs.front() ==
        "items:items.element:__orison_drop.Payload"
    );
    assert(
        dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_availability
            .missing_element_drop_pairs.front() ==
        "items:items.element:__orison_drop.Payload"
    );
    assert(dynamic_array_blocked_owned_cleanup.drop_readiness_summary.cleanup_authorized == 0);
    assert(dynamic_array_blocked_owned_cleanup.drop_readiness_summary.cleanup_blocked == 1);
    assert(
        dynamic_array_blocked_owned_cleanup.ir_text.find("call void @__orison_drop.Payload") ==
        std::string::npos
    );
    assert(!orison::pipeline::dynamic_array_cleanup_production_ready(
        dynamic_array_blocked_owned_cleanup.dynamic_array_cleanup_production_readiness
    ));
    auto dynamic_array_blocked_owned_cleanup_production_readiness_report =
        formatted_dynamic_array_cleanup_production_readiness_report(dynamic_array_blocked_owned_cleanup);
    assert(dynamic_array_blocked_owned_cleanup_production_readiness_report.size() == 1);
    assert_line_contains(
        dynamic_array_blocked_owned_cleanup_production_readiness_report,
        0,
        "production readiness blocked"
    );
    assert_line_contains(
        dynamic_array_blocked_owned_cleanup_production_readiness_report,
        0,
        "[cleanup capability missing]"
    );
    assert_line_contains(
        dynamic_array_blocked_owned_cleanup_production_readiness_report,
        0,
        "missing-element-drop-pairs [items:items.element:__orison_drop.Payload]"
    );

    auto dynamic_array_owned_production_signature_rejected = pipeline.emit_llvm(
        dynamic_array_source_owner_path,
        orison::pipeline::CompilePipelineOptions {
            .fixture_derive_dynamic_array_cleanup_from_semantics = true,
            .dynamic_array_parameter_lowering_enabled = true,
            .dynamic_array_production_cleanup_emission_enabled = true,
        }
    );
    assert(dynamic_array_owned_production_signature_rejected.has_errors());
    assert(
        dynamic_array_owned_production_signature_rejected.error_text.find(
            "lowering DynamicArray parameter 'items' with owned element type Payload requires ownership/drop proof "
            "before production lowering"
        ) != std::string::npos
    );

    auto dynamic_array_owned_cleanup = pipeline.emit_llvm(
        dynamic_array_source_owner_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = orison::semantics::PlannedDropSite {
                        .source_type_name = "Payload",
                        .abi_symbol_name = "__orison_drop.Payload",
                        .owner_name = "items.element",
                        .site_line = 6,
                    },
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
            .fixture_derive_dynamic_array_cleanup_from_semantics = true,
            .fixture_enable_dynamic_array_parameter_descriptors = true,
            .fixture_emit_bound_dynamic_array_parameter_cleanups = true,
            .dynamic_array_parameter_lowering_enabled = false,
        }
    );
    assert(!dynamic_array_owned_cleanup.has_errors());
    assert(dynamic_array_owned_cleanup.dynamic_array_cleanup_sequence_verification_passed);
    assert(dynamic_array_owned_cleanup.dynamic_array_cleanup_capability_proven);
    assert(dynamic_array_owned_cleanup.dynamic_array_cleanup_emission_capability_state.capability_metadata_available);
    assert(dynamic_array_owned_cleanup.dynamic_array_cleanup_emission_capability_state.proven);
    assert(
        dynamic_array_owned_cleanup.dynamic_array_cleanup_emission_capability_state
            .element_cleanup_authorized_or_not_required
    );
    assert(dynamic_array_owned_cleanup.dynamic_array_cleanup_emission_capability_state.element_drop_pairs.size() == 1);
    assert(
        dynamic_array_owned_cleanup.dynamic_array_cleanup_emission_capability_state.element_drop_pairs.front() ==
        "items:items.element:__orison_drop.Payload"
    );
    assert(dynamic_array_owned_cleanup.dynamic_array_cleanup_availability.descriptor_origins_available);
    assert(dynamic_array_owned_cleanup.dynamic_array_cleanup_availability.descriptor_cleanup_plans_available);
    assert(dynamic_array_owned_cleanup.dynamic_array_cleanup_availability.cleanup_obligations_available);
    assert(dynamic_array_owned_cleanup.dynamic_array_cleanup_availability.sequence_verification_available);
    assert(dynamic_array_owned_cleanup.dynamic_array_cleanup_availability.sequence_verification_passed);
    assert(dynamic_array_owned_cleanup.dynamic_array_cleanup_availability.cleanup_capability_proven);
    assert(dynamic_array_owned_cleanup.dynamic_array_cleanup_availability.missing_element_drop_pairs.empty());
    assert(dynamic_array_owned_cleanup.drop_readiness_summary.cleanup_authorized == 1);
    assert(dynamic_array_owned_cleanup.drop_readiness_summary.cleanup_blocked == 0);
    assert(
        dynamic_array_owned_cleanup.ir_text.find("declare void @__orison_drop.Payload(ptr)") !=
        std::string::npos
    );
    assert(
        dynamic_array_owned_cleanup.ir_text.find(
            "call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup0.drop.element.addr)"
        ) != std::string::npos
    );
    assert(
        dynamic_array_owned_cleanup.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup0.cleanup.data, i64 8, "
            "i64 %items.dynamic_array_cleanup0.cleanup.capacity)"
        ) != std::string::npos
    );
    assert(!orison::pipeline::dynamic_array_cleanup_production_ready(
        dynamic_array_owned_cleanup.dynamic_array_cleanup_production_readiness
    ));
    auto dynamic_array_owned_cleanup_production_readiness_report =
        formatted_dynamic_array_cleanup_production_readiness_report(dynamic_array_owned_cleanup);
    assert(dynamic_array_owned_cleanup_production_readiness_report.size() == 1);
    assert_line_contains(
        dynamic_array_owned_cleanup_production_readiness_report,
        0,
        "production readiness blocked"
    );
    assert_line_contains(
        dynamic_array_owned_cleanup_production_readiness_report,
        0,
        "[cleanup capability ok]"
    );
    assert_line_contains(
        dynamic_array_owned_cleanup_production_readiness_report,
        0,
        "[production signatures missing]"
    );

    auto dynamic_array_owned_signature_gate_only = pipeline.emit_llvm(
        dynamic_array_source_owner_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = orison::semantics::PlannedDropSite {
                        .source_type_name = "Payload",
                        .abi_symbol_name = "__orison_drop.Payload",
                        .owner_name = "items.element",
                        .site_line = 6,
                    },
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
            .fixture_derive_dynamic_array_cleanup_from_semantics = true,
            .fixture_enable_dynamic_array_parameter_descriptors = true,
            .fixture_emit_bound_dynamic_array_parameter_cleanups = true,
            .dynamic_array_local_lowering_enabled = false,
            .dynamic_array_parameter_lowering_enabled = false,
            .dynamic_array_production_signature_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_owned_signature_gate_only.has_errors());
    assert(!orison::pipeline::dynamic_array_cleanup_production_ready(
        dynamic_array_owned_signature_gate_only.dynamic_array_cleanup_production_readiness
    ));
    auto dynamic_array_owned_signature_gate_only_production_readiness_report =
        formatted_dynamic_array_cleanup_production_readiness_report(dynamic_array_owned_signature_gate_only);
    assert_line_contains(
        dynamic_array_owned_signature_gate_only_production_readiness_report,
        0,
        "[production signatures ok]"
    );
    assert_line_contains(
        dynamic_array_owned_signature_gate_only_production_readiness_report,
        0,
        "[production construction missing]"
    );

    auto dynamic_array_owned_construction_gate = pipeline.emit_llvm(
        dynamic_array_source_owner_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = orison::semantics::PlannedDropSite {
                        .source_type_name = "Payload",
                        .abi_symbol_name = "__orison_drop.Payload",
                        .owner_name = "items.element",
                        .site_line = 6,
                    },
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
            .fixture_dynamic_array_construction_requests = {
                orison::lowering::FixtureDynamicArrayConstructionRequest {
                    .source_type_name = "DynamicArray<Payload>",
                    .owner_name = "items",
                    .initial_capacity = 2,
                },
            },
            .fixture_derive_dynamic_array_cleanup_from_semantics = true,
            .fixture_enable_dynamic_array_parameter_descriptors = true,
            .fixture_emit_bound_dynamic_array_parameter_cleanups = true,
            .dynamic_array_local_lowering_enabled = false,
            .dynamic_array_parameter_lowering_enabled = false,
            .dynamic_array_production_signature_lowering_enabled = true,
            .dynamic_array_production_construction_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_owned_construction_gate.has_errors());
    assert(dynamic_array_owned_construction_gate.dynamic_array_construction_plan_state.plans.size() == 1);
    auto dynamic_array_owned_construction_gate_plan_report =
        dynamic_array_construction_plan_report(dynamic_array_owned_construction_gate);
    assert_line_contains(
        dynamic_array_owned_construction_gate_plan_report,
        0,
        "requests __orison_dynamic_array_allocate"
    );
    assert(dynamic_array_owned_construction_gate.dynamic_array_runtime_request_state.operations.size() == 2);
    assert_any_line_contains(
        dynamic_array_runtime_request_report(dynamic_array_owned_construction_gate),
        "__orison_dynamic_array_allocate"
    );
    assert(
        dynamic_array_owned_construction_gate
            .dynamic_array_allocation_call_emission_state.rendered_call_count == 1
    );
    assert(
        dynamic_array_owned_construction_gate
            .dynamic_array_allocation_call_emission_state.allocation_calls_rendered
    );
    assert(
        dynamic_array_owned_construction_gate
            .dynamic_array_allocation_call_emission_state.ir_artifact_state.rendered_ir_snippets.front() ==
        "  %dynamic_array_alloc0.addr = alloca { ptr, i64, i64 }\n"
        "  call void @__orison_dynamic_array_allocate("
        "ptr sret({ ptr, i64, i64 }) %dynamic_array_alloc0.addr, i64 8, i64 2)\n"
        "  %dynamic_array_alloc0 = load { ptr, i64, i64 }, ptr %dynamic_array_alloc0.addr\n"
    );
    assert(
        dynamic_array_owned_construction_gate.ir_text.find(
            "call void @__orison_dynamic_array_allocate"
        ) == std::string::npos
    );
    assert(!orison::pipeline::dynamic_array_cleanup_production_ready(
        dynamic_array_owned_construction_gate.dynamic_array_cleanup_production_readiness
    ));
    auto dynamic_array_owned_construction_gate_production_readiness_report =
        formatted_dynamic_array_cleanup_production_readiness_report(dynamic_array_owned_construction_gate);
    assert_line_contains(
        dynamic_array_owned_construction_gate_production_readiness_report,
        0,
        "[production construction ok]"
    );
    assert_line_contains(
        dynamic_array_owned_construction_gate_production_readiness_report,
        0,
        "[production cleanup emission missing]"
    );

    auto dynamic_array_source_construction_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_source_construction.or";
    {
        auto source_construction_source = std::ofstream(dynamic_array_source_construction_path);
        source_construction_source
            << "package demo.pipeline.dynamicarraysourceconstruction\n"
            << "\n"
            << "function build_items<T>() -> UInt32\n"
            << "    let items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    1 as UInt32\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    0 as UInt32\n";
    }
    auto dynamic_array_source_construction = pipeline.emit_llvm(
        dynamic_array_source_construction_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_local_lowering_enabled = false,
            .dynamic_array_parameter_lowering_enabled = false,
            .dynamic_array_production_construction_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_source_construction.has_errors());
    assert(dynamic_array_source_construction.dynamic_array_construction_plan_state.plans.size() == 1);
    auto dynamic_array_source_construction_plan_report =
        dynamic_array_construction_plan_report(dynamic_array_source_construction);
    assert(
        dynamic_array_source_construction_plan_report.front() ==
        "dynamic array construction DynamicArray<UInt32> owner items element UInt32 lowers to i32 "
        "element_size 4 initial_capacity 0 requests __orison_dynamic_array_allocate (metadata only)"
    );
    assert(dynamic_array_source_construction.dynamic_array_runtime_request_state.operations.size() == 1);
    assert_line_contains(
        dynamic_array_runtime_request_report(dynamic_array_source_construction),
        0,
        "__orison_dynamic_array_allocate"
    );
    assert(
        dynamic_array_source_construction
            .dynamic_array_allocation_call_emission_state.rendered_call_count == 1
    );
    assert(
        dynamic_array_source_construction
            .dynamic_array_allocation_call_emission_state.allocation_calls_rendered
    );
    assert(
        dynamic_array_source_construction
            .dynamic_array_allocation_call_emission_state.ir_artifact_state.rendered_ir_snippets.front() ==
        "  %dynamic_array_alloc0.addr = alloca { ptr, i64, i64 }\n"
        "  call void @__orison_dynamic_array_allocate("
        "ptr sret({ ptr, i64, i64 }) %dynamic_array_alloc0.addr, i64 4, i64 0)\n"
        "  %dynamic_array_alloc0 = load { ptr, i64, i64 }, ptr %dynamic_array_alloc0.addr\n"
    );
    assert(
        dynamic_array_source_construction.ir_text.find(
            "call void @__orison_dynamic_array_allocate"
        ) == std::string::npos
    );

    auto dynamic_array_placed_construction_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_placed_construction.or";
    {
        auto placed_construction_source = std::ofstream(dynamic_array_placed_construction_path);
        placed_construction_source
            << "package demo.pipeline.dynamicarrayplacedconstruction\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    let items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    0 as UInt32\n";
    }
    auto dynamic_array_placed_construction = pipeline.emit_llvm(
        dynamic_array_placed_construction_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_local_lowering_enabled = false,
            .dynamic_array_parameter_lowering_enabled = false,
            .dynamic_array_production_construction_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_placed_construction.has_errors());
    assert(dynamic_array_placed_construction.dynamic_array_construction_plan_state.plans.size() == 1);
    auto dynamic_array_placed_construction_plan_report =
        dynamic_array_construction_plan_report(dynamic_array_placed_construction);
    assert_line_contains(
        dynamic_array_placed_construction_plan_report,
        0,
        "owner items"
    );
    assert(
        dynamic_array_placed_construction.ir_text.find(
            "  %items.dynamic_array_alloc.addr = alloca { ptr, i64, i64 }\n"
            "  call void @__orison_dynamic_array_allocate("
            "ptr sret({ ptr, i64, i64 }) %items.dynamic_array_alloc.addr, i64 4, i64 0)\n"
            "  %items.dynamic_array_alloc = load { ptr, i64, i64 }, ptr %items.dynamic_array_alloc.addr\n"
        ) != std::string::npos
    );
    assert(
        dynamic_array_placed_construction.ir_text.find(
            "  %items.addr = alloca { ptr, i64, i64 }\n"
            "  store { ptr, i64, i64 } %items.dynamic_array_alloc, ptr %items.addr\n"
        ) != std::string::npos
    );
    assert(dynamic_array_placed_construction.ir_text.find("__orison_dynamic_array_grow") == std::string::npos);
    assert(dynamic_array_placed_construction.ir_text.find("__orison_dynamic_array_deallocate") == std::string::npos);

    auto dynamic_array_local_cleanup = pipeline.emit_llvm(
        dynamic_array_placed_construction_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_cleanup_emission_enabled = true,
        }
    );
    assert(!dynamic_array_local_cleanup.has_errors());
    assert(dynamic_array_local_cleanup.dynamic_array_runtime_request_state.operations.size() == 2);
    assert_line_contains(
        dynamic_array_runtime_request_report(dynamic_array_local_cleanup),
        0,
        "__orison_dynamic_array_allocate"
    );
    assert_line_contains(
        dynamic_array_runtime_request_report(dynamic_array_local_cleanup),
        1,
        "__orison_dynamic_array_deallocate"
    );
    assert(
        dynamic_array_local_cleanup.ir_text.find(
            "  %items.dynamic_array_cleanup0.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
        ) != std::string::npos
    );
    assert(
        dynamic_array_local_cleanup.ir_text.find(
            "  call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup0.cleanup.data, i64 4, "
            "i64 %items.dynamic_array_cleanup0.cleanup.capacity)\n"
        ) != std::string::npos
    );
    auto local_cleanup = dynamic_array_local_cleanup.ir_text.find("call void @__orison_dynamic_array_deallocate");
    auto local_return = dynamic_array_local_cleanup.ir_text.find("ret i32 0");
    assert(local_cleanup != std::string::npos);
    assert(local_return != std::string::npos);
    assert(local_cleanup < local_return);

    auto dynamic_array_local_index_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_local_index.or";
    {
        auto local_index_source = std::ofstream(dynamic_array_local_index_path);
        local_index_source
            << "package demo.pipeline.dynamicarraylocalindex\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    let items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    items[0]\n";
    }
    auto dynamic_array_local_index = pipeline.emit_llvm(
        dynamic_array_local_index_path,
        orison::pipeline::CompilePipelineOptions {
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_index_lowering_enabled = true,
            .dynamic_array_production_cleanup_emission_enabled = true,
        }
    );
    assert(!dynamic_array_local_index.has_errors());
    assert(dynamic_array_local_index.dynamic_array_runtime_request_state.operations.size() == 3);
    assert_line_contains(
        dynamic_array_runtime_request_report(dynamic_array_local_index),
        1,
        "__orison_dynamic_array_bounds_failed"
    );
    assert(
        dynamic_array_local_index.ir_text.find("declare void @__orison_dynamic_array_bounds_failed()") !=
        std::string::npos
    );
    assert(
        dynamic_array_local_index.ir_text.find(
            "  %items.dynamic_array_index0.in_bounds = icmp ult i64 0, %items.dynamic_array_index0.length\n"
        ) != std::string::npos
    );
    assert(
        dynamic_array_local_index.ir_text.find(
            "  br i1 %items.dynamic_array_index0.in_bounds, label %dynamic_array.index.in_bounds.0, "
            "label %dynamic_array.index.out_of_bounds.0\n"
        ) != std::string::npos
    );
    assert(
        dynamic_array_local_index.ir_text.find(
            "dynamic_array.index.out_of_bounds.0:\n"
            "  call void @__orison_dynamic_array_bounds_failed()\n"
            "  unreachable\n"
            "dynamic_array.index.in_bounds.0:\n"
        ) != std::string::npos
    );
    assert(
        dynamic_array_local_index.ir_text.find(
            "  %items.dynamic_array_index0.value = load i32, ptr %items.dynamic_array_index0.element.addr\n"
        ) != std::string::npos
    );
    auto pipeline_index_load =
        dynamic_array_local_index.ir_text.find("%items.dynamic_array_index0.value = load i32");
    auto pipeline_index_cleanup =
        dynamic_array_local_index.ir_text.find("call void @__orison_dynamic_array_deallocate");
    auto pipeline_index_return =
        dynamic_array_local_index.ir_text.find("ret i32 %items.dynamic_array_index0.value");
    assert(pipeline_index_load != std::string::npos);
    assert(pipeline_index_cleanup != std::string::npos);
    assert(pipeline_index_return != std::string::npos);
    assert(pipeline_index_load < pipeline_index_cleanup);
    assert(pipeline_index_cleanup < pipeline_index_return);

    auto dynamic_array_local_append_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_local_append.or";
    {
        auto local_append_source = std::ofstream(dynamic_array_local_append_path);
        local_append_source
            << "package demo.pipeline.dynamicarraylocalappend\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    items.push(7 as UInt32)\n"
            << "    0 as UInt32\n";
    }
    auto dynamic_array_local_append = pipeline.emit_llvm(dynamic_array_local_append_path);
    assert(!dynamic_array_local_append.has_errors());
    assert(dynamic_array_local_append.dynamic_array_runtime_request_state.operations.size() == 3);
    assert_line_contains(
        dynamic_array_runtime_request_report(dynamic_array_local_append),
        1,
        "__orison_dynamic_array_grow"
    );
    assert(
        dynamic_array_local_append.ir_text.find(
            "  %items.dynamic_array_append0.has_capacity = icmp ult i64 %items.dynamic_array_append0.length, "
            "%items.dynamic_array_append0.capacity\n"
        ) != std::string::npos
    );
    assert(
        dynamic_array_local_append.ir_text.find(
            "  %items.dynamic_array_append0.next.capacity = select i1 %items.dynamic_array_append0.capacity.is_zero, "
            "i64 1, i64 %items.dynamic_array_append0.doubled.capacity\n"
        ) != std::string::npos
    );
    assert(
        dynamic_array_local_append.ir_text.find(
            "  call void @__orison_dynamic_array_grow("
        ) != std::string::npos
    );
    assert(
        dynamic_array_local_append.ir_text.find(
            "  store i32 7, ptr %items.dynamic_array_append0.element.addr\n"
        ) != std::string::npos
    );
    assert(dynamic_array_local_append.ir_text.find("__orison_dynamic_array_capacity_failed") == std::string::npos);
    auto dynamic_array_local_append_object =
        orison::lowering::LlvmObjectEmitter {}.emit(dynamic_array_local_append.ir_text);
    assert(!dynamic_array_local_append_object.has_errors());
    auto dynamic_array_local_append_executable = smoke_temp_root / "dynamic_array_local_append";
    auto dynamic_array_local_append_link = orison::link::HostLinker {}.link(
        dynamic_array_local_append_object.object_bytes,
        dynamic_array_local_append_executable
    );
    assert(!dynamic_array_local_append_link.has_errors());
    auto dynamic_array_local_append_status = std::system(dynamic_array_local_append_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_local_append_status));
    assert(WEXITSTATUS(dynamic_array_local_append_status) == 0);

    auto dynamic_array_append_index_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_append_index.or";
    {
        auto append_index_source = std::ofstream(dynamic_array_append_index_path);
        append_index_source
            << "package demo.pipeline.dynamicarrayappendindex\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    items.push(7 as UInt32)\n"
            << "    items[0]\n";
    }
    auto dynamic_array_append_index = pipeline.emit_llvm(dynamic_array_append_index_path);
    assert(!dynamic_array_append_index.has_errors());
    assert(dynamic_array_append_index.dynamic_array_runtime_request_state.operations.size() == 4);
    assert_line_contains(
        dynamic_array_runtime_request_report(dynamic_array_append_index),
        1,
        "__orison_dynamic_array_bounds_failed"
    );
    assert_line_contains(
        dynamic_array_runtime_request_report(dynamic_array_append_index),
        2,
        "__orison_dynamic_array_grow"
    );
    auto append_store = dynamic_array_append_index.ir_text.find(
        "  store i32 7, ptr %items.dynamic_array_append0.element.addr\n"
    );
    auto index_load = dynamic_array_append_index.ir_text.find(
        "  %items.dynamic_array_index1.value = load i32, ptr %items.dynamic_array_index1.element.addr\n"
    );
    auto append_index_cleanup = dynamic_array_append_index.ir_text.find(
        "call void @__orison_dynamic_array_deallocate"
    );
    auto append_index_return = dynamic_array_append_index.ir_text.find(
        "ret i32 %items.dynamic_array_index1.value"
    );
    assert(append_store != std::string::npos);
    assert(index_load != std::string::npos);
    assert(append_index_cleanup != std::string::npos);
    assert(append_index_return != std::string::npos);
    assert(append_store < index_load);
    assert(index_load < append_index_cleanup);
    assert(append_index_cleanup < append_index_return);
    auto dynamic_array_append_index_object =
        orison::lowering::LlvmObjectEmitter {}.emit(dynamic_array_append_index.ir_text);
    assert(!dynamic_array_append_index_object.has_errors());
    auto dynamic_array_append_index_executable = smoke_temp_root / "dynamic_array_append_index";
    auto dynamic_array_append_index_link = orison::link::HostLinker {}.link(
        dynamic_array_append_index_object.object_bytes,
        dynamic_array_append_index_executable
    );
    assert(!dynamic_array_append_index_link.has_errors());
    auto dynamic_array_append_index_status = std::system(dynamic_array_append_index_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_append_index_status));
    assert(WEXITSTATUS(dynamic_array_append_index_status) == 7);

    auto dynamic_array_index_assignment_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_index_assignment.or";
    {
        auto index_assignment_source = std::ofstream(dynamic_array_index_assignment_path);
        index_assignment_source
            << "package demo.pipeline.dynamicarrayindexassignment\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    items.push(7 as UInt32)\n"
            << "    items[0] = 11 as UInt32\n"
            << "    items[0]\n";
    }
    auto dynamic_array_index_assignment = pipeline.emit_llvm(dynamic_array_index_assignment_path);
    assert(!dynamic_array_index_assignment.has_errors());
    assert(dynamic_array_index_assignment.dynamic_array_runtime_request_state.operations.size() == 4);
    assert_line_contains(
        dynamic_array_runtime_request_report(dynamic_array_index_assignment),
        1,
        "__orison_dynamic_array_bounds_failed"
    );
    assert(
        dynamic_array_index_assignment.ir_text.find(
            "dynamic_array.assign.out_of_bounds."
        ) != std::string::npos
    );
    assert(
        dynamic_array_index_assignment.ir_text.find(
            "  store i32 11, ptr %items.dynamic_array_assign"
        ) != std::string::npos
    );
    auto index_assignment_store = dynamic_array_index_assignment.ir_text.find(
        "  store i32 11, ptr %items.dynamic_array_assign"
    );
    auto index_assignment_load = dynamic_array_index_assignment.ir_text.find(
        "  %items.dynamic_array_index"
    );
    auto index_assignment_return = dynamic_array_index_assignment.ir_text.find("ret i32 %items.dynamic_array_index");
    assert(index_assignment_store != std::string::npos);
    assert(index_assignment_load != std::string::npos);
    assert(index_assignment_return != std::string::npos);
    assert(index_assignment_store < index_assignment_load);
    assert(index_assignment_load < index_assignment_return);
    auto dynamic_array_index_assignment_object =
        orison::lowering::LlvmObjectEmitter {}.emit(dynamic_array_index_assignment.ir_text);
    assert(!dynamic_array_index_assignment_object.has_errors());
    auto dynamic_array_index_assignment_executable = smoke_temp_root / "dynamic_array_index_assignment";
    auto dynamic_array_index_assignment_link = orison::link::HostLinker {}.link(
        dynamic_array_index_assignment_object.object_bytes,
        dynamic_array_index_assignment_executable
    );
    assert(!dynamic_array_index_assignment_link.has_errors());
    auto dynamic_array_index_assignment_status =
        std::system(dynamic_array_index_assignment_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_index_assignment_status));
    assert(WEXITSTATUS(dynamic_array_index_assignment_status) == 11);

    auto dynamic_array_append_length_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_append_length.or";
    {
        auto append_length_source = std::ofstream(dynamic_array_append_length_path);
        append_length_source
            << "package demo.pipeline.dynamicarrayappendlength\n"
            << "\n"
            << "function main() -> IntSize\n"
            << "    var items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    items.push(7 as UInt32)\n"
            << "    items.length()\n";
    }
    auto dynamic_array_append_length = pipeline.emit_llvm(dynamic_array_append_length_path);
    assert(!dynamic_array_append_length.has_errors());
    assert(dynamic_array_append_length.dynamic_array_runtime_request_state.operations.size() == 3);
    assert_line_contains(
        dynamic_array_runtime_request_report(dynamic_array_append_length),
        1,
        "__orison_dynamic_array_grow"
    );
    assert(
        dynamic_array_append_length.ir_text.find(
            "  %items.dynamic_array_length1.value = extractvalue { ptr, i64, i64 } "
            "%items.dynamic_array_length1.descriptor, 1\n"
        ) != std::string::npos
    );
    assert(dynamic_array_append_length.ir_text.find("__orison_dynamic_array_bounds_failed") == std::string::npos);
    auto length_load = dynamic_array_append_length.ir_text.find(
        "%items.dynamic_array_length1.value = extractvalue { ptr, i64, i64 }"
    );
    auto append_length_cleanup = dynamic_array_append_length.ir_text.find(
        "call void @__orison_dynamic_array_deallocate"
    );
    auto append_length_return = dynamic_array_append_length.ir_text.find(
        "ret i64 %items.dynamic_array_length1.value"
    );
    assert(length_load != std::string::npos);
    assert(append_length_cleanup != std::string::npos);
    assert(append_length_return != std::string::npos);
    assert(length_load < append_length_cleanup);
    assert(append_length_cleanup < append_length_return);
    auto dynamic_array_append_length_object =
        orison::lowering::LlvmObjectEmitter {}.emit(dynamic_array_append_length.ir_text);
    assert(!dynamic_array_append_length_object.has_errors());
    auto dynamic_array_append_length_executable = smoke_temp_root / "dynamic_array_append_length";
    auto dynamic_array_append_length_link = orison::link::HostLinker {}.link(
        dynamic_array_append_length_object.object_bytes,
        dynamic_array_append_length_executable
    );
    assert(!dynamic_array_append_length_link.has_errors());
    auto dynamic_array_append_length_status = std::system(dynamic_array_append_length_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_append_length_status));
    assert(WEXITSTATUS(dynamic_array_append_length_status) == 1);

    auto dynamic_array_append_for_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_append_for.or";
    {
        auto append_for_source = std::ofstream(dynamic_array_append_for_path);
        append_for_source
            << "package demo.pipeline.dynamicarrayappendfor\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<UInt32> = DynamicArray()\n"
            << "    var total = 0 as UInt32\n"
            << "    items.push(7 as UInt32)\n"
            << "    items.push(9 as UInt32)\n"
            << "    for item in items\n"
            << "        total = total + item\n"
            << "    total\n";
    }
    auto dynamic_array_append_for = pipeline.emit_llvm(dynamic_array_append_for_path);
    assert(!dynamic_array_append_for.has_errors());
    assert(dynamic_array_append_for.dynamic_array_runtime_request_state.operations.size() == 3);
    assert(dynamic_array_append_for.ir_text.find("for.condition.") != std::string::npos);
    assert(dynamic_array_append_for.ir_text.find("for.continue.") != std::string::npos);
    assert(
        dynamic_array_append_for.ir_text.find(
            "getelementptr i32, ptr %items.sequence_for2.data, i64 %items.sequence_for2.index"
        ) != std::string::npos
    );
    assert(dynamic_array_append_for.ir_text.find("__orison_dynamic_array_bounds_failed") == std::string::npos);
    auto dynamic_array_append_for_object =
        orison::lowering::LlvmObjectEmitter {}.emit(dynamic_array_append_for.ir_text);
    assert(!dynamic_array_append_for_object.has_errors());
    auto dynamic_array_append_for_executable = smoke_temp_root / "dynamic_array_append_for";
    auto dynamic_array_append_for_link = orison::link::HostLinker {}.link(
        dynamic_array_append_for_object.object_bytes,
        dynamic_array_append_for_executable
    );
    assert(!dynamic_array_append_for_link.has_errors());
    auto dynamic_array_append_for_status = std::system(dynamic_array_append_for_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_append_for_status));
    assert(WEXITSTATUS(dynamic_array_append_for_status) == 16);

    auto dynamic_array_local_owned_cleanup_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_local_owned_cleanup.or";
    {
        auto local_owned_cleanup_source = std::ofstream(dynamic_array_local_owned_cleanup_path);
        local_owned_cleanup_source
            << "package demo.pipeline.dynamicarraylocalownedcleanup\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    let items: DynamicArray<Payload> = DynamicArray()\n"
            << "    1 as UInt32\n";
    }
    auto dynamic_array_local_owned_cleanup = pipeline.emit_llvm(
        dynamic_array_local_owned_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_cleanup_emission_enabled = true,
        }
    );
    assert(!dynamic_array_local_owned_cleanup.has_errors());
    assert(dynamic_array_local_owned_cleanup.semantic_drop_lowering_authorizations.size() == 2);
    assert(dynamic_array_local_owned_cleanup.dynamic_array_runtime_request_state.operations.size() == 2);
    assert(
        dynamic_array_local_owned_cleanup.ir_text.find("define void @__orison_drop.Payload(ptr %value)") !=
        std::string::npos
    );
    assert(
        dynamic_array_local_owned_cleanup.ir_text.find(
            "call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup0.drop.element.addr)"
        ) != std::string::npos
    );
    assert(
        dynamic_array_local_owned_cleanup.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup0.cleanup.data, i64 8, "
            "i64 %items.dynamic_array_cleanup0.cleanup.capacity)"
        ) != std::string::npos
    );
    auto local_owned_drop = dynamic_array_local_owned_cleanup.ir_text.find("call void @__orison_drop.Payload");
    auto local_owned_deallocate =
        dynamic_array_local_owned_cleanup.ir_text.find("call void @__orison_dynamic_array_deallocate");
    auto local_owned_return = dynamic_array_local_owned_cleanup.ir_text.find("ret i32 1");
    assert(local_owned_drop != std::string::npos);
    assert(local_owned_deallocate != std::string::npos);
    assert(local_owned_return != std::string::npos);
    assert(local_owned_drop < local_owned_deallocate);
    assert(local_owned_deallocate < local_owned_return);

    auto dynamic_array_owned_parameter_initialized_run_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_parameter_initialized_run.or";
    {
        auto owned_parameter_initialized_run_source = std::ofstream(dynamic_array_owned_parameter_initialized_run_path);
        owned_parameter_initialized_run_source
            << "package demo.pipeline.dynamicarrayownedparameterinitializedrun\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> Int64\n"
            << "    var total = 0 as Int64\n"
            << "    for item in items\n"
            << "        total = total + item.value\n"
            << "    items.length() == 1 ? total : 0\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    use_items(items) == 7 ? 0 as UInt32 : 1 as UInt32\n";
    }
    auto dynamic_array_owned_parameter_initialized_ir =
        pipeline.emit_llvm(dynamic_array_owned_parameter_initialized_run_path);
    assert(!dynamic_array_owned_parameter_initialized_ir.has_errors());
    assert(
        dynamic_array_owned_parameter_initialized_ir.ir_text.find(
            ".value = extractvalue { ptr, i64, i64 } %items.dynamic_array_length"
        ) != std::string::npos
    );
    assert(
        dynamic_array_owned_parameter_initialized_ir.ir_text.find(
            "%items.sequence_for0.descriptor = load { ptr, i64, i64 }, ptr %items.addr"
        ) != std::string::npos
    );
    assert(
        dynamic_array_owned_parameter_initialized_ir.ir_text.find(
            "%items.sequence_for0.more = icmp ult i64 %items.sequence_for0.index, %items.sequence_for0.length"
        ) != std::string::npos
    );
    assert(
        dynamic_array_owned_parameter_initialized_ir.ir_text.find(
            "%items.sequence_for0.value = load %record.Payload, ptr %items.sequence_for0.element.addr"
        ) != std::string::npos
    );
    assert(
        dynamic_array_owned_parameter_initialized_ir.ir_text.find(
            "getelementptr %record.Payload, ptr %item.addr, i32 0, i32 0"
        ) != std::string::npos
    );
    auto initialized_transfer_deallocate =
        dynamic_array_owned_parameter_initialized_ir.ir_text.find(
            "call void @__orison_dynamic_array_deallocate"
        );
    assert(initialized_transfer_deallocate != std::string::npos);
    assert(
        dynamic_array_owned_parameter_initialized_ir.ir_text.find(
            "call void @__orison_dynamic_array_deallocate",
            initialized_transfer_deallocate + 1
        ) == std::string::npos
    );
    auto dynamic_array_owned_parameter_initialized_run =
        pipeline.emit_object(dynamic_array_owned_parameter_initialized_run_path);
    assert(!dynamic_array_owned_parameter_initialized_run.has_errors());
    assert(!dynamic_array_owned_parameter_initialized_run.object_bytes.empty());
    auto dynamic_array_owned_parameter_initialized_run_executable =
        smoke_temp_root / "dynamic_array_owned_parameter_initialized_run";
    auto dynamic_array_owned_parameter_initialized_run_link = orison::link::HostLinker {}.link(
        dynamic_array_owned_parameter_initialized_run.object_bytes,
        dynamic_array_owned_parameter_initialized_run_executable
    );
    assert(!dynamic_array_owned_parameter_initialized_run_link.has_errors());
    auto dynamic_array_owned_parameter_initialized_run_status =
        std::system(dynamic_array_owned_parameter_initialized_run_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_owned_parameter_initialized_run_status));
    assert(WEXITSTATUS(dynamic_array_owned_parameter_initialized_run_status) == 0);

    auto dynamic_array_owned_parameter_forwarding_run_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_parameter_forwarding_run.or";
    {
        auto forwarding_run_source = std::ofstream(dynamic_array_owned_parameter_forwarding_run_path);
        forwarding_run_source
            << "package demo.pipeline.dynamicarrayownedparameterforwardingrun\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    0 as UInt32\n"
            << "\n"
            << "function forward_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    use_items(items)\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    forward_items(items)\n";
    }
    auto dynamic_array_owned_parameter_forwarding_ir =
        pipeline.emit_llvm(dynamic_array_owned_parameter_forwarding_run_path);
    assert(!dynamic_array_owned_parameter_forwarding_ir.has_errors());
    auto forwarding_deallocate =
        dynamic_array_owned_parameter_forwarding_ir.ir_text.find(
            "call void @__orison_dynamic_array_deallocate"
        );
    assert(forwarding_deallocate != std::string::npos);
    assert(
        dynamic_array_owned_parameter_forwarding_ir.ir_text.find(
            "call void @__orison_dynamic_array_deallocate",
            forwarding_deallocate + 1
        ) == std::string::npos
    );
    auto dynamic_array_owned_parameter_forwarding_run =
        pipeline.emit_object(dynamic_array_owned_parameter_forwarding_run_path);
    assert(!dynamic_array_owned_parameter_forwarding_run.has_errors());
    assert(!dynamic_array_owned_parameter_forwarding_run.object_bytes.empty());
    auto dynamic_array_owned_parameter_forwarding_run_executable =
        smoke_temp_root / "dynamic_array_owned_parameter_forwarding_run";
    auto dynamic_array_owned_parameter_forwarding_run_link = orison::link::HostLinker {}.link(
        dynamic_array_owned_parameter_forwarding_run.object_bytes,
        dynamic_array_owned_parameter_forwarding_run_executable
    );
    assert(!dynamic_array_owned_parameter_forwarding_run_link.has_errors());
    auto dynamic_array_owned_parameter_forwarding_run_status =
        std::system(dynamic_array_owned_parameter_forwarding_run_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_owned_parameter_forwarding_run_status));
    assert(WEXITSTATUS(dynamic_array_owned_parameter_forwarding_run_status) == 0);

    auto dynamic_array_owned_parameter_forwarding_reuse_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_parameter_forwarding_reuse.or";
    {
        auto forwarding_reuse_source = std::ofstream(dynamic_array_owned_parameter_forwarding_reuse_path);
        forwarding_reuse_source
            << "package demo.pipeline.dynamicarrayownedparameterforwardingreuse\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    0 as UInt32\n"
            << "\n"
            << "function forward_items(items: DynamicArray<Payload>) -> IntSize\n"
            << "    let result: UInt32 = use_items(items)\n"
            << "    items.length()\n"
            << "\n"
            << "function main() -> IntSize\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    forward_items(items)\n";
    }
    auto dynamic_array_owned_parameter_forwarding_reuse = pipeline.emit_llvm(
        dynamic_array_owned_parameter_forwarding_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(dynamic_array_owned_parameter_forwarding_reuse.has_errors());
    assert(
        dynamic_array_owned_parameter_forwarding_reuse.error_text.find("use after move: items") !=
        std::string::npos
    );

    auto dynamic_array_owned_parameter_branch_join_run_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_parameter_branch_join_run.or";
    {
        auto branch_join_source = std::ofstream(dynamic_array_owned_parameter_branch_join_run_path);
        branch_join_source
            << "package demo.pipeline.dynamicarrayownedparameterbranchjoinrun\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    0 as UInt32\n"
            << "\n"
            << "function choose(flag: Bool, items: DynamicArray<Payload>) -> UInt32\n"
            << "    if flag\n"
            << "        use_items(items)\n"
            << "    else\n"
            << "        use_items(items)\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    choose(true, items)\n";
    }
    auto dynamic_array_owned_parameter_branch_join_ir = pipeline.emit_llvm(
        dynamic_array_owned_parameter_branch_join_run_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_owned_parameter_branch_join_ir.has_errors());
    auto branch_join_deallocate =
        dynamic_array_owned_parameter_branch_join_ir.ir_text.find(
            "call void @__orison_dynamic_array_deallocate"
        );
    assert(branch_join_deallocate != std::string::npos);
    assert(
        dynamic_array_owned_parameter_branch_join_ir.ir_text.find(
            "call void @__orison_dynamic_array_deallocate",
            branch_join_deallocate + 1
        ) == std::string::npos
    );
    auto dynamic_array_owned_parameter_branch_join_run = pipeline.emit_object(
        dynamic_array_owned_parameter_branch_join_run_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_owned_parameter_branch_join_run.has_errors());
    assert(!dynamic_array_owned_parameter_branch_join_run.object_bytes.empty());
    auto dynamic_array_owned_parameter_branch_join_run_executable =
        smoke_temp_root / "dynamic_array_owned_parameter_branch_join_run";
    auto dynamic_array_owned_parameter_branch_join_run_link = orison::link::HostLinker {}.link(
        dynamic_array_owned_parameter_branch_join_run.object_bytes,
        dynamic_array_owned_parameter_branch_join_run_executable
    );
    assert(!dynamic_array_owned_parameter_branch_join_run_link.has_errors());
    auto dynamic_array_owned_parameter_branch_join_run_status =
        std::system(dynamic_array_owned_parameter_branch_join_run_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_owned_parameter_branch_join_run_status));
    assert(WEXITSTATUS(dynamic_array_owned_parameter_branch_join_run_status) == 0);

    auto dynamic_array_owned_parameter_branch_mismatch_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_parameter_branch_mismatch.or";
    {
        auto branch_mismatch_source = std::ofstream(dynamic_array_owned_parameter_branch_mismatch_path);
        branch_mismatch_source
            << "package demo.pipeline.dynamicarrayownedparameterbranchmismatch\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    0 as UInt32\n"
            << "\n"
            << "function choose(flag: Bool, items: DynamicArray<Payload>) -> UInt32\n"
            << "    if flag\n"
            << "        use_items(items)\n"
            << "    else\n"
            << "        0 as UInt32\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    choose(true, items)\n";
    }
    auto dynamic_array_owned_parameter_branch_mismatch = pipeline.emit_llvm(
        dynamic_array_owned_parameter_branch_mismatch_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(dynamic_array_owned_parameter_branch_mismatch.has_errors());
    assert(
        dynamic_array_owned_parameter_branch_mismatch.error_text.find(
            "if branch ownership mismatch: owned transfers must match across all continuing branches"
        ) != std::string::npos
    );

    auto dynamic_array_owned_parameter_statement_branch_mismatch_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_parameter_statement_branch_mismatch.or";
    {
        auto statement_branch_mismatch_source =
            std::ofstream(dynamic_array_owned_parameter_statement_branch_mismatch_path);
        statement_branch_mismatch_source
            << "package demo.pipeline.dynamicarrayownedparameterstatementbranchmismatch\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    0 as UInt32\n"
            << "\n"
            << "function main() -> IntSize\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    if true\n"
            << "        let result: UInt32 = use_items(items)\n"
            << "    else\n"
            << "        let result: UInt32 = 0 as UInt32\n"
            << "    items.length()\n";
    }
    auto dynamic_array_owned_parameter_statement_branch_mismatch = pipeline.emit_llvm(
        dynamic_array_owned_parameter_statement_branch_mismatch_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(dynamic_array_owned_parameter_statement_branch_mismatch.has_errors());
    assert(
        dynamic_array_owned_parameter_statement_branch_mismatch.error_text.find(
            "if branch ownership mismatch: owned transfers must match across all continuing branches"
        ) != std::string::npos
    );

    auto dynamic_array_owned_parameter_second_use_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_parameter_second_use.or";
    {
        auto second_use_source = std::ofstream(dynamic_array_owned_parameter_second_use_path);
        second_use_source
            << "package demo.pipeline.dynamicarrayownedparameterseconduse\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    0 as UInt32\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    let result: UInt32 = use_items(items)\n"
            << "    use_items(items)\n";
    }
    auto dynamic_array_owned_parameter_second_use = pipeline.emit_llvm(
        dynamic_array_owned_parameter_second_use_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(dynamic_array_owned_parameter_second_use.has_errors());
    assert(
        dynamic_array_owned_parameter_second_use.error_text.find("use after move: items") !=
        std::string::npos
    );

    auto dynamic_array_owned_parameter_length_after_move_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_parameter_length_after_move.or";
    {
        auto length_after_move_source = std::ofstream(dynamic_array_owned_parameter_length_after_move_path);
        length_after_move_source
            << "package demo.pipeline.dynamicarrayownedparameterlengthaftermove\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    0 as UInt32\n"
            << "\n"
            << "function main() -> IntSize\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    let result: UInt32 = use_items(items)\n"
            << "    items.length()\n";
    }
    auto dynamic_array_owned_parameter_length_after_move = pipeline.emit_llvm(
        dynamic_array_owned_parameter_length_after_move_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(dynamic_array_owned_parameter_length_after_move.has_errors());
    assert(
        dynamic_array_owned_parameter_length_after_move.error_text.find("use after move: items") !=
        std::string::npos
    );

    auto dynamic_array_owned_parameter_push_after_move_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_parameter_push_after_move.or";
    {
        auto push_after_move_source = std::ofstream(dynamic_array_owned_parameter_push_after_move_path);
        push_after_move_source
            << "package demo.pipeline.dynamicarrayownedparameterpushaftermove\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function use_items(items: DynamicArray<Payload>) -> UInt32\n"
            << "    0 as UInt32\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    let result: UInt32 = use_items(items)\n"
            << "    items.push(Payload(8))\n"
            << "    result\n";
    }
    auto dynamic_array_owned_parameter_push_after_move = pipeline.emit_llvm(
        dynamic_array_owned_parameter_push_after_move_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(dynamic_array_owned_parameter_push_after_move.has_errors());
    assert(
        dynamic_array_owned_parameter_push_after_move.error_text.find("use after move: items") !=
        std::string::npos
    );

    auto dynamic_array_push_owned_payload_reuse_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_push_owned_payload_reuse.or";
    {
        auto push_owned_payload_reuse_source = std::ofstream(dynamic_array_push_owned_payload_reuse_path);
        push_owned_payload_reuse_source
            << "package demo.pipeline.dynamicarraypushownedpayloadreuse\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function main() -> Int64\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    let payload = Payload(7)\n"
            << "    items.push(payload)\n"
            << "    payload.value\n";
    }
    auto dynamic_array_push_owned_payload_reuse = pipeline.emit_llvm(
        dynamic_array_push_owned_payload_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(dynamic_array_push_owned_payload_reuse.has_errors());
    assert(
        dynamic_array_push_owned_payload_reuse.error_text.find("use after move: payload") !=
        std::string::npos
    );

    auto dynamic_array_push_owned_field_reuse_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_push_owned_field_reuse.or";
    {
        auto push_owned_field_reuse_source = std::ofstream(dynamic_array_push_owned_field_reuse_path);
        push_owned_field_reuse_source
            << "package demo.pipeline.dynamicarraypushownedfieldreuse\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "record Box\n"
            << "    public payload: Payload\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function main() -> Int64\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    let box = Box(Payload(7))\n"
            << "    items.push(box.payload)\n"
            << "    box.payload.value\n";
    }
    auto dynamic_array_push_owned_field_reuse = pipeline.emit_llvm(
        dynamic_array_push_owned_field_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(dynamic_array_push_owned_field_reuse.has_errors());
    assert(
        dynamic_array_push_owned_field_reuse.error_text.find("use after move: box.payload") !=
        std::string::npos
    );

    auto dynamic_array_owned_element_assignment_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_element_assignment.or";
    {
        auto owned_element_assignment_source =
            std::ofstream(dynamic_array_owned_element_assignment_path);
        owned_element_assignment_source
            << "package demo.pipeline.dynamicarrayownedelementassignment\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function main() -> UInt32\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    items[0] = Payload(8)\n"
            << "    0 as UInt32\n";
    }
    auto dynamic_array_owned_element_assignment = pipeline.emit_llvm(
        dynamic_array_owned_element_assignment_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_owned_element_assignment.has_errors());
    auto replacement_drop = dynamic_array_owned_element_assignment.ir_text.find(
        "call void @__orison_drop.Payload(ptr %items.dynamic_array_assign"
    );
    auto cleanup_drop = dynamic_array_owned_element_assignment.ir_text.find(
        "call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup"
    );
    assert(replacement_drop != std::string::npos);
    auto replacement_store = dynamic_array_owned_element_assignment.ir_text.find(
        "store %record.Payload ",
        replacement_drop
    );
    assert(replacement_store != std::string::npos);
    auto replacement_store_end = dynamic_array_owned_element_assignment.ir_text.find("\n", replacement_store);
    assert(
        dynamic_array_owned_element_assignment.ir_text.find(
            "ptr %items.dynamic_array_assign",
            replacement_store
        ) < replacement_store_end
    );
    assert(cleanup_drop != std::string::npos);
    assert(replacement_drop < replacement_store);
    assert(replacement_store < cleanup_drop);
    auto dynamic_array_owned_element_assignment_object =
        orison::lowering::LlvmObjectEmitter {}.emit(dynamic_array_owned_element_assignment.ir_text);
    assert(!dynamic_array_owned_element_assignment_object.has_errors());
    auto dynamic_array_owned_element_assignment_executable =
        smoke_temp_root / "dynamic_array_owned_element_assignment";
    auto dynamic_array_owned_element_assignment_link = orison::link::HostLinker {}.link(
        dynamic_array_owned_element_assignment_object.object_bytes,
        dynamic_array_owned_element_assignment_executable
    );
    assert(!dynamic_array_owned_element_assignment_link.has_errors());
    auto dynamic_array_owned_element_assignment_status =
        std::system(dynamic_array_owned_element_assignment_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_owned_element_assignment_status));
    assert(WEXITSTATUS(dynamic_array_owned_element_assignment_status) == 0);

    auto dynamic_array_owned_element_assignment_rhs_reuse_path =
        smoke_temp_root / "orison_pipeline_dynamic_array_owned_element_assignment_rhs_reuse.or";
    {
        auto owned_element_assignment_rhs_reuse_source =
            std::ofstream(dynamic_array_owned_element_assignment_rhs_reuse_path);
        owned_element_assignment_rhs_reuse_source
            << "package demo.pipeline.dynamicarrayownedelementassignmentrhsreuse\n"
            << "\n"
            << "record Payload\n"
            << "    public value: Int64\n"
            << "\n"
            << "interface Drop\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "\n"
            << "implements Drop for Payload\n"
            << "    function drop(this: exclusive This) -> Unit\n"
            << "        return\n"
            << "\n"
            << "function main() -> Int64\n"
            << "    var items: DynamicArray<Payload> = DynamicArray()\n"
            << "    items.push(Payload(7))\n"
            << "    let payload = Payload(8)\n"
            << "    items[0] = payload\n"
            << "    payload.value\n";
    }
    auto dynamic_array_owned_element_assignment_rhs_reuse = pipeline.emit_llvm(
        dynamic_array_owned_element_assignment_rhs_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(dynamic_array_owned_element_assignment_rhs_reuse.has_errors());
    assert(
        dynamic_array_owned_element_assignment_rhs_reuse.error_text.find(
            "use after move: payload"
        ) != std::string::npos
    );

    auto dynamic_array_owned_production_ready = pipeline.emit_llvm(
        dynamic_array_source_owner_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = orison::semantics::PlannedDropSite {
                        .source_type_name = "Payload",
                        .abi_symbol_name = "__orison_drop.Payload",
                        .owner_name = "items.element",
                        .site_line = 6,
                    },
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
            .fixture_derive_dynamic_array_cleanup_from_semantics = true,
            .dynamic_array_parameter_lowering_enabled = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_cleanup_emission_enabled = true,
        }
    );
    assert(!dynamic_array_owned_production_ready.has_errors());
    assert(dynamic_array_owned_production_ready.drop_readiness_summary.cleanup_authorized == 1);
    assert(dynamic_array_owned_production_ready.drop_readiness_summary.cleanup_blocked == 0);
    assert(dynamic_array_owned_production_ready.dynamic_array_runtime_request_state.operations.size() == 1);
    assert_line_contains(
        dynamic_array_runtime_request_report(dynamic_array_owned_production_ready),
        0,
        "dynamic array runtime __orison_dynamic_array_deallocate"
    );
    assert(
        dynamic_array_owned_production_ready.ir_text.find("define i32 @use_items({ ptr, i64, i64 } %items)") !=
        std::string::npos
    );
    assert(
        dynamic_array_owned_production_ready.ir_text.find("declare void @__orison_drop.Payload(ptr)") !=
        std::string::npos
    );
    assert(
        dynamic_array_owned_production_ready.ir_text.find(
            "call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup0.drop.element.addr)"
        ) != std::string::npos
    );
    assert(
        dynamic_array_owned_production_ready.ir_text.find(
            "call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup0.cleanup.data, i64 8, "
            "i64 %items.dynamic_array_cleanup0.cleanup.capacity)"
        ) != std::string::npos
    );
    auto parameter_owned_drop = dynamic_array_owned_production_ready.ir_text.find(
        "call void @__orison_drop.Payload"
    );
    auto parameter_owned_deallocate = dynamic_array_owned_production_ready.ir_text.find(
        "call void @__orison_dynamic_array_deallocate"
    );
    auto parameter_owned_return = dynamic_array_owned_production_ready.ir_text.find("ret i32 1");
    assert(parameter_owned_drop != std::string::npos);
    assert(parameter_owned_deallocate != std::string::npos);
    assert(parameter_owned_return != std::string::npos);
    assert(parameter_owned_drop < parameter_owned_deallocate);
    assert(parameter_owned_deallocate < parameter_owned_return);

    auto dynamic_array_authorized_readiness = pipeline.emit_llvm(
        dynamic_array_drop_report_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = dynamic_array_source_owner.semantic_result.planned_drop_sites[1],
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
            .fixture_dynamic_array_construction_requests = {
                orison::lowering::FixtureDynamicArrayConstructionRequest {
                    .source_type_name = "DynamicArray<Payload>",
                    .owner_name = "items",
                    .initial_capacity = 2,
                },
            },
            .test_only_render_dynamic_array_element_drop_walks = true,
        }
    );
    assert(!dynamic_array_authorized_readiness.has_errors());
    auto dynamic_array_authorized_readiness_emitted_report =
        emitted_drop_declaration_report(dynamic_array_authorized_readiness);
    assert(dynamic_array_authorized_readiness_emitted_report.size() == 1);
    assert_line_contains(
        dynamic_array_authorized_readiness_emitted_report,
        0,
        "__orison_drop.Payload"
    );
    assert(drop_cleanup_authorization_report(dynamic_array_authorized_readiness).empty());
    assert(dynamic_array_authorized_readiness.drop_readiness_snapshot.semantic_authorizations.size() == 1);
    assert(
        dynamic_array_authorized_readiness.drop_readiness_snapshot.semantic_authorizations.front().site.owner_name ==
        "items.element"
    );
    assert(dynamic_array_authorized_readiness.drop_readiness_snapshot.emitted_declarations.size() == 1);
    assert(dynamic_array_authorized_readiness.drop_readiness_snapshot.cleanup_authorizations.size() == 1);
    auto dynamic_array_authorized_readiness_action_report =
        planned_drop_action_report(dynamic_array_authorized_readiness);
    assert(dynamic_array_authorized_readiness.planned_drop_action_state.actions.size() == 1);
    assert_line_contains(dynamic_array_authorized_readiness_action_report, 0, "capture items.element");
    assert(dynamic_array_authorized_readiness.drop_readiness_summary.semantic_authorized == 1);
    assert(dynamic_array_authorized_readiness.drop_readiness_summary.cleanup_authorized == 1);
    assert(dynamic_array_authorized_readiness.drop_readiness_summary.cleanup_blocked == 0);
    auto dynamic_array_authorized_readiness_snapshot_report =
        drop_readiness_snapshot_report(dynamic_array_authorized_readiness);
    assert(dynamic_array_authorized_readiness_snapshot_report.size() == 4);
    assert_line_contains(
        dynamic_array_authorized_readiness_snapshot_report,
        3,
        "__orison_dynamic_array_cleanup.0 authorized"
    );
    auto dynamic_array_authorized_readiness_relation_report =
        drop_readiness_relation_report(dynamic_array_authorized_readiness);
    assert(dynamic_array_authorized_readiness_relation_report.size() == 1);
    assert_line_contains(
        dynamic_array_authorized_readiness_relation_report,
        0,
        "__orison_dynamic_array_cleanup.0 authorized"
    );
    assert(dynamic_array_authorized_readiness.drop_readiness_blocker_summary.blocked_cleanups == 0);
    auto dynamic_array_authorized_readiness_source_correlation_report =
        drop_readiness_source_correlation_report(dynamic_array_authorized_readiness);
    assert(dynamic_array_authorized_readiness_source_correlation_report.size() == 1);
    assert(
        dynamic_array_authorized_readiness_source_correlation_report.front() ==
        "drop readiness source correlations actions 0 semantic sites 1"
    );
    assert(dynamic_array_authorized_readiness.ir_text.find("declare void @__orison_drop.Payload") != std::string::npos);
    assert(dynamic_array_authorized_readiness.ir_text.find("call void @__orison_drop.Payload") == std::string::npos);

    auto multi_drop_readiness_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" / "drop_readiness_multi.or";
    auto multi_drop_readiness = pipeline.emit_llvm(multi_drop_readiness_path);
    assert(!multi_drop_readiness.has_errors());
    auto multi_drop_readiness_planned_report =
        planned_drop_declaration_report(multi_drop_readiness);
    assert(multi_drop_readiness.planned_drop_declaration_state.declarations.size() == 2);
    assert_line_contains(multi_drop_readiness_planned_report, 0, "__orison_drop.Payload");
    assert_line_contains(multi_drop_readiness_planned_report, 1, "__orison_drop.OtherPayload");
    auto multi_drop_readiness_action_report =
        planned_drop_action_report(multi_drop_readiness);
    assert(multi_drop_readiness.planned_drop_action_state.actions.size() == 2);
    assert_line_contains(multi_drop_readiness_action_report, 0, "capture payload: Payload");
    assert_line_contains(multi_drop_readiness_action_report, 1, "capture other: OtherPayload");
    auto multi_drop_readiness_authorization_report =
        drop_cleanup_authorization_report(multi_drop_readiness);
    assert(multi_drop_readiness_authorization_report.size() == 7);
    assert(
        multi_drop_readiness_authorization_report[0].find(
            "__orison_thread_cleanup.launch.20.0 blocked"
        ) != std::string::npos
    );
    assert(
        multi_drop_readiness_authorization_report[1].find(
            "semantic drop lowering blocked __orison_drop.Payload"
        ) != std::string::npos
    );
    assert(
        multi_drop_readiness_authorization_report[2].find(
            "semantic drop lowering blocked __orison_drop.OtherPayload"
        ) != std::string::npos
    );
    assert(
        multi_drop_readiness_authorization_report[3].find(
            "semantic drop unresolved __orison_drop.Payload"
        ) != std::string::npos
    );
    assert(
        multi_drop_readiness_authorization_report[4].find(
            "semantic drop unresolved __orison_drop.OtherPayload"
        ) != std::string::npos
    );
    assert(
        multi_drop_readiness_authorization_report[5].find(
            "missing drop declaration __orison_drop.Payload"
        ) != std::string::npos
    );
    assert(
        multi_drop_readiness_authorization_report[6].find(
            "missing drop declaration __orison_drop.OtherPayload"
        ) != std::string::npos
    );
    assert(multi_drop_readiness.drop_readiness_snapshot.semantic_authorizations.size() == 2);
    assert(multi_drop_readiness.drop_readiness_snapshot.emitted_declarations.empty());
    assert(multi_drop_readiness.drop_readiness_snapshot.cleanup_authorizations.size() == 1);
    auto multi_drop_readiness_snapshot_report = drop_readiness_snapshot_report(multi_drop_readiness);
    assert(multi_drop_readiness_snapshot_report.size() == 4);
    assert(
        multi_drop_readiness_snapshot_report[0].find("semantic authorizations 2") !=
        std::string::npos
    );
    assert(
        multi_drop_readiness_snapshot_report[1].find("__orison_drop.Payload") !=
        std::string::npos
    );
    assert(
        multi_drop_readiness_snapshot_report[2].find("__orison_drop.OtherPayload") !=
        std::string::npos
    );
    assert(
        multi_drop_readiness_snapshot_report[3].find(
            "__orison_thread_cleanup.launch.20.0 blocked"
        ) != std::string::npos
    );
    assert(multi_drop_readiness.drop_readiness_summary.semantic_authorized == 0);
    assert(multi_drop_readiness.drop_readiness_summary.semantic_blocked == 2);
    assert(multi_drop_readiness.drop_readiness_summary.emitted_declarations == 0);
    assert(multi_drop_readiness.drop_readiness_summary.cleanup_authorized == 0);
    assert(multi_drop_readiness.drop_readiness_summary.cleanup_blocked == 1);
    auto multi_drop_readiness_summary_report = drop_readiness_summary_report(multi_drop_readiness);
    assert(multi_drop_readiness_summary_report.size() == 1);
    assert(
        multi_drop_readiness_summary_report.front().find("semantic authorized 0 blocked 2") !=
        std::string::npos
    );
    auto multi_drop_readiness_relation_report = drop_readiness_relation_report(multi_drop_readiness);
    assert(multi_drop_readiness_relation_report.size() == 5);
    assert(
        multi_drop_readiness_relation_report[0].find(
            "__orison_thread_cleanup.launch.20.0 blocked"
        ) != std::string::npos
    );
    assert(
        multi_drop_readiness_relation_report[1].find("__orison_drop.Payload") !=
        std::string::npos
    );
    assert(
        multi_drop_readiness_relation_report[2].find("__orison_drop.OtherPayload") !=
        std::string::npos
    );
    assert(
        multi_drop_readiness_relation_report[3].find(
            "missing declaration __orison_drop.Payload"
        ) != std::string::npos
    );
    assert(
        multi_drop_readiness_relation_report[4].find(
            "missing declaration __orison_drop.OtherPayload"
        ) != std::string::npos
    );
    assert(multi_drop_readiness.drop_readiness_blocker_summary.blocked_cleanups == 1);
    assert(multi_drop_readiness.drop_readiness_blocker_summary.semantic_lowering_blockers.size() == 2);
    assert(multi_drop_readiness.drop_readiness_blocker_summary.semantic_unresolved_blockers.size() == 2);
    assert(multi_drop_readiness.drop_readiness_blocker_summary.source_drop_lowering_blockers.empty());
    assert(multi_drop_readiness.drop_readiness_blocker_summary.missing_declarations.size() == 2);
    auto multi_drop_readiness_blocker_report = drop_readiness_blocker_report(multi_drop_readiness);
    assert(multi_drop_readiness_blocker_report.size() == 7);
    assert(
        multi_drop_readiness_blocker_report[0] ==
        "drop readiness blockers cleanups 1 semantic blockers 2 semantic unresolved 2 "
        "source lowering blocked 0 missing declarations 2"
    );
    assert(
        multi_drop_readiness_blocker_report[2].find("__orison_drop.OtherPayload") !=
        std::string::npos
    );

    auto failed_lowering_path =
        std::filesystem::temp_directory_path() / "orison_pipeline_drop_readiness_summary_failure.or";
    {
        std::ofstream source(failed_lowering_path);
        source << "package demo.readinessfailure\n";
        source << "function same(left: Bool, right: Bool) -> Bool\n";
        source << "    left < right\n";
    }
    auto failed_lowering = pipeline.emit_llvm(failed_lowering_path);
    assert(failed_lowering.has_errors());
    assert(
        failed_lowering.error_text.find(
            "lowering does not yet support this return expression: unsupported operator: <"
        ) != std::string::npos
    );
    assert(drop_readiness_snapshot_report(failed_lowering).empty());
    assert(drop_readiness_summary_report(failed_lowering).empty());
    assert(drop_readiness_relation_report(failed_lowering).empty());
    assert(drop_readiness_blocker_report(failed_lowering).empty());

    auto failed_unary_lowering_path =
        std::filesystem::temp_directory_path() / "orison_pipeline_drop_readiness_unary_failure.or";
    {
        std::ofstream source(failed_unary_lowering_path);
        source << "package demo.readinessfailure\n";
        source << "function negate(value: UInt32) -> UInt32\n";
        source << "    -value\n";
    }
    auto failed_unary_lowering = pipeline.emit_llvm(failed_unary_lowering_path);
    assert(failed_unary_lowering.has_errors());
    assert(
        failed_unary_lowering.error_text.find(
            "lowering does not yet support this return expression: unsupported operator: -"
        ) != std::string::npos
    );
    assert(drop_readiness_snapshot_report(failed_unary_lowering).empty());
    assert(drop_readiness_summary_report(failed_unary_lowering).empty());
    assert(drop_readiness_relation_report(failed_unary_lowering).empty());
    assert(drop_readiness_blocker_report(failed_unary_lowering).empty());

    auto failed_cast_lowering_path =
        std::filesystem::temp_directory_path() / "orison_pipeline_drop_readiness_cast_failure.or";
    {
        std::ofstream source(failed_cast_lowering_path);
        source << "package demo.readinessfailure\n";
        source << "function main() -> UInt32\n";
        source << "    -1 as UInt32\n";
    }
    auto failed_cast_lowering = pipeline.emit_llvm(failed_cast_lowering_path);
    assert(failed_cast_lowering.has_errors());
    assert(
        failed_cast_lowering.error_text.find(
            "lowering does not yet support this return expression: unsupported cast: negative value to UInt32"
        ) != std::string::npos
    );
    assert(drop_readiness_snapshot_report(failed_cast_lowering).empty());
    assert(drop_readiness_summary_report(failed_cast_lowering).empty());
    assert(drop_readiness_relation_report(failed_cast_lowering).empty());
    assert(drop_readiness_blocker_report(failed_cast_lowering).empty());

    auto failed_final_if_lowering_path =
        std::filesystem::temp_directory_path() / "orison_pipeline_drop_readiness_final_if_failure.or";
    {
        std::ofstream source(failed_final_if_lowering_path);
        source << "package demo.readinessfailure\n";
        source << "function same(flag: Bool, left: Bool, right: Bool) -> Bool\n";
        source << "    if flag\n";
        source << "        left < right\n";
        source << "    else\n";
        source << "        false\n";
    }
    auto failed_final_if_lowering = pipeline.emit_llvm(failed_final_if_lowering_path);
    assert(failed_final_if_lowering.has_errors());
    assert(
        failed_final_if_lowering.error_text.find(
            "lowering does not yet support this final control-flow statement: "
            "if then arm lowering failed: unsupported operator: <"
        ) != std::string::npos
    );
    assert(drop_readiness_snapshot_report(failed_final_if_lowering).empty());
    assert(drop_readiness_summary_report(failed_final_if_lowering).empty());
    assert(drop_readiness_relation_report(failed_final_if_lowering).empty());
    assert(drop_readiness_blocker_report(failed_final_if_lowering).empty());

    auto failed_final_switch_lowering_path =
        std::filesystem::temp_directory_path() / "orison_pipeline_drop_readiness_final_switch_failure.or";
    {
        std::ofstream source(failed_final_switch_lowering_path);
        source << "package demo.readinessfailure\n";
        source << "function same(flag: Bool, left: Bool, right: Bool) -> Bool\n";
        source << "    switch flag\n";
        source << "        true => left < right\n";
        source << "        false => false\n";
    }
    auto failed_final_switch_lowering = pipeline.emit_llvm(failed_final_switch_lowering_path);
    assert(failed_final_switch_lowering.has_errors());
    assert(
        failed_final_switch_lowering.error_text.find(
            "lowering does not yet support this final control-flow statement: "
            "switch case lowering failed: unsupported operator: <"
        ) != std::string::npos
    );
    assert(drop_readiness_snapshot_report(failed_final_switch_lowering).empty());
    assert(drop_readiness_summary_report(failed_final_switch_lowering).empty());
    assert(drop_readiness_relation_report(failed_final_switch_lowering).empty());
    assert(drop_readiness_blocker_report(failed_final_switch_lowering).empty());

    auto object = pipeline.emit_object(source_path);
    assert(!object.has_errors());
    assert(!object.object_bytes.empty());

    auto missing = pipeline.analyze(source_path.parent_path() / "missing.or");
    assert(missing.has_errors());
    assert(missing.error_text == "error: unable to read source file\n");

    auto library_path = std::filesystem::temp_directory_path() / "orison_pipeline_libraries.or";
    {
        std::ofstream source(library_path);
        source << "package demo.libraries\n";
        source << "package foreign \"c\" library \"m\"\n";
        source << "    function first(value: Int32) -> Int32\n";
        source << "package foreign \"c\" library \"m\"\n";
        source << "    function second(value: Int32) -> Int32\n";
        source << "function main() -> Int32\n";
        source << "    0 as Int32\n";
    }
    auto libraries = pipeline.analyze(library_path);
    assert(!libraries.has_errors());
    assert(libraries.link_libraries.size() == 1);
    assert(libraries.link_libraries.front() == "m");

    auto semantic_drop_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" / "semantic_planned_drop.or";
    auto semantic_drops = pipeline.analyze(semantic_drop_path);
    assert(!semantic_drops.has_errors());
    auto semantic_drops_planned_report = semantic_planned_drop_report(semantic_drops);
    auto semantic_drops_resolution_report = semantic_drop_resolution_report(semantic_drops);
    auto semantic_drops_diagnostic_report = semantic_drop_diagnostic_report(semantic_drops);
    auto semantic_drops_authorization_report = semantic_drop_lowering_authorization_report(semantic_drops);
    auto semantic_drops_summary_report = semantic_drop_resolution_summary_report(semantic_drops);
    assert(semantic_drops_planned_report.size() == 2);
    assert_line_contains(semantic_drops_planned_report, 0, "owner input");
    assert_line_contains(semantic_drops_planned_report, 1, "owner local");
    assert(semantic_drops_resolution_report.size() == 2);
    assert_line_contains(semantic_drops_resolution_report, 0, "missing drop site");
    assert_line_contains(semantic_drops_resolution_report, 1, "owner local");
    assert(semantic_drops_diagnostic_report.size() == 2);
    assert_line_contains(semantic_drops_diagnostic_report, 0, "blocked no implementation discovered");
    assert_line_contains(semantic_drops_diagnostic_report, 1, "owner local");
    assert(semantic_drops_authorization_report.size() == 2);
    assert(semantic_drops.semantic_drop_lowering_authorizations.size() == 2);
    assert(!semantic_drops.semantic_drop_lowering_authorizations[0].semantic_resolved);
    assert(!semantic_drops.semantic_drop_lowering_authorizations[0].source_drop_lowering_enabled);
    assert(!semantic_drops.semantic_drop_lowering_authorizations[0].authorized);
    assert_line_contains(
        semantic_drops_authorization_report,
        0,
        "semantic-unresolved lowering-blocked"
    );
    assert(semantic_drops_summary_report.size() == 1);
    assert_line_contains(semantic_drops_summary_report, 0, "resolved 0 missing 2");

    auto parsed_drop_path = std::filesystem::temp_directory_path() / "orison_pipeline_parsed_drop_candidate.or";
    {
        std::ofstream source(parsed_drop_path);
        source << "package demo.parseddrop\n";
        source << "record Payload\n";
        source << "    public value: Int64\n";
        source << "interface Drop\n";
        source << "    function drop(this: exclusive This) -> Unit\n";
        source << "implements Drop for Payload\n";
        source << "    function drop(this: exclusive This) -> Unit\n";
        source << "        return\n";
        source << "function read(input: Payload) -> Int64\n";
        source << "    input.value\n";
    }
    auto parsed_drop = pipeline.analyze(parsed_drop_path);
    assert(!parsed_drop.has_errors());
    auto parsed_drop_planned_report = semantic_planned_drop_report(parsed_drop);
    auto parsed_drop_implementation_report =
        semantic_drop_implementation_discovery_report(parsed_drop.semantic_drop_state);
    auto parsed_drop_resolution_report = semantic_drop_resolution_report(parsed_drop);
    auto parsed_drop_diagnostic_report = semantic_drop_diagnostic_report(parsed_drop);
    auto parsed_drop_authorization_report = semantic_drop_lowering_authorization_report(parsed_drop);
    assert(parsed_drop_planned_report.size() == 1);
    assert_line_contains(parsed_drop_planned_report, 0, "owner input");
    assert(parsed_drop_implementation_report.size() == 1);
    assert_line_contains(parsed_drop_implementation_report, 0, "parsed-candidate-collection");
    assert(parsed_drop_resolution_report.size() == 1);
    assert_line_contains(parsed_drop_resolution_report, 0, "resolved drop site");
    assert(parsed_drop_diagnostic_report.size() == 1);
    assert_line_contains(parsed_drop_diagnostic_report, 0, "resolved");
    assert(parsed_drop_authorization_report.size() == 1);
    assert(parsed_drop.semantic_drop_lowering_authorizations.size() == 1);
    assert(parsed_drop.semantic_drop_lowering_authorizations.front().semantic_resolved);
    assert(!parsed_drop.semantic_drop_lowering_authorizations.front().source_drop_lowering_enabled);
    assert(!parsed_drop.semantic_drop_lowering_authorizations.front().authorized);
    assert_line_contains(
        parsed_drop_authorization_report,
        0,
        "semantic-resolved lowering-blocked"
    );
    auto parsed_drop_ir = pipeline.emit_llvm(parsed_drop_path);
    assert(!parsed_drop_ir.has_errors());
    assert(parsed_drop_ir.semantic_drop_lowering_authorizations.size() == 1);
    assert(parsed_drop_ir.semantic_drop_lowering_authorizations.front().semantic_resolved);
    assert(!parsed_drop_ir.semantic_drop_lowering_authorizations.front().source_drop_lowering_enabled);
    assert(!parsed_drop_ir.semantic_drop_lowering_authorizations.front().authorized);
    assert(parsed_drop_ir.ir_text.find("__orison_drop.Payload") == std::string::npos);
    auto parsed_drop_source_lowering_ir = pipeline.emit_llvm(
        parsed_drop_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(!parsed_drop_source_lowering_ir.has_errors());
    assert(parsed_drop_source_lowering_ir.semantic_drop_lowering_authorizations.size() == 1);
    assert(parsed_drop_source_lowering_ir.semantic_drop_lowering_authorizations.front().semantic_resolved);
    assert(parsed_drop_source_lowering_ir.semantic_drop_lowering_authorizations.front().source_drop_lowering_enabled);
    assert(parsed_drop_source_lowering_ir.semantic_drop_lowering_authorizations.front().authorized);
    assert(
        parsed_drop_source_lowering_ir.ir_text.find("define void @__orison_drop.Payload(ptr %value)") !=
        std::string::npos
    );

    auto runtime_indexed_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "choice_constructor_multi_variant_computed_index_member_path_move_rejected.or";
    auto runtime_indexed_cleanup = pipeline.emit_llvm(
        runtime_indexed_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
        }
    );
    assert(runtime_indexed_cleanup.has_errors());
    assert(
        runtime_indexed_cleanup.error_text.find(
            "indexed constructor ownership move requires explicit partial ownership support"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_capability_state
            .capability_metadata_available
    );
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_capability_state.capability_count == 1);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_capability_state.all_prerequisites_ready);
    assert(!runtime_indexed_cleanup.runtime_indexed_cleanup_capability_state.any_production_enabled);
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_capability_state.capabilities.front()
            .owner_name == "holder.items"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_capability_state.capabilities.front()
            .index_expression_text == "index"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_capability_state.capabilities.front()
            .element_source_type_name == "Inner"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.plan_metadata_available
    );
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.plan_count == 1);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.operation_count == 5);
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state
            .comment_ir_preview_line_count == 5
    );
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.all_prerequisites_ready);
    assert(!runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.any_production_gate_requested);
    assert(!runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.any_production_enabled);
    assert(!runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.any_length_load_slice_lowerable);
    assert(!runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.any_loop_block_slice_lowerable);
    assert(!runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.any_skip_branch_slice_lowerable);
    assert(!runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.any_live_element_drop_slice_lowerable);
    assert(!runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.any_cleanup_tail_slice_lowerable);
    assert(!runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.any_structured_ir_plan_complete);
    assert(!runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.all_function_insertion_targets_known);
    assert(!runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.any_function_insertion_planned);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.gated_ir_slice_line_count == 0);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.structured_ir_plan_count == 0);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.function_insertion_plan_count == 0);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_ir_render_state.render_metadata_available);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_ir_render_state.plan_count == 1);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_ir_render_state.rendered_plan_count == 0);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_ir_render_state.rendered_ir_line_count == 0);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_ir_render_state.rendered_ir_lines.empty());
    assert(!runtime_indexed_cleanup.runtime_indexed_cleanup_ir_render_state.all_structured_plans_complete);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_ir_render_state.all_rendered_lines_match_artifact);
    assert(!runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_artifact_state.artifact_available);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_artifact_state.separate_from_module_ir);
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_artifact_state
            .rendered_ir_line_count == 0
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_artifact_state.rendered_ir_lines.empty()
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_insertion_gate_state
            .insertion_requested
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_insertion_gate_state
            .artifact_available
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_insertion_gate_state
            .render_parity_verified
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_insertion_gate_state
            .insertion_enabled
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_insertion_gate_state
            .remains_separate_from_module_ir
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .preview_available
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .insertion_point_found
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .would_modify_module_ir
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .original_module_line_count == logical_line_count(runtime_indexed_cleanup.ir_text)
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .projected_module_line_count ==
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .original_module_line_count
    );
    assert(!runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_candidate_state.candidate_available);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_candidate_state.separate_from_module_ir);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_candidate_state.candidate_ir_text.empty());
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_candidate_state
            .candidate_module_line_count ==
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .original_module_line_count
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_candidate_verification_state
            .verification_available
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_candidate_verification_state
            .verified
    );
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.metadata_available);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plan_count == 1);
    assert(!runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.all_targets_known);
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_plan_state
            .any_rewrite_candidate_available
    );
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.function_ir_unchanged);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.rewrite_candidate_count == 0);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.cleanup_slice_line_count == 0);
    assert(!runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.any_continuation_block_generated);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.candidate_cfg_line_count == 0);
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plans.front()
            .function_symbol_name == "main"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plans.front()
            .replaced_terminator_text.empty()
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plans.front()
            .inserted_branch_text.empty()
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plans.front()
            .continuation_block_text.empty()
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plans.front()
            .candidate_cfg_lines.empty()
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plans.front()
             .continuation_block_generated
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .verification_metadata_available
    );
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verification_count == 1);
    assert(!runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.all_functions_found);
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .all_predecessor_blocks_found
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .all_insertion_blocks_absent
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
             .all_continuation_blocks_found
    );
    assert(!runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.all_verified);
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
             .all_candidate_insertion_blocks_found
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
             .all_candidate_continuation_blocks_found
    );
    assert(!runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.all_candidates_verified);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.candidate_verified_count == 0);
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verified_count == 0);
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
             .front()
             .verification_available
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
            .front()
            .function_found
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
            .front()
            .predecessor_block_found
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
            .front()
            .insertion_block_absent
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
             .front()
             .continuation_block_found
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
             .front()
             .candidate_insertion_block_found
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
             .front()
             .candidate_continuation_block_found
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
             .front()
             .candidate_verified
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
            .insertion_gate_ready
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
            .insertion_preview_ready
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
            .candidate_ready
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
            .candidate_verified
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
            .module_mutation_enabled
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
            .function_integration_ready
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
            .production_ready
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_mutation_state
            .mutation_requested
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_mutation_state
            .candidate_verified
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_mutation_state
            .mutation_applied
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_mutation_state
            .module_matches_candidate
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_mutation_state
            .final_module_cleanup_block_count == 0
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_mutation_state
            .final_module_line_count == logical_line_count(runtime_indexed_cleanup.ir_text)
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .owner_name == "holder.items"
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .production_gate_requested
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .production_enabled
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .operation_names.size() == 5
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .owner_deallocation_planned
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .length_load_slice_lowerable
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .loop_block_slice_lowerable
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .skip_branch_slice_lowerable
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .live_element_drop_slice_lowerable
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .cleanup_tail_slice_lowerable
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .ir_plan.complete
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .function_symbol_name == "main"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .function_insertion_block_name.empty()
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .function_insertion_target_known
    );
    assert(
        !runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .function_insertion_planned
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .comment_ir_preview_lines.front() ==
        "; runtime-index cleanup preview load-length owner holder.items\n"
    );
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines.size() == 7);

    auto runtime_indexed_cleanup_gate_on = pipeline.emit_llvm(
        runtime_indexed_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
        }
    );
    assert(runtime_indexed_cleanup_gate_on.has_errors());
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state
            .any_production_gate_requested
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state
            .any_production_enabled
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state
            .any_length_load_slice_lowerable
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state
            .any_loop_block_slice_lowerable
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state
            .any_skip_branch_slice_lowerable
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state
            .any_live_element_drop_slice_lowerable
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state
            .any_cleanup_tail_slice_lowerable
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state
            .any_structured_ir_plan_complete
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state
            .all_function_insertion_targets_known
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state
            .any_function_insertion_planned
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state
            .gated_ir_slice_line_count == 17
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state
            .structured_ir_plan_count == 1
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state
            .function_insertion_plan_count == 1
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .function_symbol_name == "main"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .function_insertion_block_name == "holder.items.runtime_cleanup.entry"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .function_predecessor_block_name == "entry"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .function_continuation_block_name == "holder.items.runtime_cleanup.exit"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .function_insertion_target_known
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .function_insertion_planned
    );
    assert(runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_ir_render_state.render_metadata_available);
    assert(runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_ir_render_state.plan_count == 1);
    assert(runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_ir_render_state.rendered_plan_count == 1);
    assert(runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_ir_render_state.rendered_ir_line_count == 17);
    assert(runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_ir_render_state.all_structured_plans_complete);
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_ir_render_state
            .all_rendered_lines_match_artifact
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_ir_render_state.rendered_ir_lines ==
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_artifact_state
            .artifact_available
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_artifact_state
            .separate_from_module_ir
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_artifact_state
            .rendered_ir_line_count == 17
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_artifact_state.rendered_ir_lines ==
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_ir_render_state.rendered_ir_lines
    );
    assert(
        runtime_indexed_cleanup_gate_on.ir_text.find(
            runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_artifact_state
                .rendered_ir_lines.front()
        ) == std::string::npos
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_insertion_gate_state
            .insertion_requested
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_insertion_gate_state
            .artifact_available
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_insertion_gate_state
            .render_parity_verified
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_insertion_gate_state
            .insertion_enabled
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_insertion_gate_state
            .remains_separate_from_module_ir
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .preview_available
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .insertion_point_found
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .would_modify_module_ir
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .original_module_line_count == logical_line_count(runtime_indexed_cleanup_gate_on.ir_text)
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .projected_module_line_count ==
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .original_module_line_count
    );
    assert(!runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_candidate_state.candidate_available);
    assert(runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_candidate_state.separate_from_module_ir);
    assert(runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_candidate_state.candidate_ir_text.empty());
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_candidate_state
            .candidate_module_line_count ==
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .original_module_line_count
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_candidate_verification_state
            .verification_available
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_candidate_verification_state
            .verified
    );
    assert(runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.metadata_available);
    assert(runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plan_count == 1);
    assert(runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.all_targets_known);
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state
            .any_rewrite_candidate_available
    );
    assert(runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.function_ir_unchanged);
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state
            .rewrite_candidate_count == 1
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state
            .cleanup_slice_line_count == 17
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state
            .any_continuation_block_generated
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state
            .candidate_cfg_line_count == 19
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plans.front()
            .function_symbol_name == "main"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plans.front()
            .owner_name == "holder.items"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plans.front()
            .predecessor_block_name == "entry"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plans.front()
            .insertion_block_name == "holder.items.runtime_cleanup.entry"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plans.front()
            .continuation_block_name == "holder.items.runtime_cleanup.exit"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plans.front()
            .replaced_terminator_text == "br label %holder.items.runtime_cleanup.exit"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plans.front()
            .inserted_branch_text == "br label %holder.items.runtime_cleanup.entry"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plans.front()
            .continuation_block_text == "holder.items.runtime_cleanup.exit:\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plans.front()
            .candidate_cfg_line_count == 19
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plans.front()
            .candidate_cfg_lines.front() == "  br label %holder.items.runtime_cleanup.entry\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plans.front()
            .continuation_block_generated
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state.plans.front()
            .function_ir_unchanged
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .verification_metadata_available
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .verification_count == 1
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .all_functions_found
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .all_predecessor_blocks_found
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .all_insertion_blocks_absent
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
             .all_continuation_blocks_found
    );
    assert(!runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.all_verified);
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .all_candidate_insertion_blocks_found
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .all_candidate_continuation_blocks_found
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .all_candidates_verified
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .candidate_verified_count == 1
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .verified_count == 0
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
            .front()
            .verification_available
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
            .front()
            .function_symbol_name == "main"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
            .front()
            .predecessor_block_name == "entry"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
            .front()
            .insertion_block_name == "holder.items.runtime_cleanup.entry"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
            .front()
            .continuation_block_name == "holder.items.runtime_cleanup.exit"
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
            .front()
            .function_found
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
            .front()
            .predecessor_block_found
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
            .front()
            .insertion_block_absent
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
             .front()
             .continuation_block_found
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
            .front()
            .candidate_insertion_block_found
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
            .front()
            .candidate_continuation_block_found
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
            .front()
            .candidate_verified
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.verifications
             .front()
             .verified
    );
    assert(runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state.metadata_available);
    assert(runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state.candidate_count == 1);
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
             .any_candidate_available
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .available_candidate_count == 0
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .inserted_cfg_line_count == 18
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .candidate_function_line_count == 0
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state.candidates
            .front()
            .original_function_ir_text.empty()
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state.candidates
            .front()
            .candidate_function_ir_text.empty()
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state.candidates
            .front()
            .separate_from_module_ir
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
             .all_verified
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .verified_count == 0
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
             .rewrite_requested
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
            .candidate_count == 1
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
             .any_candidate_available
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
            .available_candidate_count == 0
    );
    assert(
        !runtime_indexed_cleanup_gate_on
             .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
             .all_verified
    );
    assert(
        runtime_indexed_cleanup_gate_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .verified_count == 0
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
             .verifications.front().verification_available
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
             .verifications.front().candidate_contains_cleanup_cfg_once
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .insertion_gate_ready
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .insertion_preview_ready
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .candidate_ready
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .candidate_verified
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .module_mutation_enabled
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .function_integration_ready
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .production_ready
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_mutation_state
            .mutation_requested
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_mutation_state
            .candidate_verified
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_mutation_state
            .mutation_applied
    );
    assert(
        !runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_mutation_state
            .module_matches_candidate
    );

    auto runtime_indexed_cleanup_insertion_gate_on = pipeline.emit_llvm(
        runtime_indexed_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
            .runtime_indexed_cleanup_module_ir_insertion_enabled = true,
        }
    );
    assert(runtime_indexed_cleanup_insertion_gate_on.has_errors());
    assert(
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_insertion_gate_state
            .insertion_requested
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_insertion_gate_state
            .artifact_available
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_insertion_gate_state
            .render_parity_verified
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_insertion_gate_state
            .insertion_enabled
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_insertion_gate_state
            .remains_separate_from_module_ir
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .preview_available
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .insertion_point_found
    );
    assert(
        !runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .would_modify_module_ir
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .original_module_line_count ==
        logical_line_count(runtime_indexed_cleanup_insertion_gate_on.ir_text)
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .insertion_line_index ==
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .original_module_line_count
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .inserted_ir_line_count == 17
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .projected_module_line_count ==
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .original_module_line_count + 17
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_candidate_state
            .candidate_available
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_candidate_state
            .separate_from_module_ir
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_candidate_state
            .original_module_line_count ==
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .original_module_line_count
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_candidate_state
            .inserted_ir_line_count == 17
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_candidate_state
            .candidate_module_line_count ==
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .projected_module_line_count
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_candidate_state
            .candidate_ir_text != runtime_indexed_cleanup_insertion_gate_on.ir_text
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_candidate_state
            .candidate_ir_text.find(
                runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_artifact_state
                    .rendered_ir_lines.front()
            ) != std::string::npos
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on
            .runtime_indexed_cleanup_module_ir_candidate_verification_state
            .verification_available
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on
            .runtime_indexed_cleanup_module_ir_candidate_verification_state
            .candidate_contains_cleanup_block_once
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on
            .runtime_indexed_cleanup_module_ir_candidate_verification_state
            .emitted_module_excludes_cleanup_block
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on
            .runtime_indexed_cleanup_module_ir_candidate_verification_state
            .candidate_cleanup_block_count == 1
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on
            .runtime_indexed_cleanup_module_ir_candidate_verification_state
            .emitted_module_cleanup_block_count == 0
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on
            .runtime_indexed_cleanup_module_ir_candidate_verification_state
            .verified
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .insertion_gate_ready
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .insertion_preview_ready
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .candidate_ready
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .candidate_verified
    );
    assert(
        !runtime_indexed_cleanup_insertion_gate_on
             .runtime_indexed_cleanup_module_ir_production_readiness_state
             .module_mutation_enabled
    );
    assert(
        !runtime_indexed_cleanup_insertion_gate_on
             .runtime_indexed_cleanup_module_ir_production_readiness_state
             .function_integration_ready
    );
    assert(
        !runtime_indexed_cleanup_insertion_gate_on
             .runtime_indexed_cleanup_module_ir_production_readiness_state
             .production_ready
    );
    assert(
        !runtime_indexed_cleanup_insertion_gate_on
             .runtime_indexed_cleanup_module_ir_mutation_state
             .mutation_requested
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on
            .runtime_indexed_cleanup_module_ir_mutation_state
            .candidate_verified
    );
    assert(
        !runtime_indexed_cleanup_insertion_gate_on
             .runtime_indexed_cleanup_module_ir_mutation_state
             .mutation_applied
    );
    assert(
        !runtime_indexed_cleanup_insertion_gate_on
             .runtime_indexed_cleanup_module_ir_mutation_state
             .module_matches_candidate
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on.ir_text.find(
            runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_artifact_state
                .rendered_ir_lines.front()
        ) == std::string::npos
    );

    auto runtime_indexed_cleanup_mutation_on = pipeline.emit_llvm(
        runtime_indexed_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
            .runtime_indexed_cleanup_module_ir_insertion_enabled = true,
            .runtime_indexed_cleanup_module_ir_mutation_enabled = true,
        }
    );
    assert(runtime_indexed_cleanup_mutation_on.has_errors());
    assert(
        runtime_indexed_cleanup_mutation_on.runtime_indexed_cleanup_module_ir_mutation_state
            .mutation_requested
    );
    assert(
        runtime_indexed_cleanup_mutation_on.runtime_indexed_cleanup_module_ir_mutation_state
            .candidate_verified
    );
    assert(
        runtime_indexed_cleanup_mutation_on.runtime_indexed_cleanup_module_ir_mutation_state
            .mutation_applied
    );
    assert(
        runtime_indexed_cleanup_mutation_on.runtime_indexed_cleanup_module_ir_mutation_state
            .module_matches_candidate
    );
    assert(
        runtime_indexed_cleanup_mutation_on.runtime_indexed_cleanup_module_ir_mutation_state
            .final_module_cleanup_block_count == 1
    );
    assert(
        runtime_indexed_cleanup_mutation_on.runtime_indexed_cleanup_module_ir_mutation_state
            .final_module_line_count ==
        runtime_indexed_cleanup_mutation_on.runtime_indexed_cleanup_module_ir_candidate_state
            .candidate_module_line_count
    );
    assert(
        runtime_indexed_cleanup_mutation_on.ir_text ==
        runtime_indexed_cleanup_mutation_on.runtime_indexed_cleanup_module_ir_candidate_state
            .candidate_ir_text
    );
    assert(
        runtime_indexed_cleanup_mutation_on.ir_text.find(
            runtime_indexed_cleanup_mutation_on.runtime_indexed_cleanup_module_ir_artifact_state
                .rendered_ir_lines.front()
        ) != std::string::npos
    );
    assert(
        runtime_indexed_cleanup_mutation_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .module_mutation_enabled
    );
    assert(
        !runtime_indexed_cleanup_mutation_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .function_integration_ready
    );
    assert(
        !runtime_indexed_cleanup_mutation_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .production_ready
    );
    assert(
        !runtime_indexed_cleanup_mutation_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .all_functions_found
    );
    assert(
        !runtime_indexed_cleanup_mutation_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
             .all_predecessor_blocks_found
    );
    assert(
        !runtime_indexed_cleanup_mutation_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
             .all_continuation_blocks_found
    );
    assert(!runtime_indexed_cleanup_mutation_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state.all_verified);

    auto runtime_indexed_cleanup_constructor_move_on = pipeline.emit_llvm(
        runtime_indexed_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
            .runtime_indexed_cleanup_module_ir_insertion_enabled = true,
            .runtime_indexed_cleanup_module_ir_mutation_enabled = true,
            .runtime_indexed_constructor_move_enabled = true,
        }
    );
    assert(!runtime_indexed_cleanup_constructor_move_on.has_errors());
    assert(runtime_indexed_cleanup_constructor_move_on.error_text.empty());
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_capability_state
            .capability_count == 1
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_emission_plan_state
            .any_structured_ir_plan_complete
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_module_ir_mutation_state
            .mutation_applied
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .all_functions_found
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .all_predecessor_blocks_found
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .all_insertion_blocks_absent
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state
            .any_continuation_block_generated
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state
            .candidate_cfg_line_count == 19
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .all_candidate_insertion_blocks_found
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .all_candidate_continuation_blocks_found
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .all_candidates_verified
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .candidate_verified_count == 1
    );
    assert(
        !runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
             .all_continuation_blocks_found
    );
    assert(
        !runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
             .all_verified
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .verifications.front().function_found
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .verifications.front().predecessor_block_found
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
            .verifications.front().insertion_block_absent
    );
    assert(
        !runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_cfg_rewrite_verification_state
             .verifications.front().continuation_block_found
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .metadata_available
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .candidate_count == 1
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .any_candidate_available
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .available_candidate_count == 1
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .all_candidates_separate_from_module_ir
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .any_function_ir_changed
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .inserted_cfg_line_count == 19
    );
    auto const& runtime_indexed_cleanup_function_candidate =
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .candidates.front();
    assert(runtime_indexed_cleanup_function_candidate.candidate_available);
    assert(runtime_indexed_cleanup_function_candidate.separate_from_module_ir);
    assert(runtime_indexed_cleanup_function_candidate.function_ir_changed);
    assert(runtime_indexed_cleanup_function_candidate.predecessor_terminator_replaced);
    assert(runtime_indexed_cleanup_function_candidate.original_function_line_count > 0);
    assert(
        runtime_indexed_cleanup_function_candidate.candidate_function_line_count ==
        runtime_indexed_cleanup_function_candidate.original_function_line_count + 19
    );
    assert(
        runtime_indexed_cleanup_function_candidate.candidate_function_ir_text !=
        runtime_indexed_cleanup_constructor_move_on.ir_text
    );
    assert(
        occurrence_count(
            runtime_indexed_cleanup_function_candidate.original_function_ir_text,
            "holder.items.runtime_cleanup.entry:\n"
        ) == 0
    );
    assert(
        occurrence_count(
            runtime_indexed_cleanup_function_candidate.candidate_function_ir_text,
            "holder.items.runtime_cleanup.entry:\n"
        ) == 1
    );
    assert(
        occurrence_count(
            runtime_indexed_cleanup_function_candidate.candidate_function_ir_text,
            "holder.items.runtime_cleanup.exit:\n"
        ) == 1
    );
    assert(
        occurrence_count(
            runtime_indexed_cleanup_function_candidate.original_function_ir_text,
            "  ret i32 0\n"
        ) == 1
    );
    assert(
        occurrence_count(
            runtime_indexed_cleanup_function_candidate.candidate_function_ir_text,
            "  br label %holder.items.runtime_cleanup.entry\n"
        ) == 1
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .verification_metadata_available
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .verification_count == 1
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .all_original_functions_exclude_cleanup_cfg
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .all_candidates_contain_cleanup_cfg_once
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .all_candidates_contain_continuation_once
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .all_original_predecessor_terminators_found
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .all_candidates_route_predecessors_to_cleanup
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .all_predecessor_terminators_replaced
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .all_candidate_functions_changed
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .all_candidates_separate_from_module_ir
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .all_verified
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .verified_count == 1
    );
    assert(
        !runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
             .rewrite_requested
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
            .candidate_count == 1
    );
    assert(
        !runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
             .any_candidate_available
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .verified_count == 0
    );
    auto runtime_indexed_cleanup_function_module_rewrite_on = pipeline.emit_llvm(
        runtime_indexed_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
            .runtime_indexed_cleanup_module_ir_insertion_enabled = true,
            .runtime_indexed_cleanup_module_ir_mutation_enabled = true,
            .runtime_indexed_cleanup_function_ir_module_rewrite_enabled = true,
            .runtime_indexed_constructor_move_enabled = true,
        }
    );
    assert(!runtime_indexed_cleanup_function_module_rewrite_on.has_errors());
    assert(runtime_indexed_cleanup_function_module_rewrite_on.error_text.empty());
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .all_verified
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
            .rewrite_requested
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
            .candidate_count == 1
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
            .any_candidate_available
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
            .available_candidate_count == 1
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
            .all_candidates_separate_from_module_ir
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
            .any_module_ir_changed
    );
    auto const& runtime_indexed_cleanup_function_module_candidate =
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state.candidates.front();
    auto const& runtime_indexed_cleanup_function_rewrite_candidate =
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_rewrite_candidate_state.candidates.front();
    assert(runtime_indexed_cleanup_function_module_candidate.rewrite_requested);
    assert(runtime_indexed_cleanup_function_module_candidate.function_candidate_verified);
    assert(runtime_indexed_cleanup_function_module_candidate.candidate_available);
    assert(runtime_indexed_cleanup_function_module_candidate.separate_from_module_ir);
    assert(runtime_indexed_cleanup_function_module_candidate.module_ir_changed);
    assert(runtime_indexed_cleanup_function_module_candidate.function_replacement_count == 1);
    assert(
        runtime_indexed_cleanup_function_module_candidate.candidate_module_ir_text !=
        runtime_indexed_cleanup_function_module_rewrite_on.ir_text
    );
    assert(
        runtime_indexed_cleanup_function_module_candidate.candidate_module_line_count ==
        runtime_indexed_cleanup_function_module_candidate.original_module_line_count + 19
    );
    assert(
        occurrence_count(
            runtime_indexed_cleanup_function_module_candidate.candidate_module_ir_text,
            runtime_indexed_cleanup_function_rewrite_candidate.candidate_function_ir_text
        ) == 1
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .all_verified
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .verified_count == 1
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .all_candidate_functions_match_verified_candidates
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .all_replacement_targets_unique
    );
    assert(
        !runtime_indexed_cleanup_function_module_rewrite_on.runtime_indexed_cleanup_module_ir_production_readiness_state
             .function_integration_ready
    );
    assert(
        !runtime_indexed_cleanup_function_module_rewrite_on.runtime_indexed_cleanup_module_ir_production_readiness_state
             .production_ready
    );
    assert(
        !runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .function_integration_ready
    );
    assert(
        !runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .production_ready
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_module_ir_mutation_state
            .final_module_cleanup_block_count == 1
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.ir_text.find(
            runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_module_ir_artifact_state
                .rendered_ir_lines.front()
        ) != std::string::npos
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .production_enabled
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_capability_state.capabilities.front()
            .production_enabled
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_audit_lines[1].find(
            "constructor-move enabled"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .ir_plan.complete
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .ir_plan.length_value_name == "%holder.items.runtime_cleanup.length"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .ir_plan.condition_block_name == "holder.items.runtime_cleanup.condition"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .ir_plan.drop_callee_name == "__orison_drop.Inner"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .ir_plan.deallocate_callee_name == "__orison_dynamic_array_deallocate"
    );
    assert(
        orison::lowering::render_runtime_indexed_cleanup_ir_plan(
            runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
                .ir_plan
        ) ==
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines.front() ==
        "  %holder.items.runtime_cleanup.length = load i64, ptr %holder.items.length\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[2] ==
        "holder.items.runtime_cleanup.condition:\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[4] ==
        "  %holder.items.runtime_cleanup.more = icmp ult i64 %holder.items.runtime_cleanup.index, "
        "%holder.items.runtime_cleanup.length\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[5] ==
        "  %holder.items.runtime_cleanup.skip_moved = icmp eq i64 "
        "%holder.items.runtime_cleanup.index, %index\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[6] ==
        "  br i1 %holder.items.runtime_cleanup.skip_moved, label "
        "%holder.items.runtime_cleanup.skip, label %holder.items.runtime_cleanup.drop\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[9] ==
        "  %holder.items.runtime_cleanup.element.addr = getelementptr Inner, ptr "
        "%holder.items.data, i64 %holder.items.runtime_cleanup.index\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[10] ==
        "  call void @__orison_drop.Inner(ptr %holder.items.runtime_cleanup.element.addr)\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[12] ==
        "holder.items.runtime_cleanup.continue:\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[13] ==
        "  %holder.items.runtime_cleanup.next_index = add i64 "
        "%holder.items.runtime_cleanup.index, 1\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[16] ==
        "  call void @__orison_dynamic_array_deallocate(ptr %holder.items)\n"
    );

    auto parsed_drop_readiness_path =
        std::filesystem::temp_directory_path() / "orison_pipeline_parsed_drop_readiness.or";
    {
        std::ofstream source(parsed_drop_readiness_path);
        source << "package demo.parseddropreadiness\n";
        source << "record Payload\n";
        source << "    public value: Int64\n";
        source << "interface Drop\n";
        source << "    function drop(this: exclusive This) -> Unit\n";
        source << "implements Transferable for Payload\n";
        source << "    function placeholder(this: shared This) -> Unit\n";
        source << "        return\n";
        source << "implements Drop for Payload\n";
        source << "    function drop(this: exclusive This) -> Unit\n";
        source << "        return\n";
        source << "function launch(value: Int64) -> Int64\n";
        source << "    let payload: Payload = Payload(value)\n";
        source << "    let worker = thread\n";
        source << "        payload.value\n";
        source << "\n";
        source << "    worker.join()\n";
    }
    auto parsed_drop_readiness = pipeline.emit_llvm(parsed_drop_readiness_path);
    assert(!parsed_drop_readiness.has_errors());
    assert(parsed_drop_readiness.semantic_drop_lowering_authorizations.size() == 1);
    assert(parsed_drop_readiness.semantic_drop_lowering_authorizations.front().semantic_resolved);
    assert(!parsed_drop_readiness.semantic_drop_lowering_authorizations.front().source_drop_lowering_enabled);
    assert(!parsed_drop_readiness.semantic_drop_lowering_authorizations.front().authorized);
    assert(parsed_drop_readiness.drop_readiness_blocker_summary.blocked_cleanups == 1);
    assert(parsed_drop_readiness.drop_readiness_blocker_summary.semantic_lowering_blockers.size() == 1);
    assert(parsed_drop_readiness.drop_readiness_blocker_summary.semantic_unresolved_blockers.empty());
    assert(parsed_drop_readiness.drop_readiness_blocker_summary.source_drop_lowering_blockers.size() == 1);
    assert(parsed_drop_readiness.drop_readiness_blocker_summary.missing_declarations.size() == 1);
    auto parsed_drop_readiness_blocker_report = drop_readiness_blocker_report(parsed_drop_readiness);
    assert(parsed_drop_readiness_blocker_report.size() == 4);
    assert(
        parsed_drop_readiness_blocker_report[0] ==
        "drop readiness blockers cleanups 1 semantic blockers 1 semantic unresolved 0 "
        "source lowering blocked 1 missing declarations 1"
    );
    assert(
        parsed_drop_readiness_blocker_report[2].find("source lowering not accepted") !=
        std::string::npos
    );
    auto parsed_drop_readiness_source_correlation_report =
        drop_readiness_source_correlation_report(parsed_drop_readiness);
    assert(parsed_drop_readiness_source_correlation_report.size() == 2);
    assert(
        parsed_drop_readiness_source_correlation_report[0] ==
        "drop readiness source correlations actions 1 semantic sites 1"
    );
    assert(
        parsed_drop_readiness_source_correlation_report[1].find("semantic resolved") !=
        std::string::npos
    );
    assert(parsed_drop_readiness.ir_text.find("call void @__orison_drop.Payload") == std::string::npos);

    auto resolved_semantic_drops = pipeline.analyze(
        semantic_drop_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_implementations = {
                orison::semantics::source_derived_drop_implementation(
                    "Payload",
                    3,
                    orison::semantics::DropImplementationBodySummary {
                        .finite = true,
                    }
                ),
            },
        }
    );
    assert(!resolved_semantic_drops.has_errors());
    auto resolved_semantic_drops_implementation_report =
        semantic_drop_implementation_discovery_report(resolved_semantic_drops.semantic_drop_state);
    auto resolved_semantic_drops_resolution_report = semantic_drop_resolution_report(resolved_semantic_drops);
    auto resolved_semantic_drops_diagnostic_report = semantic_drop_diagnostic_report(resolved_semantic_drops);
    auto resolved_semantic_drops_authorization_report =
        semantic_drop_lowering_authorization_report(resolved_semantic_drops);
    auto resolved_semantic_drops_summary_report = semantic_drop_resolution_summary_report(resolved_semantic_drops);
    assert(resolved_semantic_drops_implementation_report.size() == 1);
    assert_line_contains(resolved_semantic_drops_implementation_report, 0, "discovery test-injection");
    assert(resolved_semantic_drops_resolution_report.size() == 2);
    assert_line_contains(resolved_semantic_drops_resolution_report, 0, "resolved drop site");
    assert_line_contains(resolved_semantic_drops_resolution_report, 1, "owner local");
    assert(resolved_semantic_drops_diagnostic_report.size() == 2);
    assert_line_contains(resolved_semantic_drops_diagnostic_report, 0, "resolved");
    assert_line_contains(resolved_semantic_drops_diagnostic_report, 1, "owner local");
    assert(resolved_semantic_drops_authorization_report.size() == 2);
    assert(resolved_semantic_drops.semantic_drop_lowering_authorizations.size() == 2);
    assert(resolved_semantic_drops.semantic_drop_lowering_authorizations[0].semantic_resolved);
    assert(!resolved_semantic_drops.semantic_drop_lowering_authorizations[0].source_drop_lowering_enabled);
    assert(!resolved_semantic_drops.semantic_drop_lowering_authorizations[0].authorized);
    assert_line_contains(
        resolved_semantic_drops_authorization_report,
        0,
        "semantic-resolved lowering-blocked"
    );
    assert(resolved_semantic_drops_summary_report.size() == 1);
    assert_line_contains(resolved_semantic_drops_summary_report, 0, "resolved 2 missing 0");

    auto candidate_resolved_semantic_drops = pipeline.analyze(
        semantic_drop_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_implementation_candidates = {
                orison::semantics::DropImplementationCandidate {
                    .source_type_name = "Payload",
                    .declaration_line = 3,
                    .body = orison::semantics::DropImplementationBodySummary {
                        .finite = true,
                    },
                },
                orison::semantics::DropImplementationCandidate {
                    .source_type_name = "Payload",
                    .declaration_line = 4,
                    .body = orison::semantics::DropImplementationBodySummary {},
                },
            },
        }
    );
    assert(!candidate_resolved_semantic_drops.has_errors());
    auto candidate_resolved_semantic_drops_implementation_report =
        semantic_drop_implementation_discovery_report(candidate_resolved_semantic_drops.semantic_drop_state);
    auto candidate_resolved_semantic_drops_resolution_report =
        semantic_drop_resolution_report(candidate_resolved_semantic_drops);
    auto candidate_resolved_semantic_drops_summary_report =
        semantic_drop_resolution_summary_report(candidate_resolved_semantic_drops);
    assert(candidate_resolved_semantic_drops_implementation_report.size() == 1);
    assert_line_contains(
        candidate_resolved_semantic_drops_implementation_report,
        0,
        "discovery candidate-collection"
    );
    assert(candidate_resolved_semantic_drops_resolution_report.size() == 2);
    assert_line_contains(candidate_resolved_semantic_drops_resolution_report, 0, "resolved drop site");
    assert_line_contains(candidate_resolved_semantic_drops_resolution_report, 1, "owner local");
    assert(candidate_resolved_semantic_drops_summary_report.size() == 1);
    assert_line_contains(
        candidate_resolved_semantic_drops_summary_report,
        0,
        "resolved 2 missing 0"
    );

    auto unproven_semantic_drops = pipeline.analyze(
        semantic_drop_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_implementations = {
                orison::semantics::source_derived_drop_implementation(
                    "Payload",
                    3,
                    orison::semantics::DropImplementationBodySummary {}
                ),
            },
        }
    );
    assert(!unproven_semantic_drops.has_errors());
    auto unproven_semantic_drops_resolution_report = semantic_drop_resolution_report(unproven_semantic_drops);
    auto unproven_semantic_drops_diagnostic_report = semantic_drop_diagnostic_report(unproven_semantic_drops);
    assert(unproven_semantic_drops_resolution_report.size() == 2);
    assert_line_contains(unproven_semantic_drops_resolution_report, 0, "missing drop site");
    assert_line_contains(unproven_semantic_drops_resolution_report, 1, "owner local");
    assert(unproven_semantic_drops_diagnostic_report.size() == 2);
    assert_line_contains(unproven_semantic_drops_diagnostic_report, 0, "discovered but unproven");
    assert_line_contains(unproven_semantic_drops_diagnostic_report, 1, "owner local");

    auto partial_drop_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" / "semantic_partial_drop_resolution.or";
    auto partial_semantic_drops = pipeline.analyze(
        partial_drop_path,
        orison::pipeline::CompilePipelineOptions {
            .test_only_semantic_drop_implementations = {
                orison::semantics::source_derived_drop_implementation(
                    "Payload",
                    3,
                    orison::semantics::DropImplementationBodySummary {
                        .finite = true,
                    }
                ),
            },
        }
    );
    assert(!partial_semantic_drops.has_errors());
    auto partial_semantic_drops_resolution_report = semantic_drop_resolution_report(partial_semantic_drops);
    auto partial_semantic_drops_summary_report = semantic_drop_resolution_summary_report(partial_semantic_drops);
    assert(partial_semantic_drops_resolution_report.size() == 4);
    assert_line_contains(partial_semantic_drops_resolution_report, 0, "resolved drop site");
    assert_line_contains(partial_semantic_drops_resolution_report, 1, "__orison_drop.Resource");
    assert_line_contains(partial_semantic_drops_resolution_report, 2, "owner local_payload");
    assert_line_contains(partial_semantic_drops_resolution_report, 3, "owner local_resource");
    assert(partial_semantic_drops_summary_report.size() == 2);
    assert_line_contains(partial_semantic_drops_summary_report, 0, "resolved 2 missing 0");
    assert_line_contains(partial_semantic_drops_summary_report, 1, "resolved 0 missing 2");
    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
