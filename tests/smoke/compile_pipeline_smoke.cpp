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
#include "orison/pipeline/runtime_indexed_member_cleanup_match_key.hpp"
#include "orison/pipeline/runtime_indexed_member_cleanup_execution_summary.hpp"
#include "orison/pipeline/runtime_indexed_member_cleanup_readiness_report.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <utility>
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

auto dynamic_array_payload_lifetime_plan_count(
    orison::pipeline::CompilePipelineResult const& result
) -> std::size_t {
    return std::count_if(
        result.dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
        result.dynamic_array_descriptor_lifetime_plan_state.plans.end(),
        [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
            return plan.source_type_name == "DynamicArray<Payload>" &&
                plan.cleanup_plan_available;
        }
    );
}

auto dynamic_array_payload_parameter_lifetime_plan_count(
    orison::pipeline::CompilePipelineResult const& result,
    std::string_view owner_name
) -> std::size_t {
    return std::count_if(
        result.dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
        result.dynamic_array_descriptor_lifetime_plan_state.plans.end(),
        [&](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
            return plan.owner_name == owner_name &&
                plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::parameter_binding &&
                plan.cleanup_responsibility == "callee-owned-parameter-cleanup";
        }
    );
}

void assert_dynamic_array_payload_returned_lifetime_owner(
    orison::pipeline::CompilePipelineResult const& result,
    std::string_view owner_name
) {
    assert(
        std::any_of(
            result.dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
            result.dynamic_array_descriptor_lifetime_plan_state.plans.end(),
            [&](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
                return plan.owner_name == owner_name &&
                    plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::returned_binding &&
                    plan.cleanup_responsibility == "caller-owned-returned-cleanup";
            }
        )
    );
}

void assert_dynamic_array_payload_cleanup_ready(
    orison::pipeline::CompilePipelineResult const& result
) {
    assert(result.dynamic_array_descriptor_lifetime_plan_state.origin_blockers.empty());
    assert(result.dynamic_array_cleanup_capability_proven);
    assert(result.dynamic_array_cleanup_emission_capability_state.proven);
    assert(
        orison::pipeline::dynamic_array_cleanup_production_ready(
            result.dynamic_array_cleanup_production_readiness
        )
    );
}

void assert_no_deallocate_after_function(
    std::string const& ir_text,
    std::string_view function_header
) {
    auto const function_start = ir_text.find(function_header);
    assert(function_start != std::string::npos);
    assert(ir_text.find("__orison_dynamic_array_deallocate", function_start) == std::string::npos);
}

void assert_ir_contains(std::string const& ir_text, std::string_view expected_fragment) {
    if (ir_text.find(expected_fragment) == std::string::npos) {
        std::cerr << "expected IR to contain '" << expected_fragment << "'\n";
    }
    assert(ir_text.find(expected_fragment) != std::string::npos);
}

void assert_ir_excludes(std::string const& ir_text, std::string_view unexpected_fragment) {
    if (ir_text.find(unexpected_fragment) != std::string::npos) {
        std::cerr << "expected IR to exclude '" << unexpected_fragment << "'\n";
    }
    assert(ir_text.find(unexpected_fragment) == std::string::npos);
}

void assert_branch_local_named_dynamic_array_cleanup_ir(
    std::string const& ir_text,
    std::string_view left_owner,
    std::string_view right_owner,
    std::string_view phi_type
) {
    auto left_cleanup = std::string {left_owner} + ".dynamic_array_cleanup";
    auto right_cleanup = std::string {right_owner} + ".dynamic_array_cleanup";
    assert_ir_contains(ir_text, left_cleanup);
    assert_ir_contains(ir_text, right_cleanup);
    assert_ir_contains(ir_text, "call void @__orison_drop.Payload(ptr %" + left_cleanup);
    assert_ir_contains(ir_text, "call void @__orison_drop.Payload(ptr %" + right_cleanup);
    assert_ir_contains(ir_text, "call void @__orison_dynamic_array_deallocate(ptr %" + left_cleanup);
    assert_ir_contains(ir_text, "call void @__orison_dynamic_array_deallocate(ptr %" + right_cleanup);
    assert_ir_contains(ir_text, "phi " + std::string {phi_type} + " [0, %" + left_cleanup);
    assert_ir_contains(ir_text, "[1, %" + right_cleanup);
}

void assert_branch_local_scratch_dynamic_array_cleanup_ir(
    std::string const& ir_text
) {
    assert_ir_contains(ir_text, "scratch.dynamic_array_cleanup");
    assert_ir_contains(ir_text, "call void @__orison_drop.Payload(ptr %scratch.dynamic_array_cleanup");
    assert_ir_contains(ir_text, "call void @__orison_dynamic_array_deallocate(ptr %scratch.dynamic_array_cleanup");
    assert_ir_contains(ir_text, "phi { ptr, i64, i64 } [%tmp");
    assert_ir_contains(ir_text, "%scratch.dynamic_array_cleanup");
    assert_ir_excludes(ir_text, "returned.dynamic_array_cleanup");
}

void assert_branch_local_returned_dynamic_array_cleanup_ir(
    std::string const& ir_text
) {
    assert_ir_contains(ir_text, "left_values.dynamic_array_cleanup");
    assert_ir_contains(ir_text, "right_values.dynamic_array_cleanup");
    assert_ir_contains(ir_text, "call void @__orison_drop.Payload(ptr %left_values.dynamic_array_cleanup");
    assert_ir_contains(ir_text, "call void @__orison_drop.Payload(ptr %right_values.dynamic_array_cleanup");
    assert_ir_contains(ir_text, "call void @__orison_dynamic_array_deallocate(ptr %left_values.dynamic_array_cleanup");
    assert_ir_contains(ir_text, "call void @__orison_dynamic_array_deallocate(ptr %right_values.dynamic_array_cleanup");
    assert_ir_contains(ir_text, "phi { ptr, i64, i64 } [%tmp");
    assert_ir_excludes(ir_text, "if branch ownership mismatch");
    assert_ir_excludes(ir_text, "switch case ownership mismatch");
}

void assert_branch_local_dynamic_array_cleanup_for_owners_ir(
    std::string const& ir_text,
    std::initializer_list<std::string_view> owner_names
) {
    for (auto const owner_name : owner_names) {
        auto cleanup_name = std::string {owner_name} + ".dynamic_array_cleanup";
        assert_ir_contains(ir_text, cleanup_name);
        assert_ir_contains(ir_text, "call void @__orison_drop.Payload(ptr %" + cleanup_name);
        assert_ir_contains(ir_text, "call void @__orison_dynamic_array_deallocate(ptr %" + cleanup_name);
    }
}

void assert_no_branch_local_dynamic_array_cleanup_for_owners_ir(
    std::string const& ir_text,
    std::initializer_list<std::string_view> owner_names
) {
    for (auto const owner_name : owner_names) {
        assert_ir_excludes(ir_text, std::string {owner_name} + ".dynamic_array_cleanup");
    }
}

void assert_branch_local_three_case_returned_dynamic_array_cleanup_ir(
    std::string const& ir_text
) {
    assert_branch_local_dynamic_array_cleanup_for_owners_ir(
        ir_text,
        {"first_scratch", "second_scratch", "third_scratch"}
    );
    assert_ir_contains(ir_text, "phi { ptr, i64, i64 } [%tmp");
    assert_no_branch_local_dynamic_array_cleanup_for_owners_ir(
        ir_text,
        {"first_values", "second_values", "third_values"}
    );
    assert_ir_excludes(ir_text, "switch case ownership mismatch");
}

void assert_branch_local_three_case_mixed_switch_dynamic_array_cleanup_ir(
    std::string const& ir_text
) {
    assert_branch_local_dynamic_array_cleanup_for_owners_ir(
        ir_text,
        {"first_scratch", "left_scratch", "right_scratch", "third_scratch"}
    );
    assert_ir_contains(ir_text, "phi { ptr, i64, i64 } [%tmp");
    assert_no_branch_local_dynamic_array_cleanup_for_owners_ir(
        ir_text,
        {"first_values", "left_values", "right_values", "third_values"}
    );
    assert_ir_excludes(ir_text, "if branch ownership mismatch");
    assert_ir_excludes(ir_text, "switch case ownership mismatch");
}

void assert_branch_local_multi_nested_switch_dynamic_array_cleanup_ir(
    std::string const& ir_text
) {
    assert_branch_local_dynamic_array_cleanup_for_owners_ir(
        ir_text,
        {
            "first_left_scratch",
            "first_right_scratch",
            "second_left_scratch",
            "second_right_scratch",
            "third_scratch",
        }
    );
    assert_ir_contains(ir_text, "phi { ptr, i64, i64 } [%tmp");
    assert_no_branch_local_dynamic_array_cleanup_for_owners_ir(
        ir_text,
        {
            "first_left_values",
            "first_right_values",
            "second_left_values",
            "second_right_values",
            "third_values",
        }
    );
    assert_ir_excludes(ir_text, "switch case ownership mismatch");
}

void assert_branch_local_if_two_switches_dynamic_array_cleanup_ir(
    std::string const& ir_text
) {
    assert_branch_local_dynamic_array_cleanup_for_owners_ir(
        ir_text,
        {
            "first_left_scratch",
            "first_right_scratch",
            "second_left_scratch",
            "second_right_scratch",
        }
    );
    assert_ir_contains(ir_text, "phi { ptr, i64, i64 } [%tmp");
    assert_no_branch_local_dynamic_array_cleanup_for_owners_ir(
        ir_text,
        {
            "first_left_values",
            "first_right_values",
            "second_left_values",
            "second_right_values",
        }
    );
    assert_ir_excludes(ir_text, "if branch ownership mismatch");
    assert_ir_excludes(ir_text, "switch case ownership mismatch");
}

void assert_branch_local_switch_two_ifs_dynamic_array_cleanup_ir(
    std::string const& ir_text
) {
    assert_branch_local_dynamic_array_cleanup_for_owners_ir(
        ir_text,
        {
            "first_left_scratch",
            "first_right_scratch",
            "second_left_scratch",
            "second_right_scratch",
            "third_scratch",
        }
    );
    assert_ir_contains(ir_text, "phi { ptr, i64, i64 } [%tmp");
    assert_no_branch_local_dynamic_array_cleanup_for_owners_ir(
        ir_text,
        {
            "first_left_values",
            "first_right_values",
            "second_left_values",
            "second_right_values",
            "third_values",
        }
    );
    assert_ir_excludes(ir_text, "if branch ownership mismatch");
    assert_ir_excludes(ir_text, "switch case ownership mismatch");
}

void assert_emit_object_link_run_success(
    orison::pipeline::CompilePipeline& pipeline,
    std::filesystem::path const& source_path,
    std::filesystem::path const& executable_path
) {
    auto object = pipeline.emit_object(
        source_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!object.has_errors());
    assert(!object.object_bytes.empty());
    auto link = orison::link::HostLinker {}.link(object.object_bytes, executable_path);
    assert(!link.has_errors());
    auto status = std::system(executable_path.string().c_str());
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);
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

    auto rejected_source_path = smoke_temp_root / "aggregate_projection_access_plan_state_rejected.or";
    {
        auto output = std::ofstream(rejected_source_path);
        output <<
            "package smoke.aggregate_access_plan_state_rejected\n"
            "\n"
            "record Payload\n"
            "    public value: UInt32\n"
            "\n"
            "record Box\n"
            "    public payload: Payload\n"
            "\n"
            "function main() -> Payload\n"
            "    let box: Box = Box(Payload(13 as UInt32))\n"
            "    box.payload\n";
    }

    auto rejected_result = pipeline.emit_llvm(
        rejected_source_path,
        orison::pipeline::CompilePipelineOptions {
            .collect_aggregate_projection_access_metadata = true,
        }
    );
    assert(rejected_result.has_errors());
    auto const& rejected_state = rejected_result.aggregate_projection_access_plan_state;
    assert(rejected_state.access_plans_available);
    assert(rejected_state.plan_count == 1);
    assert(rejected_state.allowed_count == 0);
    assert(rejected_state.blocked_count == 1);
    assert(rejected_state.receiver_projection_count == 0);
    assert(rejected_state.function_symbol_names.size() == 1);
    assert(rejected_state.function_symbol_names[0] == "main");
    assert(rejected_state.intents[0] == orison::lowering::AggregateProjectionAccessIntent::value_read);
    assert(
        rejected_state.statuses[0] ==
        orison::lowering::AggregateProjectionAccessStatus::requires_explicit_boundary
    );
    assert(rejected_state.binding_names[0] == "box.payload");
    assert(rejected_state.source_type_names[0] == "Payload");
    assert(
        rejected_state.diagnostics[0] ==
        "aggregate path read of owned projection requires an explicit ownership transfer"
    );
    assert(!rejected_state.receiver_projections[0]);

    auto receiver_source_path = smoke_temp_root / "aggregate_projection_access_plan_state_receiver.or";
    {
        auto output = std::ofstream(receiver_source_path);
        output <<
            "package smoke.aggregate_access_plan_state_receiver\n"
            "\n"
            "record Payload\n"
            "    public value: UInt32\n"
            "\n"
            "record Box\n"
            "    public payload: Payload\n"
            "\n"
            "extend Box\n"
            "    function payload(this: shared This) -> Payload\n"
            "        this.payload\n"
            "\n"
            "function main() -> UInt32\n"
            "    let box: Box = Box(Payload(13 as UInt32))\n"
            "    let payload: Payload = box.payload()\n"
            "    payload.value\n";
    }

    auto receiver_result = pipeline.emit_llvm(
        receiver_source_path,
        orison::pipeline::CompilePipelineOptions {
            .collect_aggregate_projection_access_metadata = true,
        }
    );
    assert(!receiver_result.has_errors());
    auto const& receiver_state = receiver_result.aggregate_projection_access_plan_state;
    assert(receiver_state.access_plans_available);
    assert(receiver_state.plan_count == 2);
    assert(receiver_state.allowed_count == 2);
    assert(receiver_state.blocked_count == 0);
    assert(receiver_state.receiver_projection_count == 1);
    assert(receiver_state.function_symbol_names.size() == 2);
    assert(receiver_state.function_symbol_names[0] == "main");
    assert(receiver_state.intents[0] == orison::lowering::AggregateProjectionAccessIntent::value_read);
    assert(receiver_state.statuses[0] == orison::lowering::AggregateProjectionAccessStatus::non_owned_projection);
    assert(receiver_state.binding_names[0] == "payload.value");
    assert(receiver_state.source_type_names[0] == "UInt32");
    assert(receiver_state.diagnostics[0].empty());
    assert(!receiver_state.receiver_projections[0]);
    assert(receiver_state.function_symbol_names[1] == "method.Box.payload");
    assert(receiver_state.intents[1] == orison::lowering::AggregateProjectionAccessIntent::value_read);
    assert(receiver_state.statuses[1] == orison::lowering::AggregateProjectionAccessStatus::allowed);
    assert(receiver_state.binding_names[1] == "this.payload");
    assert(receiver_state.source_type_names[1] == "Payload");
    assert(receiver_state.diagnostics[1].empty());
    assert(receiver_state.receiver_projections[1]);
}

auto ready_runtime_indexed_cleanup_module_ir_production_readiness(
    orison::lowering::RuntimeIndexedCleanupEmissionPlan plan,
    std::string function_symbol_name = "main",
    std::size_t source_line = 44
) -> orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessState {
    auto gated_ir_slice_line_count = plan.gated_ir_slice_lines.size();
    return orison::pipeline::runtime_indexed_cleanup_module_ir_production_readiness_state(
        orison::pipeline::RuntimeIndexedCleanupEmissionPlanState {
            .plans = {std::move(plan)},
            .plan_metadata_available = true,
            .all_prerequisites_ready = true,
            .any_production_gate_requested = true,
            .any_production_enabled = true,
            .gated_ir_slice_line_count = gated_ir_slice_line_count,
            .structured_ir_plan_count = 1,
        },
        orison::pipeline::RuntimeIndexedCleanupModuleIrInsertionGateState {
            .insertion_requested = true,
            .artifact_available = true,
            .render_parity_verified = true,
            .insertion_enabled = true,
        },
        orison::pipeline::RuntimeIndexedCleanupModuleIrInsertionPreviewState {
            .preview_available = true,
            .insertion_point_found = true,
            .would_modify_module_ir = true,
        },
        orison::pipeline::RuntimeIndexedCleanupModuleIrCandidateState {
            .candidate_available = true,
            .separate_from_module_ir = true,
        },
        orison::pipeline::RuntimeIndexedCleanupModuleIrCandidateVerificationState {
            .verification_available = true,
            .candidate_contains_cleanup_block_once = true,
            .emitted_module_excludes_cleanup_block = true,
            .verified = true,
            .candidate_cleanup_block_count = 1,
        },
        orison::pipeline::RuntimeIndexedCleanupModuleIrMutationState {
            .mutation_requested = true,
            .candidate_verified = true,
            .mutation_applied = true,
            .module_matches_candidate = true,
            .final_module_cleanup_block_count = 1,
        },
        orison::pipeline::RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateVerificationState {
            .verifications = {
                orison::pipeline::RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateVerification {
                    .function_symbol_name = std::move(function_symbol_name),
                    .verification_available = true,
                    .candidate_function_found = true,
                    .candidate_function_matches_verified_candidate = true,
                    .replacement_target_unique = true,
                    .module_ir_changed = true,
                    .separate_from_module_ir = true,
                    .llvm_verifier_ran = true,
                    .llvm_verifier_passed = true,
                    .verified = true,
                    .source_line = source_line,
                    .function_replacement_count = 1,
                },
            },
            .verification_metadata_available = true,
            .all_candidate_functions_found = true,
            .all_candidate_functions_match_verified_candidates = true,
            .all_replacement_targets_unique = true,
            .all_module_ir_changed = true,
            .all_candidates_separate_from_module_ir = true,
            .same_function_splice_ranges_non_overlapping = true,
            .any_llvm_verifier_ran = true,
            .all_llvm_verifier_passed = true,
            .all_verified = true,
            .verification_count = 1,
            .verified_count = 1,
            .llvm_verified_count = 1,
        },
        orison::pipeline::RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState {
            .mutation_requested = true,
            .candidate_verified = true,
            .replacement_targets_unique = true,
            .mutation_applied = true,
            .module_matches_candidate = true,
            .llvm_verifier_passed = true,
            .candidate_count = 1,
        }
    );
}

auto inline_runtime_indexed_cleanup_ir_shape_plan()
    -> orison::lowering::RuntimeIndexedCleanupEmissionPlan {
    return orison::lowering::RuntimeIndexedCleanupEmissionPlan {
        .function_symbol_name = "main",
        .owner_name = "items",
        .gated_ir_slice_lines = {
            "  br label %items.runtime_cleanup.condition\n",
            "items.runtime_cleanup.condition:\n",
            "  %items.runtime_cleanup.index = phi i64 [ 0, %entry ], "
                "[ %items.runtime_cleanup.next, %items.runtime_cleanup.continue ]\n",
            "  %items.runtime_cleanup.bounds = icmp ult i64 %items.runtime_cleanup.index, 2\n",
            "  br i1 %items.runtime_cleanup.bounds, "
                "label %items.runtime_cleanup.live, label %items.runtime_cleanup.exit\n",
            "items.runtime_cleanup.live:\n",
            "  %items.runtime_cleanup.skip = icmp eq i64 %items.runtime_cleanup.index, %index\n",
            "  br i1 %items.runtime_cleanup.skip, "
                "label %items.runtime_cleanup.skip, label %items.runtime_cleanup.drop\n",
            "items.runtime_cleanup.skip:\n",
            "  br label %items.runtime_cleanup.continue\n",
            "items.runtime_cleanup.drop:\n",
            "  %items.runtime_cleanup.element.addr = getelementptr [2 x %record.Inner], "
                "ptr %items.addr, i64 0, i64 %items.runtime_cleanup.index\n",
            "  call void @__orison_drop.Inner(ptr %items.runtime_cleanup.element.addr)\n",
            "  store %record.Inner zeroinitializer, ptr %items.runtime_cleanup.element.addr\n",
            "  br label %items.runtime_cleanup.continue\n",
            "items.runtime_cleanup.continue:\n",
            "  %items.runtime_cleanup.next = add i64 %items.runtime_cleanup.index, 1\n",
            "  br label %items.runtime_cleanup.condition\n",
            "items.runtime_cleanup.exit:\n",
            "  ret void\n",
        },
        .ir_plan = orison::lowering::RuntimeIndexedCleanupIrPlan {
            .owner_name = "items",
            .element_llvm_type_name = "%record.Inner",
            .owner_llvm_type_name = "[2 x %record.Inner]",
            .owner_address_name = "%items.addr",
            .condition_block_name = "items.runtime_cleanup.condition",
            .cleanup_index_name = "%items.runtime_cleanup.index",
            .bounds_check_name = "%items.runtime_cleanup.bounds",
            .live_check_block_name = "items.runtime_cleanup.live",
            .skip_check_name = "%items.runtime_cleanup.skip",
            .skip_block_name = "items.runtime_cleanup.skip",
            .drop_block_name = "items.runtime_cleanup.drop",
            .element_address_name = "%items.runtime_cleanup.element.addr",
            .drop_callee_name = "__orison_drop.Inner",
            .continue_block_name = "items.runtime_cleanup.continue",
            .next_index_name = "%items.runtime_cleanup.next",
            .exit_block_name = "items.runtime_cleanup.exit",
            .complete = true,
        },
    };
}

auto inline_runtime_indexed_cleanup_ir_shape_plan_without_zero_store()
    -> orison::lowering::RuntimeIndexedCleanupEmissionPlan {
    auto plan = inline_runtime_indexed_cleanup_ir_shape_plan();
    plan.gated_ir_slice_lines.erase(
        std::remove_if(
            plan.gated_ir_slice_lines.begin(),
            plan.gated_ir_slice_lines.end(),
            [](std::string const& line) {
                return line.find("zeroinitializer") != std::string::npos;
            }
        ),
        plan.gated_ir_slice_lines.end()
    );
    return plan;
}

auto descriptor_runtime_indexed_cleanup_ir_shape_plan()
    -> orison::lowering::RuntimeIndexedCleanupEmissionPlan {
    return orison::lowering::RuntimeIndexedCleanupEmissionPlan {
        .function_symbol_name = "main",
        .owner_name = "items",
        .gated_ir_slice_lines = {
            "  %items.runtime_cleanup.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n",
            "  %items.runtime_cleanup.data = extractvalue { ptr, i64, i64 } "
                "%items.runtime_cleanup.descriptor, 0\n",
            "  %items.runtime_cleanup.length = extractvalue { ptr, i64, i64 } "
                "%items.runtime_cleanup.descriptor, 1\n",
            "  %items.runtime_cleanup.capacity = extractvalue { ptr, i64, i64 } "
                "%items.runtime_cleanup.descriptor, 2\n",
            "  br label %items.runtime_cleanup.condition\n",
            "items.runtime_cleanup.condition:\n",
            "  %items.runtime_cleanup.index = phi i64 [ 0, %entry ], "
                "[ %items.runtime_cleanup.next, %items.runtime_cleanup.continue ]\n",
            "  %items.runtime_cleanup.bounds = icmp ult i64 "
                "%items.runtime_cleanup.index, %items.runtime_cleanup.length\n",
            "  br i1 %items.runtime_cleanup.bounds, "
                "label %items.runtime_cleanup.live, label %items.runtime_cleanup.exit\n",
            "items.runtime_cleanup.live:\n",
            "  %items.runtime_cleanup.skip = icmp eq i64 %items.runtime_cleanup.index, %index\n",
            "  br i1 %items.runtime_cleanup.skip, "
                "label %items.runtime_cleanup.skip, label %items.runtime_cleanup.drop\n",
            "items.runtime_cleanup.skip:\n",
            "  br label %items.runtime_cleanup.continue\n",
            "items.runtime_cleanup.drop:\n",
            "  %items.runtime_cleanup.element.addr = getelementptr %record.Inner, "
                "ptr %items.runtime_cleanup.data, i64 %items.runtime_cleanup.index\n",
            "  call void @__orison_drop.Inner(ptr %items.runtime_cleanup.element.addr)\n",
            "  br label %items.runtime_cleanup.continue\n",
            "items.runtime_cleanup.continue:\n",
            "  %items.runtime_cleanup.next = add i64 %items.runtime_cleanup.index, 1\n",
            "  br label %items.runtime_cleanup.condition\n",
            "items.runtime_cleanup.exit:\n",
            "  call void @__orison_dynamic_array_deallocate("
                "ptr %items.runtime_cleanup.data, i64 4, i64 %items.runtime_cleanup.capacity)\n",
            "  ret void\n",
        },
        .ir_plan = orison::lowering::RuntimeIndexedCleanupIrPlan {
            .owner_name = "items",
            .element_llvm_type_name = "%record.Inner",
            .owner_llvm_type_name = "{ ptr, i64, i64 }",
            .owner_address_name = "%items.addr",
            .descriptor_value_name = "%items.runtime_cleanup.descriptor",
            .descriptor_data_value_name = "%items.runtime_cleanup.data",
            .descriptor_capacity_value_name = "%items.runtime_cleanup.capacity",
            .length_value_name = "%items.runtime_cleanup.length",
            .condition_block_name = "items.runtime_cleanup.condition",
            .cleanup_index_name = "%items.runtime_cleanup.index",
            .bounds_check_name = "%items.runtime_cleanup.bounds",
            .live_check_block_name = "items.runtime_cleanup.live",
            .skip_check_name = "%items.runtime_cleanup.skip",
            .skip_block_name = "items.runtime_cleanup.skip",
            .drop_block_name = "items.runtime_cleanup.drop",
            .element_address_name = "%items.runtime_cleanup.element.addr",
            .drop_callee_name = "__orison_drop.Inner",
            .continue_block_name = "items.runtime_cleanup.continue",
            .next_index_name = "%items.runtime_cleanup.next",
            .exit_block_name = "items.runtime_cleanup.exit",
            .deallocate_callee_name = "__orison_dynamic_array_deallocate",
            .complete = true,
        },
    };
}

auto descriptor_runtime_indexed_cleanup_ir_shape_plan_without_deallocate_tail()
    -> orison::lowering::RuntimeIndexedCleanupEmissionPlan {
    auto plan = descriptor_runtime_indexed_cleanup_ir_shape_plan();
    plan.gated_ir_slice_lines.erase(
        std::remove_if(
            plan.gated_ir_slice_lines.begin(),
            plan.gated_ir_slice_lines.end(),
            [](std::string const& line) {
                return line.find("__orison_dynamic_array_deallocate") != std::string::npos;
            }
        ),
        plan.gated_ir_slice_lines.end()
    );
    return plan;
}

void assert_runtime_indexed_cleanup_ir_shape_blocked_with_ready_upstream(
    orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessState const& readiness
) {
    assert(!readiness.production_ready);
    assert(!readiness.ir_shape_ready);
    assert(readiness.insertion_gate_ready);
    assert(readiness.insertion_preview_ready);
    assert(readiness.candidate_ready);
    assert(readiness.candidate_verified);
    assert(readiness.module_mutation_enabled);
    assert(readiness.function_integration_ready);
    assert(readiness.function_splice_conflict_free);
    assert(readiness.blockers.size() == 1);
    assert(
        readiness.blockers.front().kind ==
        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::IrShape
    );
    assert(
        readiness.diagnostic_blocker_kind ==
        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::IrShape
    );
    assert(readiness.diagnostic_function_symbol_name == "main");
    assert(readiness.diagnostic_source_line == 44);
    assert(readiness.diagnostic_text == "runtime-index cleanup blocked: cleanup ir shape blocked");
}

struct RuntimeIndexedConstructorMoveIrShapeFaultExpectation {
    orison::pipeline::RuntimeIndexedCleanupIrShapeFaultInjection fault =
        orison::pipeline::RuntimeIndexedCleanupIrShapeFaultInjection::None;
    std::string_view expected_shape_detail;
    std::string_view expected_ir_shape;
};

auto runtime_indexed_constructor_move_shape_fault_options(
    orison::pipeline::RuntimeIndexedCleanupIrShapeFaultInjection fault
) -> orison::pipeline::CompilePipelineOptions {
    return orison::pipeline::CompilePipelineOptions {
        .source_drop_lowering_enabled = true,
        .collect_runtime_indexed_cleanup_audit = true,
        .runtime_indexed_cleanup_emission_enabled = true,
        .runtime_indexed_cleanup_verified_function_ir_rewrite_enabled = true,
        .runtime_indexed_constructor_move_enabled = true,
        .test_only_runtime_indexed_cleanup_ir_shape_fault = fault,
        .dynamic_array_production_construction_lowering_enabled = true,
        .dynamic_array_production_index_lowering_enabled = true,
        .dynamic_array_production_append_lowering_enabled = true,
    };
}

void assert_runtime_indexed_constructor_move_shape_faults(
    orison::pipeline::CompilePipeline& pipeline,
    std::filesystem::path const& path,
    std::vector<RuntimeIndexedConstructorMoveIrShapeFaultExpectation> const& expectations
) {
    for (auto const& expectation : expectations) {
        auto result = pipeline.emit_llvm(
            path,
            runtime_indexed_constructor_move_shape_fault_options(expectation.fault)
        );
        assert(!result.runtime_indexed_cleanup_module_ir_production_readiness_state.ir_shape_ready);
        assert(!result.runtime_indexed_cleanup_module_ir_production_readiness_state.production_ready);
        auto const promotion_state = orison::pipeline::runtime_indexed_member_cleanup_promotion_state(result);
        assert(!promotion_state.module_ir_shape_ready);
        assert(promotion_state.module_ir_shape_blocker_detail == expectation.expected_shape_detail);
        auto const ir_shape_lines =
            orison::pipeline::runtime_indexed_constructor_move_ir_shape_report_lines(
                result.runtime_indexed_cleanup_emission_plan_state
            );
        assert(ir_shape_lines.size() == 1);
        assert(ir_shape_lines.front() == expectation.expected_ir_shape);
    }
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
        "dynamic array descriptor origin DynamicArray<Payload> owner items element Payload at line 6 origin parameter "
        "(metadata only)"
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
    assert(dynamic_array_bound_descriptor.dynamic_array_descriptor_lifetime_plan_state.plans.size() == 1);
    assert(dynamic_array_bound_descriptor.dynamic_array_descriptor_lifetime_plan_state.all_origins_have_cleanup_plans);
    assert(dynamic_array_bound_descriptor.dynamic_array_descriptor_lifetime_plan_state.all_cleanup_plans_have_origins);
    assert(dynamic_array_bound_descriptor.dynamic_array_descriptor_lifetime_plan_state.origin_blockers.empty());
    assert(
        dynamic_array_bound_descriptor.dynamic_array_descriptor_lifetime_plan_state.plans.front().origin_kind ==
        orison::semantics::DynamicArrayDescriptorOriginKind::parameter_binding
    );
    assert(
        dynamic_array_bound_descriptor.dynamic_array_descriptor_lifetime_plan_state.plans.front()
            .cleanup_responsibility == "callee-owned-parameter-cleanup"
    );
    assert(
        dynamic_array_bound_descriptor.dynamic_array_descriptor_lifetime_plan_state.plans.front()
            .descriptor_storage_status ==
        orison::lowering::DynamicArrayDescriptorStorageStatus::bound_parameter_descriptor
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
    assert(dynamic_array_owned_parameter_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.size() == 3);
    auto const forwarding_parameter_lifetime_plans = std::count_if(
        dynamic_array_owned_parameter_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
        dynamic_array_owned_parameter_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.end(),
        [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
            return plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::parameter_binding &&
                plan.cleanup_responsibility == "callee-owned-parameter-cleanup";
        }
    );
    auto const forwarding_local_lifetime_plans = std::count_if(
        dynamic_array_owned_parameter_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
        dynamic_array_owned_parameter_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.end(),
        [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
            return plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::local_binding &&
                plan.cleanup_responsibility == "caller-owned-local-cleanup";
        }
    );
    assert(forwarding_parameter_lifetime_plans == 2);
    assert(forwarding_local_lifetime_plans == 1);
    assert(
        !dynamic_array_owned_parameter_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
            .all_origins_have_cleanup_plans
    );
    assert(!dynamic_array_owned_parameter_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.origin_blockers.empty());
    assert(
        !dynamic_array_owned_parameter_forwarding_ir.dynamic_array_cleanup_production_readiness
            .descriptor_origin_blockers_absent
    );
    assert(
        !orison::pipeline::dynamic_array_cleanup_production_ready(
            dynamic_array_owned_parameter_forwarding_ir.dynamic_array_cleanup_production_readiness
        )
    );
    assert(
        orison::pipeline::format_dynamic_array_cleanup_production_readiness(
            dynamic_array_owned_parameter_forwarding_ir.dynamic_array_cleanup_production_readiness
        ).find("[descriptor origin blockers present]") != std::string::npos
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

    auto dynamic_array_returned_payload_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "choice_dynamic_array_return_payload_run.or";
    auto dynamic_array_returned_payload_ir = pipeline.emit_llvm(
        dynamic_array_returned_payload_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_payload_ir.has_errors());
    auto const returned_payload_lifetime_plans = std::count_if(
        dynamic_array_returned_payload_ir.dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
        dynamic_array_returned_payload_ir.dynamic_array_descriptor_lifetime_plan_state.plans.end(),
        [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
            return plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::returned_binding &&
                plan.cleanup_plan_available &&
                plan.cleanup_responsibility == "caller-owned-returned-cleanup" &&
                plan.descriptor_storage_status ==
                    orison::lowering::DynamicArrayDescriptorStorageStatus::lowered_local_descriptor;
        }
    );
    assert(returned_payload_lifetime_plans == 1);
    assert(
        dynamic_array_returned_payload_ir.dynamic_array_descriptor_lifetime_plan_state.origin_blockers.empty()
    );
    assert(dynamic_array_returned_payload_ir.dynamic_array_cleanup_capability_proven);
    assert(dynamic_array_returned_payload_ir.dynamic_array_cleanup_emission_capability_state.capability_metadata_available);
    assert(dynamic_array_returned_payload_ir.dynamic_array_cleanup_emission_capability_state.proven);
    assert(dynamic_array_returned_payload_ir.dynamic_array_cleanup_emission_capability_state.descriptor_storage_bound);
    assert(
        std::find(
            dynamic_array_returned_payload_ir.dynamic_array_cleanup_emission_capability_state
                .function_symbol_names.begin(),
            dynamic_array_returned_payload_ir.dynamic_array_cleanup_emission_capability_state
                .function_symbol_names.end(),
            "main"
        ) != dynamic_array_returned_payload_ir.dynamic_array_cleanup_emission_capability_state
            .function_symbol_names.end()
    );
    assert(
        std::find(
            dynamic_array_returned_payload_ir.dynamic_array_cleanup_emission_capability_state
                .cleanup_owner_names.begin(),
            dynamic_array_returned_payload_ir.dynamic_array_cleanup_emission_capability_state
                .cleanup_owner_names.end(),
            "returned"
        ) != dynamic_array_returned_payload_ir.dynamic_array_cleanup_emission_capability_state
            .cleanup_owner_names.end()
    );
    assert(
        orison::pipeline::dynamic_array_cleanup_production_ready(
            dynamic_array_returned_payload_ir.dynamic_array_cleanup_production_readiness
        )
    );
    auto dynamic_array_returned_payload_object = pipeline.emit_object(
        dynamic_array_returned_payload_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_payload_object.has_errors());
    assert(!dynamic_array_returned_payload_object.object_bytes.empty());
    auto dynamic_array_returned_payload_executable =
        smoke_temp_root / "dynamic_array_returned_payload_run";
    auto dynamic_array_returned_payload_link = orison::link::HostLinker {}.link(
        dynamic_array_returned_payload_object.object_bytes,
        dynamic_array_returned_payload_executable
    );
    assert(!dynamic_array_returned_payload_link.has_errors());
    auto dynamic_array_returned_payload_status =
        std::system(dynamic_array_returned_payload_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_returned_payload_status));
    assert(WEXITSTATUS(dynamic_array_returned_payload_status) == 0);

    auto dynamic_array_returned_parameter_forwarding_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_parameter_forwarding_run.or";
    auto dynamic_array_returned_parameter_forwarding_ir = pipeline.emit_llvm(
        dynamic_array_returned_parameter_forwarding_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_parameter_forwarding_ir.has_errors());
    auto const returned_parameter_lifetime_plans = std::count_if(
        dynamic_array_returned_parameter_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
        dynamic_array_returned_parameter_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.end(),
        [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
            return plan.source_type_name == "DynamicArray<Payload>" &&
                plan.cleanup_plan_available;
        }
    );
    assert(returned_parameter_lifetime_plans == 3);
    assert(
        std::any_of(
            dynamic_array_returned_parameter_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
            dynamic_array_returned_parameter_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.end(),
            [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
                return plan.owner_name == "items" &&
                    plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::parameter_binding &&
                    plan.cleanup_responsibility == "callee-owned-parameter-cleanup";
            }
        )
    );
    assert(
        std::any_of(
            dynamic_array_returned_parameter_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
            dynamic_array_returned_parameter_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.end(),
            [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
                return plan.owner_name == "returned" &&
                    plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::returned_binding &&
                    plan.cleanup_responsibility == "caller-owned-returned-cleanup";
            }
        )
    );
    assert(
        dynamic_array_returned_parameter_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
            .origin_blockers.empty()
    );
    assert(dynamic_array_returned_parameter_forwarding_ir.dynamic_array_cleanup_capability_proven);
    assert(dynamic_array_returned_parameter_forwarding_ir.dynamic_array_cleanup_emission_capability_state.proven);
    assert(
        std::find(
            dynamic_array_returned_parameter_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .function_symbol_names.begin(),
            dynamic_array_returned_parameter_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .function_symbol_names.end(),
            "use_items"
        ) != dynamic_array_returned_parameter_forwarding_ir.dynamic_array_cleanup_emission_capability_state
            .function_symbol_names.end()
    );
    assert(
        std::find(
            dynamic_array_returned_parameter_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .cleanup_owner_names.begin(),
            dynamic_array_returned_parameter_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .cleanup_owner_names.end(),
            "items"
        ) != dynamic_array_returned_parameter_forwarding_ir.dynamic_array_cleanup_emission_capability_state
            .cleanup_owner_names.end()
    );
    assert(
        dynamic_array_returned_parameter_forwarding_ir.ir_text.find("%returned.dynamic_array_cleanup") ==
        std::string::npos
    );
    assert(
        orison::pipeline::dynamic_array_cleanup_production_ready(
            dynamic_array_returned_parameter_forwarding_ir.dynamic_array_cleanup_production_readiness
        )
    );
    auto dynamic_array_returned_parameter_forwarding_object = pipeline.emit_object(
        dynamic_array_returned_parameter_forwarding_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_parameter_forwarding_object.has_errors());
    assert(!dynamic_array_returned_parameter_forwarding_object.object_bytes.empty());
    auto dynamic_array_returned_parameter_forwarding_executable =
        smoke_temp_root / "dynamic_array_returned_parameter_forwarding_run";
    auto dynamic_array_returned_parameter_forwarding_link = orison::link::HostLinker {}.link(
        dynamic_array_returned_parameter_forwarding_object.object_bytes,
        dynamic_array_returned_parameter_forwarding_executable
    );
    assert(!dynamic_array_returned_parameter_forwarding_link.has_errors());
    auto dynamic_array_returned_parameter_forwarding_status =
        std::system(dynamic_array_returned_parameter_forwarding_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_returned_parameter_forwarding_status));
    assert(WEXITSTATUS(dynamic_array_returned_parameter_forwarding_status) == 0);

    auto dynamic_array_returned_owned_computed_for_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_owned_computed_for_cleanup_run.or";
    auto dynamic_array_returned_owned_computed_for_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_returned_owned_computed_for_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_owned_computed_for_cleanup_ir.has_errors());
    assert_dynamic_array_payload_returned_lifetime_owner(
        dynamic_array_returned_owned_computed_for_cleanup_ir,
        "returned"
    );
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_returned_owned_computed_for_cleanup_ir);
    assert_ir_contains(
        dynamic_array_returned_owned_computed_for_cleanup_ir.ir_text,
        "returned.computed_for.0.condition:\n"
    );
    assert_ir_contains(
        dynamic_array_returned_owned_computed_for_cleanup_ir.ir_text,
        "call void @__orison_drop.Payload(ptr %returned.computed_dynamic_array_cleanup"
    );
    assert_ir_contains(
        dynamic_array_returned_owned_computed_for_cleanup_ir.ir_text,
        "call void @__orison_dynamic_array_deallocate(ptr %returned.computed_for.0.data"
    );
    assert_ir_contains(
        dynamic_array_returned_owned_computed_for_cleanup_ir.ir_text,
        "store { ptr, i64, i64 } zeroinitializer, ptr %returned.addr"
    );
    assert_ir_excludes(
        dynamic_array_returned_owned_computed_for_cleanup_ir.ir_text,
        "%returned.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_returned_owned_computed_for_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_owned_computed_for_cleanup_run"
    );
    auto dynamic_array_branch_returned_owned_computed_for_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_branch_returned_owned_computed_for_cleanup_run.or";
    auto dynamic_array_branch_returned_owned_computed_for_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_branch_returned_owned_computed_for_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_branch_returned_owned_computed_for_cleanup_ir.has_errors());
    assert_dynamic_array_payload_returned_lifetime_owner(
        dynamic_array_branch_returned_owned_computed_for_cleanup_ir,
        "selected"
    );
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_branch_returned_owned_computed_for_cleanup_ir);
    assert_ir_contains(
        dynamic_array_branch_returned_owned_computed_for_cleanup_ir.ir_text,
        "%tmp2 = phi { ptr, i64, i64 } [%tmp0, %if.then.0], [%tmp1, %if.else.0]"
    );
    assert_ir_contains(
        dynamic_array_branch_returned_owned_computed_for_cleanup_ir.ir_text,
        "selected.computed_for.0.condition:\n"
    );
    assert_ir_contains(
        dynamic_array_branch_returned_owned_computed_for_cleanup_ir.ir_text,
        "call void @__orison_drop.Payload(ptr %selected.computed_dynamic_array_cleanup"
    );
    assert_ir_contains(
        dynamic_array_branch_returned_owned_computed_for_cleanup_ir.ir_text,
        "call void @__orison_dynamic_array_deallocate(ptr %selected.computed_for.0.data"
    );
    assert_ir_contains(
        dynamic_array_branch_returned_owned_computed_for_cleanup_ir.ir_text,
        "store { ptr, i64, i64 } zeroinitializer, ptr %selected.addr"
    );
    assert_ir_excludes(
        dynamic_array_branch_returned_owned_computed_for_cleanup_ir.ir_text,
        "%selected.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_branch_returned_owned_computed_for_cleanup_path,
        smoke_temp_root / "dynamic_array_branch_returned_owned_computed_for_cleanup_run"
    );
    auto dynamic_array_switch_returned_owned_computed_for_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_switch_returned_owned_computed_for_cleanup_run.or";
    auto dynamic_array_switch_returned_owned_computed_for_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_switch_returned_owned_computed_for_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_switch_returned_owned_computed_for_cleanup_ir.has_errors());
    assert_dynamic_array_payload_returned_lifetime_owner(
        dynamic_array_switch_returned_owned_computed_for_cleanup_ir,
        "selected"
    );
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_switch_returned_owned_computed_for_cleanup_ir);
    assert_ir_contains(
        dynamic_array_switch_returned_owned_computed_for_cleanup_ir.ir_text,
        "switch i32 %selector, label %switch.default.0"
    );
    assert_ir_contains(
        dynamic_array_switch_returned_owned_computed_for_cleanup_ir.ir_text,
        "%tmp3 = phi { ptr, i64, i64 } [%tmp0, %switch.case.0.0], [%tmp1, %switch.case.0.1], "
        "[%tmp2, %switch.default.0]"
    );
    assert_ir_contains(
        dynamic_array_switch_returned_owned_computed_for_cleanup_ir.ir_text,
        "selected.computed_for.0.condition:\n"
    );
    assert_ir_contains(
        dynamic_array_switch_returned_owned_computed_for_cleanup_ir.ir_text,
        "call void @__orison_drop.Payload(ptr %selected.computed_dynamic_array_cleanup"
    );
    assert_ir_contains(
        dynamic_array_switch_returned_owned_computed_for_cleanup_ir.ir_text,
        "call void @__orison_dynamic_array_deallocate(ptr %selected.computed_for.0.data"
    );
    assert_ir_contains(
        dynamic_array_switch_returned_owned_computed_for_cleanup_ir.ir_text,
        "store { ptr, i64, i64 } zeroinitializer, ptr %selected.addr"
    );
    assert_ir_excludes(
        dynamic_array_switch_returned_owned_computed_for_cleanup_ir.ir_text,
        "%selected.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_switch_returned_owned_computed_for_cleanup_path,
        smoke_temp_root / "dynamic_array_switch_returned_owned_computed_for_cleanup_run"
    );
    auto dynamic_array_returned_aggregate_field_owned_computed_for_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_aggregate_field_owned_computed_for_cleanup_run.or";
    auto dynamic_array_returned_aggregate_field_owned_computed_for_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_returned_aggregate_field_owned_computed_for_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_aggregate_field_owned_computed_for_cleanup_ir.has_errors());
    assert_dynamic_array_payload_returned_lifetime_owner(
        dynamic_array_returned_aggregate_field_owned_computed_for_cleanup_ir,
        "returned.values"
    );
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_returned_aggregate_field_owned_computed_for_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_returned_aggregate_field_owned_computed_for_cleanup_ir.ir_text,
        "returned.values.computed_for.0.condition:\n"
    );
    assert_ir_contains(
        dynamic_array_returned_aggregate_field_owned_computed_for_cleanup_ir.ir_text,
        "call void @__orison_drop.Payload(ptr %returned.values.computed_dynamic_array_cleanup"
    );
    assert_ir_contains(
        dynamic_array_returned_aggregate_field_owned_computed_for_cleanup_ir.ir_text,
        "call void @__orison_dynamic_array_deallocate(ptr %returned.values.computed_for.0.data"
    );
    assert_ir_contains(
        dynamic_array_returned_aggregate_field_owned_computed_for_cleanup_ir.ir_text,
        "store { ptr, i64, i64 } zeroinitializer, ptr %returned.values.addr"
    );
    assert_ir_excludes(
        dynamic_array_returned_aggregate_field_owned_computed_for_cleanup_ir.ir_text,
        "%returned.values.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_returned_aggregate_field_owned_computed_for_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_owned_computed_for_cleanup_run"
    );
    auto dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_run.or";
    auto dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_ir.has_errors());
    assert_dynamic_array_payload_returned_lifetime_owner(
        dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_ir,
        "returned.values"
    );
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "switch i1 %flag"
    );
    assert_ir_contains(
        dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "returned.values.computed_for.1.condition:\n"
    );
    assert_ir_contains(
        dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "call void @__orison_drop.Payload(ptr %returned.values.computed_dynamic_array_cleanup"
    );
    assert_ir_contains(
        dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "call void @__orison_dynamic_array_deallocate(ptr %returned.values.computed_for.1.data"
    );
    assert_ir_contains(
        dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "store { ptr, i64, i64 } zeroinitializer, ptr %returned.values.addr"
    );
    assert_ir_contains(
        dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "call void @__orison_drop.Payload(ptr %scratch.dynamic_array_cleanup"
    );
    assert_ir_contains(
        dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "call void @__orison_dynamic_array_deallocate(ptr %scratch.dynamic_array_cleanup"
    );
    assert_ir_contains(
        dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "store { ptr, i64, i64 } zeroinitializer, ptr %scratch.addr"
    );
    assert_ir_excludes(
        dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "switch case ownership mismatch"
    );
    assert_ir_excludes(
        dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "%returned.values.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_final_switch_branch_local_cleanup_run"
    );
    auto dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_run.or";
    auto dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_ir.has_errors());
    assert_dynamic_array_payload_returned_lifetime_owner(
        dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_ir,
        "returned.inner.values"
    );
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "switch i1 %flag"
    );
    assert_ir_contains(
        dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "returned.inner.values.computed_for.1.condition:\n"
    );
    assert_ir_contains(
        dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "call void @__orison_drop.Payload(ptr %returned.inner.values.computed_dynamic_array_cleanup"
    );
    assert_ir_contains(
        dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "call void @__orison_dynamic_array_deallocate(ptr %returned.inner.values.computed_for.1.data"
    );
    assert_ir_contains(
        dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "store { ptr, i64, i64 } zeroinitializer, ptr %returned.inner.values.addr"
    );
    assert_ir_contains(
        dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "call void @__orison_drop.Payload(ptr %scratch.dynamic_array_cleanup"
    );
    assert_ir_contains(
        dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "call void @__orison_dynamic_array_deallocate(ptr %scratch.dynamic_array_cleanup"
    );
    assert_ir_contains(
        dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "store { ptr, i64, i64 } zeroinitializer, ptr %scratch.addr"
    );
    assert_ir_excludes(
        dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "switch case ownership mismatch"
    );
    assert_ir_excludes(
        dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_ir.ir_text,
        "%returned.inner.values.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_cleanup_run"
    );
    auto dynamic_array_returned_nested_aggregate_field_owned_computed_for_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_nested_aggregate_field_owned_computed_for_cleanup_run.or";
    auto dynamic_array_returned_nested_aggregate_field_owned_computed_for_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_returned_nested_aggregate_field_owned_computed_for_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_nested_aggregate_field_owned_computed_for_cleanup_ir.has_errors());
    assert_dynamic_array_payload_returned_lifetime_owner(
        dynamic_array_returned_nested_aggregate_field_owned_computed_for_cleanup_ir,
        "returned.inner.values"
    );
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_returned_nested_aggregate_field_owned_computed_for_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_returned_nested_aggregate_field_owned_computed_for_cleanup_ir.ir_text,
        "returned.inner.values.computed_for.0.condition:\n"
    );
    assert_ir_contains(
        dynamic_array_returned_nested_aggregate_field_owned_computed_for_cleanup_ir.ir_text,
        "call void @__orison_drop.Payload(ptr %returned.inner.values.computed_dynamic_array_cleanup"
    );
    assert_ir_contains(
        dynamic_array_returned_nested_aggregate_field_owned_computed_for_cleanup_ir.ir_text,
        "call void @__orison_dynamic_array_deallocate(ptr %returned.inner.values.computed_for.0.data"
    );
    assert_ir_contains(
        dynamic_array_returned_nested_aggregate_field_owned_computed_for_cleanup_ir.ir_text,
        "store { ptr, i64, i64 } zeroinitializer, ptr %returned.inner.values.addr"
    );
    assert_ir_excludes(
        dynamic_array_returned_nested_aggregate_field_owned_computed_for_cleanup_ir.ir_text,
        "%returned.inner.values.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_returned_nested_aggregate_field_owned_computed_for_cleanup_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_owned_computed_for_cleanup_run"
    );
    auto dynamic_array_choice_payload_switch_binding_owned_computed_for_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_choice_payload_switch_binding_owned_computed_for_cleanup_run.or";
    auto dynamic_array_choice_payload_switch_binding_owned_computed_for_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_choice_payload_switch_binding_owned_computed_for_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_choice_payload_switch_binding_owned_computed_for_cleanup_ir.has_errors());
    assert_ir_contains(
        dynamic_array_choice_payload_switch_binding_owned_computed_for_cleanup_ir.ir_text,
        "define i32 @consume_packet({ i32, { ptr, i64, i64 } } %packet)"
    );
    assert_ir_contains(
        dynamic_array_choice_payload_switch_binding_owned_computed_for_cleanup_ir.ir_text,
        "values.computed_for.1.condition:\n"
    );
    assert_ir_contains(
        dynamic_array_choice_payload_switch_binding_owned_computed_for_cleanup_ir.ir_text,
        "call void @__orison_drop.Payload(ptr %values.computed_dynamic_array_cleanup"
    );
    assert_ir_contains(
        dynamic_array_choice_payload_switch_binding_owned_computed_for_cleanup_ir.ir_text,
        "call void @__orison_dynamic_array_deallocate(ptr %values.computed_for.1.data"
    );
    assert_ir_contains(
        dynamic_array_choice_payload_switch_binding_owned_computed_for_cleanup_ir.ir_text,
        "store { ptr, i64, i64 } zeroinitializer, ptr %values.addr"
    );
    assert_ir_excludes(
        dynamic_array_choice_payload_switch_binding_owned_computed_for_cleanup_ir.ir_text,
        "packet.Primary.values.choice_dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_choice_payload_switch_binding_owned_computed_for_cleanup_path,
        smoke_temp_root / "dynamic_array_choice_payload_switch_binding_owned_computed_for_cleanup_run"
    );
    auto dynamic_array_choice_payload_final_switch_binding_owned_computed_for_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_choice_payload_final_switch_binding_owned_computed_for_cleanup_run.or";
    auto dynamic_array_choice_payload_final_switch_binding_owned_computed_for_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_choice_payload_final_switch_binding_owned_computed_for_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_choice_payload_final_switch_binding_owned_computed_for_cleanup_ir.has_errors());
    assert_ir_contains(
        dynamic_array_choice_payload_final_switch_binding_owned_computed_for_cleanup_ir.ir_text,
        "define i32 @consume_packet({ i32, { ptr, i64, i64 } } %packet)"
    );
    assert_ir_contains(
        dynamic_array_choice_payload_final_switch_binding_owned_computed_for_cleanup_ir.ir_text,
        "values.computed_for.1.condition:\n"
    );
    assert_ir_contains(
        dynamic_array_choice_payload_final_switch_binding_owned_computed_for_cleanup_ir.ir_text,
        "call void @__orison_drop.Payload(ptr %values.computed_dynamic_array_cleanup"
    );
    assert_ir_contains(
        dynamic_array_choice_payload_final_switch_binding_owned_computed_for_cleanup_ir.ir_text,
        "call void @__orison_dynamic_array_deallocate(ptr %values.computed_for.1.data"
    );
    assert_ir_contains(
        dynamic_array_choice_payload_final_switch_binding_owned_computed_for_cleanup_ir.ir_text,
        "store { ptr, i64, i64 } zeroinitializer, ptr %values.addr"
    );
    assert_ir_excludes(
        dynamic_array_choice_payload_final_switch_binding_owned_computed_for_cleanup_ir.ir_text,
        "packet.Primary.values.choice_dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_choice_payload_final_switch_binding_owned_computed_for_cleanup_path,
        smoke_temp_root / "dynamic_array_choice_payload_final_switch_binding_owned_computed_for_cleanup_run"
    );
    auto dynamic_array_returned_owned_computed_cleanup_missing_drop_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_owned_computed_cleanup_missing_drop.or";
    auto dynamic_array_returned_owned_computed_cleanup_missing_drop_ir = pipeline.emit_llvm(
        dynamic_array_returned_owned_computed_cleanup_missing_drop_path
    );
    assert(dynamic_array_returned_owned_computed_cleanup_missing_drop_ir.has_errors());
    assert(
        dynamic_array_returned_owned_computed_cleanup_missing_drop_ir.error_text.find(
            "lowering computed DynamicArray cleanup for owned element type Payload requires authorized element drop"
        ) != std::string::npos
    );
    auto dynamic_array_returned_aggregate_field_owned_computed_cleanup_missing_drop_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_aggregate_field_owned_computed_cleanup_missing_drop.or";
    auto dynamic_array_returned_aggregate_field_owned_computed_cleanup_missing_drop_ir =
        pipeline.emit_llvm(dynamic_array_returned_aggregate_field_owned_computed_cleanup_missing_drop_path);
    assert(dynamic_array_returned_aggregate_field_owned_computed_cleanup_missing_drop_ir.has_errors());
    assert(
        dynamic_array_returned_aggregate_field_owned_computed_cleanup_missing_drop_ir.error_text.find(
            "lowering computed DynamicArray cleanup for owned element type Payload requires authorized element drop"
        ) != std::string::npos
    );
    auto dynamic_array_returned_nested_aggregate_field_owned_computed_cleanup_missing_drop_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_nested_aggregate_field_owned_computed_cleanup_missing_drop.or";
    auto dynamic_array_returned_nested_aggregate_field_owned_computed_cleanup_missing_drop_ir =
        pipeline.emit_llvm(dynamic_array_returned_nested_aggregate_field_owned_computed_cleanup_missing_drop_path);
    assert(dynamic_array_returned_nested_aggregate_field_owned_computed_cleanup_missing_drop_ir.has_errors());
    assert(
        dynamic_array_returned_nested_aggregate_field_owned_computed_cleanup_missing_drop_ir.error_text.find(
            "lowering computed DynamicArray cleanup for owned element type Payload requires authorized element drop"
        ) != std::string::npos
    );
    auto dynamic_array_returned_aggregate_field_owned_computed_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_aggregate_field_owned_computed_reuse_rejected.or";
    auto dynamic_array_returned_aggregate_field_owned_computed_reuse_ir =
        pipeline.emit_llvm(dynamic_array_returned_aggregate_field_owned_computed_reuse_path);
    assert(dynamic_array_returned_aggregate_field_owned_computed_reuse_ir.has_errors());
    assert(
        dynamic_array_returned_aggregate_field_owned_computed_reuse_ir.error_text.find(
            "use after move: returned.values"
        ) != std::string::npos
    );
    auto dynamic_array_returned_nested_aggregate_field_owned_computed_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_nested_aggregate_field_owned_computed_reuse_rejected.or";
    auto dynamic_array_returned_nested_aggregate_field_owned_computed_reuse_ir =
        pipeline.emit_llvm(dynamic_array_returned_nested_aggregate_field_owned_computed_reuse_path);
    assert(dynamic_array_returned_nested_aggregate_field_owned_computed_reuse_ir.has_errors());
    assert(
        dynamic_array_returned_nested_aggregate_field_owned_computed_reuse_ir.error_text.find(
            "use after move: returned.inner.values"
        ) != std::string::npos
    );
    auto dynamic_array_returned_aggregate_field_final_switch_branch_local_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_aggregate_field_final_switch_branch_local_reuse_rejected.or";
    auto dynamic_array_returned_aggregate_field_final_switch_branch_local_reuse_ir =
        pipeline.emit_llvm(
            dynamic_array_returned_aggregate_field_final_switch_branch_local_reuse_path,
            orison::pipeline::CompilePipelineOptions {
                .source_drop_lowering_enabled = true,
                .dynamic_array_descriptor_cleanup_planning_enabled = true,
            }
        );
    assert(dynamic_array_returned_aggregate_field_final_switch_branch_local_reuse_ir.has_errors());
    assert(
        dynamic_array_returned_aggregate_field_final_switch_branch_local_reuse_ir.error_text.find(
            "use after move: scratch"
        ) != std::string::npos
    );
    auto dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_reuse_rejected.or";
    auto dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_reuse_ir =
        pipeline.emit_llvm(
            dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_reuse_path,
            orison::pipeline::CompilePipelineOptions {
                .source_drop_lowering_enabled = true,
                .dynamic_array_descriptor_cleanup_planning_enabled = true,
            }
        );
    assert(dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_reuse_ir.has_errors());
    assert(
        dynamic_array_returned_nested_aggregate_field_final_switch_branch_local_reuse_ir.error_text.find(
            "use after move: scratch"
        ) != std::string::npos
    );
    auto dynamic_array_choice_payload_switch_binding_owned_computed_cleanup_missing_drop_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_choice_payload_switch_binding_owned_computed_cleanup_missing_drop.or";
    auto dynamic_array_choice_payload_switch_binding_owned_computed_cleanup_missing_drop_ir =
        pipeline.emit_llvm(dynamic_array_choice_payload_switch_binding_owned_computed_cleanup_missing_drop_path);
    assert(dynamic_array_choice_payload_switch_binding_owned_computed_cleanup_missing_drop_ir.has_errors());
    assert(
        dynamic_array_choice_payload_switch_binding_owned_computed_cleanup_missing_drop_ir.error_text.find(
            "lowering computed DynamicArray cleanup for owned element type Payload requires authorized element drop"
        ) != std::string::npos
    );
    auto dynamic_array_choice_payload_final_switch_binding_owned_computed_cleanup_missing_drop_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_choice_payload_final_switch_binding_owned_computed_cleanup_missing_drop.or";
    auto dynamic_array_choice_payload_final_switch_binding_owned_computed_cleanup_missing_drop_ir =
        pipeline.emit_llvm(
            dynamic_array_choice_payload_final_switch_binding_owned_computed_cleanup_missing_drop_path
        );
    assert(dynamic_array_choice_payload_final_switch_binding_owned_computed_cleanup_missing_drop_ir.has_errors());
    assert(
        dynamic_array_choice_payload_final_switch_binding_owned_computed_cleanup_missing_drop_ir.error_text.find(
            "lowering computed DynamicArray cleanup for owned element type Payload requires authorized element drop"
        ) != std::string::npos
    );
    assert(
        dynamic_array_choice_payload_final_switch_binding_owned_computed_cleanup_missing_drop_ir.error_text.find(
            "lowering does not yet support this final control-flow statement"
        ) == std::string::npos
    );
    auto dynamic_array_choice_payload_final_switch_binding_owned_computed_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_choice_payload_final_switch_binding_owned_computed_reuse_rejected.or";
    auto dynamic_array_choice_payload_final_switch_binding_owned_computed_reuse_ir =
        pipeline.emit_llvm(
            dynamic_array_choice_payload_final_switch_binding_owned_computed_reuse_path,
            orison::pipeline::CompilePipelineOptions {
                .source_drop_lowering_enabled = true,
                .dynamic_array_descriptor_cleanup_planning_enabled = true,
            }
        );
    assert(dynamic_array_choice_payload_final_switch_binding_owned_computed_reuse_ir.has_errors());
    assert(
        dynamic_array_choice_payload_final_switch_binding_owned_computed_reuse_ir.error_text.find(
            "use after move: values"
        ) != std::string::npos
    );
    auto dynamic_array_branch_returned_owned_computed_owner_mismatch_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_branch_returned_owned_computed_owner_mismatch_rejected.or";
    auto dynamic_array_branch_returned_owned_computed_owner_mismatch_ir = pipeline.emit_llvm(
        dynamic_array_branch_returned_owned_computed_owner_mismatch_path
    );
    assert(dynamic_array_branch_returned_owned_computed_owner_mismatch_ir.has_errors());
    assert(
        dynamic_array_branch_returned_owned_computed_owner_mismatch_ir.error_text.find(
            "computed DynamicArray ownership plan ternary branch owner mismatch source DynamicArray<Payload> "
            "element Payload owners left right [ownership join blocked] [cleanup owner blocked] (metadata only)"
        ) != std::string::npos
    );
    assert(
        dynamic_array_branch_returned_owned_computed_owner_mismatch_ir.error_text.find(
            "computed DynamicArray descriptor handoff plan ownership join blocked source DynamicArray<Payload> "
            "element Payload [descriptor storage blocked] [cleanup owner blocked] [lowering disabled] (metadata only)"
        ) != std::string::npos
    );
    auto dynamic_array_switch_returned_owned_computed_owner_mismatch_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_switch_returned_owned_computed_owner_mismatch_rejected.or";
    auto dynamic_array_switch_returned_owned_computed_owner_mismatch_ir = pipeline.emit_llvm(
        dynamic_array_switch_returned_owned_computed_owner_mismatch_path
    );
    assert(dynamic_array_switch_returned_owned_computed_owner_mismatch_ir.has_errors());
    assert(
        dynamic_array_switch_returned_owned_computed_owner_mismatch_ir.error_text.find(
            "computed DynamicArray ownership plan ternary branch owner mismatch source DynamicArray<Payload> "
            "element Payload owners left right [ownership join blocked] [cleanup owner blocked] (metadata only)"
        ) != std::string::npos
    );
    assert(
        dynamic_array_switch_returned_owned_computed_owner_mismatch_ir.error_text.find(
            "computed DynamicArray descriptor handoff plan ownership join blocked source DynamicArray<Payload> "
            "element Payload [descriptor storage blocked] [cleanup owner blocked] [lowering disabled] (metadata only)"
        ) != std::string::npos
    );

    auto dynamic_array_returned_multi_hop_forwarding_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_multi_hop_forwarding_run.or";
    auto dynamic_array_returned_multi_hop_forwarding_ir = pipeline.emit_llvm(
        dynamic_array_returned_multi_hop_forwarding_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_multi_hop_forwarding_ir.has_errors());
    auto const multi_hop_lifetime_plans = std::count_if(
        dynamic_array_returned_multi_hop_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
        dynamic_array_returned_multi_hop_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.end(),
        [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
            return plan.source_type_name == "DynamicArray<Payload>" &&
                plan.cleanup_plan_available;
        }
    );
    assert(multi_hop_lifetime_plans == 4);
    auto const multi_hop_parameter_lifetime_plans = std::count_if(
        dynamic_array_returned_multi_hop_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
        dynamic_array_returned_multi_hop_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.end(),
        [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
            return plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::parameter_binding &&
                plan.cleanup_responsibility == "callee-owned-parameter-cleanup";
        }
    );
    assert(multi_hop_parameter_lifetime_plans == 2);
    assert(
        std::any_of(
            dynamic_array_returned_multi_hop_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
            dynamic_array_returned_multi_hop_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.end(),
            [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
                return plan.owner_name == "returned" &&
                    plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::returned_binding &&
                    plan.cleanup_responsibility == "caller-owned-returned-cleanup";
            }
        )
    );
    assert(
        dynamic_array_returned_multi_hop_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
            .origin_blockers.empty()
    );
    assert(dynamic_array_returned_multi_hop_forwarding_ir.dynamic_array_cleanup_capability_proven);
    assert(dynamic_array_returned_multi_hop_forwarding_ir.dynamic_array_cleanup_emission_capability_state.proven);
    assert(
        std::find(
            dynamic_array_returned_multi_hop_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .function_symbol_names.begin(),
            dynamic_array_returned_multi_hop_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .function_symbol_names.end(),
            "consume_items"
        ) != dynamic_array_returned_multi_hop_forwarding_ir.dynamic_array_cleanup_emission_capability_state
            .function_symbol_names.end()
    );
    assert(
        std::find(
            dynamic_array_returned_multi_hop_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .cleanup_owner_names.begin(),
            dynamic_array_returned_multi_hop_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .cleanup_owner_names.end(),
            "items"
        ) != dynamic_array_returned_multi_hop_forwarding_ir.dynamic_array_cleanup_emission_capability_state
            .cleanup_owner_names.end()
    );
    assert(
        dynamic_array_returned_multi_hop_forwarding_ir.ir_text.find("%returned.dynamic_array_cleanup") ==
        std::string::npos
    );
    auto const multi_hop_forward_start =
        dynamic_array_returned_multi_hop_forwarding_ir.ir_text.find("define i32 @forward_items");
    auto const multi_hop_main_start =
        dynamic_array_returned_multi_hop_forwarding_ir.ir_text.find("define i32 @main", multi_hop_forward_start);
    assert(multi_hop_forward_start != std::string::npos);
    assert(multi_hop_main_start != std::string::npos);
    assert(
        dynamic_array_returned_multi_hop_forwarding_ir.ir_text.find(
            "__orison_dynamic_array_deallocate",
            multi_hop_forward_start
        ) > multi_hop_main_start
    );
    assert(
        orison::pipeline::dynamic_array_cleanup_production_ready(
            dynamic_array_returned_multi_hop_forwarding_ir.dynamic_array_cleanup_production_readiness
        )
    );
    auto dynamic_array_returned_multi_hop_forwarding_object = pipeline.emit_object(
        dynamic_array_returned_multi_hop_forwarding_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_multi_hop_forwarding_object.has_errors());
    assert(!dynamic_array_returned_multi_hop_forwarding_object.object_bytes.empty());
    auto dynamic_array_returned_multi_hop_forwarding_executable =
        smoke_temp_root / "dynamic_array_returned_multi_hop_forwarding_run";
    auto dynamic_array_returned_multi_hop_forwarding_link = orison::link::HostLinker {}.link(
        dynamic_array_returned_multi_hop_forwarding_object.object_bytes,
        dynamic_array_returned_multi_hop_forwarding_executable
    );
    assert(!dynamic_array_returned_multi_hop_forwarding_link.has_errors());
    auto dynamic_array_returned_multi_hop_forwarding_status =
        std::system(dynamic_array_returned_multi_hop_forwarding_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_returned_multi_hop_forwarding_status));
    assert(WEXITSTATUS(dynamic_array_returned_multi_hop_forwarding_status) == 0);

    auto dynamic_array_returned_branch_join_forwarding_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_branch_join_forwarding_run.or";
    auto dynamic_array_returned_branch_join_forwarding_ir = pipeline.emit_llvm(
        dynamic_array_returned_branch_join_forwarding_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_branch_join_forwarding_ir.has_errors());
    auto const branch_join_lifetime_plans = std::count_if(
        dynamic_array_returned_branch_join_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
        dynamic_array_returned_branch_join_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.end(),
        [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
            return plan.source_type_name == "DynamicArray<Payload>" &&
                plan.cleanup_plan_available;
        }
    );
    assert(branch_join_lifetime_plans == 4);
    auto const branch_join_parameter_lifetime_plans = std::count_if(
        dynamic_array_returned_branch_join_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
        dynamic_array_returned_branch_join_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.end(),
        [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
            return plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::parameter_binding &&
                plan.cleanup_responsibility == "callee-owned-parameter-cleanup";
        }
    );
    assert(branch_join_parameter_lifetime_plans == 2);
    assert(
        std::any_of(
            dynamic_array_returned_branch_join_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
            dynamic_array_returned_branch_join_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.end(),
            [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
                return plan.owner_name == "returned" &&
                    plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::returned_binding &&
                    plan.cleanup_responsibility == "caller-owned-returned-cleanup";
            }
        )
    );
    assert(
        dynamic_array_returned_branch_join_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
            .origin_blockers.empty()
    );
    assert(dynamic_array_returned_branch_join_forwarding_ir.dynamic_array_cleanup_capability_proven);
    assert(dynamic_array_returned_branch_join_forwarding_ir.dynamic_array_cleanup_emission_capability_state.proven);
    assert(
        std::find(
            dynamic_array_returned_branch_join_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .function_symbol_names.begin(),
            dynamic_array_returned_branch_join_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .function_symbol_names.end(),
            "consume_items"
        ) != dynamic_array_returned_branch_join_forwarding_ir.dynamic_array_cleanup_emission_capability_state
            .function_symbol_names.end()
    );
    assert(
        std::find(
            dynamic_array_returned_branch_join_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .cleanup_owner_names.begin(),
            dynamic_array_returned_branch_join_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .cleanup_owner_names.end(),
            "items"
        ) != dynamic_array_returned_branch_join_forwarding_ir.dynamic_array_cleanup_emission_capability_state
            .cleanup_owner_names.end()
    );
    assert(
        dynamic_array_returned_branch_join_forwarding_ir.ir_text.find("%returned.dynamic_array_cleanup") ==
        std::string::npos
    );
    auto const branch_join_choose_start =
        dynamic_array_returned_branch_join_forwarding_ir.ir_text.find("define i32 @choose_items");
    auto const branch_join_main_start =
        dynamic_array_returned_branch_join_forwarding_ir.ir_text.find("define i32 @main", branch_join_choose_start);
    assert(branch_join_choose_start != std::string::npos);
    assert(branch_join_main_start != std::string::npos);
    assert(
        dynamic_array_returned_branch_join_forwarding_ir.ir_text.find(
            "__orison_dynamic_array_deallocate",
            branch_join_choose_start
        ) > branch_join_main_start
    );
    assert(
        orison::pipeline::dynamic_array_cleanup_production_ready(
            dynamic_array_returned_branch_join_forwarding_ir.dynamic_array_cleanup_production_readiness
        )
    );
    auto dynamic_array_returned_branch_join_forwarding_object = pipeline.emit_object(
        dynamic_array_returned_branch_join_forwarding_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_branch_join_forwarding_object.has_errors());
    assert(!dynamic_array_returned_branch_join_forwarding_object.object_bytes.empty());
    auto dynamic_array_returned_branch_join_forwarding_executable =
        smoke_temp_root / "dynamic_array_returned_branch_join_forwarding_run";
    auto dynamic_array_returned_branch_join_forwarding_link = orison::link::HostLinker {}.link(
        dynamic_array_returned_branch_join_forwarding_object.object_bytes,
        dynamic_array_returned_branch_join_forwarding_executable
    );
    assert(!dynamic_array_returned_branch_join_forwarding_link.has_errors());
    auto dynamic_array_returned_branch_join_forwarding_status =
        std::system(dynamic_array_returned_branch_join_forwarding_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_returned_branch_join_forwarding_status));
    assert(WEXITSTATUS(dynamic_array_returned_branch_join_forwarding_status) == 0);

    auto dynamic_array_returned_choice_branch_forwarding_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_choice_branch_forwarding_run.or";
    auto dynamic_array_returned_choice_branch_forwarding_ir = pipeline.emit_llvm(
        dynamic_array_returned_choice_branch_forwarding_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_choice_branch_forwarding_ir.has_errors());
    auto const choice_branch_lifetime_plans = std::count_if(
        dynamic_array_returned_choice_branch_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
        dynamic_array_returned_choice_branch_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.end(),
        [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
            return plan.source_type_name == "DynamicArray<Payload>" &&
                plan.cleanup_plan_available;
        }
    );
    assert(choice_branch_lifetime_plans == 5);
    auto const choice_branch_parameter_lifetime_plans = std::count_if(
        dynamic_array_returned_choice_branch_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
        dynamic_array_returned_choice_branch_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.end(),
        [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
            return plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::parameter_binding &&
                plan.cleanup_responsibility == "callee-owned-parameter-cleanup";
        }
    );
    assert(choice_branch_parameter_lifetime_plans == 2);
    assert(
        std::any_of(
            dynamic_array_returned_choice_branch_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
            dynamic_array_returned_choice_branch_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.end(),
            [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
                return plan.owner_name == "returned" &&
                    plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::returned_binding &&
                    plan.cleanup_responsibility == "caller-owned-returned-cleanup";
            }
        )
    );
    assert(
        dynamic_array_returned_choice_branch_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
            .origin_blockers.empty()
    );
    assert(dynamic_array_returned_choice_branch_forwarding_ir.dynamic_array_cleanup_capability_proven);
    assert(dynamic_array_returned_choice_branch_forwarding_ir.dynamic_array_cleanup_emission_capability_state.proven);
    assert(
        std::find(
            dynamic_array_returned_choice_branch_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .function_symbol_names.begin(),
            dynamic_array_returned_choice_branch_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .function_symbol_names.end(),
            "consume_items"
        ) != dynamic_array_returned_choice_branch_forwarding_ir.dynamic_array_cleanup_emission_capability_state
            .function_symbol_names.end()
    );
    assert(
        std::find(
            dynamic_array_returned_choice_branch_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .cleanup_owner_names.begin(),
            dynamic_array_returned_choice_branch_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .cleanup_owner_names.end(),
            "items"
        ) != dynamic_array_returned_choice_branch_forwarding_ir.dynamic_array_cleanup_emission_capability_state
            .cleanup_owner_names.end()
    );
    assert(
        dynamic_array_returned_choice_branch_forwarding_ir.ir_text.find("%returned.dynamic_array_cleanup") ==
        std::string::npos
    );
    auto const choice_branch_choice_start =
        dynamic_array_returned_choice_branch_forwarding_ir.ir_text.find(
            "define { ptr, i64, i64 } @return_payload"
        );
    auto const choice_branch_make_start =
        dynamic_array_returned_choice_branch_forwarding_ir.ir_text.find(
            "define { ptr, i64, i64 } @make_payload",
            choice_branch_choice_start
        );
    assert(choice_branch_choice_start != std::string::npos);
    assert(choice_branch_make_start != std::string::npos);
    assert(
        dynamic_array_returned_choice_branch_forwarding_ir.ir_text.find(
            "__orison_dynamic_array_deallocate",
            choice_branch_choice_start
        ) > choice_branch_make_start
    );
    auto const choice_branch_choose_start =
        dynamic_array_returned_choice_branch_forwarding_ir.ir_text.find("define i32 @choose_items");
    auto const choice_branch_main_start =
        dynamic_array_returned_choice_branch_forwarding_ir.ir_text.find(
            "define i32 @main",
            choice_branch_choose_start
        );
    assert(choice_branch_choose_start != std::string::npos);
    assert(choice_branch_main_start != std::string::npos);
    assert(
        dynamic_array_returned_choice_branch_forwarding_ir.ir_text.find(
            "__orison_dynamic_array_deallocate",
            choice_branch_choose_start
        ) > choice_branch_main_start
    );
    assert(
        orison::pipeline::dynamic_array_cleanup_production_ready(
            dynamic_array_returned_choice_branch_forwarding_ir.dynamic_array_cleanup_production_readiness
        )
    );
    auto dynamic_array_returned_choice_branch_forwarding_object = pipeline.emit_object(
        dynamic_array_returned_choice_branch_forwarding_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_choice_branch_forwarding_object.has_errors());
    assert(!dynamic_array_returned_choice_branch_forwarding_object.object_bytes.empty());
    auto dynamic_array_returned_choice_branch_forwarding_executable =
        smoke_temp_root / "dynamic_array_returned_choice_branch_forwarding_run";
    auto dynamic_array_returned_choice_branch_forwarding_link = orison::link::HostLinker {}.link(
        dynamic_array_returned_choice_branch_forwarding_object.object_bytes,
        dynamic_array_returned_choice_branch_forwarding_executable
    );
    assert(!dynamic_array_returned_choice_branch_forwarding_link.has_errors());
    auto dynamic_array_returned_choice_branch_forwarding_status =
        std::system(dynamic_array_returned_choice_branch_forwarding_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_returned_choice_branch_forwarding_status));
    assert(WEXITSTATUS(dynamic_array_returned_choice_branch_forwarding_status) == 0);

    auto dynamic_array_returned_aggregate_field_forwarding_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_aggregate_field_forwarding_run.or";
    auto dynamic_array_returned_aggregate_field_forwarding_ir = pipeline.emit_llvm(
        dynamic_array_returned_aggregate_field_forwarding_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_aggregate_field_forwarding_ir.has_errors());
    auto const aggregate_field_lifetime_plans = std::count_if(
        dynamic_array_returned_aggregate_field_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
        dynamic_array_returned_aggregate_field_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state.plans.end(),
        [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
            return plan.source_type_name == "DynamicArray<Payload>" &&
                plan.cleanup_plan_available;
        }
    );
    assert(aggregate_field_lifetime_plans == 3);
    assert(
        std::any_of(
            dynamic_array_returned_aggregate_field_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
                .plans.begin(),
            dynamic_array_returned_aggregate_field_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
                .plans.end(),
            [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
                return plan.owner_name == "returned.values" &&
                    plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::returned_binding &&
                    plan.cleanup_responsibility == "caller-owned-returned-cleanup";
            }
        )
    );
    assert(
        std::any_of(
            dynamic_array_returned_aggregate_field_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
                .plans.begin(),
            dynamic_array_returned_aggregate_field_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
                .plans.end(),
            [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
                return plan.owner_name == "items" &&
                    plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::parameter_binding &&
                    plan.cleanup_responsibility == "callee-owned-parameter-cleanup";
            }
        )
    );
    assert(
        dynamic_array_returned_aggregate_field_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
            .origin_blockers.empty()
    );
    assert(dynamic_array_returned_aggregate_field_forwarding_ir.dynamic_array_cleanup_capability_proven);
    assert(dynamic_array_returned_aggregate_field_forwarding_ir.dynamic_array_cleanup_emission_capability_state.proven);
    assert(
        std::find(
            dynamic_array_returned_aggregate_field_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .function_symbol_names.begin(),
            dynamic_array_returned_aggregate_field_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .function_symbol_names.end(),
            "consume_items"
        ) != dynamic_array_returned_aggregate_field_forwarding_ir.dynamic_array_cleanup_emission_capability_state
            .function_symbol_names.end()
    );
    assert(
        std::find(
            dynamic_array_returned_aggregate_field_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .cleanup_owner_names.begin(),
            dynamic_array_returned_aggregate_field_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .cleanup_owner_names.end(),
            "items"
        ) != dynamic_array_returned_aggregate_field_forwarding_ir.dynamic_array_cleanup_emission_capability_state
            .cleanup_owner_names.end()
    );
    assert(
        dynamic_array_returned_aggregate_field_forwarding_ir.ir_text.find("%returned.dynamic_array_cleanup") ==
        std::string::npos
    );
    assert(
        dynamic_array_returned_aggregate_field_forwarding_ir.ir_text.find(
            "%returned.values.dynamic_array_cleanup"
        ) == std::string::npos
    );
    auto const aggregate_field_make_start =
        dynamic_array_returned_aggregate_field_forwarding_ir.ir_text.find("define %record.PayloadBox @make_box");
    auto const aggregate_field_consume_start =
        dynamic_array_returned_aggregate_field_forwarding_ir.ir_text.find(
            "define i32 @consume_items",
            aggregate_field_make_start
        );
    assert(aggregate_field_make_start != std::string::npos);
    assert(aggregate_field_consume_start != std::string::npos);
    assert(
        dynamic_array_returned_aggregate_field_forwarding_ir.ir_text.find(
            "__orison_dynamic_array_deallocate",
            aggregate_field_make_start
        ) > aggregate_field_consume_start
    );
    auto const aggregate_field_main_start =
        dynamic_array_returned_aggregate_field_forwarding_ir.ir_text.find("define i32 @main");
    assert(aggregate_field_main_start != std::string::npos);
    assert(
        dynamic_array_returned_aggregate_field_forwarding_ir.ir_text.find(
            "__orison_dynamic_array_deallocate",
            aggregate_field_main_start
        ) == std::string::npos
    );
    assert(
        orison::pipeline::dynamic_array_cleanup_production_ready(
            dynamic_array_returned_aggregate_field_forwarding_ir.dynamic_array_cleanup_production_readiness
        )
    );
    auto dynamic_array_returned_aggregate_field_forwarding_object = pipeline.emit_object(
        dynamic_array_returned_aggregate_field_forwarding_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_aggregate_field_forwarding_object.has_errors());
    assert(!dynamic_array_returned_aggregate_field_forwarding_object.object_bytes.empty());
    auto dynamic_array_returned_aggregate_field_forwarding_executable =
        smoke_temp_root / "dynamic_array_returned_aggregate_field_forwarding_run";
    auto dynamic_array_returned_aggregate_field_forwarding_link = orison::link::HostLinker {}.link(
        dynamic_array_returned_aggregate_field_forwarding_object.object_bytes,
        dynamic_array_returned_aggregate_field_forwarding_executable
    );
    assert(!dynamic_array_returned_aggregate_field_forwarding_link.has_errors());
    auto dynamic_array_returned_aggregate_field_forwarding_status =
        std::system(dynamic_array_returned_aggregate_field_forwarding_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_returned_aggregate_field_forwarding_status));
    assert(WEXITSTATUS(dynamic_array_returned_aggregate_field_forwarding_status) == 0);

    auto dynamic_array_returned_nested_aggregate_field_forwarding_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_nested_aggregate_field_forwarding_run.or";
    auto dynamic_array_returned_nested_aggregate_field_forwarding_ir = pipeline.emit_llvm(
        dynamic_array_returned_nested_aggregate_field_forwarding_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_nested_aggregate_field_forwarding_ir.has_errors());
    auto const nested_aggregate_field_lifetime_plans = std::count_if(
        dynamic_array_returned_nested_aggregate_field_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
            .plans.begin(),
        dynamic_array_returned_nested_aggregate_field_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
            .plans.end(),
        [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
            return plan.source_type_name == "DynamicArray<Payload>" &&
                plan.cleanup_plan_available;
        }
    );
    assert(nested_aggregate_field_lifetime_plans == 4);
    assert(
        std::any_of(
            dynamic_array_returned_nested_aggregate_field_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
                .plans.begin(),
            dynamic_array_returned_nested_aggregate_field_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
                .plans.end(),
            [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
                return plan.owner_name == "inner.values" &&
                    plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::returned_binding &&
                    plan.cleanup_responsibility == "caller-owned-returned-cleanup";
            }
        )
    );
    assert(
        std::any_of(
            dynamic_array_returned_nested_aggregate_field_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
                .plans.begin(),
            dynamic_array_returned_nested_aggregate_field_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
                .plans.end(),
            [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
                return plan.owner_name == "returned.inner.values" &&
                    plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::returned_binding &&
                    plan.cleanup_responsibility == "caller-owned-returned-cleanup";
            }
        )
    );
    assert(
        std::any_of(
            dynamic_array_returned_nested_aggregate_field_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
                .plans.begin(),
            dynamic_array_returned_nested_aggregate_field_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
                .plans.end(),
            [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
                return plan.owner_name == "items" &&
                    plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::parameter_binding &&
                    plan.cleanup_responsibility == "callee-owned-parameter-cleanup";
            }
        )
    );
    assert(
        dynamic_array_returned_nested_aggregate_field_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
            .origin_blockers.empty()
    );
    assert(dynamic_array_returned_nested_aggregate_field_forwarding_ir.dynamic_array_cleanup_capability_proven);
    assert(
        dynamic_array_returned_nested_aggregate_field_forwarding_ir.dynamic_array_cleanup_emission_capability_state
            .proven
    );
    assert(
        std::find(
            dynamic_array_returned_nested_aggregate_field_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .function_symbol_names.begin(),
            dynamic_array_returned_nested_aggregate_field_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .function_symbol_names.end(),
            "consume_items"
        ) != dynamic_array_returned_nested_aggregate_field_forwarding_ir.dynamic_array_cleanup_emission_capability_state
            .function_symbol_names.end()
    );
    assert(
        std::find(
            dynamic_array_returned_nested_aggregate_field_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .cleanup_owner_names.begin(),
            dynamic_array_returned_nested_aggregate_field_forwarding_ir.dynamic_array_cleanup_emission_capability_state
                .cleanup_owner_names.end(),
            "items"
        ) != dynamic_array_returned_nested_aggregate_field_forwarding_ir.dynamic_array_cleanup_emission_capability_state
            .cleanup_owner_names.end()
    );
    assert(
        dynamic_array_returned_nested_aggregate_field_forwarding_ir.ir_text.find(
            "%inner.values.dynamic_array_cleanup"
        ) == std::string::npos
    );
    assert(
        dynamic_array_returned_nested_aggregate_field_forwarding_ir.ir_text.find(
            "%returned.inner.values.dynamic_array_cleanup"
        ) == std::string::npos
    );
    auto const nested_aggregate_field_make_start =
        dynamic_array_returned_nested_aggregate_field_forwarding_ir.ir_text.find(
            "define %record.OuterBox @make_outer_box"
        );
    auto const nested_aggregate_field_consume_start =
        dynamic_array_returned_nested_aggregate_field_forwarding_ir.ir_text.find(
            "define i32 @consume_items",
            nested_aggregate_field_make_start
        );
    assert(nested_aggregate_field_make_start != std::string::npos);
    assert(nested_aggregate_field_consume_start != std::string::npos);
    assert(
        dynamic_array_returned_nested_aggregate_field_forwarding_ir.ir_text.find(
            "__orison_dynamic_array_deallocate",
            nested_aggregate_field_make_start
        ) > nested_aggregate_field_consume_start
    );
    auto const nested_aggregate_field_main_start =
        dynamic_array_returned_nested_aggregate_field_forwarding_ir.ir_text.find("define i32 @main");
    assert(nested_aggregate_field_main_start != std::string::npos);
    assert(
        dynamic_array_returned_nested_aggregate_field_forwarding_ir.ir_text.find(
            "__orison_dynamic_array_deallocate",
            nested_aggregate_field_main_start
        ) == std::string::npos
    );
    assert(
        orison::pipeline::dynamic_array_cleanup_production_ready(
            dynamic_array_returned_nested_aggregate_field_forwarding_ir.dynamic_array_cleanup_production_readiness
        )
    );
    auto dynamic_array_returned_nested_aggregate_field_forwarding_object = pipeline.emit_object(
        dynamic_array_returned_nested_aggregate_field_forwarding_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_nested_aggregate_field_forwarding_object.has_errors());
    assert(!dynamic_array_returned_nested_aggregate_field_forwarding_object.object_bytes.empty());
    auto dynamic_array_returned_nested_aggregate_field_forwarding_executable =
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_forwarding_run";
    auto dynamic_array_returned_nested_aggregate_field_forwarding_link = orison::link::HostLinker {}.link(
        dynamic_array_returned_nested_aggregate_field_forwarding_object.object_bytes,
        dynamic_array_returned_nested_aggregate_field_forwarding_executable
    );
    assert(!dynamic_array_returned_nested_aggregate_field_forwarding_link.has_errors());
    auto dynamic_array_returned_nested_aggregate_field_forwarding_status =
        std::system(dynamic_array_returned_nested_aggregate_field_forwarding_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_returned_nested_aggregate_field_forwarding_status));
    assert(WEXITSTATUS(dynamic_array_returned_nested_aggregate_field_forwarding_status) == 0);

    auto dynamic_array_returned_nested_aggregate_field_branch_forwarding_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_nested_aggregate_field_branch_forwarding_run.or";
    auto dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir = pipeline.emit_llvm(
        dynamic_array_returned_nested_aggregate_field_branch_forwarding_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir.has_errors());
    auto const nested_aggregate_field_branch_lifetime_plans = std::count_if(
        dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
            .plans.begin(),
        dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
            .plans.end(),
        [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
            return plan.source_type_name == "DynamicArray<Payload>" &&
                plan.cleanup_plan_available;
        }
    );
    assert(nested_aggregate_field_branch_lifetime_plans == 5);
    auto const nested_aggregate_field_branch_parameter_lifetime_plans = std::count_if(
        dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
            .plans.begin(),
        dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
            .plans.end(),
        [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
            return plan.owner_name == "items" &&
                plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::parameter_binding &&
                plan.cleanup_responsibility == "callee-owned-parameter-cleanup";
        }
    );
    assert(nested_aggregate_field_branch_parameter_lifetime_plans == 2);
    assert(
        std::any_of(
            dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir
                .dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
            dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir
                .dynamic_array_descriptor_lifetime_plan_state.plans.end(),
            [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
                return plan.owner_name == "inner.values" &&
                    plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::returned_binding &&
                    plan.cleanup_responsibility == "caller-owned-returned-cleanup";
            }
        )
    );
    assert(
        std::any_of(
            dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir
                .dynamic_array_descriptor_lifetime_plan_state.plans.begin(),
            dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir
                .dynamic_array_descriptor_lifetime_plan_state.plans.end(),
            [](orison::pipeline::DynamicArrayDescriptorLifetimePlan const& plan) {
                return plan.owner_name == "returned.inner.values" &&
                    plan.origin_kind == orison::semantics::DynamicArrayDescriptorOriginKind::returned_binding &&
                    plan.cleanup_responsibility == "caller-owned-returned-cleanup";
            }
        )
    );
    assert(
        dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir.dynamic_array_descriptor_lifetime_plan_state
            .origin_blockers.empty()
    );
    assert(dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir.dynamic_array_cleanup_capability_proven);
    assert(
        dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir
            .dynamic_array_cleanup_emission_capability_state.proven
    );
    assert(
        std::find(
            dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir
                .dynamic_array_cleanup_emission_capability_state.function_symbol_names.begin(),
            dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir
                .dynamic_array_cleanup_emission_capability_state.function_symbol_names.end(),
            "consume_items"
        ) != dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir
            .dynamic_array_cleanup_emission_capability_state.function_symbol_names.end()
    );
    assert(
        std::find(
            dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir
                .dynamic_array_cleanup_emission_capability_state.cleanup_owner_names.begin(),
            dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir
                .dynamic_array_cleanup_emission_capability_state.cleanup_owner_names.end(),
            "items"
        ) != dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir
            .dynamic_array_cleanup_emission_capability_state.cleanup_owner_names.end()
    );
    assert(
        dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir.ir_text.find(
            "%inner.values.dynamic_array_cleanup"
        ) == std::string::npos
    );
    assert(
        dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir.ir_text.find(
            "%returned.inner.values.dynamic_array_cleanup"
        ) == std::string::npos
    );
    auto const nested_aggregate_field_branch_make_start =
        dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir.ir_text.find(
            "define %record.OuterBox @make_outer_box"
        );
    auto const nested_aggregate_field_branch_consume_start =
        dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir.ir_text.find(
            "define i32 @consume_items",
            nested_aggregate_field_branch_make_start
        );
    assert(nested_aggregate_field_branch_make_start != std::string::npos);
    assert(nested_aggregate_field_branch_consume_start != std::string::npos);
    assert(
        dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir.ir_text.find(
            "__orison_dynamic_array_deallocate",
            nested_aggregate_field_branch_make_start
        ) > nested_aggregate_field_branch_consume_start
    );
    auto const nested_aggregate_field_branch_choose_start =
        dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir.ir_text.find("define i32 @choose_items");
    auto const nested_aggregate_field_branch_main_start =
        dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir.ir_text.find(
            "define i32 @main",
            nested_aggregate_field_branch_choose_start
        );
    assert(nested_aggregate_field_branch_choose_start != std::string::npos);
    assert(nested_aggregate_field_branch_main_start != std::string::npos);
    assert(
        dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir.ir_text.find(
            "__orison_dynamic_array_deallocate",
            nested_aggregate_field_branch_choose_start
        ) > nested_aggregate_field_branch_main_start
    );
    assert(
        dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir.ir_text.find(
            "__orison_dynamic_array_deallocate",
            nested_aggregate_field_branch_main_start
        ) == std::string::npos
    );
    assert(
        orison::pipeline::dynamic_array_cleanup_production_ready(
            dynamic_array_returned_nested_aggregate_field_branch_forwarding_ir
                .dynamic_array_cleanup_production_readiness
        )
    );
    auto dynamic_array_returned_nested_aggregate_field_branch_forwarding_object = pipeline.emit_object(
        dynamic_array_returned_nested_aggregate_field_branch_forwarding_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_nested_aggregate_field_branch_forwarding_object.has_errors());
    assert(!dynamic_array_returned_nested_aggregate_field_branch_forwarding_object.object_bytes.empty());
    auto dynamic_array_returned_nested_aggregate_field_branch_forwarding_executable =
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_branch_forwarding_run";
    auto dynamic_array_returned_nested_aggregate_field_branch_forwarding_link = orison::link::HostLinker {}.link(
        dynamic_array_returned_nested_aggregate_field_branch_forwarding_object.object_bytes,
        dynamic_array_returned_nested_aggregate_field_branch_forwarding_executable
    );
    assert(!dynamic_array_returned_nested_aggregate_field_branch_forwarding_link.has_errors());
    auto dynamic_array_returned_nested_aggregate_field_branch_forwarding_status =
        std::system(dynamic_array_returned_nested_aggregate_field_branch_forwarding_executable.string().c_str());
    assert(WIFEXITED(dynamic_array_returned_nested_aggregate_field_branch_forwarding_status));
    assert(WEXITSTATUS(dynamic_array_returned_nested_aggregate_field_branch_forwarding_status) == 0);

    auto dynamic_array_returned_aggregate_field_choice_payload_forwarding_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_aggregate_field_choice_payload_forwarding_run.or";
    auto dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir = pipeline.emit_llvm(
        dynamic_array_returned_aggregate_field_choice_payload_forwarding_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir.has_errors());
    assert(dynamic_array_payload_lifetime_plan_count(
        dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir
    ) == 5);
    assert(dynamic_array_payload_parameter_lifetime_plan_count(
        dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir,
        "items"
    ) == 1);
    assert_dynamic_array_payload_returned_lifetime_owner(
        dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir,
        "returned.values"
    );
    assert_dynamic_array_payload_returned_lifetime_owner(
        dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir,
        "unwrapped"
    );
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir);
    assert(
        std::find(
            dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir
                .dynamic_array_cleanup_emission_capability_state.function_symbol_names.begin(),
            dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir
                .dynamic_array_cleanup_emission_capability_state.function_symbol_names.end(),
            "consume_items"
        ) != dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir
            .dynamic_array_cleanup_emission_capability_state.function_symbol_names.end()
    );
    assert(
        std::find(
            dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir
                .dynamic_array_cleanup_emission_capability_state.cleanup_owner_names.begin(),
            dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir
                .dynamic_array_cleanup_emission_capability_state.cleanup_owner_names.end(),
            "items"
        ) != dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir
            .dynamic_array_cleanup_emission_capability_state.cleanup_owner_names.end()
    );
    assert(
        dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir.ir_text.find(
            "%returned.values.dynamic_array_cleanup"
        ) == std::string::npos
    );
    assert(
        dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir.ir_text.find(
            "%unwrapped.dynamic_array_cleanup"
        ) == std::string::npos
    );
    assert(
        dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir.ir_text.find(
            ".choice_dynamic_array_cleanup0.cleanup.entry"
        ) == std::string::npos
    );
    auto const aggregate_field_choice_payload_unwrap_start =
        dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir.ir_text.find(
            "define { ptr, i64, i64 } @unwrap_payload"
        );
    auto const aggregate_field_choice_payload_consume_start =
        dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir.ir_text.find(
            "define i32 @consume_items",
            aggregate_field_choice_payload_unwrap_start
        );
    assert(aggregate_field_choice_payload_unwrap_start != std::string::npos);
    assert(aggregate_field_choice_payload_consume_start != std::string::npos);
    assert(
        dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir.ir_text.find(
            "__orison_dynamic_array_deallocate",
            aggregate_field_choice_payload_unwrap_start
        ) > aggregate_field_choice_payload_consume_start
    );
    assert_no_deallocate_after_function(
        dynamic_array_returned_aggregate_field_choice_payload_forwarding_ir.ir_text,
        "define i32 @main"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_returned_aggregate_field_choice_payload_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_choice_payload_forwarding_run"
    );

    auto dynamic_array_returned_aggregate_field_stored_choice_payload_forwarding_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_aggregate_field_stored_choice_payload_forwarding_run.or";
    auto dynamic_array_returned_aggregate_field_stored_choice_payload_forwarding_ir = pipeline.emit_llvm(
        dynamic_array_returned_aggregate_field_stored_choice_payload_forwarding_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_aggregate_field_stored_choice_payload_forwarding_ir.has_errors());
    assert(dynamic_array_payload_lifetime_plan_count(
        dynamic_array_returned_aggregate_field_stored_choice_payload_forwarding_ir
    ) == 5);
    assert_dynamic_array_payload_returned_lifetime_owner(
        dynamic_array_returned_aggregate_field_stored_choice_payload_forwarding_ir,
        "returned.values"
    );
    assert_dynamic_array_payload_returned_lifetime_owner(
        dynamic_array_returned_aggregate_field_stored_choice_payload_forwarding_ir,
        "unwrapped"
    );
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_returned_aggregate_field_stored_choice_payload_forwarding_ir
    );
    assert(
        dynamic_array_returned_aggregate_field_stored_choice_payload_forwarding_ir.ir_text.find(
            "%packet.addr = alloca { i32, { ptr, i64, i64 } }"
        ) != std::string::npos
    );
    assert(
        dynamic_array_returned_aggregate_field_stored_choice_payload_forwarding_ir.ir_text.find(
            "%packet.Primary.values.choice_dynamic_array_cleanup0.cleanup.entry"
        ) == std::string::npos
    );
    assert_no_deallocate_after_function(
        dynamic_array_returned_aggregate_field_stored_choice_payload_forwarding_ir.ir_text,
        "define i32 @main"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_returned_aggregate_field_stored_choice_payload_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_stored_choice_payload_forwarding_run"
    );

    auto dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding_run.or";
    auto dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding_ir = pipeline.emit_llvm(
        dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding_ir.has_errors());
    assert(dynamic_array_payload_lifetime_plan_count(
        dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding_ir
    ) == 6);
    assert(dynamic_array_payload_parameter_lifetime_plan_count(
        dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding_ir,
        "items"
    ) == 2);
    assert_dynamic_array_payload_returned_lifetime_owner(
        dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding_ir,
        "returned.values"
    );
    assert_dynamic_array_payload_returned_lifetime_owner(
        dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding_ir,
        "unwrapped"
    );
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding_ir
    );
    assert(
        dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding_ir.ir_text.find(
            "%packet.addr = alloca { i32, { ptr, i64, i64 } }"
        ) != std::string::npos
    );
    assert(
        dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding_ir.ir_text.find(
            "%packet.Primary.values.choice_dynamic_array_cleanup0.cleanup.entry"
        ) == std::string::npos
    );
    auto const aggregate_field_stored_choice_payload_branch_choose_start =
        dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding_ir.ir_text.find(
            "define i32 @choose_items"
        );
    auto const aggregate_field_stored_choice_payload_branch_main_start =
        dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding_ir.ir_text.find(
            "define i32 @main",
            aggregate_field_stored_choice_payload_branch_choose_start
        );
    assert(aggregate_field_stored_choice_payload_branch_choose_start != std::string::npos);
    assert(aggregate_field_stored_choice_payload_branch_main_start != std::string::npos);
    assert(
        dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding_ir.ir_text.find(
            "__orison_dynamic_array_deallocate",
            aggregate_field_stored_choice_payload_branch_choose_start
        ) > aggregate_field_stored_choice_payload_branch_main_start
    );
    assert(
        dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding_ir.ir_text.find(
            "__orison_dynamic_array_deallocate",
            aggregate_field_stored_choice_payload_branch_main_start
        ) == std::string::npos
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_aggregate_field_stored_choice_payload_branch_forwarding_run"
    );

    auto dynamic_array_returned_nested_aggregate_field_stored_choice_payload_forwarding_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_nested_aggregate_field_stored_choice_payload_forwarding_run.or";
    auto dynamic_array_returned_nested_aggregate_field_stored_choice_payload_forwarding_ir = pipeline.emit_llvm(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_forwarding_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_returned_nested_aggregate_field_stored_choice_payload_forwarding_ir.has_errors());
    assert(dynamic_array_payload_lifetime_plan_count(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_forwarding_ir
    ) == 6);
    assert_dynamic_array_payload_returned_lifetime_owner(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_forwarding_ir,
        "inner.values"
    );
    assert_dynamic_array_payload_returned_lifetime_owner(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_forwarding_ir,
        "returned.inner.values"
    );
    assert_dynamic_array_payload_returned_lifetime_owner(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_forwarding_ir,
        "unwrapped"
    );
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_forwarding_ir
    );
    assert(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_forwarding_ir.ir_text.find(
            "%record.OuterBox = type { %record.PayloadBox }"
        ) != std::string::npos
    );
    assert(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_forwarding_ir.ir_text.find(
            "%packet.addr = alloca { i32, { ptr, i64, i64 } }"
        ) != std::string::npos
    );
    assert(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_forwarding_ir.ir_text.find(
            "%packet.Primary.values.choice_dynamic_array_cleanup0.cleanup.entry"
        ) == std::string::npos
    );
    assert_no_deallocate_after_function(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_forwarding_ir.ir_text,
        "define i32 @main"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_forwarding_path,
        smoke_temp_root / "dynamic_array_returned_nested_aggregate_field_stored_choice_payload_forwarding_run"
    );

    auto dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_run.or";
    auto dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_ir =
        pipeline.emit_llvm(
            dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_path,
            orison::pipeline::CompilePipelineOptions {
                .source_drop_lowering_enabled = true,
                .dynamic_array_descriptor_cleanup_planning_enabled = true,
            }
        );
    assert(!dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_ir.has_errors());
    assert(dynamic_array_payload_lifetime_plan_count(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_ir
    ) == 7);
    assert(dynamic_array_payload_parameter_lifetime_plan_count(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_ir,
        "items"
    ) == 2);
    assert_dynamic_array_payload_returned_lifetime_owner(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_ir,
        "inner.values"
    );
    assert_dynamic_array_payload_returned_lifetime_owner(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_ir,
        "returned.inner.values"
    );
    assert_dynamic_array_payload_returned_lifetime_owner(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_ir,
        "unwrapped"
    );
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_ir
    );
    assert(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_ir.ir_text.find(
            "%record.OuterBox = type { %record.PayloadBox }"
        ) != std::string::npos
    );
    assert(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_ir.ir_text.find(
            "%packet.addr = alloca { i32, { ptr, i64, i64 } }"
        ) != std::string::npos
    );
    assert(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_ir.ir_text.find(
            "%packet.Primary.values.choice_dynamic_array_cleanup0.cleanup.entry"
        ) == std::string::npos
    );
    auto const nested_aggregate_field_stored_choice_payload_branch_choose_start =
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_ir.ir_text.find(
            "define i32 @choose_items"
        );
    auto const nested_aggregate_field_stored_choice_payload_branch_main_start =
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_ir.ir_text.find(
            "define i32 @main",
            nested_aggregate_field_stored_choice_payload_branch_choose_start
        );
    assert(nested_aggregate_field_stored_choice_payload_branch_choose_start != std::string::npos);
    assert(nested_aggregate_field_stored_choice_payload_branch_main_start != std::string::npos);
    assert(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_ir.ir_text.find(
            "__orison_dynamic_array_deallocate",
            nested_aggregate_field_stored_choice_payload_branch_choose_start
        ) > nested_aggregate_field_stored_choice_payload_branch_main_start
    );
    assert_no_deallocate_after_function(
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_ir.ir_text,
        "define i32 @main"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_path,
        smoke_temp_root /
            "dynamic_array_returned_nested_aggregate_field_stored_choice_payload_branch_forwarding_run"
    );

    auto dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_branch_forwarding_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_branch_forwarding_run.or";
    auto dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_branch_forwarding_ir =
        pipeline.emit_llvm(
            dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_branch_forwarding_path,
            orison::pipeline::CompilePipelineOptions {
                .source_drop_lowering_enabled = true,
                .dynamic_array_descriptor_cleanup_planning_enabled = true,
            }
        );
    assert(!dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_branch_forwarding_ir.has_errors());
    assert(
        dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_branch_forwarding_ir
            .ir_text.find("right.Primary.values.choice_dynamic_array_cleanup") != std::string::npos
    );
    assert(
        dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_branch_forwarding_ir
            .ir_text.find("left.Primary.values.choice_dynamic_array_cleanup") != std::string::npos
    );
    assert(
        dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_branch_forwarding_ir
            .ir_text.find(
                "if branch ownership mismatch: owned transfers must match across all continuing branches"
            ) == std::string::npos
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_branch_forwarding_path,
        smoke_temp_root /
            "dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_branch_forwarding_run"
    );

    auto dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_switch_forwarding_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_switch_forwarding_run.or";
    auto dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_switch_forwarding_ir =
        pipeline.emit_llvm(
            dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_switch_forwarding_path,
            orison::pipeline::CompilePipelineOptions {
                .source_drop_lowering_enabled = true,
                .dynamic_array_descriptor_cleanup_planning_enabled = true,
            }
        );
    assert(!dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_switch_forwarding_ir.has_errors());
    assert(
        dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_switch_forwarding_ir
            .ir_text.find("right.Primary.values.choice_dynamic_array_cleanup") != std::string::npos
    );
    assert(
        dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_switch_forwarding_ir
            .ir_text.find("left.Primary.values.choice_dynamic_array_cleanup") != std::string::npos
    );
    assert(
        dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_switch_forwarding_ir
            .ir_text.find(
                "switch case ownership mismatch: owned transfers must match across all continuing cases"
            ) == std::string::npos
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_switch_forwarding_path,
        smoke_temp_root /
            "dynamic_array_returned_nested_aggregate_field_distinct_stored_choice_payload_switch_forwarding_run"
    );

    auto dynamic_array_local_final_if_branch_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_local_final_if_branch_cleanup_run.or";
    auto dynamic_array_local_final_if_branch_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_local_final_if_branch_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_local_final_if_branch_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_local_final_if_branch_cleanup_ir);
    assert_branch_local_named_dynamic_array_cleanup_ir(
        dynamic_array_local_final_if_branch_cleanup_ir.ir_text,
        "left_values",
        "right_values",
        "i32"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_local_final_if_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_local_final_if_branch_cleanup_run"
    );

    auto dynamic_array_local_final_switch_case_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_local_final_switch_case_cleanup_run.or";
    auto dynamic_array_local_final_switch_case_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_local_final_switch_case_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_local_final_switch_case_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_local_final_switch_case_cleanup_ir);
    assert_ir_contains(dynamic_array_local_final_switch_case_cleanup_ir.ir_text, "switch i1 %flag");
    assert_branch_local_named_dynamic_array_cleanup_ir(
        dynamic_array_local_final_switch_case_cleanup_ir.ir_text,
        "left_values",
        "right_values",
        "i32"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_local_final_switch_case_cleanup_path,
        smoke_temp_root / "dynamic_array_local_final_switch_case_cleanup_run"
    );

    auto dynamic_array_owned_result_final_if_branch_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_final_if_branch_cleanup_run.or";
    auto dynamic_array_owned_result_final_if_branch_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_final_if_branch_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_final_if_branch_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_owned_result_final_if_branch_cleanup_ir);
    assert_ir_contains(
        dynamic_array_owned_result_final_if_branch_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i1 %flag)"
    );
    assert_branch_local_scratch_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_final_if_branch_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_final_if_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_final_if_branch_cleanup_run"
    );

    auto dynamic_array_owned_result_final_switch_case_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_final_switch_case_cleanup_run.or";
    auto dynamic_array_owned_result_final_switch_case_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_final_switch_case_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_final_switch_case_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_owned_result_final_switch_case_cleanup_ir);
    assert_ir_contains(
        dynamic_array_owned_result_final_switch_case_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i1 %flag)"
    );
    assert_ir_contains(dynamic_array_owned_result_final_switch_case_cleanup_ir.ir_text, "switch i1 %flag");
    assert_branch_local_scratch_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_final_switch_case_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_final_switch_case_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_final_switch_case_cleanup_run"
    );

    auto dynamic_array_owned_result_returned_local_final_if_branch_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_returned_local_final_if_branch_cleanup_run.or";
    auto dynamic_array_owned_result_returned_local_final_if_branch_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_returned_local_final_if_branch_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_returned_local_final_if_branch_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_returned_local_final_if_branch_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_returned_local_final_if_branch_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i1 %flag)"
    );
    assert_branch_local_returned_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_returned_local_final_if_branch_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_returned_local_final_if_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_returned_local_final_if_branch_cleanup_run"
    );

    auto dynamic_array_owned_result_returned_local_final_switch_case_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_returned_local_final_switch_case_cleanup_run.or";
    auto dynamic_array_owned_result_returned_local_final_switch_case_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_returned_local_final_switch_case_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_returned_local_final_switch_case_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_returned_local_final_switch_case_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_returned_local_final_switch_case_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i1 %flag)"
    );
    assert_ir_contains(
        dynamic_array_owned_result_returned_local_final_switch_case_cleanup_ir.ir_text,
        "switch i1 %flag"
    );
    assert_branch_local_returned_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_returned_local_final_switch_case_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_returned_local_final_switch_case_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_returned_local_final_switch_case_cleanup_run"
    );

    auto dynamic_array_owned_result_nested_final_if_branch_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_nested_final_if_branch_cleanup_run.or";
    auto dynamic_array_owned_result_nested_final_if_branch_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_nested_final_if_branch_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_nested_final_if_branch_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_nested_final_if_branch_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_nested_final_if_branch_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i1 %outer, i1 %inner)"
    );
    assert_branch_local_returned_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_nested_final_if_branch_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_nested_final_if_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_nested_final_if_branch_cleanup_run"
    );

    auto dynamic_array_owned_result_nested_final_switch_case_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_nested_final_switch_case_cleanup_run.or";
    auto dynamic_array_owned_result_nested_final_switch_case_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_nested_final_switch_case_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_nested_final_switch_case_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_nested_final_switch_case_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_nested_final_switch_case_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i1 %outer, i1 %inner)"
    );
    assert_ir_contains(dynamic_array_owned_result_nested_final_switch_case_cleanup_ir.ir_text, "switch i1 %outer");
    assert_ir_contains(dynamic_array_owned_result_nested_final_switch_case_cleanup_ir.ir_text, "switch i1 %inner");
    assert_branch_local_returned_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_nested_final_switch_case_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_nested_final_switch_case_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_nested_final_switch_case_cleanup_run"
    );

    auto dynamic_array_owned_result_if_switch_branch_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_if_switch_branch_cleanup_run.or";
    auto dynamic_array_owned_result_if_switch_branch_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_if_switch_branch_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_if_switch_branch_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_owned_result_if_switch_branch_cleanup_ir);
    assert_ir_contains(
        dynamic_array_owned_result_if_switch_branch_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i1 %outer, i1 %inner)"
    );
    assert_ir_contains(dynamic_array_owned_result_if_switch_branch_cleanup_ir.ir_text, "switch i1 %inner");
    assert_branch_local_returned_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_if_switch_branch_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_if_switch_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_if_switch_branch_cleanup_run"
    );

    auto dynamic_array_owned_result_switch_if_branch_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_switch_if_branch_cleanup_run.or";
    auto dynamic_array_owned_result_switch_if_branch_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_switch_if_branch_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_switch_if_branch_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_owned_result_switch_if_branch_cleanup_ir);
    assert_ir_contains(
        dynamic_array_owned_result_switch_if_branch_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i1 %outer, i1 %inner)"
    );
    assert_ir_contains(dynamic_array_owned_result_switch_if_branch_cleanup_ir.ir_text, "switch i1 %outer");
    assert_branch_local_returned_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_switch_if_branch_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_switch_if_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_switch_if_branch_cleanup_run"
    );

    auto dynamic_array_owned_result_direct_nested_if_branch_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_direct_nested_if_branch_cleanup_run.or";
    auto dynamic_array_owned_result_direct_nested_if_branch_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_direct_nested_if_branch_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_direct_nested_if_branch_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_owned_result_direct_nested_if_branch_cleanup_ir);
    assert_ir_contains(
        dynamic_array_owned_result_direct_nested_if_branch_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i1 %outer, i1 %inner)"
    );
    assert_branch_local_returned_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_direct_nested_if_branch_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_direct_nested_if_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_direct_nested_if_branch_cleanup_run"
    );

    auto dynamic_array_owned_result_direct_nested_switch_branch_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_direct_nested_switch_branch_cleanup_run.or";
    auto dynamic_array_owned_result_direct_nested_switch_branch_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_direct_nested_switch_branch_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_direct_nested_switch_branch_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_owned_result_direct_nested_switch_branch_cleanup_ir);
    assert_ir_contains(
        dynamic_array_owned_result_direct_nested_switch_branch_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i1 %outer, i1 %inner)"
    );
    assert_ir_contains(
        dynamic_array_owned_result_direct_nested_switch_branch_cleanup_ir.ir_text,
        "switch i1 %inner"
    );
    assert_branch_local_returned_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_direct_nested_switch_branch_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_direct_nested_switch_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_direct_nested_switch_branch_cleanup_run"
    );

    auto dynamic_array_owned_result_three_case_switch_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_three_case_switch_cleanup_run.or";
    auto dynamic_array_owned_result_three_case_switch_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_three_case_switch_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_three_case_switch_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_owned_result_three_case_switch_cleanup_ir);
    assert_ir_contains(
        dynamic_array_owned_result_three_case_switch_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i32 %selector)"
    );
    assert_ir_contains(dynamic_array_owned_result_three_case_switch_cleanup_ir.ir_text, "switch i32 %selector");
    assert_branch_local_three_case_returned_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_three_case_switch_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_three_case_switch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_three_case_switch_cleanup_run"
    );

    auto dynamic_array_owned_result_three_case_mixed_switch_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_three_case_mixed_switch_cleanup_run.or";
    auto dynamic_array_owned_result_three_case_mixed_switch_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_three_case_mixed_switch_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_three_case_mixed_switch_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_owned_result_three_case_mixed_switch_cleanup_ir);
    assert_ir_contains(
        dynamic_array_owned_result_three_case_mixed_switch_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i32 %selector, i1 %flag)"
    );
    assert_ir_contains(
        dynamic_array_owned_result_three_case_mixed_switch_cleanup_ir.ir_text,
        "switch i32 %selector"
    );
    assert_branch_local_three_case_mixed_switch_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_three_case_mixed_switch_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_three_case_mixed_switch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_three_case_mixed_switch_cleanup_run"
    );

    auto dynamic_array_owned_result_three_case_nested_switch_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_three_case_nested_switch_cleanup_run.or";
    auto dynamic_array_owned_result_three_case_nested_switch_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_three_case_nested_switch_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_three_case_nested_switch_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_owned_result_three_case_nested_switch_cleanup_ir);
    assert_ir_contains(
        dynamic_array_owned_result_three_case_nested_switch_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i32 %selector, i1 %inner)"
    );
    assert_ir_contains(
        dynamic_array_owned_result_three_case_nested_switch_cleanup_ir.ir_text,
        "switch i32 %selector"
    );
    assert_ir_contains(
        dynamic_array_owned_result_three_case_nested_switch_cleanup_ir.ir_text,
        "switch i1 %inner"
    );
    assert_branch_local_three_case_mixed_switch_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_three_case_nested_switch_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_three_case_nested_switch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_three_case_nested_switch_cleanup_run"
    );

    auto dynamic_array_owned_result_multi_nested_switch_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_multi_nested_switch_cleanup_run.or";
    auto dynamic_array_owned_result_multi_nested_switch_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_multi_nested_switch_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_multi_nested_switch_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_owned_result_multi_nested_switch_cleanup_ir);
    assert_ir_contains(
        dynamic_array_owned_result_multi_nested_switch_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i32 %selector, i1 %left_selector, i1 %right_selector)"
    );
    assert_ir_contains(
        dynamic_array_owned_result_multi_nested_switch_cleanup_ir.ir_text,
        "switch i32 %selector"
    );
    assert_ir_contains(
        dynamic_array_owned_result_multi_nested_switch_cleanup_ir.ir_text,
        "switch i1 %left_selector"
    );
    assert_ir_contains(
        dynamic_array_owned_result_multi_nested_switch_cleanup_ir.ir_text,
        "switch i1 %right_selector"
    );
    assert_branch_local_multi_nested_switch_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_multi_nested_switch_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_multi_nested_switch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_multi_nested_switch_cleanup_run"
    );

    auto dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_run.or";
    auto dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i32 %selector, i1 %left_selector, i1 %right_selector)"
    );
    assert_ir_contains(
        dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_ir.ir_text,
        "switch i32 %selector"
    );
    assert_ir_contains(
        dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_ir.ir_text,
        "switch i1 %left_selector"
    );
    assert_ir_contains(
        dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_ir.ir_text,
        "switch i1 %right_selector"
    );
    assert_ir_contains(
        dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_ir.ir_text,
        "%values.dynamic_array_cleanup"
    );
    assert_branch_local_multi_nested_switch_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_multi_nested_switch_helper_call_cleanup_run"
    );

    auto dynamic_array_owned_result_if_two_switches_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_if_two_switches_cleanup_run.or";
    auto dynamic_array_owned_result_if_two_switches_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_if_two_switches_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_if_two_switches_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_owned_result_if_two_switches_cleanup_ir);
    assert_ir_contains(
        dynamic_array_owned_result_if_two_switches_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i1 %outer, i1 %left_selector, i1 %right_selector)"
    );
    assert_ir_contains(
        dynamic_array_owned_result_if_two_switches_cleanup_ir.ir_text,
        "switch i1 %left_selector"
    );
    assert_ir_contains(
        dynamic_array_owned_result_if_two_switches_cleanup_ir.ir_text,
        "switch i1 %right_selector"
    );
    assert_branch_local_if_two_switches_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_if_two_switches_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_if_two_switches_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_if_two_switches_cleanup_run"
    );

    auto dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_run.or";
    auto dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i1 %outer, i1 %left_selector, i1 %right_selector)"
    );
    assert_ir_contains(dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_ir.ir_text, "br i1 %outer");
    assert_ir_contains(
        dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_ir.ir_text,
        "switch i1 %left_selector"
    );
    assert_ir_contains(
        dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_ir.ir_text,
        "switch i1 %right_selector"
    );
    assert_ir_contains(
        dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_ir.ir_text,
        "call i32 @consume_items({ ptr, i64, i64 } %tmp"
    );
    assert_branch_local_dynamic_array_cleanup_for_owners_ir(
        dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_ir.ir_text,
        {"first_right_scratch", "second_left_scratch", "second_right_scratch"}
    );
    assert_no_branch_local_dynamic_array_cleanup_for_owners_ir(
        dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_ir.ir_text,
        {"first_left_scratch", "first_left_values", "first_right_values", "second_left_values", "second_right_values"}
    );
    assert_ir_excludes(
        dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_ir.ir_text,
        "if branch ownership mismatch"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_ir.ir_text,
        "switch case ownership mismatch"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_if_two_switches_consumed_scratch_cleanup_run"
    );

    auto dynamic_array_owned_result_if_two_switches_consumed_scratch_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_if_two_switches_consumed_scratch_reuse_rejected.or";
    auto dynamic_array_owned_result_if_two_switches_consumed_scratch_reuse_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_if_two_switches_consumed_scratch_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(dynamic_array_owned_result_if_two_switches_consumed_scratch_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_if_two_switches_consumed_scratch_reuse_ir.error_text.find(
            "use after move: first_left_scratch"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_if_two_switches_helper_call_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_if_two_switches_helper_call_cleanup_run.or";
    auto dynamic_array_owned_result_if_two_switches_helper_call_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_if_two_switches_helper_call_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_if_two_switches_helper_call_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_owned_result_if_two_switches_helper_call_cleanup_ir);
    assert_ir_contains(
        dynamic_array_owned_result_if_two_switches_helper_call_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i1 %outer, i1 %left_selector, i1 %right_selector)"
    );
    assert_ir_contains(dynamic_array_owned_result_if_two_switches_helper_call_cleanup_ir.ir_text, "br i1 %outer");
    assert_ir_contains(
        dynamic_array_owned_result_if_two_switches_helper_call_cleanup_ir.ir_text,
        "switch i1 %left_selector"
    );
    assert_ir_contains(
        dynamic_array_owned_result_if_two_switches_helper_call_cleanup_ir.ir_text,
        "switch i1 %right_selector"
    );
    assert_ir_contains(
        dynamic_array_owned_result_if_two_switches_helper_call_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_if_two_switches_helper_call_cleanup_ir.ir_text,
        "%values.dynamic_array_cleanup"
    );
    assert_branch_local_if_two_switches_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_if_two_switches_helper_call_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_if_two_switches_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_if_two_switches_helper_call_cleanup_run"
    );

    auto dynamic_array_owned_result_switch_two_ifs_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_switch_two_ifs_cleanup_run.or";
    auto dynamic_array_owned_result_switch_two_ifs_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_switch_two_ifs_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_switch_two_ifs_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_owned_result_switch_two_ifs_cleanup_ir);
    assert_ir_contains(
        dynamic_array_owned_result_switch_two_ifs_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i32 %selector, i1 %left_selector, i1 %right_selector)"
    );
    assert_ir_contains(
        dynamic_array_owned_result_switch_two_ifs_cleanup_ir.ir_text,
        "switch i32 %selector"
    );
    assert_ir_contains(
        dynamic_array_owned_result_switch_two_ifs_cleanup_ir.ir_text,
        "br i1 %left_selector"
    );
    assert_ir_contains(
        dynamic_array_owned_result_switch_two_ifs_cleanup_ir.ir_text,
        "br i1 %right_selector"
    );
    assert_branch_local_switch_two_ifs_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_switch_two_ifs_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_switch_two_ifs_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_switch_two_ifs_cleanup_run"
    );

    auto dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_run.or";
    auto dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_ir);
    assert_ir_contains(
        dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i32 %selector, i1 %left_selector, i1 %right_selector)"
    );
    assert_ir_contains(
        dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_ir.ir_text,
        "switch i32 %selector"
    );
    assert_ir_contains(
        dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_ir.ir_text,
        "br i1 %left_selector"
    );
    assert_ir_contains(
        dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_ir.ir_text,
        "br i1 %right_selector"
    );
    assert_ir_contains(
        dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_ir.ir_text,
        "%values.dynamic_array_cleanup"
    );
    assert_branch_local_switch_two_ifs_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_switch_two_ifs_helper_call_cleanup_run"
    );

    auto dynamic_array_owned_result_helper_call_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_helper_call_cleanup_run.or";
    auto dynamic_array_owned_result_helper_call_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_helper_call_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_helper_call_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_owned_result_helper_call_cleanup_ir);
    assert_ir_contains(
        dynamic_array_owned_result_helper_call_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i32 %selector, i1 %inner)"
    );
    assert_ir_contains(dynamic_array_owned_result_helper_call_cleanup_ir.ir_text, "switch i32 %selector");
    assert_ir_contains(dynamic_array_owned_result_helper_call_cleanup_ir.ir_text, "br i1 %inner");
    assert_ir_contains(
        dynamic_array_owned_result_helper_call_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(dynamic_array_owned_result_helper_call_cleanup_ir.ir_text, "%values.dynamic_array_cleanup");
    assert_branch_local_three_case_mixed_switch_dynamic_array_cleanup_ir(
        dynamic_array_owned_result_helper_call_cleanup_ir.ir_text
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_helper_call_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_helper_call_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_helper_call_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_helper_call_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_helper_call_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_ternary_helper_call_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_owned_result_ternary_helper_call_cleanup_ir);
    assert_ir_contains(
        dynamic_array_owned_result_ternary_helper_call_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i1 %flag)"
    );
    assert_ir_contains(dynamic_array_owned_result_ternary_helper_call_cleanup_ir.ir_text, "br i1 %flag");
    assert_ir_contains(dynamic_array_owned_result_ternary_helper_call_cleanup_ir.ir_text, "ternary.then.");
    assert_ir_contains(dynamic_array_owned_result_ternary_helper_call_cleanup_ir.ir_text, "ternary.else.");
    assert_ir_contains(dynamic_array_owned_result_ternary_helper_call_cleanup_ir.ir_text, "ternary.merge.");
    assert_ir_contains(
        dynamic_array_owned_result_ternary_helper_call_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_helper_call_cleanup_ir.ir_text,
        "%values.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_helper_call_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_named_helper_call_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_named_helper_call_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_named_helper_call_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_named_helper_call_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_ternary_named_helper_call_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(dynamic_array_owned_result_ternary_named_helper_call_cleanup_ir);
    assert_ir_contains(
        dynamic_array_owned_result_ternary_named_helper_call_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i1 %flag)"
    );
    assert_ir_contains(dynamic_array_owned_result_ternary_named_helper_call_cleanup_ir.ir_text, "br i1 %flag");
    assert_ir_contains(dynamic_array_owned_result_ternary_named_helper_call_cleanup_ir.ir_text, "ternary.then.");
    assert_ir_contains(dynamic_array_owned_result_ternary_named_helper_call_cleanup_ir.ir_text, "ternary.else.");
    assert_ir_contains(dynamic_array_owned_result_ternary_named_helper_call_cleanup_ir.ir_text, "ternary.merge.");
    assert_ir_contains(
        dynamic_array_owned_result_ternary_named_helper_call_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %"
    );
    assert_branch_local_dynamic_array_cleanup_for_owners_ir(
        dynamic_array_owned_result_ternary_named_helper_call_cleanup_ir.ir_text,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_named_helper_call_cleanup_ir.ir_text,
        "%values.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_named_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_named_helper_call_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @choose(i1 %flag)"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %"
    );
    assert_branch_local_dynamic_array_cleanup_for_owners_ir(
        dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup_ir.ir_text,
        {"left_values", "left_scratch", "right_values", "right_scratch"}
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup_ir.ir_text,
        "%values.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_named_chained_helper_call_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_named_helper_call_local_return_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_named_helper_call_local_return_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_named_helper_call_local_return_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_named_helper_call_local_return_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_ternary_named_helper_call_local_return_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_named_helper_call_local_return_cleanup_ir
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_named_helper_call_local_return_cleanup_ir.ir_text,
        "%values.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_named_helper_call_local_return_cleanup_ir.ir_text,
        "%selected.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_named_helper_call_local_return_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_named_helper_call_local_return_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_local_return_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_local_return_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_local_return_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_ternary_branch_consumer_local_return_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_branch_consumer_local_return_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_local_return_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_local_return_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_local_return_cleanup_ir.ir_text,
        "%values.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_local_return_cleanup_ir.ir_text,
        "%selected.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_local_return_cleanup_ir.ir_text,
        "%final_selected.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_branch_consumer_local_return_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_local_return_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_chained_branch_consumer_local_return_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_chained_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_chained_branch_consumer_local_return_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_chained_branch_consumer_local_return_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_ternary_chained_branch_consumer_local_return_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_chained_branch_consumer_local_return_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_chained_branch_consumer_local_return_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_chained_branch_consumer_local_return_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_chained_branch_consumer_local_return_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_chained_branch_consumer_local_return_cleanup_ir.ir_text,
        "%values.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_chained_branch_consumer_local_return_cleanup_ir.ir_text,
        "%selected.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_chained_branch_consumer_local_return_cleanup_ir.ir_text,
        "%final_selected.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_chained_branch_consumer_local_return_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_chained_branch_consumer_local_return_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_scratch_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @make_values(i32 31)"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @make_values(i32 41)"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup_ir.ir_text,
        "%forwarded_scratch.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup_ir.ir_text,
        "%values.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup_ir.ir_text,
        "%scratch.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup_ir.ir_text,
        "%selected.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup_ir.ir_text,
        "%final_selected.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_scratch_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_alias_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup_ir.ir_text,
        "%alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup_ir.ir_text,
        "%values.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup_ir.ir_text,
        "%alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup_ir.ir_text,
        "%selected.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup_ir.ir_text,
        "%final_selected.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_alias_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_nested_alias_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_ir.ir_text,
        "%alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_ir.ir_text,
        "%second_alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_ir.ir_text,
        "%values.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_ir.ir_text,
        "%alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_ir.ir_text,
        "%second_alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_ir.ir_text,
        "%selected.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_ir.ir_text,
        "%final_selected.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_nested_alias_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_asymmetric_alias_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_ir.ir_text,
        "%alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_ir.ir_text,
        "%second_alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_ir.ir_text,
        "%values.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_ir.ir_text,
        "%alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_ir.ir_text,
        "%second_alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_ir.ir_text,
        "%selected.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_ir.ir_text,
        "%final_selected.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_alias_helper_call_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_ir.ir_text,
        "%alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_ir.ir_text,
        "%second_alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_ir.ir_text,
        "%values.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_ir.ir_text,
        "%alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_ir.ir_text,
        "%second_alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_ir.ir_text,
        "%selected.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_ir.ir_text,
        "%final_selected.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_alias_helper_call_local_return_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_ir =
        pipeline.emit_llvm(
            dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_path,
            orison::pipeline::CompilePipelineOptions {
                .source_drop_lowering_enabled = true,
                .dynamic_array_descriptor_cleanup_planning_enabled = true,
            }
        );
    assert(!dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_ir.ir_text,
        "%alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_ir.ir_text,
        "%second_alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_ir.ir_text,
        "%returned.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_ir.ir_text,
        "%values.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_ir.ir_text,
        "%alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_ir.ir_text,
        "%second_alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_ir.ir_text,
        "%returned.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_ir.ir_text,
        "%selected.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_ir.ir_text,
        "%final_selected.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_asymmetric_stored_helper_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_ir.ir_text,
        "%alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_ir.ir_text,
        "%second_alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_ir.ir_text,
        "%returned.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_ir.ir_text,
        "%final_return.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_ir.ir_text,
        "%values.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_ir.ir_text,
        "%alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_ir.ir_text,
        "%second_alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_ir.ir_text,
        "%returned.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_ir.ir_text,
        "%final_return.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_ir.ir_text,
        "%selected.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_ir.ir_text,
        "%final_selected.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_three_local_helper_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_ir.ir_text,
        "%alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_ir.ir_text,
        "%second_alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_ir.ir_text,
        "%returned.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_ir.ir_text,
        "%middle_return.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_ir.ir_text,
        "%final_return.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_ir.ir_text,
        "%values.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_ir.ir_text,
        "%alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_ir.ir_text,
        "%second_alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_ir.ir_text,
        "%returned.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_ir.ir_text,
        "%middle_return.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_ir.ir_text,
        "%final_return.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_ir.ir_text,
        "%selected.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_ir.ir_text,
        "%final_selected.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_distinct_local_names_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "%left_alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "%left_result.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "%left_middle.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "%left_final.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "%right_alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "%right_second.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "%right_result.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "%right_middle.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "%right_final.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "%left_alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "%left_result.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "%left_middle.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "%left_final.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "%right_alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "%right_second.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "%right_result.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "%right_middle.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_ir.ir_text,
        "%right_final.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_mixed_direct_distinct_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_ir.ir_text,
        "%left_alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_ir.ir_text,
        "%right_alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_ir.ir_text,
        "%right_second.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_ir.ir_text,
        "%right_result.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_ir.ir_text,
        "%right_middle.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_ir.ir_text,
        "%right_final.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_ir.ir_text,
        "%left_alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_ir.ir_text,
        "%right_alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_ir.ir_text,
        "%right_second.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_ir.ir_text,
        "%right_result.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_ir.ir_text,
        "%right_middle.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_ir.ir_text,
        "%right_final.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_nested_helper_argument_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup_ir.ir_text,
        "%selected.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup_ir.ir_text,
        "%final_selected.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup_ir.ir_text,
        "%selected.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup_ir.ir_text,
        "%final_selected.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_asymmetric_nested_helper_argument_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_ir =
        pipeline.emit_llvm(
            dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_path,
            orison::pipeline::CompilePipelineOptions {
                .source_drop_lowering_enabled = true,
                .dynamic_array_descriptor_cleanup_planning_enabled = true,
            }
        );
    assert(!dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_ir.ir_text,
        "%selected.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_ir.ir_text,
        "%final_selected.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_ir.ir_text,
        "%selected.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_ir.ir_text,
        "%final_selected.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_nested_argument_local_chain_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir.ir_text,
        "%left_alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir.ir_text,
        "%left_result.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir.ir_text,
        "%left_final.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir.ir_text,
        "%right_alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir.ir_text,
        "%right_result.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir.ir_text,
        "%right_middle.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir.ir_text,
        "%right_final.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_values({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @forward_again({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir.ir_text,
        "%selected.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir.ir_text,
        "%final_selected.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir.ir_text,
        "%left_alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir.ir_text,
        "%left_result.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir.ir_text,
        "%left_final.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir.ir_text,
        "%right_alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir.ir_text,
        "%right_result.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir.ir_text,
        "%right_middle.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_ir.ir_text,
        "%right_final.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_ir.ir_text,
        "%finished.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_ir.ir_text,
        "%final_selected.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_ir.ir_text,
        "%wrapped_left.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_ir.ir_text,
        "%wrapped_right.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @finish_left({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @finish_right({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @wrap_left({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_ir.ir_text,
        "call { ptr, i64, i64 } @wrap_right({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_ir.ir_text,
        "%selected.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_ir.ir_text,
        "%finished.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_ir.ir_text,
        "%final_selected.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_ir.ir_text,
        "%wrapped_left.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_ir.ir_text,
        "%wrapped_right.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_ir =
        pipeline.emit_llvm(
            dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_path,
            orison::pipeline::CompilePipelineOptions {
                .source_drop_lowering_enabled = true,
                .dynamic_array_descriptor_cleanup_planning_enabled = true,
            }
        );
    assert(
        !dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_ir
             .has_errors()
    );
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_ir.ir_text,
        "%wrapped_right_alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_ir.ir_text,
        "%wrapped_right_result.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_ir.ir_text,
        "%wrapped_right_middle.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_ir.ir_text,
        "%wrapped_right_final.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_ir.ir_text,
        "%finished.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_ir.ir_text,
        "%wrapped_right_alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_ir.ir_text,
        "%wrapped_right_result.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_ir.ir_text,
        "%wrapped_right_middle.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_ir.ir_text,
        "%wrapped_right_final.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_path,
        smoke_temp_root /
            "dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_ir =
        pipeline.emit_llvm(
            dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_path,
            orison::pipeline::CompilePipelineOptions {
                .source_drop_lowering_enabled = true,
                .dynamic_array_descriptor_cleanup_planning_enabled = true,
            }
        );
    assert(!dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_ir.ir_text,
        "define { ptr, i64, i64 } @wrap_left({ ptr, i64, i64 } %values)"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_ir.ir_text,
        "%wrapped_right_alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_ir.ir_text,
        "%wrapped_right_result.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_ir.ir_text,
        "%wrapped_right_middle.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_ir.ir_text,
        "%wrapped_right_final.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_ir.ir_text,
        "%wrapped_left.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_ir.ir_text,
        "%finished.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_ir.ir_text,
        "%wrapped_right_alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_ir.ir_text,
        "%wrapped_right_result.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_ir.ir_text,
        "%wrapped_right_middle.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_ir.ir_text,
        "%wrapped_right_final.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_path,
        smoke_temp_root /
            "dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_ir =
        pipeline.emit_llvm(
            dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_path,
            orison::pipeline::CompilePipelineOptions {
                .source_drop_lowering_enabled = true,
                .dynamic_array_descriptor_cleanup_planning_enabled = true,
            }
        );
    assert(
        !dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_ir
             .has_errors()
    );
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_ir
            .ir_text,
        "%wrapped_left.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_ir
            .ir_text,
        "%wrapped_right_alias.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_ir
            .ir_text,
        "%wrapped_right_result.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_ir
            .ir_text,
        "%wrapped_right_final.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_ir
            .ir_text,
        "call { ptr, i64, i64 } @wrap_left({ ptr, i64, i64 } %"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_ir
            .ir_text,
        "call { ptr, i64, i64 } @wrap_right({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_ir
            .ir_text,
        "%finished.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_ir
            .ir_text,
        "%wrapped_left.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_ir
            .ir_text,
        "%wrapped_right_alias.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_ir
            .ir_text,
        "%wrapped_right_result.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_ir
            .ir_text,
        "%wrapped_right_final.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_path,
        smoke_temp_root /
            "dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_run.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_ir =
        pipeline.emit_llvm(
            dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_path,
            orison::pipeline::CompilePipelineOptions {
                .source_drop_lowering_enabled = true,
                .dynamic_array_descriptor_cleanup_planning_enabled = true,
            }
        );
    assert(
        !dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_ir
             .has_errors()
    );
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_ir
            .ir_text,
        "define { ptr, i64, i64 } @complete_values({ ptr, i64, i64 } %values)"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_ir
            .ir_text,
        "%completed.addr = alloca { ptr, i64, i64 }"
    );
    assert_ir_contains(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_ir
            .ir_text,
        "call { ptr, i64, i64 } @complete_values({ ptr, i64, i64 } %"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_ir
            .ir_text,
        "%finished.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_ir
            .ir_text,
        "%final_selected.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_ir
            .ir_text,
        "%completed.dynamic_array_cleanup"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_path,
        smoke_temp_root /
            "dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_cleanup_run"
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_reuse_ir =
        pipeline.emit_llvm(
            dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_reuse_path,
            orison::pipeline::CompilePipelineOptions {
                .source_drop_lowering_enabled = true,
                .dynamic_array_descriptor_cleanup_planning_enabled = true,
            }
        );
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_reuse_ir
            .has_errors()
    );
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_wrapper_result_final_consumer_reuse_ir
            .error_text.find("use after move: final_selected") != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_reuse_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_reuse_ir.error_text.find(
            "use after move: finished"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_nested_wrapper_argument_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_reuse_ir =
        pipeline.emit_llvm(
            dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_reuse_path,
            orison::pipeline::CompilePipelineOptions {
                .source_drop_lowering_enabled = true,
                .dynamic_array_descriptor_cleanup_planning_enabled = true,
            }
        );
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_reuse_ir
            .has_errors()
    );
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_nested_wrapper_argument_reuse_ir
            .error_text.find("use after move: finished") != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_asymmetric_wrapper_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_reuse_ir =
        pipeline.emit_llvm(
            dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_reuse_path,
            orison::pipeline::CompilePipelineOptions {
                .source_drop_lowering_enabled = true,
                .dynamic_array_descriptor_cleanup_planning_enabled = true,
            }
        );
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_reuse_ir
            .has_errors()
    );
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_asymmetric_wrapper_reuse_ir.error_text
            .find("use after move: wrapped_right_middle") != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_result_nested_ternary_mixed_wrapper_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_reuse_ir =
        pipeline.emit_llvm(
            dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_reuse_path,
            orison::pipeline::CompilePipelineOptions {
                .source_drop_lowering_enabled = true,
                .dynamic_array_descriptor_cleanup_planning_enabled = true,
            }
        );
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_reuse_ir.has_errors()
    );
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_result_nested_ternary_mixed_wrapper_reuse_ir.error_text.find(
            "use after move: wrapped_right_middle"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_scratch_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_scratch_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_scratch_reuse_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_scratch_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(dynamic_array_owned_result_ternary_branch_consumer_scratch_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_scratch_reuse_ir.error_text.find(
            "use after move: scratch"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_alias_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_alias_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_alias_reuse_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_alias_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(dynamic_array_owned_result_ternary_branch_consumer_alias_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_alias_reuse_ir.error_text.find(
            "use after move: values"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_nested_alias_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_nested_alias_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_nested_alias_reuse_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_nested_alias_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(dynamic_array_owned_result_ternary_branch_consumer_nested_alias_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_nested_alias_reuse_ir.error_text.find(
            "use after move: alias"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_asymmetric_alias_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_reuse_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_alias_reuse_ir.error_text.find(
            "use after move: alias"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_alias_helper_call_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_reuse_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_reuse_ir.error_text.find(
            "use after move: alias"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_alias_helper_call_local_return_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_reuse_ir =
        pipeline.emit_llvm(
            dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_reuse_path,
            orison::pipeline::CompilePipelineOptions {
                .source_drop_lowering_enabled = true,
                .dynamic_array_descriptor_cleanup_planning_enabled = true,
            }
        );
    assert(dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_alias_helper_call_local_return_reuse_ir.error_text.find(
            "use after move: returned"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_three_local_helper_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_reuse_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_three_local_helper_reuse_ir.error_text.find(
            "use after move: middle_return"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_distinct_local_names_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_reuse_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_distinct_local_names_reuse_ir.error_text.find(
            "use after move: right_middle"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_nested_argument_local_chain_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_reuse_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_nested_argument_local_chain_reuse_ir.error_text.find(
            "use after move: right_middle"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_mixed_direct_distinct_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_reuse_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_mixed_direct_distinct_reuse_ir.error_text.find(
            "use after move: right_middle"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_selected_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_nested_helper_argument_selected_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_selected_reuse_ir =
        pipeline.emit_llvm(
            dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_selected_reuse_path,
            orison::pipeline::CompilePipelineOptions {
                .source_drop_lowering_enabled = true,
                .dynamic_array_descriptor_cleanup_planning_enabled = true,
            }
        );
    assert(dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_selected_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_nested_helper_argument_selected_reuse_ir.error_text.find(
            "use after move: selected"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_selected_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_asymmetric_nested_helper_argument_selected_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_selected_reuse_ir =
        pipeline.emit_llvm(
            dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_selected_reuse_path,
            orison::pipeline::CompilePipelineOptions {
                .source_drop_lowering_enabled = true,
                .dynamic_array_descriptor_cleanup_planning_enabled = true,
            }
        );
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_selected_reuse_ir.has_errors()
    );
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_nested_helper_argument_selected_reuse_ir.error_text
            .find("use after move: selected") != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_asymmetric_stored_helper_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_reuse_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_asymmetric_stored_helper_reuse_ir.error_text.find(
            "use after move: final_return"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_chained_branch_consumer_selected_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_chained_selected_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_chained_branch_consumer_selected_reuse_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_chained_branch_consumer_selected_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(dynamic_array_owned_result_ternary_chained_branch_consumer_selected_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_ternary_chained_branch_consumer_selected_reuse_ir.error_text.find(
            "use after move: selected"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_branch_consumer_local_return_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_local_return_branch_consumer_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_branch_consumer_local_return_reuse_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_branch_consumer_local_return_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(dynamic_array_owned_result_ternary_branch_consumer_local_return_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_ternary_branch_consumer_local_return_reuse_ir.error_text.find(
            "use after move: final_selected"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_named_helper_call_local_return_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_named_helper_call_local_return_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_named_helper_call_local_return_reuse_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_named_helper_call_local_return_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(dynamic_array_owned_result_ternary_named_helper_call_local_return_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_ternary_named_helper_call_local_return_reuse_ir.error_text.find(
            "use after move: selected"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_ternary_named_chained_helper_call_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_ternary_named_chained_helper_call_reuse_rejected.or";
    auto dynamic_array_owned_result_ternary_named_chained_helper_call_reuse_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_ternary_named_chained_helper_call_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(dynamic_array_owned_result_ternary_named_chained_helper_call_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_ternary_named_chained_helper_call_reuse_ir.error_text.find(
            "use after move: left_values"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_multi_nested_switch_returned_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_multi_nested_switch_returned_reuse_rejected.or";
    auto dynamic_array_owned_result_multi_nested_switch_returned_reuse_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_multi_nested_switch_returned_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(dynamic_array_owned_result_multi_nested_switch_returned_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_multi_nested_switch_returned_reuse_ir.error_text.find(
            "use after move: first_left_values"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_helper_call_returned_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_helper_call_returned_reuse_rejected.or";
    auto dynamic_array_owned_result_helper_call_returned_reuse_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_helper_call_returned_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(dynamic_array_owned_result_helper_call_returned_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_helper_call_returned_reuse_ir.error_text.find(
            "use after move: first_left_values"
        ) != std::string::npos
    );

    auto dynamic_array_owned_result_multi_nested_switch_consumed_scratch_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_multi_nested_switch_consumed_scratch_cleanup_run.or";
    auto dynamic_array_owned_result_multi_nested_switch_consumed_scratch_cleanup_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_multi_nested_switch_consumed_scratch_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(!dynamic_array_owned_result_multi_nested_switch_consumed_scratch_cleanup_ir.has_errors());
    assert_dynamic_array_payload_cleanup_ready(
        dynamic_array_owned_result_multi_nested_switch_consumed_scratch_cleanup_ir
    );
    assert_ir_contains(
        dynamic_array_owned_result_multi_nested_switch_consumed_scratch_cleanup_ir.ir_text,
        "call i32 @consume_items({ ptr, i64, i64 } %tmp"
    );
    assert_branch_local_dynamic_array_cleanup_for_owners_ir(
        dynamic_array_owned_result_multi_nested_switch_consumed_scratch_cleanup_ir.ir_text,
        {"first_right_scratch", "second_left_scratch", "second_right_scratch", "third_scratch"}
    );
    assert_ir_excludes(
        dynamic_array_owned_result_multi_nested_switch_consumed_scratch_cleanup_ir.ir_text,
        "%first_left_scratch.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_owned_result_multi_nested_switch_consumed_scratch_cleanup_ir.ir_text,
        "switch case ownership mismatch"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_result_multi_nested_switch_consumed_scratch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_result_multi_nested_switch_consumed_scratch_cleanup_run"
    );

    auto dynamic_array_owned_result_multi_nested_switch_consumed_scratch_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_result_multi_nested_switch_consumed_scratch_reuse_rejected.or";
    auto dynamic_array_owned_result_multi_nested_switch_consumed_scratch_reuse_ir = pipeline.emit_llvm(
        dynamic_array_owned_result_multi_nested_switch_consumed_scratch_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
        }
    );
    assert(dynamic_array_owned_result_multi_nested_switch_consumed_scratch_reuse_ir.has_errors());
    assert(
        dynamic_array_owned_result_multi_nested_switch_consumed_scratch_reuse_ir.error_text.find(
            "use after move: first_left_scratch"
        ) != std::string::npos
    );

    auto dynamic_array_returned_payload_mismatched_lifetime_ir = pipeline.emit_llvm(
        dynamic_array_returned_payload_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .dynamic_array_descriptor_cleanup_planning_enabled = true,
            .test_only_dynamic_array_descriptor_lifetime_plan_fault =
                orison::pipeline::DynamicArrayDescriptorLifetimePlanFaultInjection::MismatchCleanupPlanOwners,
        }
    );
    assert(!dynamic_array_returned_payload_mismatched_lifetime_ir.has_errors());
    assert(
        !dynamic_array_returned_payload_mismatched_lifetime_ir.dynamic_array_descriptor_lifetime_plan_state
            .all_origins_have_cleanup_plans
    );
    assert(
        std::any_of(
            dynamic_array_returned_payload_mismatched_lifetime_ir.dynamic_array_descriptor_lifetime_plan_state
                .origin_blockers.begin(),
            dynamic_array_returned_payload_mismatched_lifetime_ir.dynamic_array_descriptor_lifetime_plan_state
                .origin_blockers.end(),
            [](orison::pipeline::DynamicArrayDescriptorOriginBlocker const& blocker) {
                return blocker.reason == "shared-lifetime-plan-mismatched";
            }
        )
    );
    auto mismatched_lifetime_diagnostics =
        orison::pipeline::format_dynamic_array_cleanup_production_readiness_diagnostics(
            dynamic_array_returned_payload_mismatched_lifetime_ir.dynamic_array_cleanup_production_readiness
        );
    assert(
        std::any_of(
            mismatched_lifetime_diagnostics.begin(),
            mismatched_lifetime_diagnostics.end(),
            [](std::string const& diagnostic) {
                return diagnostic.find(
                    "dynamic array cleanup production blocked: descriptor lifetime metadata "
                    "shared-lifetime-plan-mismatched"
                ) != std::string::npos &&
                    diagnostic.find("source DynamicArray<Payload>") != std::string::npos &&
                    diagnostic.find("origin returned") != std::string::npos;
            }
        )
    );
    assert(
        !orison::pipeline::dynamic_array_cleanup_production_ready(
            dynamic_array_returned_payload_mismatched_lifetime_ir.dynamic_array_cleanup_production_readiness
        )
    );

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

    auto dynamic_array_owned_parameter_branch_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_parameter_branch_cleanup_run.or";
    auto dynamic_array_owned_parameter_branch_cleanup = pipeline.emit_llvm(
        dynamic_array_owned_parameter_branch_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_owned_parameter_branch_cleanup.has_errors());
    assert(dynamic_array_owned_parameter_branch_cleanup.dynamic_array_cleanup_emission_capability_state.proven);
    assert_ir_contains(
        dynamic_array_owned_parameter_branch_cleanup.ir_text,
        "define i32 @choose(i1 %flag, { ptr, i64, i64 } %items)"
    );
    assert_ir_contains(dynamic_array_owned_parameter_branch_cleanup.ir_text, "br i1 %flag");
    assert_ir_contains(
        dynamic_array_owned_parameter_branch_cleanup.ir_text,
        "call i32 @use_items({ ptr, i64, i64 } %items)"
    );
    assert_ir_contains(
        dynamic_array_owned_parameter_branch_cleanup.ir_text,
        "call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup"
    );
    assert_ir_contains(
        dynamic_array_owned_parameter_branch_cleanup.ir_text,
        "call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup"
    );
    assert_ir_excludes(dynamic_array_owned_parameter_branch_cleanup.ir_text, "if branch ownership mismatch");
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_parameter_branch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_parameter_branch_cleanup_run"
    );

    auto dynamic_array_local_final_if_consumed_owner_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_local_final_if_consumed_owner_cleanup_run.or";
    auto dynamic_array_local_final_if_consumed_owner_cleanup = pipeline.emit_llvm(
        dynamic_array_local_final_if_consumed_owner_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_local_final_if_consumed_owner_cleanup.has_errors());
    assert(dynamic_array_local_final_if_consumed_owner_cleanup.dynamic_array_cleanup_emission_capability_state.proven);
    assert_ir_contains(
        dynamic_array_local_final_if_consumed_owner_cleanup.ir_text,
        "define i32 @choose(i1 %flag)"
    );
    assert_ir_contains(dynamic_array_local_final_if_consumed_owner_cleanup.ir_text, "br i1 %flag");
    assert_ir_contains(
        dynamic_array_local_final_if_consumed_owner_cleanup.ir_text,
        "call i32 @use_items({ ptr, i64, i64 } %tmp"
    );
    assert_ir_contains(
        dynamic_array_local_final_if_consumed_owner_cleanup.ir_text,
        "call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup"
    );
    assert_ir_contains(
        dynamic_array_local_final_if_consumed_owner_cleanup.ir_text,
        "call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup"
    );
    assert_ir_excludes(dynamic_array_local_final_if_consumed_owner_cleanup.ir_text, "if branch ownership mismatch");
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_local_final_if_consumed_owner_cleanup_path,
        smoke_temp_root / "dynamic_array_local_final_if_consumed_owner_cleanup_run"
    );

    auto dynamic_array_owned_parameter_branch_cleanup_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_parameter_branch_cleanup_reuse_rejected.or";
    auto dynamic_array_owned_parameter_branch_cleanup_reuse = pipeline.emit_llvm(
        dynamic_array_owned_parameter_branch_cleanup_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(dynamic_array_owned_parameter_branch_cleanup_reuse.has_errors());
    assert(
        dynamic_array_owned_parameter_branch_cleanup_reuse.error_text.find("use after move: items") !=
        std::string::npos
    );

    auto dynamic_array_owned_parameter_switch_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_parameter_switch_cleanup_run.or";
    auto dynamic_array_owned_parameter_switch_cleanup = pipeline.emit_llvm(
        dynamic_array_owned_parameter_switch_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_owned_parameter_switch_cleanup.has_errors());
    assert(dynamic_array_owned_parameter_switch_cleanup.dynamic_array_cleanup_emission_capability_state.proven);
    assert_ir_contains(
        dynamic_array_owned_parameter_switch_cleanup.ir_text,
        "define i32 @choose(i1 %flag, { ptr, i64, i64 } %items)"
    );
    assert_ir_contains(dynamic_array_owned_parameter_switch_cleanup.ir_text, "switch i1 %flag");
    assert_ir_contains(
        dynamic_array_owned_parameter_switch_cleanup.ir_text,
        "call i32 @use_items({ ptr, i64, i64 } %items)"
    );
    assert_ir_contains(
        dynamic_array_owned_parameter_switch_cleanup.ir_text,
        "call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup"
    );
    assert_ir_contains(
        dynamic_array_owned_parameter_switch_cleanup.ir_text,
        "call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup"
    );
    assert_ir_excludes(dynamic_array_owned_parameter_switch_cleanup.ir_text, "switch case ownership mismatch");
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_owned_parameter_switch_cleanup_path,
        smoke_temp_root / "dynamic_array_owned_parameter_switch_cleanup_run"
    );

    auto dynamic_array_local_final_switch_consumed_owner_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_local_final_switch_consumed_owner_cleanup_run.or";
    auto dynamic_array_local_final_switch_consumed_owner_cleanup = pipeline.emit_llvm(
        dynamic_array_local_final_switch_consumed_owner_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(!dynamic_array_local_final_switch_consumed_owner_cleanup.has_errors());
    assert(dynamic_array_local_final_switch_consumed_owner_cleanup.dynamic_array_cleanup_emission_capability_state.proven);
    assert_ir_contains(
        dynamic_array_local_final_switch_consumed_owner_cleanup.ir_text,
        "define i32 @choose(i1 %flag)"
    );
    assert_ir_contains(dynamic_array_local_final_switch_consumed_owner_cleanup.ir_text, "switch i1 %flag");
    assert_ir_contains(
        dynamic_array_local_final_switch_consumed_owner_cleanup.ir_text,
        "call i32 @use_items({ ptr, i64, i64 } %tmp"
    );
    assert_ir_contains(
        dynamic_array_local_final_switch_consumed_owner_cleanup.ir_text,
        "call void @__orison_drop.Payload(ptr %items.dynamic_array_cleanup"
    );
    assert_ir_contains(
        dynamic_array_local_final_switch_consumed_owner_cleanup.ir_text,
        "call void @__orison_dynamic_array_deallocate(ptr %items.dynamic_array_cleanup"
    );
    assert_ir_excludes(
        dynamic_array_local_final_switch_consumed_owner_cleanup.ir_text,
        "switch case ownership mismatch"
    );
    assert_emit_object_link_run_success(
        pipeline,
        dynamic_array_local_final_switch_consumed_owner_cleanup_path,
        smoke_temp_root / "dynamic_array_local_final_switch_consumed_owner_cleanup_run"
    );

    auto dynamic_array_owned_parameter_switch_cleanup_reuse_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "dynamic_array_owned_parameter_switch_cleanup_reuse_rejected.or";
    auto dynamic_array_owned_parameter_switch_cleanup_reuse = pipeline.emit_llvm(
        dynamic_array_owned_parameter_switch_cleanup_reuse_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
        }
    );
    assert(dynamic_array_owned_parameter_switch_cleanup_reuse.has_errors());
    assert(
        dynamic_array_owned_parameter_switch_cleanup_reuse.error_text.find("use after move: items") !=
        std::string::npos
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
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_blocker_kind ==
        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::InsertionGate
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
            .blockers.size() == 6
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
            .blockers[0].kind ==
        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::InsertionGate
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
            .blockers[1].kind ==
        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::InsertionPreview
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
            .blockers[5].kind ==
        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::FunctionIntegration
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_blocker_stage_name == "module insertion gate"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_function_symbol_name == "main"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_source_available
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_source_line != 0
    );
    assert(
        orison::pipeline::runtime_indexed_cleanup_production_readiness_blocker_kind_name(
            runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
                .diagnostic_blocker_kind
        ) == "insertion-gate"
    );
    assert(
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_diagnostic(
            runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
        ) == "runtime-index cleanup blocked: module insertion gate disabled"
    );
    assert(
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_report(
            runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
        ).find(
            "ir-shape ready production blocked blocker-count 6 blocker-kind insertion-gate function main source-line "
        ) !=
        std::string::npos
    );
    assert(
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_report(
            runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
        ).find("diagnostic runtime-index cleanup blocked: module insertion gate disabled") !=
        std::string::npos
    );
    auto runtime_indexed_cleanup_readiness_blocker_report =
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_blocker_report(
            runtime_indexed_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
        );
    assert(runtime_indexed_cleanup_readiness_blocker_report.size() == 6);
    assert(
        runtime_indexed_cleanup_readiness_blocker_report.front().find(
            "index 0 kind insertion-gate stage module insertion gate function main source-line "
        ) != std::string::npos
    );
    assert(
        runtime_indexed_cleanup_readiness_blocker_report.back().find(
            "index 5 kind function-integration stage function integration function main source-line "
        ) != std::string::npos
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
    assert(runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines.size() == 76);
    assert(runtime_indexed_cleanup.runtime_indexed_member_cleanup_typed_promotion_gates.size() == 1);
    assert(!runtime_indexed_cleanup.runtime_indexed_member_cleanup_typed_promotion_gates.front().gate_ready);
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[7] ==
        "runtime-index member cleanup owner holder.items index index element Inner moved Inner "
        "member-path none owner-known true index-known true element-type-known true "
        "moved-type-known true member-path-known false cleanup-element-matches-move true "
        "member-granular-required false prerequisites missing production disabled"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[8] ==
        "runtime-index member cleanup proof owner holder.items index index element Inner moved Inner "
        "member-path none plan-ready false whole-element-cleanup-matches-move true "
        "member-cleanup-required false member-scope-proven false whole-element-cleanup-blocked false "
        "prerequisites missing production disabled"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[9] ==
        "runtime-index member cleanup emission-sketch owner holder.items index index element Inner "
        "moved Inner member-path none snippets 0 proof-ready false report-only true "
        "production-emission disabled"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[10] ==
        "runtime-index member cleanup emission-gate owner holder.items index index element Inner "
        "moved Inner member-path none sketch-ready false member-drop-metadata missing "
        "ir-insertion missing prerequisites missing production disabled blockers 3 "
        "blocker member-cleanup-sketch blocker member-drop-metadata blocker member-cleanup-ir-insertion"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[11] ==
        "runtime-index member cleanup ir-insertion-plan owner holder.items index index element Inner "
        "moved Inner member-path none anchor missing entry missing skip missing sibling-drop missing "
        "preserve missing exit missing target-metadata missing insertion-points missing report-only true "
        "production disabled preview-operations 0"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[12] ==
        "runtime-index member cleanup ir-composition-plan owner holder.items index index element Inner "
        "moved Inner member-path none anchor missing entry missing skip missing sibling-drop missing "
        "preserve missing exit missing cleanup-target missing insertion-plan missing block-topology missing "
        "preview-operations missing report-only true production disabled topology-edges 0"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[13] ==
        "runtime-index member cleanup cfg-slice owner holder.items index index element Inner "
        "moved Inner member-path none anchor missing entry missing skip missing sibling-drop missing "
        "preserve missing exit missing cleanup-target missing composition missing slice missing report-only true "
        "production disabled cfg-lines 0"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[14] ==
        "runtime-index member cleanup function-rewrite-candidate owner holder.items index index "
        "element Inner moved Inner member-path none anchor missing entry missing sibling-drop missing "
        "preserve missing exit missing cleanup-target missing cfg-slice missing anchor-state missing "
        "branch-rewrite blocked cfg-append blocked "
        "candidate missing verification blocked report-only true production disabled "
        "replaced-terminator missing replacement-branch missing appended-cfg-lines 0"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[15] ==
        "runtime-index member cleanup function-rewrite-edit-script-plan owner holder.items "
        "index index element Inner moved Inner member-path none anchor missing entry missing "
        "sibling-drop missing preserve missing exit missing cleanup-target missing candidate blocked "
        "branch-replacement missing cleanup-cfg-append missing phi-retarget missing edit-script blocked "
        "report-only true production disabled "
        "expected-branch missing replacement-branch missing append-placement missing "
        "expected-closing missing phi-old missing phi-new missing appended-cfg-lines 0"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[16] ==
        "runtime-index member cleanup function-rewrite-edit-script-validation owner holder.items "
        "index index element Inner moved Inner member-path none anchor missing entry missing "
        "exit missing edit-script blocked branch-replacement invalid cleanup-cfg-append invalid "
        "phi-retarget invalid validation blocked report-only true production disabled blockers 5 "
        "blocker member-cleanup-edit-script blocker member-cleanup-branch-replacement "
        "blocker member-cleanup-cfg-append blocker member-cleanup-phi-retarget "
        "blocker production-member-cleanup-module-mutation"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[17] ==
        "runtime-index member cleanup edit-script validation diagnostic owner holder.items "
        "index index element Inner moved Inner member-path none blocker member-cleanup-edit-script "
        "detail member cleanup edit script is not ready"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[18] ==
        "runtime-index member cleanup edit-script validation diagnostic owner holder.items "
        "index index element Inner moved Inner member-path none blocker member-cleanup-branch-replacement "
        "detail member cleanup branch replacement is invalid"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[19] ==
        "runtime-index member cleanup edit-script validation diagnostic owner holder.items "
        "index index element Inner moved Inner member-path none blocker member-cleanup-cfg-append "
        "detail member cleanup CFG append is invalid"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[20] ==
        "runtime-index member cleanup edit-script validation diagnostic owner holder.items "
        "index index element Inner moved Inner member-path none blocker member-cleanup-phi-retarget "
        "detail member cleanup PHI retarget is invalid"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[21] ==
        "runtime-index member cleanup edit-script validation diagnostic owner holder.items "
        "index index element Inner moved Inner member-path none blocker production-member-cleanup-module-mutation "
        "detail member cleanup edit script is validated but production module mutation is disabled"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[22] ==
        "runtime-index member cleanup function-rewrite-staged-apply-plan owner holder.items "
        "index index element Inner moved Inner member-path none anchor missing entry missing "
        "exit missing validation blocked branch-replacement blocked cleanup-cfg-append blocked "
        "phi-retarget blocked staged-apply blocked branch-applied false cfg-appended false "
        "phi-applied false report-only true production disabled blockers 2 "
        "blocker member-cleanup-edit-script-validation "
        "blocker production-member-cleanup-module-mutation"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[23] ==
        "runtime-index member cleanup staged-apply diagnostic owner holder.items "
        "index index element Inner moved Inner member-path none blocker member-cleanup-edit-script-validation "
        "detail member cleanup staged apply is blocked by edit script validation"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[24] ==
        "runtime-index member cleanup staged-apply diagnostic owner holder.items "
        "index index element Inner moved Inner member-path none blocker production-member-cleanup-module-mutation "
        "detail member cleanup staged plan is ready but production module mutation is disabled"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[25] ==
        "runtime-index member cleanup module-mutation-gate owner holder.items index index element Inner "
        "moved Inner member-path none anchor missing entry missing skip missing sibling-drop missing "
        "preserve missing exit missing cfg-slice missing edit-script-validation missing staged-apply missing "
        "module-mutation disabled production-member-cleanup disabled prerequisites missing "
        "production disabled blockers 5 blocker member-cleanup-cfg-slice "
        "blocker member-cleanup-edit-script-validation blocker member-cleanup-staged-apply "
        "blocker member-cleanup-module-mutation blocker production-member-cleanup"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[26] ==
        "runtime-index member cleanup module-mutation diagnostic owner holder.items "
        "index index element Inner moved Inner member-path none blocker member-cleanup-cfg-slice "
        "detail member cleanup module mutation is blocked by missing CFG slice"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[27] ==
        "runtime-index member cleanup module-mutation diagnostic owner holder.items "
        "index index element Inner moved Inner member-path none blocker member-cleanup-edit-script-validation "
        "detail member cleanup module mutation is blocked by edit script validation"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[28] ==
        "runtime-index member cleanup module-mutation diagnostic owner holder.items "
        "index index element Inner moved Inner member-path none blocker member-cleanup-staged-apply "
        "detail member cleanup module mutation is blocked by staged apply readiness"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[29] ==
        "runtime-index member cleanup module-mutation diagnostic owner holder.items "
        "index index element Inner moved Inner member-path none blocker member-cleanup-module-mutation "
        "detail member cleanup module mutation is disabled"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[30] ==
        "runtime-index member cleanup module-mutation diagnostic owner holder.items "
        "index index element Inner moved Inner member-path none blocker production-member-cleanup "
        "detail production member cleanup is disabled"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[31] ==
        "runtime-index member cleanup production-readiness owner holder.items index index element Inner "
        "moved Inner member-path none proof missing target-metadata missing helper-drop-bindings ready cfg-slice missing "
        "module-mutation blocked production-member-cleanup blocked production-gate blocked "
        "production-enabled false production blocked blockers 5 "
        "blocker member-cleanup-proof blocker member-drop-metadata blocker member-cleanup-cfg-slice "
        "blocker member-cleanup-module-mutation blocker production-member-cleanup"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[32] ==
        "runtime-index member cleanup production blocker owner holder.items index index element Inner "
        "moved Inner member-path none blocker member-cleanup-proof "
        "detail member cleanup proof is missing"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[33] ==
        "runtime-index member cleanup production blocker owner holder.items index index element Inner "
        "moved Inner member-path none blocker member-drop-metadata "
        "detail member Drop metadata is missing"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[34] ==
        "runtime-index member cleanup production blocker owner holder.items index index element Inner "
        "moved Inner member-path none blocker member-cleanup-cfg-slice "
        "detail member cleanup CFG slice is missing"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[35] ==
        "runtime-index member cleanup production blocker owner holder.items index index element Inner "
        "moved Inner member-path none blocker member-cleanup-module-mutation "
        "detail member cleanup module mutation is disabled"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[36] ==
        "runtime-index member cleanup production blocker owner holder.items index index element Inner "
        "moved Inner member-path none blocker production-member-cleanup "
        "detail production member cleanup is disabled"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[37] ==
        "runtime-index member cleanup promotion-checklist owner holder.items index index "
        "element Inner moved Inner member-path none candidate blocked edit-script blocked "
        "validation blocked staged-apply blocked module-mutation blocked production-readiness blocked "
        "promotion blocked report-only true production disabled blockers 9 "
        "blocker member-cleanup-rewrite-candidate blocker member-cleanup-edit-script "
        "blocker member-cleanup-edit-script-validation blocker member-cleanup-staged-apply "
        "blocker member-cleanup-cfg-slice blocker member-cleanup-module-mutation "
        "blocker production-member-cleanup blocker member-cleanup-proof blocker member-drop-metadata"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[38] ==
        "runtime-index member cleanup promotion-seam owner holder.items index index "
        "element Inner moved Inner member-path none checklist blocked mutation-seam blocked "
        "ir-mutation disabled production-gate disabled promotion blocked report-only true "
        "production disabled blockers 12 blocker member-cleanup-rewrite-candidate "
        "blocker member-cleanup-edit-script blocker member-cleanup-edit-script-validation "
        "blocker member-cleanup-staged-apply blocker member-cleanup-cfg-slice "
        "blocker member-cleanup-module-mutation blocker production-member-cleanup "
        "blocker member-cleanup-proof blocker member-drop-metadata "
        "blocker member-cleanup-promotion-checklist blocker member-cleanup-ir-mutation "
        "blocker production-member-cleanup-ir-mutation"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[39] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_operation_plan_report(
            runtime_indexed_cleanup.runtime_indexed_member_cleanup_mutation_operation_plans.front()
        )
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[40] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_operation_validation_report(
            runtime_indexed_cleanup.runtime_indexed_member_cleanup_mutation_operation_validations.front()
        )
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[41] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_conflict_detection_report(
            runtime_indexed_cleanup.runtime_indexed_member_cleanup_mutation_conflict_detections.front()
        )
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[42] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_apply_authorization_report(
            runtime_indexed_cleanup.runtime_indexed_member_cleanup_mutation_apply_authorizations.front()
        )
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[43] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_apply_preview_report(
            runtime_indexed_cleanup.runtime_indexed_member_cleanup_mutation_apply_previews.front()
        )
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[44] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_post_apply_verification_report(
            runtime_indexed_cleanup.runtime_indexed_member_cleanup_mutation_post_apply_verifications.front()
        )
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[45] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_promotion_summary_report(
            runtime_indexed_cleanup.runtime_indexed_member_cleanup_mutation_promotion_summaries.front()
        )
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[46] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_production_readiness_report(
            runtime_indexed_cleanup.runtime_indexed_member_cleanup_mutation_production_readiness.front()
        )
    );
    auto runtime_indexed_mutation_production_diagnostics =
        orison::lowering::runtime_indexed_member_cleanup_mutation_production_readiness_diagnostics(
            runtime_indexed_cleanup.runtime_indexed_member_cleanup_mutation_production_readiness.front()
        );
    assert(runtime_indexed_mutation_production_diagnostics.size() == 19);
    for (auto index = std::size_t {0}; index < runtime_indexed_mutation_production_diagnostics.size(); ++index) {
        assert(
            runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[47 + index] ==
            runtime_indexed_mutation_production_diagnostics[index]
        );
    }
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[66] ==
        "runtime-index member cleanup mutation readiness verdict owner holder.items index index "
        "element Inner moved Inner member-path none readiness blocked guarded-rewrite blocked blockers 19 "
        "diagnostics 19 report-only true production disabled"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[67] ==
        "runtime-index member cleanup mutation rewrite authorization owner holder.items index index "
        "element Inner moved Inner member-path none verdict blocked guarded-rewrite blocked "
        "authorization blocked rewrite-requested false rewrite-authorized false report-only true "
        "production disabled blockers 2 blocker member-cleanup-mutation-readiness-verdict "
        "blocker member-cleanup-mutation-guarded-rewrite"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[68] ==
        "runtime-index member cleanup mutation rewrite authorization blocker owner holder.items index index "
        "element Inner moved Inner member-path none blocker member-cleanup-mutation-readiness-verdict "
        "detail member cleanup mutation readiness verdict is blocked"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[69] ==
        "runtime-index member cleanup mutation rewrite authorization blocker owner holder.items index index "
        "element Inner moved Inner member-path none blocker member-cleanup-mutation-guarded-rewrite "
        "detail member cleanup mutation guarded rewrite is blocked"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[70] ==
        "runtime-index member cleanup mutation rewrite execution-plan owner holder.items index index "
        "element Inner moved Inner member-path none authorization blocked rewrite-authorized false "
        "execution-plan blocked execution-requested false execution disabled report-only true "
        "production disabled blockers 2 blocker member-cleanup-mutation-rewrite-authorization "
        "blocker member-cleanup-mutation-rewrite-not-authorized"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[71] ==
        "runtime-index member cleanup mutation rewrite execution-plan blocker owner holder.items index index "
        "element Inner moved Inner member-path none blocker member-cleanup-mutation-rewrite-authorization "
        "detail member cleanup mutation rewrite authorization is blocked"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[72] ==
        "runtime-index member cleanup mutation rewrite execution-plan blocker owner holder.items index index "
        "element Inner moved Inner member-path none blocker member-cleanup-mutation-rewrite-not-authorized "
        "detail member cleanup mutation rewrite is not authorized"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[73] ==
        "runtime-index member cleanup mutation rewrite execution verdict owner holder.items index index "
        "element Inner moved Inner member-path none execution-plan blocked execution disabled blockers 2 "
        "diagnostics 2 report-only true production disabled"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[74] ==
        "runtime-index member cleanup mutation rewrite promotion-status owner holder.items index index "
        "element Inner moved Inner member-path none authorization blocked execution-plan blocked "
        "execution-verdict blocked promotion blocked blockers 2 diagnostics 2 report-only true "
        "production disabled"
    );
    assert(
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[75] ==
        orison::lowering::runtime_indexed_member_cleanup_typed_promotion_gate_report(
            runtime_indexed_cleanup.runtime_indexed_member_cleanup_typed_promotion_gates.front()
        )
    );

    auto runtime_indexed_cleanup_rewrite_request = pipeline.emit_llvm(
        runtime_indexed_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_member_cleanup_rewrite_execution_enabled = true,
        }
    );
    assert(runtime_indexed_cleanup_rewrite_request.has_errors());
    assert(runtime_indexed_cleanup_rewrite_request.runtime_indexed_cleanup_audit_lines.size() == 76);
    assert(
        runtime_indexed_cleanup_rewrite_request.runtime_indexed_cleanup_audit_lines[67] ==
        "runtime-index member cleanup mutation rewrite authorization owner holder.items index index "
        "element Inner moved Inner member-path none verdict blocked guarded-rewrite blocked "
        "authorization blocked rewrite-requested true rewrite-authorized false report-only true "
        "production disabled blockers 2 blocker member-cleanup-mutation-readiness-verdict "
        "blocker member-cleanup-mutation-guarded-rewrite"
    );
    assert(
        runtime_indexed_cleanup_rewrite_request.runtime_indexed_cleanup_audit_lines[70] ==
        "runtime-index member cleanup mutation rewrite execution-plan owner holder.items index index "
        "element Inner moved Inner member-path none authorization blocked rewrite-authorized false "
        "execution-plan blocked execution-requested true execution disabled report-only true "
        "production disabled blockers 2 blocker member-cleanup-mutation-rewrite-authorization "
        "blocker member-cleanup-mutation-rewrite-not-authorized"
    );
    assert(
        runtime_indexed_cleanup_rewrite_request.runtime_indexed_cleanup_audit_lines[74] ==
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[74]
    );
    assert(
        runtime_indexed_cleanup_rewrite_request.runtime_indexed_cleanup_audit_lines[75] ==
        runtime_indexed_cleanup.runtime_indexed_cleanup_audit_lines[75]
    );

    auto runtime_indexed_member_transfer_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "runtime_indexed_dynamic_array_constructor_computed_expression_nested_member_transfer.or";
    auto runtime_indexed_member_transfer_audit = pipeline.emit_llvm(
        runtime_indexed_member_transfer_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
            .runtime_indexed_cleanup_module_ir_insertion_enabled = true,
            .runtime_indexed_cleanup_module_ir_mutation_enabled = true,
            .runtime_indexed_cleanup_function_ir_module_rewrite_enabled = true,
            .runtime_indexed_constructor_move_enabled = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_index_lowering_enabled = true,
            .dynamic_array_production_append_lowering_enabled = true,
        }
    );
    assert(runtime_indexed_member_transfer_audit.has_errors());
    assert(
        runtime_indexed_member_transfer_audit.error_text.find(
            "DynamicArray element path read of owned projection requires a non-owning scalar projection"
        ) != std::string::npos
    );
    auto has_runtime_indexed_member_transfer_audit_line =
        [&](std::string_view expected_line) {
            return std::any_of(
                runtime_indexed_member_transfer_audit.runtime_indexed_cleanup_audit_lines.begin(),
                runtime_indexed_member_transfer_audit.runtime_indexed_cleanup_audit_lines.end(),
                [&](std::string const& line) {
                    return line == expected_line;
                }
            );
        };
    assert(
        has_runtime_indexed_member_transfer_audit_line(
            "runtime-index member cleanup target owner items index (index + zero) element Box moved Inner "
            "member-path item operation drop-live-member-siblings "
            "drop-metadata __orison_member_cleanup.Box.except.item metadata ready production disabled"
        )
    );
    assert(
        has_runtime_indexed_member_transfer_audit_line(
            "runtime-index member cleanup production-readiness owner items index (index + zero) "
            "element Box moved Inner member-path item proof ready target-metadata ready helper-drop-bindings ready "
            "cfg-slice ready module-mutation blocked production-member-cleanup blocked production-gate blocked "
            "production-enabled false production blocked blockers 2 "
            "blocker member-cleanup-module-mutation blocker production-member-cleanup"
        )
    );
    assert(
        has_runtime_indexed_member_transfer_audit_line(
            "runtime-index member cleanup mutation-operation-validation owner items index (index + zero) "
            "element Box moved Inner member-path item seam selected count valid order valid "
            "branch-replacement-fields valid cfg-append-fields valid phi-retarget-fields valid "
            "operations-ready ready no-operations-applied true validation ready report-only true "
            "production disabled blockers 4 blocker member-cleanup-module-mutation "
            "blocker production-member-cleanup blocker member-cleanup-ir-mutation "
            "blocker production-member-cleanup-ir-mutation"
        )
    );
    assert(
        has_runtime_indexed_member_transfer_audit_line(
            "runtime-index member cleanup mutation rewrite authorization owner items index (index + zero) "
            "element Box moved Inner member-path item verdict blocked guarded-rewrite blocked "
            "authorization blocked rewrite-requested false rewrite-authorized false report-only true "
            "production disabled blockers 2 blocker member-cleanup-mutation-readiness-verdict "
            "blocker member-cleanup-mutation-guarded-rewrite"
        )
    );
    auto runtime_indexed_member_transfer_ir_mutation_request = pipeline.emit_llvm(
        runtime_indexed_member_transfer_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
            .runtime_indexed_cleanup_module_ir_insertion_enabled = true,
            .runtime_indexed_cleanup_module_ir_mutation_enabled = true,
            .runtime_indexed_cleanup_function_ir_module_rewrite_enabled = true,
            .runtime_indexed_constructor_move_enabled = true,
            .runtime_indexed_member_cleanup_ir_mutation_enabled = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_index_lowering_enabled = true,
            .dynamic_array_production_append_lowering_enabled = true,
        }
    );
    assert(runtime_indexed_member_transfer_ir_mutation_request.has_errors());
    assert(
        runtime_indexed_member_transfer_ir_mutation_request
            .runtime_indexed_member_cleanup_mutation_operation_plans.size() == 1
    );
    assert(
        runtime_indexed_member_transfer_ir_mutation_request
            .runtime_indexed_member_cleanup_mutation_operation_validations.size() == 1
    );
    assert(
        runtime_indexed_member_transfer_ir_mutation_request
            .runtime_indexed_member_cleanup_mutation_conflict_detections.size() == 1
    );
    assert(
        runtime_indexed_member_transfer_ir_mutation_request
            .runtime_indexed_member_cleanup_mutation_operation_validations.front()
            .blockers.size() == 3
    );
    assert(
        !std::ranges::contains(
            runtime_indexed_member_transfer_ir_mutation_request
                .runtime_indexed_member_cleanup_mutation_operation_validations.front()
                .blockers,
            "member-helper-drop-bindings"
        )
    );
    assert(
        runtime_indexed_member_transfer_ir_mutation_request
            .runtime_indexed_member_cleanup_mutation_apply_authorizations.size() == 1
    );
    assert(
        runtime_indexed_member_transfer_ir_mutation_request
            .runtime_indexed_member_cleanup_mutation_apply_previews.size() == 1
    );
    assert(
        runtime_indexed_member_transfer_ir_mutation_request
            .runtime_indexed_member_cleanup_mutation_post_apply_verifications.size() == 1
    );
    assert(
        runtime_indexed_member_transfer_ir_mutation_request
            .runtime_indexed_member_cleanup_mutation_promotion_summaries.size() == 1
    );
    assert(
        runtime_indexed_member_transfer_ir_mutation_request
            .runtime_indexed_member_cleanup_mutation_apply_authorizations.front()
            .blockers.size() == 3
    );
    assert(
        !std::ranges::contains(
            runtime_indexed_member_transfer_ir_mutation_request
                .runtime_indexed_member_cleanup_mutation_apply_authorizations.front()
                .blockers,
            "member-helper-drop-bindings"
        )
    );
    auto has_requested_member_transfer_audit_line =
        [&](std::string_view expected_line) {
            return std::any_of(
                runtime_indexed_member_transfer_ir_mutation_request
                    .runtime_indexed_cleanup_audit_lines.begin(),
                runtime_indexed_member_transfer_ir_mutation_request
                    .runtime_indexed_cleanup_audit_lines.end(),
                [&](std::string const& line) {
                    return line == expected_line;
                }
            );
        };
    assert(
        has_requested_member_transfer_audit_line(
            "runtime-index member cleanup mutation-apply-authorization owner items index (index + zero) "
            "element Box moved Inner member-path item validation ready conflict-free true "
            "ir-mutation requested production-gate disabled apply-requested false authorization blocked "
            "apply-authorized false "
            "report-only true production disabled blockers 3 blocker member-cleanup-module-mutation "
            "blocker production-member-cleanup "
            "blocker production-member-cleanup-ir-mutation"
        )
    );
    assert(
        has_requested_member_transfer_audit_line(
            "runtime-index member cleanup mutation-post-apply-verification owner items index (index + zero) "
            "element Box moved Inner member-path item preview ready apply-authorized false "
            "actions-applied false expected-checks 3 expected-checks-ready true verification blocked "
            "report-only true production disabled blockers 5 blocker member-cleanup-module-mutation "
            "blocker production-member-cleanup "
            "blocker production-member-cleanup-ir-mutation "
            "blocker member-cleanup-mutation-apply-authorization "
            "blocker member-cleanup-mutation-actions-applied expected-check branch-target items.final-cleanup "
            "expected-check cfg-appended items.member_cleanup.exit "
            "expected-check phi-predecessor items.member_cleanup.exit"
        )
    );
    auto runtime_indexed_member_transfer_production_gate_request = pipeline.emit_llvm(
        runtime_indexed_member_transfer_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
            .runtime_indexed_cleanup_module_ir_insertion_enabled = true,
            .runtime_indexed_cleanup_module_ir_mutation_enabled = true,
            .runtime_indexed_cleanup_function_ir_module_rewrite_enabled = true,
            .runtime_indexed_constructor_move_enabled = true,
            .runtime_indexed_member_cleanup_ir_mutation_enabled = true,
            .runtime_indexed_member_cleanup_production_gate_enabled = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_index_lowering_enabled = true,
            .dynamic_array_production_append_lowering_enabled = true,
        }
    );
    assert(runtime_indexed_member_transfer_production_gate_request.has_errors());
    assert(
        runtime_indexed_member_transfer_production_gate_request
            .runtime_indexed_member_cleanup_mutation_apply_authorizations.size() == 1
    );
    assert(
        runtime_indexed_member_transfer_production_gate_request
            .runtime_indexed_member_cleanup_mutation_apply_authorizations.front()
            .blockers.empty()
    );
    assert(
        runtime_indexed_member_transfer_production_gate_request
            .runtime_indexed_member_cleanup_mutation_post_apply_verifications.size() == 1
    );
    assert(
        runtime_indexed_member_transfer_production_gate_request
            .runtime_indexed_member_cleanup_mutation_post_apply_verifications.front()
            .blockers.size() == 2
    );
    auto has_production_gate_member_transfer_audit_line =
        [&](std::string_view expected_line) {
            return std::any_of(
                runtime_indexed_member_transfer_production_gate_request
                    .runtime_indexed_cleanup_audit_lines.begin(),
                runtime_indexed_member_transfer_production_gate_request
                    .runtime_indexed_cleanup_audit_lines.end(),
                [&](std::string const& line) {
                    return line == expected_line;
                }
            );
        };
    assert(
        has_production_gate_member_transfer_audit_line(
            "runtime-index member cleanup mutation-apply-authorization owner items index (index + zero) "
            "element Box moved Inner member-path item validation ready conflict-free true "
            "ir-mutation requested production-gate enabled apply-requested false authorization ready "
            "apply-authorized false "
            "report-only true production disabled blockers 0"
        )
    );
    assert(
        has_production_gate_member_transfer_audit_line(
            "runtime-index member cleanup mutation-post-apply-verification owner items index (index + zero) "
            "element Box moved Inner member-path item preview ready apply-authorized false "
            "actions-applied false expected-checks 3 expected-checks-ready true verification blocked "
            "report-only true production disabled blockers 2 blocker member-cleanup-mutation-apply-authorization "
            "blocker member-cleanup-mutation-actions-applied expected-check branch-target items.final-cleanup "
            "expected-check cfg-appended items.member_cleanup.exit "
            "expected-check phi-predecessor items.member_cleanup.exit"
        )
    );
    auto runtime_indexed_member_transfer_apply_request = pipeline.emit_llvm(
        runtime_indexed_member_transfer_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
            .runtime_indexed_cleanup_module_ir_insertion_enabled = true,
            .runtime_indexed_cleanup_module_ir_mutation_enabled = true,
            .runtime_indexed_cleanup_function_ir_module_rewrite_enabled = true,
            .runtime_indexed_constructor_move_enabled = true,
            .runtime_indexed_member_cleanup_ir_mutation_enabled = true,
            .runtime_indexed_member_cleanup_production_gate_enabled = true,
            .runtime_indexed_member_cleanup_apply_authorization_enabled = true,
            .runtime_indexed_member_cleanup_rewrite_execution_enabled = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_index_lowering_enabled = true,
            .dynamic_array_production_append_lowering_enabled = true,
        }
    );
    assert(!runtime_indexed_member_transfer_apply_request.has_errors());
    assert(runtime_indexed_member_transfer_apply_request.runtime_indexed_member_cleanup_typed_promotion_gates.size() == 1);
    assert(runtime_indexed_member_transfer_apply_request.runtime_indexed_member_cleanup_typed_promotion_gates.front().gate_ready);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_typed_promotion_gate_report(
            runtime_indexed_member_transfer_apply_request.runtime_indexed_member_cleanup_typed_promotion_gates.front()
        ) ==
        "runtime-index member cleanup typed-promotion-gate owner items index (index + zero) "
        "element Box moved Inner member-path item checklist ready ir-mutation-requested true "
        "production-gate-requested true ir-mutation enabled production-gate enabled "
        "gate ready report-only false production enabled blockers 0"
    );
    assert(runtime_indexed_member_transfer_apply_request.runtime_indexed_member_cleanup_execution_summaries.size() == 1);
    auto const& runtime_indexed_member_transfer_execution_summary =
        runtime_indexed_member_transfer_apply_request.runtime_indexed_member_cleanup_execution_summaries.front();
    assert(runtime_indexed_member_transfer_execution_summary.owner_name == "items");
    assert(runtime_indexed_member_transfer_execution_summary.index_expression_text == "(index + zero)");
    assert(runtime_indexed_member_transfer_execution_summary.element_source_type_name == "Box");
    assert(runtime_indexed_member_transfer_execution_summary.moved_source_type_name == "Inner");
    assert(runtime_indexed_member_transfer_execution_summary.moved_member_path == std::vector<std::string> {"item"});
    assert(runtime_indexed_member_transfer_execution_summary.helper_symbol_name == "__orison_member_cleanup.Box.except.item");
    assert(runtime_indexed_member_transfer_execution_summary.helper_binding_count == 1);
    assert(runtime_indexed_member_transfer_execution_summary.helper_sibling_binding_count == 0);
    assert(runtime_indexed_member_transfer_execution_summary.typed_gate_ready);
    assert(runtime_indexed_member_transfer_execution_summary.apply_authorized);
    assert(runtime_indexed_member_transfer_execution_summary.rewrite_authorized);
    assert(runtime_indexed_member_transfer_execution_summary.rewrite_execution_enabled);
    assert(runtime_indexed_member_transfer_execution_summary.rewrite_verdict_enabled);
    assert(runtime_indexed_member_transfer_execution_summary.rewrite_promotion_ready);
    assert(runtime_indexed_member_transfer_execution_summary.helper_definition_ready);
    assert(runtime_indexed_member_transfer_execution_summary.production_enabled);
    assert(
        orison::pipeline::runtime_indexed_member_cleanup_execution_summary_report(
            runtime_indexed_member_transfer_execution_summary
        ) ==
        "runtime-index member cleanup execution-summary owner items index (index + zero) "
        "element Box moved Inner member-path item typed-gate ready apply authorized "
        "rewrite-authorization authorized rewrite-execution enabled rewrite-verdict enabled "
        "rewrite-promotion ready helper-bindings 1 helper-target "
        "__orison_member_cleanup.Box.except.item helper-sibling-bindings 0 "
        "helper-definition ready production enabled"
    );

    auto runtime_indexed_two_member_transfers_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "runtime_indexed_dynamic_array_constructor_two_computed_member_transfers.or";
    auto runtime_indexed_two_member_transfers_apply_request = pipeline.emit_llvm(
        runtime_indexed_two_member_transfers_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
            .runtime_indexed_cleanup_module_ir_insertion_enabled = true,
            .runtime_indexed_cleanup_module_ir_mutation_enabled = true,
            .runtime_indexed_cleanup_function_ir_module_rewrite_enabled = true,
            .runtime_indexed_constructor_move_enabled = true,
            .runtime_indexed_member_cleanup_ir_mutation_enabled = true,
            .runtime_indexed_member_cleanup_production_gate_enabled = true,
            .runtime_indexed_member_cleanup_apply_authorization_enabled = true,
            .runtime_indexed_member_cleanup_rewrite_execution_enabled = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_index_lowering_enabled = true,
            .dynamic_array_production_append_lowering_enabled = true,
        }
    );
    assert(!runtime_indexed_two_member_transfers_apply_request.has_errors());
    auto const left_member_cleanup_key = orison::pipeline::RuntimeIndexedMemberCleanupMatchKey {
        .owner_name = "left_items",
        .index_expression_text = "(left_index + left_zero)",
        .element_source_type_name = "Box",
        .moved_source_type_name = "Inner",
        .moved_member_path = {"item"},
    };
    auto const right_member_cleanup_key = orison::pipeline::RuntimeIndexedMemberCleanupMatchKey {
        .owner_name = "right_items",
        .index_expression_text = "(right_index + right_zero)",
        .element_source_type_name = "Box",
        .moved_source_type_name = "Inner",
        .moved_member_path = {"item"},
    };
    auto assert_ready_member_cleanup_refresh_chain =
        [&](orison::pipeline::RuntimeIndexedMemberCleanupMatchKey const& key) {
            auto const* summary = orison::pipeline::find_runtime_indexed_member_cleanup_record(
                key,
                runtime_indexed_two_member_transfers_apply_request
                    .runtime_indexed_member_cleanup_execution_summaries
            );
            assert(summary != nullptr);
            assert(summary->typed_gate_ready);
            assert(summary->apply_authorized);
            assert(summary->rewrite_authorized);
            assert(summary->rewrite_execution_enabled);
            assert(summary->rewrite_verdict_enabled);
            assert(summary->rewrite_promotion_ready);
            assert(summary->helper_definition_ready);
            assert(summary->production_enabled);

            auto const* readiness = orison::pipeline::find_runtime_indexed_member_cleanup_record(
                key,
                runtime_indexed_two_member_transfers_apply_request
                    .runtime_indexed_member_cleanup_mutation_production_readiness
            );
            assert(readiness != nullptr);
            assert(readiness->readiness_ready);
            assert(readiness->production_enabled);

            auto const* authorization = orison::pipeline::find_runtime_indexed_member_cleanup_record(
                key,
                runtime_indexed_two_member_transfers_apply_request
                    .runtime_indexed_member_cleanup_mutation_rewrite_authorizations
            );
            assert(authorization != nullptr);
            assert(authorization->rewrite_authorized);

            auto const* execution_plan = orison::pipeline::find_runtime_indexed_member_cleanup_record(
                key,
                runtime_indexed_two_member_transfers_apply_request
                    .runtime_indexed_member_cleanup_mutation_rewrite_execution_plans
            );
            assert(execution_plan != nullptr);
            assert(execution_plan->execution_enabled);

            auto const* promotion_status = orison::pipeline::find_runtime_indexed_member_cleanup_record(
                key,
                runtime_indexed_two_member_transfers_apply_request
                    .runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses
            );
            assert(promotion_status != nullptr);
            assert(promotion_status->promotion_ready);
            assert(promotion_status->production_enabled);
        };
    assert(
        runtime_indexed_two_member_transfers_apply_request
            .runtime_indexed_member_cleanup_execution_summaries.size() == 2
    );
    assert(
        runtime_indexed_two_member_transfers_apply_request
            .runtime_indexed_member_cleanup_mutation_production_readiness.size() == 2
    );
    assert(
        runtime_indexed_two_member_transfers_apply_request
            .runtime_indexed_member_cleanup_mutation_rewrite_authorizations.size() == 2
    );
    assert(
        runtime_indexed_two_member_transfers_apply_request
            .runtime_indexed_member_cleanup_mutation_rewrite_execution_plans.size() == 2
    );
    assert(
        runtime_indexed_two_member_transfers_apply_request
            .runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses.size() == 2
    );
    assert_ready_member_cleanup_refresh_chain(left_member_cleanup_key);
    assert_ready_member_cleanup_refresh_chain(right_member_cleanup_key);
    auto const& two_member_transfers_ir = runtime_indexed_two_member_transfers_apply_request.ir_text;
    auto assert_owner_member_cleanup_ir =
        [&](std::string_view owner_name) {
            auto const owner = std::string {owner_name};
            assert(
                two_member_transfers_ir.find(
                    "  br label %" + owner + ".member_cleanup.entry\n"
                ) != std::string::npos
            );
            assert(
                two_member_transfers_ir.find(
                    owner + ".member_cleanup.entry:\n"
                ) != std::string::npos
            );
            assert(
                two_member_transfers_ir.find(
                    owner + ".member_cleanup.drop_siblings:\n"
                    "  %" + owner + ".member_cleanup.descriptor = load { ptr, i64, i64 }, ptr %" +
                    owner + ".addr\n"
                ) != std::string::npos
            );
            assert(
                two_member_transfers_ir.find(
                    owner + ".member_cleanup.drop_siblings_for_moved:\n"
                    "  %" + owner + ".member_cleanup.moved.addr = getelementptr %record.Box, ptr %" +
                    owner + ".member_cleanup.cleanup.data, i64 %" + owner + ".member_cleanup.index\n"
                    "  call void @__orison_member_cleanup.Box.except.item(ptr %" +
                    owner + ".member_cleanup.moved.addr)\n"
                ) != std::string::npos
            );
            assert(
                two_member_transfers_ir.find(
                    owner + ".member_cleanup.drop_element:\n"
                    "  %" + owner + ".member_cleanup.element.addr = getelementptr %record.Box, ptr %" +
                    owner + ".member_cleanup.cleanup.data, i64 %" + owner + ".member_cleanup.index\n"
                    "  call void @__orison_drop.Box(ptr %" + owner + ".member_cleanup.element.addr)\n"
                    "  store %record.Box zeroinitializer, ptr %" + owner + ".member_cleanup.element.addr\n"
                ) != std::string::npos
            );
            assert(
                two_member_transfers_ir.find(
                    owner + ".member_cleanup.deallocate:\n"
                    "  call void @__orison_dynamic_array_deallocate(ptr %" +
                    owner + ".member_cleanup.cleanup.data, i64 "
                ) != std::string::npos
            );
            assert(
                two_member_transfers_ir.find(
                    "  store { ptr, i64, i64 } zeroinitializer, ptr %" + owner + ".addr\n"
                    "  br label %" + owner + ".member_cleanup.preserve_moved\n"
                ) != std::string::npos
            );
        };
    assert_owner_member_cleanup_ir("left_items");
    assert_owner_member_cleanup_ir("right_items");
    assert(
        two_member_transfers_ir.find(
            "  %left_items.member_cleanup.is_moved = icmp eq i64 "
            "%left_items.member_cleanup.index, %tmp6\n"
        ) != std::string::npos
    );
    assert(
        two_member_transfers_ir.find(
            "  %right_items.member_cleanup.is_moved = icmp eq i64 "
            "%right_items.member_cleanup.index, %tmp17\n"
        ) != std::string::npos
    );
    assert(
        two_member_transfers_ir.find(
            "  %left_items.member_cleanup.is_moved = icmp eq i64 "
            "%left_items.member_cleanup.index, %tmp17\n"
        ) == std::string::npos
    );
    assert(
        two_member_transfers_ir.find(
            "  %right_items.member_cleanup.is_moved = icmp eq i64 "
            "%right_items.member_cleanup.index, %left_items.member_cleanup.index\n"
        ) == std::string::npos
    );
    assert(
        occurrence_count(
            two_member_transfers_ir,
            "call void @__orison_member_cleanup.Box.except.item"
        ) == 2
    );
    assert(
        occurrence_count(
            two_member_transfers_ir,
            "call void @__orison_drop.Box"
        ) == 2
    );
    assert(
        occurrence_count(
            two_member_transfers_ir,
            "call void @__orison_dynamic_array_deallocate"
        ) == 2
    );
    assert(
        occurrence_count(
            two_member_transfers_ir,
            "define void @__orison_member_cleanup.Box.except.item(ptr %value)"
        ) == 1
    );
    assert(
        two_member_transfers_ir.find(
            "declare void @__orison_member_cleanup.Box.except.item(ptr)"
        ) == std::string::npos
    );
    auto runtime_indexed_two_member_transfers_object =
        orison::lowering::LlvmObjectEmitter {}.emit(two_member_transfers_ir);
    assert(!runtime_indexed_two_member_transfers_object.has_errors());
    auto runtime_indexed_two_member_transfers_executable =
        smoke_temp_root / "runtime_indexed_two_member_transfers";
    auto runtime_indexed_two_member_transfers_link =
        orison::link::HostLinker {}.link(
            runtime_indexed_two_member_transfers_object.object_bytes,
            runtime_indexed_two_member_transfers_executable
        );
    assert(!runtime_indexed_two_member_transfers_link.has_errors());
    auto runtime_indexed_two_member_transfers_status =
        std::system(runtime_indexed_two_member_transfers_executable.string().c_str());
    assert(WIFEXITED(runtime_indexed_two_member_transfers_status));
    assert(WEXITSTATUS(runtime_indexed_two_member_transfers_status) == 0);

    auto runtime_indexed_two_nested_member_transfers_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "runtime_indexed_dynamic_array_constructor_two_computed_nested_member_sibling_transfers.or";
    auto runtime_indexed_two_nested_member_transfers_apply_request = pipeline.emit_llvm(
        runtime_indexed_two_nested_member_transfers_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
            .runtime_indexed_cleanup_module_ir_insertion_enabled = true,
            .runtime_indexed_cleanup_module_ir_mutation_enabled = true,
            .runtime_indexed_cleanup_function_ir_module_rewrite_enabled = true,
            .runtime_indexed_constructor_move_enabled = true,
            .runtime_indexed_member_cleanup_ir_mutation_enabled = true,
            .runtime_indexed_member_cleanup_production_gate_enabled = true,
            .runtime_indexed_member_cleanup_apply_authorization_enabled = true,
            .runtime_indexed_member_cleanup_rewrite_execution_enabled = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_index_lowering_enabled = true,
            .dynamic_array_production_append_lowering_enabled = true,
        }
    );
    assert(!runtime_indexed_two_nested_member_transfers_apply_request.has_errors());
    assert(
        runtime_indexed_two_nested_member_transfers_apply_request
            .runtime_indexed_member_cleanup_execution_summaries.size() == 2
    );
    assert(
        runtime_indexed_two_nested_member_transfers_apply_request
            .runtime_indexed_member_cleanup_helper_drop_bindings.size() == 2
    );
    assert(
        runtime_indexed_two_nested_member_transfers_apply_request
            .runtime_indexed_member_cleanup_sibling_fields.size() == 8
    );
    auto assert_ready_nested_member_cleanup_binding =
        [&](std::string_view owner_name, std::string_view index_expression) {
            auto const key = orison::pipeline::RuntimeIndexedMemberCleanupMatchKey {
                .owner_name = std::string {owner_name},
                .index_expression_text = std::string {index_expression},
                .element_source_type_name = "Wrap",
                .moved_source_type_name = "Inner",
                .moved_member_path = {"box", "item"},
            };
            auto const* summary = orison::pipeline::find_runtime_indexed_member_cleanup_record(
                key,
                runtime_indexed_two_nested_member_transfers_apply_request
                    .runtime_indexed_member_cleanup_execution_summaries
            );
            assert(summary != nullptr);
            assert(summary->helper_symbol_name == "__orison_member_cleanup.Wrap.except.box.item");
            assert(summary->helper_binding_count == 1);
            assert(summary->helper_sibling_binding_count == 4);
            assert(summary->helper_definition_ready);
            assert(summary->production_enabled);

            auto const* bindings = orison::pipeline::find_runtime_indexed_member_cleanup_record(
                key,
                runtime_indexed_two_nested_member_transfers_apply_request
                    .runtime_indexed_member_cleanup_helper_drop_bindings
            );
            assert(bindings != nullptr);
            assert(bindings->sibling_binding_count == 4);
            assert(bindings->all_drop_definitions_available);
            assert(bindings->nested_member_path);
            assert(bindings->helper_definition_ready);
            assert(!bindings->production_enabled);
        };
    assert_ready_nested_member_cleanup_binding("left_items", "(left_index + left_zero)");
    assert_ready_nested_member_cleanup_binding("right_items", "(right_index + right_zero)");
    auto const& two_nested_member_transfers_ir =
        runtime_indexed_two_nested_member_transfers_apply_request.ir_text;
    assert(
        occurrence_count(
            two_nested_member_transfers_ir,
            "define void @__orison_member_cleanup.Wrap.except.box.item(ptr %value)"
        ) == 1
    );
    assert(
        occurrence_count(
            two_nested_member_transfers_ir,
            "call void @__orison_member_cleanup.Wrap.except.box.item"
        ) == 2
    );
    assert(
        occurrence_count(
            two_nested_member_transfers_ir,
            "call void @__orison_drop.Wrap"
        ) == 2
    );
    assert(
        occurrence_count(
            two_nested_member_transfers_ir,
            "call void @__orison_drop.Tail"
        ) == 2
    );
    assert(
        two_nested_member_transfers_ir.find(
            "declare void @__orison_member_cleanup.Wrap.except.box.item(ptr)"
        ) == std::string::npos
    );
    auto runtime_indexed_two_nested_member_transfers_object =
        orison::lowering::LlvmObjectEmitter {}.emit(two_nested_member_transfers_ir);
    assert(!runtime_indexed_two_nested_member_transfers_object.has_errors());
    auto runtime_indexed_two_nested_member_transfers_executable =
        smoke_temp_root / "runtime_indexed_two_nested_member_transfers";
    auto runtime_indexed_two_nested_member_transfers_link =
        orison::link::HostLinker {}.link(
            runtime_indexed_two_nested_member_transfers_object.object_bytes,
            runtime_indexed_two_nested_member_transfers_executable
        );
    assert(!runtime_indexed_two_nested_member_transfers_link.has_errors());
    auto runtime_indexed_two_nested_member_transfers_status =
        std::system(runtime_indexed_two_nested_member_transfers_executable.string().c_str());
    assert(WIFEXITED(runtime_indexed_two_nested_member_transfers_status));
    assert(WEXITSTATUS(runtime_indexed_two_nested_member_transfers_status) == 0);

    auto synthetic_member_cleanup_summary_result = orison::pipeline::CompilePipelineResult {};
    synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_typed_promotion_gates = {
        orison::lowering::RuntimeIndexedMemberCleanupTypedPromotionGate {
            .owner_name = "lefts",
            .index_expression_text = "i",
            .element_source_type_name = "LeftBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"payload"},
            .gate_ready = true,
            .production_enabled = true,
        },
        orison::lowering::RuntimeIndexedMemberCleanupTypedPromotionGate {
            .owner_name = "rights",
            .index_expression_text = "j",
            .element_source_type_name = "RightBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"inner", "payload"},
            .gate_ready = true,
            .production_enabled = false,
        },
    };
    synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_production_readiness = {
        orison::lowering::RuntimeIndexedMemberCleanupProductionReadiness {
            .owner_name = "rights",
            .index_expression_text = "j",
            .element_source_type_name = "RightBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"inner", "payload"},
            .blockers = {"right-production-blocker"},
        },
        orison::lowering::RuntimeIndexedMemberCleanupProductionReadiness {
            .owner_name = "lefts",
            .index_expression_text = "i",
            .element_source_type_name = "LeftBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"payload"},
            .production_ready = true,
        },
    };
    synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_mutation_operation_plans = {
        orison::lowering::RuntimeIndexedMemberCleanupMutationOperationPlan {
            .owner_name = "rights",
            .index_expression_text = "j",
            .element_source_type_name = "RightBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"inner", "payload"},
        },
        orison::lowering::RuntimeIndexedMemberCleanupMutationOperationPlan {
            .owner_name = "lefts",
            .index_expression_text = "i",
            .element_source_type_name = "LeftBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"payload"},
        },
    };
    synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_mutation_operation_validations = {
        orison::lowering::RuntimeIndexedMemberCleanupMutationOperationValidation {
            .owner_name = "rights",
            .index_expression_text = "j",
            .element_source_type_name = "RightBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"inner", "payload"},
        },
        orison::lowering::RuntimeIndexedMemberCleanupMutationOperationValidation {
            .owner_name = "lefts",
            .index_expression_text = "i",
            .element_source_type_name = "LeftBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"payload"},
        },
    };
    synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_mutation_conflict_detections = {
        orison::lowering::RuntimeIndexedMemberCleanupMutationConflictDetection {
            .owner_name = "rights",
            .index_expression_text = "j",
            .element_source_type_name = "RightBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"inner", "payload"},
        },
        orison::lowering::RuntimeIndexedMemberCleanupMutationConflictDetection {
            .owner_name = "lefts",
            .index_expression_text = "i",
            .element_source_type_name = "LeftBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"payload"},
        },
    };
    synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_mutation_apply_authorizations = {
        orison::lowering::RuntimeIndexedMemberCleanupMutationApplyAuthorization {
            .owner_name = "rights",
            .index_expression_text = "j",
            .element_source_type_name = "RightBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"inner", "payload"},
            .apply_authorized = false,
        },
        orison::lowering::RuntimeIndexedMemberCleanupMutationApplyAuthorization {
            .owner_name = "lefts",
            .index_expression_text = "i",
            .element_source_type_name = "LeftBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"payload"},
            .apply_authorized = true,
        },
    };
    synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_mutation_apply_previews = {
        orison::lowering::RuntimeIndexedMemberCleanupMutationApplyPreview {
            .owner_name = "rights",
            .index_expression_text = "j",
            .element_source_type_name = "RightBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"inner", "payload"},
        },
        orison::lowering::RuntimeIndexedMemberCleanupMutationApplyPreview {
            .owner_name = "lefts",
            .index_expression_text = "i",
            .element_source_type_name = "LeftBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"payload"},
        },
    };
    synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_mutation_post_apply_verifications = {
        orison::lowering::RuntimeIndexedMemberCleanupMutationPostApplyVerification {
            .owner_name = "rights",
            .index_expression_text = "j",
            .element_source_type_name = "RightBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"inner", "payload"},
        },
        orison::lowering::RuntimeIndexedMemberCleanupMutationPostApplyVerification {
            .owner_name = "lefts",
            .index_expression_text = "i",
            .element_source_type_name = "LeftBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"payload"},
        },
    };
    synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_mutation_promotion_summaries = {
        orison::lowering::RuntimeIndexedMemberCleanupMutationPromotionSummary {
            .owner_name = "rights",
            .index_expression_text = "j",
            .element_source_type_name = "RightBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"inner", "payload"},
        },
        orison::lowering::RuntimeIndexedMemberCleanupMutationPromotionSummary {
            .owner_name = "lefts",
            .index_expression_text = "i",
            .element_source_type_name = "LeftBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"payload"},
        },
    };
    synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_mutation_production_readiness = {
        orison::lowering::RuntimeIndexedMemberCleanupMutationProductionReadiness {
            .owner_name = "rights",
            .index_expression_text = "j",
            .element_source_type_name = "RightBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"inner", "payload"},
            .blockers = {"right-mutation-blocker"},
        },
        orison::lowering::RuntimeIndexedMemberCleanupMutationProductionReadiness {
            .owner_name = "lefts",
            .index_expression_text = "i",
            .element_source_type_name = "LeftBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"payload"},
            .readiness_ready = true,
        },
    };
    synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_mutation_readiness_verdicts = {
        orison::lowering::RuntimeIndexedMemberCleanupMutationReadinessVerdict {
            .owner_name = "rights",
            .index_expression_text = "j",
            .element_source_type_name = "RightBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"inner", "payload"},
        },
        orison::lowering::RuntimeIndexedMemberCleanupMutationReadinessVerdict {
            .owner_name = "lefts",
            .index_expression_text = "i",
            .element_source_type_name = "LeftBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"payload"},
            .readiness_ready = true,
        },
    };
    synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_mutation_rewrite_authorizations = {
        orison::lowering::RuntimeIndexedMemberCleanupMutationRewriteAuthorization {
            .owner_name = "rights",
            .index_expression_text = "j",
            .element_source_type_name = "RightBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"inner", "payload"},
            .rewrite_authorized = false,
        },
        orison::lowering::RuntimeIndexedMemberCleanupMutationRewriteAuthorization {
            .owner_name = "lefts",
            .index_expression_text = "i",
            .element_source_type_name = "LeftBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"payload"},
            .rewrite_authorized = true,
        },
    };
    synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans = {
        orison::lowering::RuntimeIndexedMemberCleanupMutationRewriteExecutionPlan {
            .owner_name = "rights",
            .index_expression_text = "j",
            .element_source_type_name = "RightBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"inner", "payload"},
            .execution_enabled = false,
        },
        orison::lowering::RuntimeIndexedMemberCleanupMutationRewriteExecutionPlan {
            .owner_name = "lefts",
            .index_expression_text = "i",
            .element_source_type_name = "LeftBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"payload"},
            .execution_enabled = true,
        },
    };
    synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_mutation_rewrite_execution_verdicts = {
        orison::lowering::RuntimeIndexedMemberCleanupMutationRewriteExecutionVerdict {
            .owner_name = "rights",
            .index_expression_text = "j",
            .element_source_type_name = "RightBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"inner", "payload"},
            .execution_enabled = false,
        },
        orison::lowering::RuntimeIndexedMemberCleanupMutationRewriteExecutionVerdict {
            .owner_name = "lefts",
            .index_expression_text = "i",
            .element_source_type_name = "LeftBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"payload"},
            .execution_enabled = true,
        },
    };
    synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses = {
        orison::lowering::RuntimeIndexedMemberCleanupMutationRewritePromotionStatus {
            .owner_name = "rights",
            .index_expression_text = "j",
            .element_source_type_name = "RightBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"inner", "payload"},
            .promotion_ready = false,
        },
        orison::lowering::RuntimeIndexedMemberCleanupMutationRewritePromotionStatus {
            .owner_name = "lefts",
            .index_expression_text = "i",
            .element_source_type_name = "LeftBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"payload"},
            .promotion_ready = true,
        },
    };
    synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_helper_drop_bindings = {
        orison::lowering::RuntimeIndexedMemberCleanupHelperDropBindings {
            .owner_name = "rights",
            .index_expression_text = "j",
            .element_source_type_name = "RightBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"inner", "payload"},
            .helper_symbol_name = "__orison_member_cleanup.RightBox.except.inner.payload",
            .sibling_binding_count = 2,
            .helper_definition_ready = true,
        },
        orison::lowering::RuntimeIndexedMemberCleanupHelperDropBindings {
            .owner_name = "lefts",
            .index_expression_text = "i",
            .element_source_type_name = "LeftBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"payload"},
            .helper_symbol_name = "__orison_member_cleanup.LeftBox.except.payload",
            .sibling_binding_count = 1,
            .helper_definition_ready = true,
        },
    };
    auto const synthetic_left_key = orison::pipeline::RuntimeIndexedMemberCleanupMatchKey {
        .owner_name = "lefts",
        .index_expression_text = "i",
        .element_source_type_name = "LeftBox",
        .moved_source_type_name = "Payload",
        .moved_member_path = {"payload"},
    };
    auto const synthetic_right_key = orison::pipeline::RuntimeIndexedMemberCleanupMatchKey {
        .owner_name = "rights",
        .index_expression_text = "j",
        .element_source_type_name = "RightBox",
        .moved_source_type_name = "Payload",
        .moved_member_path = {"inner", "payload"},
    };
    assert(
        orison::pipeline::runtime_indexed_member_cleanup_match_key(
            synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_typed_promotion_gates[0]
        ) == synthetic_left_key
    );
    assert(
        orison::pipeline::runtime_indexed_member_cleanup_match_key(
            synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_helper_drop_bindings[0]
        ) == synthetic_right_key
    );
    assert(
        orison::pipeline::same_runtime_indexed_member_cleanup_key(
            synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_typed_promotion_gates[0],
            synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_mutation_apply_authorizations[1]
        )
    );
    assert(
        !orison::pipeline::same_runtime_indexed_member_cleanup_key(
            synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_typed_promotion_gates[0],
            synthetic_member_cleanup_summary_result.runtime_indexed_member_cleanup_mutation_apply_authorizations[0]
        )
    );
    auto synthetic_member_cleanup_summaries =
        orison::pipeline::runtime_indexed_member_cleanup_execution_summaries(
            synthetic_member_cleanup_summary_result
        );
    assert(synthetic_member_cleanup_summaries.size() == 2);
    assert(synthetic_member_cleanup_summaries[0].owner_name == "lefts");
    assert(synthetic_member_cleanup_summaries[0].helper_symbol_name == "__orison_member_cleanup.LeftBox.except.payload");
    assert(synthetic_member_cleanup_summaries[0].helper_binding_count == 1);
    assert(synthetic_member_cleanup_summaries[0].helper_sibling_binding_count == 1);
    assert(synthetic_member_cleanup_summaries[0].apply_authorized);
    assert(synthetic_member_cleanup_summaries[0].rewrite_authorized);
    assert(synthetic_member_cleanup_summaries[0].rewrite_execution_enabled);
    assert(synthetic_member_cleanup_summaries[0].rewrite_verdict_enabled);
    assert(synthetic_member_cleanup_summaries[0].rewrite_promotion_ready);
    assert(synthetic_member_cleanup_summaries[0].production_enabled);
    assert(synthetic_member_cleanup_summaries[1].owner_name == "rights");
    assert(
        synthetic_member_cleanup_summaries[1].helper_symbol_name ==
        "__orison_member_cleanup.RightBox.except.inner.payload"
    );
    assert(synthetic_member_cleanup_summaries[1].helper_binding_count == 1);
    assert(synthetic_member_cleanup_summaries[1].helper_sibling_binding_count == 2);
    assert(!synthetic_member_cleanup_summaries[1].apply_authorized);
    assert(!synthetic_member_cleanup_summaries[1].rewrite_authorized);
    assert(!synthetic_member_cleanup_summaries[1].rewrite_execution_enabled);
    assert(!synthetic_member_cleanup_summaries[1].rewrite_verdict_enabled);
    assert(!synthetic_member_cleanup_summaries[1].rewrite_promotion_ready);
    assert(!synthetic_member_cleanup_summaries[1].production_enabled);
    auto const synthetic_member_cleanup_readiness_lines =
        orison::pipeline::runtime_indexed_member_cleanup_readiness_report_lines(
            synthetic_member_cleanup_summary_result
        );
    auto const left_helper_line = line_index_containing(
        synthetic_member_cleanup_readiness_lines,
        "helper-drop-bindings owner lefts index i element LeftBox"
    );
    auto const left_plan_line = line_index_containing(
        synthetic_member_cleanup_readiness_lines,
        "mutation-operation-plan owner lefts index i element LeftBox"
    );
    auto const left_gate_line = line_index_containing(
        synthetic_member_cleanup_readiness_lines,
        "typed-promotion-gate owner lefts index i element LeftBox"
    );
    auto const left_rewrite_status_line = line_index_containing(
        synthetic_member_cleanup_readiness_lines,
        "rewrite promotion-status owner lefts index i element LeftBox"
    );
    auto const right_helper_line = line_index_containing(
        synthetic_member_cleanup_readiness_lines,
        "helper-drop-bindings owner rights index j element RightBox"
    );
    auto const right_gate_line = line_index_containing(
        synthetic_member_cleanup_readiness_lines,
        "typed-promotion-gate owner rights index j element RightBox"
    );
    assert(left_helper_line < left_plan_line);
    assert(left_plan_line < left_gate_line);
    assert(left_gate_line < left_rewrite_status_line);
    assert(left_rewrite_status_line < right_helper_line);
    assert(right_helper_line < right_gate_line);
    auto const synthetic_member_cleanup_promotion_state =
        orison::pipeline::runtime_indexed_member_cleanup_promotion_state(
            synthetic_member_cleanup_summary_result
        );
    assert(synthetic_member_cleanup_promotion_state.state == "blocked");
    assert(synthetic_member_cleanup_promotion_state.production_readiness_count == 2);
    assert(synthetic_member_cleanup_promotion_state.typed_gate_count == 2);
    assert(synthetic_member_cleanup_promotion_state.mutation_readiness_count == 2);
    assert(synthetic_member_cleanup_promotion_state.rewrite_promotion_count == 2);
    auto const synthetic_member_cleanup_promotion_lines =
        orison::pipeline::runtime_indexed_member_cleanup_promotion_state_report_lines(
            synthetic_member_cleanup_summary_result
        );
    assert_any_line_contains(
        synthetic_member_cleanup_promotion_lines,
        "runtime-index member cleanup promotion blocker owner rights index j "
        "element RightBox moved Payload member-path inner.payload "
        "blocker blocked-production-readiness"
    );
    assert_any_line_contains(
        synthetic_member_cleanup_promotion_lines,
        "runtime-index member cleanup promotion blocker owner rights index j "
        "element RightBox moved Payload member-path inner.payload "
        "blocker typed-promotion-disabled"
    );
    assert_any_line_contains(
        synthetic_member_cleanup_promotion_lines,
        "runtime-index member cleanup promotion blocker owner rights index j "
        "element RightBox moved Payload member-path inner.payload "
        "blocker blocked-mutation-readiness"
    );
    assert_any_line_contains(
        synthetic_member_cleanup_promotion_lines,
        "runtime-index member cleanup promotion blocker owner rights index j "
        "element RightBox moved Payload member-path inner.payload "
        "blocker blocked-rewrite-promotion"
    );

    auto synthetic_ready_member_cleanup_promotion_result = orison::pipeline::CompilePipelineResult {};
    synthetic_ready_member_cleanup_promotion_result.runtime_indexed_member_cleanup_typed_promotion_gates = {
        orison::lowering::RuntimeIndexedMemberCleanupTypedPromotionGate {
            .owner_name = "lefts",
            .index_expression_text = "i",
            .element_source_type_name = "LeftBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"payload"},
            .production_enabled = true,
        },
        orison::lowering::RuntimeIndexedMemberCleanupTypedPromotionGate {
            .owner_name = "rights",
            .index_expression_text = "j",
            .element_source_type_name = "RightBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"inner", "payload"},
            .production_enabled = true,
        },
    };
    synthetic_ready_member_cleanup_promotion_result.runtime_indexed_member_cleanup_production_readiness = {
        orison::lowering::RuntimeIndexedMemberCleanupProductionReadiness {
            .owner_name = "rights",
            .index_expression_text = "j",
            .element_source_type_name = "RightBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"inner", "payload"},
            .blockers = {"member-cleanup-module-mutation", "production-member-cleanup"},
            .proof_ready = true,
            .target_metadata_ready = true,
            .helper_drop_bindings_ready = true,
            .cfg_slice_ready = true,
            .module_mutation_ready = false,
            .production_member_cleanup_ready = false,
            .production_gate_ready = false,
            .production_enabled = false,
            .production_ready = false,
        },
        orison::lowering::RuntimeIndexedMemberCleanupProductionReadiness {
            .owner_name = "lefts",
            .index_expression_text = "i",
            .element_source_type_name = "LeftBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"payload"},
            .blockers = {"member-cleanup-module-mutation", "production-member-cleanup"},
            .proof_ready = true,
            .target_metadata_ready = true,
            .helper_drop_bindings_ready = true,
            .cfg_slice_ready = true,
            .module_mutation_ready = false,
            .production_member_cleanup_ready = false,
            .production_gate_ready = false,
            .production_enabled = false,
            .production_ready = false,
        },
    };
    synthetic_ready_member_cleanup_promotion_result.runtime_indexed_member_cleanup_mutation_production_readiness = {
        orison::lowering::RuntimeIndexedMemberCleanupMutationProductionReadiness {
            .owner_name = "rights",
            .index_expression_text = "j",
            .element_source_type_name = "RightBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"inner", "payload"},
            .production_enabled = true,
        },
        orison::lowering::RuntimeIndexedMemberCleanupMutationProductionReadiness {
            .owner_name = "lefts",
            .index_expression_text = "i",
            .element_source_type_name = "LeftBox",
            .moved_source_type_name = "Payload",
            .moved_member_path = {"payload"},
            .production_enabled = true,
        },
    };
    synthetic_ready_member_cleanup_promotion_result
        .runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses = {
            orison::lowering::RuntimeIndexedMemberCleanupMutationRewritePromotionStatus {
                .owner_name = "rights",
                .index_expression_text = "j",
                .element_source_type_name = "RightBox",
                .moved_source_type_name = "Payload",
                .moved_member_path = {"inner", "payload"},
                .production_enabled = true,
            },
            orison::lowering::RuntimeIndexedMemberCleanupMutationRewritePromotionStatus {
                .owner_name = "lefts",
                .index_expression_text = "i",
                .element_source_type_name = "LeftBox",
                .moved_source_type_name = "Payload",
                .moved_member_path = {"payload"},
                .production_enabled = true,
            },
        };
    auto const ready_member_cleanup_promotion_state =
        orison::pipeline::runtime_indexed_member_cleanup_promotion_state(
            synthetic_ready_member_cleanup_promotion_result
        );
    assert(ready_member_cleanup_promotion_state.state == "ready");
    assert(ready_member_cleanup_promotion_state.module_ir_shape_ready);
    assert(
        orison::pipeline::runtime_indexed_member_cleanup_promotion_state_report_lines(
            synthetic_ready_member_cleanup_promotion_result
        ).empty()
    );

    synthetic_ready_member_cleanup_promotion_result
        .runtime_indexed_cleanup_module_ir_production_readiness_state.ir_shape_ready = false;
    synthetic_ready_member_cleanup_promotion_result
        .runtime_indexed_cleanup_emission_plan_state.plans = {
            descriptor_runtime_indexed_cleanup_ir_shape_plan_without_deallocate_tail(),
        };
    auto const ir_shape_blocked_member_cleanup_promotion_state =
        orison::pipeline::runtime_indexed_member_cleanup_promotion_state(
            synthetic_ready_member_cleanup_promotion_result
        );
    assert(ir_shape_blocked_member_cleanup_promotion_state.state == "blocked");
    assert(!ir_shape_blocked_member_cleanup_promotion_state.module_ir_shape_ready);
    assert(
        ir_shape_blocked_member_cleanup_promotion_state.module_ir_shape_blocker_detail.find(
            "owner items common-loop ready drop-call ready descriptor-storage blocked inline-storage blocked"
        ) != std::string::npos
    );
    assert(
        ir_shape_blocked_member_cleanup_promotion_state.module_ir_shape_blocker_detail.find(
            "descriptor-load present descriptor-gep present inline-gep absent zero-store absent deallocate absent"
        ) != std::string::npos
    );
    auto const ir_shape_blocked_member_cleanup_promotion_lines =
        orison::pipeline::runtime_indexed_member_cleanup_promotion_state_report_lines(
            synthetic_ready_member_cleanup_promotion_result
        );
    assert_any_line_contains(
        ir_shape_blocked_member_cleanup_promotion_lines,
        "runtime-index member cleanup promotion blocker owner lefts index i "
        "element LeftBox moved Payload member-path payload "
        "blocker blocked-module-ir-shape"
    );
    assert_any_line_contains(
        ir_shape_blocked_member_cleanup_promotion_lines,
        "detail runtime-index cleanup module-ir shape is blocked owner items "
        "common-loop ready drop-call ready descriptor-storage blocked inline-storage blocked"
    );
    assert_any_line_contains(
        ir_shape_blocked_member_cleanup_promotion_lines,
        "descriptor-load present descriptor-gep present inline-gep absent zero-store absent deallocate absent"
    );
    synthetic_ready_member_cleanup_promotion_result
        .runtime_indexed_cleanup_module_ir_production_readiness_state.ir_shape_ready = true;
    synthetic_ready_member_cleanup_promotion_result
        .runtime_indexed_cleanup_emission_plan_state.plans.clear();

    synthetic_ready_member_cleanup_promotion_result
        .runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses.pop_back();
    auto const incomplete_member_cleanup_promotion_state =
        orison::pipeline::runtime_indexed_member_cleanup_promotion_state(
            synthetic_ready_member_cleanup_promotion_result
        );
    assert(incomplete_member_cleanup_promotion_state.state == "blocked");
    auto const incomplete_member_cleanup_promotion_lines =
        orison::pipeline::runtime_indexed_member_cleanup_promotion_state_report_lines(
            synthetic_ready_member_cleanup_promotion_result
        );
    assert_any_line_contains(
        incomplete_member_cleanup_promotion_lines,
        "runtime-index member cleanup promotion blocker owner lefts index i "
        "element LeftBox moved Payload member-path payload "
        "blocker missing-rewrite-promotion"
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_apply_authorizations.size() == 1
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_apply_authorizations.front()
            .blockers.empty()
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_post_apply_verifications.size() == 1
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_post_apply_verifications.front()
            .blockers.empty()
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_promotion_summaries.size() == 1
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_promotion_summaries.front()
            .blockers.empty()
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_promotion_summaries.front()
            .promotion_ready
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_promotion_summaries.front()
            .production_enabled
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_production_readiness.size() == 1
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_production_readiness.front()
            .readiness_ready
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_production_readiness.front()
            .production_enabled
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_readiness_verdicts.size() == 1
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_readiness_verdicts.front()
            .guarded_rewrite_ready
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_rewrite_authorizations.size() == 1
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_rewrite_authorizations.front()
            .rewrite_authorized
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_rewrite_execution_plans.size() == 1
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_rewrite_execution_plans.front()
            .execution_enabled
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_rewrite_execution_verdicts.size() == 1
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_rewrite_execution_verdicts.front()
            .execution_enabled
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses.size() == 1
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses.front()
            .promotion_ready
    );
    auto has_apply_requested_member_transfer_audit_line =
        [&](std::string_view expected_line) {
            return std::any_of(
                runtime_indexed_member_transfer_apply_request
                    .runtime_indexed_cleanup_audit_lines.begin(),
                runtime_indexed_member_transfer_apply_request
                    .runtime_indexed_cleanup_audit_lines.end(),
                [&](std::string const& line) {
                    return line == expected_line;
                }
            );
        };
    assert(
        has_apply_requested_member_transfer_audit_line(
            "runtime-index member cleanup mutation-apply-authorization owner items index (index + zero) "
            "element Box moved Inner member-path item validation ready conflict-free true "
            "ir-mutation requested production-gate enabled apply-requested true authorization ready "
            "apply-authorized true report-only false production enabled blockers 0"
        )
    );
    assert(
        has_apply_requested_member_transfer_audit_line(
            "runtime-index member cleanup mutation-post-apply-verification owner items index (index + zero) "
            "element Box moved Inner member-path item preview ready apply-authorized true "
            "actions-applied true expected-checks 3 expected-checks-ready true verification ready "
            "report-only false production enabled blockers 0 expected-check branch-target items.final-cleanup "
            "expected-check cfg-appended items.member_cleanup.exit "
            "expected-check phi-predecessor items.member_cleanup.exit"
        )
    );
    assert(
        has_apply_requested_member_transfer_audit_line(
            "runtime-index member cleanup mutation-promotion-summary owner items index (index + zero) "
            "element Box moved Inner member-path item operations 3 operations-ready ready validation ready "
            "conflict-free true authorization ready preview ready actions 3 post-apply-verification ready "
            "expected-checks 3 promotion ready report-only false production enabled blockers 0"
        )
    );
    assert(
        has_apply_requested_member_transfer_audit_line(
            "runtime-index member cleanup mutation-production-readiness owner items index (index + zero) "
            "element Box moved Inner member-path item promotion ready post-apply-verification ready "
            "authorization ready ir-mutation requested production-gate enabled readiness ready "
            "report-only false production enabled blockers 0"
        )
    );
    assert(
        has_apply_requested_member_transfer_audit_line(
            "runtime-index member cleanup mutation readiness verdict owner items index (index + zero) "
            "element Box moved Inner member-path item readiness ready guarded-rewrite ready "
            "blockers 0 diagnostics 0 report-only false production enabled"
        )
    );
    assert(
        has_apply_requested_member_transfer_audit_line(
            "runtime-index member cleanup mutation rewrite authorization owner items index (index + zero) "
            "element Box moved Inner member-path item verdict ready guarded-rewrite ready "
            "authorization ready rewrite-requested true rewrite-authorized true report-only false "
            "production enabled blockers 0"
        )
    );
    assert(
        has_apply_requested_member_transfer_audit_line(
            "runtime-index member cleanup mutation rewrite execution-plan owner items index (index + zero) "
            "element Box moved Inner member-path item authorization ready rewrite-authorized true "
            "execution-plan ready execution-requested true execution enabled report-only false "
            "production enabled blockers 0"
        )
    );
    assert(
        has_apply_requested_member_transfer_audit_line(
            "runtime-index member cleanup mutation rewrite execution verdict owner items index (index + zero) "
            "element Box moved Inner member-path item execution-plan ready execution enabled blockers 0 "
            "diagnostics 0 report-only false production enabled"
        )
    );
    assert(
        has_apply_requested_member_transfer_audit_line(
            "runtime-index member cleanup mutation rewrite promotion-status owner items index (index + zero) "
            "element Box moved Inner member-path item authorization ready execution-plan ready "
            "execution-verdict ready promotion ready blockers 0 diagnostics 0 report-only false "
            "production enabled"
        )
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_function_ir_module_rewrite_mutation_state.mutation_requested
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_function_ir_module_rewrite_mutation_state.candidate_verified
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_function_ir_module_rewrite_mutation_state.replacement_targets_unique
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_function_ir_module_rewrite_mutation_state.mutation_applied
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_function_ir_module_rewrite_mutation_state.branch_replacements_applied
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_function_ir_module_rewrite_mutation_state.cleanup_cfg_appended
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_function_ir_module_rewrite_mutation_state.phi_predecessors_retargeted
    );
    assert(
        runtime_indexed_member_transfer_apply_request
            .runtime_indexed_member_cleanup_function_ir_module_rewrite_mutation_state.llvm_verifier_passed
    );
    assert(
        runtime_indexed_member_transfer_apply_request.ir_text.find(
            "  br label %items.member_cleanup.entry\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_member_transfer_apply_request.ir_text.find(
            "items.member_cleanup.entry:\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_member_transfer_apply_request.ir_text.find(
            "items.member_cleanup.entry:\n"
            "  ; runtime-index member cleanup rewrite entry\n"
            "  br label %items.member_cleanup.drop_siblings\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_member_transfer_apply_request.ir_text.find(
            "items.member_cleanup.skip_moved:\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_member_transfer_apply_request.ir_text.find(
            "items.member_cleanup.drop_siblings:\n"
            "  %items.member_cleanup.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_member_transfer_apply_request.ir_text.find(
            "define void @__orison_member_cleanup.Box.except.item(ptr %value) {\n"
            "entry:\n"
            "  ; no sibling cleanup targets for %record.Box except item\n"
            "  ret void\n"
            "}\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_member_transfer_apply_request.ir_text.find(
            "declare void @__orison_member_cleanup.Box.except.item(ptr)"
        ) == std::string::npos
    );
    assert(
        runtime_indexed_member_transfer_apply_request.ir_text.find(
            "items.member_cleanup.drop_siblings_for_moved:\n"
            "  %items.member_cleanup.moved.addr = getelementptr %record.Box, ptr "
            "%items.member_cleanup.cleanup.data, i64 %items.member_cleanup.index\n"
            "  call void @__orison_member_cleanup.Box.except.item(ptr %items.member_cleanup.moved.addr)\n"
            "  br label %items.member_cleanup.continue\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_member_transfer_apply_request.ir_text.find(
            "items.member_cleanup.drop_element:\n"
            "  %items.member_cleanup.element.addr = getelementptr %record.Box, ptr "
            "%items.member_cleanup.cleanup.data, i64 %items.member_cleanup.index\n"
            "  call void @__orison_drop.Box(ptr %items.member_cleanup.element.addr)\n"
            "  store %record.Box zeroinitializer, ptr %items.member_cleanup.element.addr\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_member_transfer_apply_request.ir_text.find(
            "items.member_cleanup.deallocate:\n"
            "  call void @__orison_dynamic_array_deallocate(ptr %items.member_cleanup.cleanup.data, i64 "
        ) != std::string::npos
    );
    assert(
        runtime_indexed_member_transfer_apply_request.ir_text.find(
            "  store { ptr, i64, i64 } zeroinitializer, ptr %items.addr\n"
            "  br label %items.member_cleanup.preserve_moved\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_member_transfer_apply_request.ir_text.find(
            "items.member_cleanup.exit:\n  ret i32 0\n"
        ) != std::string::npos
    );
    auto runtime_indexed_sibling_member_transfer_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "runtime_indexed_dynamic_array_constructor_computed_expression_sibling_member_transfer_rejected.or";
    auto runtime_indexed_sibling_member_transfer_apply_request = pipeline.emit_llvm(
        runtime_indexed_sibling_member_transfer_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
            .runtime_indexed_cleanup_module_ir_insertion_enabled = true,
            .runtime_indexed_cleanup_module_ir_mutation_enabled = true,
            .runtime_indexed_cleanup_function_ir_module_rewrite_enabled = true,
            .runtime_indexed_constructor_move_enabled = true,
            .runtime_indexed_member_cleanup_ir_mutation_enabled = true,
            .runtime_indexed_member_cleanup_production_gate_enabled = true,
            .runtime_indexed_member_cleanup_apply_authorization_enabled = true,
            .runtime_indexed_member_cleanup_rewrite_execution_enabled = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_index_lowering_enabled = true,
            .dynamic_array_production_append_lowering_enabled = true,
        }
    );
    assert(!runtime_indexed_sibling_member_transfer_apply_request.has_errors());
    assert(
        runtime_indexed_sibling_member_transfer_apply_request
            .runtime_indexed_member_cleanup_sibling_fields.size() == 2
    );
    auto const& prefix_member_cleanup_field =
        runtime_indexed_sibling_member_transfer_apply_request
            .runtime_indexed_member_cleanup_sibling_fields[0];
    assert(prefix_member_cleanup_field.owner_name == "items");
    assert(prefix_member_cleanup_field.index_expression_text == "(index + zero)");
    assert(prefix_member_cleanup_field.element_source_type_name == "Box");
    assert(prefix_member_cleanup_field.moved_source_type_name == "Inner");
    assert(prefix_member_cleanup_field.moved_member_path == std::vector<std::string> {"item"});
    assert(prefix_member_cleanup_field.field_name == "prefix");
    assert(prefix_member_cleanup_field.field_source_type_name == "Sibling");
    assert(prefix_member_cleanup_field.field_llvm_type_name == "%record.Sibling");
    assert(prefix_member_cleanup_field.drop_symbol_name == "__orison_drop.Sibling");
    assert(prefix_member_cleanup_field.field_index == 0);
    assert(prefix_member_cleanup_field.drop_definition_available);
    auto const& tail_member_cleanup_field =
        runtime_indexed_sibling_member_transfer_apply_request
            .runtime_indexed_member_cleanup_sibling_fields[1];
    assert(tail_member_cleanup_field.owner_name == "items");
    assert(tail_member_cleanup_field.index_expression_text == "(index + zero)");
    assert(tail_member_cleanup_field.element_source_type_name == "Box");
    assert(tail_member_cleanup_field.moved_source_type_name == "Inner");
    assert(tail_member_cleanup_field.moved_member_path == std::vector<std::string> {"item"});
    assert(tail_member_cleanup_field.field_name == "tail");
    assert(tail_member_cleanup_field.field_source_type_name == "Tail");
    assert(tail_member_cleanup_field.field_llvm_type_name == "%record.Tail");
    assert(tail_member_cleanup_field.drop_symbol_name == "__orison_drop.Tail");
    assert(tail_member_cleanup_field.field_index == 2);
    assert(tail_member_cleanup_field.drop_definition_available);
    assert(
        runtime_indexed_sibling_member_transfer_apply_request.ir_text.find(
            "items.member_cleanup.drop_siblings:\n"
            "  %items.member_cleanup.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_sibling_member_transfer_apply_request.ir_text.find(
            "items.member_cleanup.drop_siblings_for_moved:\n"
            "  %items.member_cleanup.moved.addr = getelementptr %record.Box, ptr "
            "%items.member_cleanup.cleanup.data, i64 %items.member_cleanup.index\n"
            "  call void @__orison_member_cleanup.Box.except.item(ptr %items.member_cleanup.moved.addr)\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_sibling_member_transfer_apply_request.ir_text.find(
            "items.member_cleanup.drop_element:\n"
            "  %items.member_cleanup.element.addr = getelementptr %record.Box, ptr "
            "%items.member_cleanup.cleanup.data, i64 %items.member_cleanup.index\n"
            "  call void @__orison_drop.Box(ptr %items.member_cleanup.element.addr)\n"
            "  store %record.Box zeroinitializer, ptr %items.member_cleanup.element.addr\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_sibling_member_transfer_apply_request.ir_text.find(
            "items.member_cleanup.deallocate:\n"
            "  call void @__orison_dynamic_array_deallocate(ptr %items.member_cleanup.cleanup.data, i64 "
        ) != std::string::npos
    );
    assert(
        runtime_indexed_sibling_member_transfer_apply_request.ir_text.find(
            "  store { ptr, i64, i64 } zeroinitializer, ptr %items.addr\n"
            "  br label %items.member_cleanup.preserve_moved\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_sibling_member_transfer_apply_request.ir_text.find(
            "define void @__orison_member_cleanup.Box.except.item(ptr %value) {\n"
            "entry:\n"
            "  %Box.member_cleanup.prefix.addr = getelementptr %record.Box, ptr %value, i32 0, i32 0\n"
            "  call void @__orison_drop.Sibling(ptr %Box.member_cleanup.prefix.addr)\n"
            "  store %record.Sibling zeroinitializer, ptr %Box.member_cleanup.prefix.addr\n"
            "  %Box.member_cleanup.tail.addr = getelementptr %record.Box, ptr %value, i32 0, i32 2\n"
            "  call void @__orison_drop.Tail(ptr %Box.member_cleanup.tail.addr)\n"
            "  store %record.Tail zeroinitializer, ptr %Box.member_cleanup.tail.addr\n"
            "  ret void\n"
            "}\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_sibling_member_transfer_apply_request.ir_text.find(
            "declare void @__orison_member_cleanup.Box.except.item(ptr)"
        ) == std::string::npos
    );
    assert(
        runtime_indexed_sibling_member_transfer_apply_request.ir_text.find(
            "define void @__orison_drop.Sibling(ptr %value)"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_sibling_member_transfer_apply_request.ir_text.find(
            "define void @__orison_drop.Tail(ptr %value)"
        ) != std::string::npos
    );

    auto runtime_indexed_nested_sibling_member_transfer_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "runtime_indexed_dynamic_array_constructor_computed_expression_nested_member_sibling_transfer.or";
    auto runtime_indexed_nested_sibling_member_transfer_apply_request = pipeline.emit_llvm(
        runtime_indexed_nested_sibling_member_transfer_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
            .runtime_indexed_cleanup_module_ir_insertion_enabled = true,
            .runtime_indexed_cleanup_module_ir_mutation_enabled = true,
            .runtime_indexed_cleanup_function_ir_module_rewrite_enabled = true,
            .runtime_indexed_constructor_move_enabled = true,
            .runtime_indexed_member_cleanup_ir_mutation_enabled = true,
            .runtime_indexed_member_cleanup_production_gate_enabled = true,
            .runtime_indexed_member_cleanup_apply_authorization_enabled = true,
            .runtime_indexed_member_cleanup_rewrite_execution_enabled = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_index_lowering_enabled = true,
            .dynamic_array_production_append_lowering_enabled = true,
        }
    );
    assert(!runtime_indexed_nested_sibling_member_transfer_apply_request.has_errors());
    assert(
        runtime_indexed_nested_sibling_member_transfer_apply_request
            .runtime_indexed_member_cleanup_sibling_fields.size() == 4
    );
    assert(
        runtime_indexed_nested_sibling_member_transfer_apply_request
            .runtime_indexed_member_cleanup_helper_drop_bindings.size() == 1
    );
    auto const& nested_helper_drop_bindings =
        runtime_indexed_nested_sibling_member_transfer_apply_request
            .runtime_indexed_member_cleanup_helper_drop_bindings.front();
    assert(nested_helper_drop_bindings.owner_name == "items");
    assert(nested_helper_drop_bindings.index_expression_text == "(index + zero)");
    assert(nested_helper_drop_bindings.element_source_type_name == "Wrap");
    assert(nested_helper_drop_bindings.moved_source_type_name == "Inner");
    assert(nested_helper_drop_bindings.moved_member_path == (std::vector<std::string> {"box", "item"}));
    assert(nested_helper_drop_bindings.helper_symbol_name == "__orison_member_cleanup.Wrap.except.box.item");
    assert(nested_helper_drop_bindings.sibling_binding_count == 4);
    assert(nested_helper_drop_bindings.all_drop_definitions_available);
    assert(nested_helper_drop_bindings.nested_member_path);
    assert(nested_helper_drop_bindings.helper_definition_ready);
    assert(!nested_helper_drop_bindings.production_enabled);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_helper_drop_bindings_report(
            nested_helper_drop_bindings
        ) ==
        "runtime-index member cleanup helper-drop-bindings owner items index (index + zero) "
        "element Wrap moved Inner member-path box.item helper __orison_member_cleanup.Wrap.except.box.item "
        "sibling-bindings 4 drop-definitions ready nested-path true helper-definition ready production disabled"
    );
    auto assert_nested_member_cleanup_field =
        [&](std::size_t index,
            std::string_view field_name,
            std::string_view field_source_type_name,
            std::vector<std::string> const& field_path,
            std::vector<std::size_t> const& field_indices,
            std::vector<std::string> const& container_llvm_type_names,
            std::size_t field_index) {
            auto const& field =
                runtime_indexed_nested_sibling_member_transfer_apply_request
                    .runtime_indexed_member_cleanup_sibling_fields[index];
            assert(field.owner_name == "items");
            assert(field.index_expression_text == "(index + zero)");
            assert(field.element_source_type_name == "Wrap");
            assert(field.moved_source_type_name == "Inner");
            assert(field.moved_member_path == (std::vector<std::string> {"box", "item"}));
            assert(field.field_name == field_name);
            assert(field.field_source_type_name == field_source_type_name);
            assert(field.field_llvm_type_name == "%record." + std::string {field_source_type_name});
            assert(field.drop_symbol_name == "__orison_drop." + std::string {field_source_type_name});
            assert(field.field_path == field_path);
            assert(field.field_indices == field_indices);
            assert(field.container_llvm_type_names == container_llvm_type_names);
            assert(field.field_index == field_index);
            assert(field.drop_definition_available);
        };
    assert_nested_member_cleanup_field(
        0,
        "head",
        "Head",
        std::vector<std::string> {"head"},
        std::vector<std::size_t> {0},
        std::vector<std::string> {"%record.Wrap"},
        0
    );
    assert_nested_member_cleanup_field(
        1,
        "tail",
        "Tail",
        std::vector<std::string> {"tail"},
        std::vector<std::size_t> {2},
        std::vector<std::string> {"%record.Wrap"},
        2
    );
    assert_nested_member_cleanup_field(
        2,
        "left",
        "Left",
        std::vector<std::string> {"box", "left"},
        std::vector<std::size_t> {1, 0},
        std::vector<std::string> {"%record.Wrap", "%record.Box"},
        0
    );
    assert_nested_member_cleanup_field(
        3,
        "right",
        "Right",
        std::vector<std::string> {"box", "right"},
        std::vector<std::size_t> {1, 2},
        std::vector<std::string> {"%record.Wrap", "%record.Box"},
        2
    );
    assert(
        runtime_indexed_nested_sibling_member_transfer_apply_request.ir_text.find(
            "define void @__orison_member_cleanup.Wrap.except.box.item(ptr %value) {\n"
            "entry:\n"
            "  %Wrap.member_cleanup.head.addr = getelementptr %record.Wrap, ptr %value, i32 0, i32 0\n"
            "  call void @__orison_drop.Head(ptr %Wrap.member_cleanup.head.addr)\n"
            "  store %record.Head zeroinitializer, ptr %Wrap.member_cleanup.head.addr\n"
            "  %Wrap.member_cleanup.tail.addr = getelementptr %record.Wrap, ptr %value, i32 0, i32 2\n"
            "  call void @__orison_drop.Tail(ptr %Wrap.member_cleanup.tail.addr)\n"
            "  store %record.Tail zeroinitializer, ptr %Wrap.member_cleanup.tail.addr\n"
            "  %Wrap.member_cleanup.box.addr = getelementptr %record.Wrap, ptr %value, i32 0, i32 1\n"
            "  %Wrap.member_cleanup.box.left.addr = getelementptr %record.Box, ptr %Wrap.member_cleanup.box.addr, i32 0, i32 0\n"
            "  call void @__orison_drop.Left(ptr %Wrap.member_cleanup.box.left.addr)\n"
            "  store %record.Left zeroinitializer, ptr %Wrap.member_cleanup.box.left.addr\n"
            "  %Wrap.member_cleanup.box.right.addr = getelementptr %record.Box, ptr %Wrap.member_cleanup.box.addr, i32 0, i32 2\n"
            "  call void @__orison_drop.Right(ptr %Wrap.member_cleanup.box.right.addr)\n"
            "  store %record.Right zeroinitializer, ptr %Wrap.member_cleanup.box.right.addr\n"
            "  ret void\n"
            "}\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_nested_sibling_member_transfer_apply_request.ir_text.find(
            "declare void @__orison_member_cleanup.Wrap.except.box.item(ptr)"
        ) == std::string::npos
    );
    assert(
        runtime_indexed_nested_sibling_member_transfer_apply_request.ir_text.find(
            "items.member_cleanup.drop_siblings:\n"
            "  %items.member_cleanup.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_nested_sibling_member_transfer_apply_request.ir_text.find(
            "items.member_cleanup.drop_siblings_for_moved:\n"
            "  %items.member_cleanup.moved.addr = getelementptr %record.Wrap, ptr "
            "%items.member_cleanup.cleanup.data, i64 %items.member_cleanup.index\n"
            "  call void @__orison_member_cleanup.Wrap.except.box.item(ptr %items.member_cleanup.moved.addr)\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_nested_sibling_member_transfer_apply_request.ir_text.find(
            "items.member_cleanup.drop_element:\n"
            "  %items.member_cleanup.element.addr = getelementptr %record.Wrap, ptr "
            "%items.member_cleanup.cleanup.data, i64 %items.member_cleanup.index\n"
            "  call void @__orison_drop.Wrap(ptr %items.member_cleanup.element.addr)\n"
            "  store %record.Wrap zeroinitializer, ptr %items.member_cleanup.element.addr\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_nested_sibling_member_transfer_apply_request.ir_text.find(
            "items.member_cleanup.deallocate:\n"
            "  call void @__orison_dynamic_array_deallocate(ptr %items.member_cleanup.cleanup.data, i64 "
        ) != std::string::npos
    );
    assert(
        runtime_indexed_nested_sibling_member_transfer_apply_request.ir_text.find(
            "  store { ptr, i64, i64 } zeroinitializer, ptr %items.addr\n"
            "  br label %items.member_cleanup.preserve_moved\n"
        ) != std::string::npos
    );
    auto runtime_indexed_nested_missing_sibling_drop_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "runtime_indexed_dynamic_array_constructor_computed_expression_nested_member_missing_sibling_drop_rejected.or";
    auto runtime_indexed_nested_missing_sibling_drop_result = pipeline.emit_llvm(
        runtime_indexed_nested_missing_sibling_drop_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
            .runtime_indexed_cleanup_module_ir_insertion_enabled = true,
            .runtime_indexed_cleanup_module_ir_mutation_enabled = true,
            .runtime_indexed_cleanup_function_ir_module_rewrite_enabled = true,
            .runtime_indexed_constructor_move_enabled = true,
            .runtime_indexed_member_cleanup_ir_mutation_enabled = true,
            .runtime_indexed_member_cleanup_production_gate_enabled = true,
            .runtime_indexed_member_cleanup_apply_authorization_enabled = true,
            .runtime_indexed_member_cleanup_rewrite_execution_enabled = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_index_lowering_enabled = true,
            .dynamic_array_production_append_lowering_enabled = true,
        }
    );
    assert(runtime_indexed_nested_missing_sibling_drop_result.has_errors());
    assert(
        runtime_indexed_nested_missing_sibling_drop_result.error_text.find(
            "member cleanup helper Drop bindings are missing"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_nested_missing_sibling_drop_result
            .runtime_indexed_member_cleanup_sibling_fields.size() == 4
    );
    assert(
        runtime_indexed_nested_missing_sibling_drop_result
            .runtime_indexed_member_cleanup_helper_drop_bindings.size() == 1
    );
    auto const& missing_sibling_drop_bindings =
        runtime_indexed_nested_missing_sibling_drop_result
            .runtime_indexed_member_cleanup_helper_drop_bindings.front();
    assert(!missing_sibling_drop_bindings.all_drop_definitions_available);
    assert(!missing_sibling_drop_bindings.helper_definition_ready);
    auto const missing_tail_field = std::ranges::find_if(
        runtime_indexed_nested_missing_sibling_drop_result.runtime_indexed_member_cleanup_sibling_fields,
        [](auto const& field) {
            return field.field_path == (std::vector<std::string> {"tail"});
        }
    );
    assert(
        missing_tail_field !=
        runtime_indexed_nested_missing_sibling_drop_result.runtime_indexed_member_cleanup_sibling_fields.end()
    );
    assert(missing_tail_field->field_source_type_name == "Tail");
    assert(missing_tail_field->drop_symbol_name == "__orison_drop.Tail");
    assert(!missing_tail_field->drop_definition_available);
    auto runtime_indexed_nested_sibling_member_transfer_object =
        orison::lowering::LlvmObjectEmitter {}.emit(
            runtime_indexed_nested_sibling_member_transfer_apply_request.ir_text
        );
    assert(!runtime_indexed_nested_sibling_member_transfer_object.has_errors());
    auto runtime_indexed_nested_sibling_member_transfer_executable =
        smoke_temp_root / "runtime_indexed_nested_sibling_member_transfer";
    auto runtime_indexed_nested_sibling_member_transfer_link =
        orison::link::HostLinker {}.link(
            runtime_indexed_nested_sibling_member_transfer_object.object_bytes,
            runtime_indexed_nested_sibling_member_transfer_executable
        );
    assert(!runtime_indexed_nested_sibling_member_transfer_link.has_errors());
    auto runtime_indexed_nested_sibling_member_transfer_status =
        std::system(runtime_indexed_nested_sibling_member_transfer_executable.string().c_str());
    assert(WIFEXITED(runtime_indexed_nested_sibling_member_transfer_status));
    assert(WEXITSTATUS(runtime_indexed_nested_sibling_member_transfer_status) == 0);

    auto has_planned_drop_declaration = [](orison::pipeline::CompilePipelineResult const& result,
                                           std::string_view symbol_name) {
        return std::any_of(
            result.planned_drop_declaration_state.declarations.begin(),
            result.planned_drop_declaration_state.declarations.end(),
            [&](orison::lowering::PlannedDropDeclaration const& declaration) {
                return declaration.symbol_name == symbol_name;
            }
        );
    };

    auto runtime_indexed_cleanup_narrow_drop_surface = pipeline.emit_llvm(
        runtime_indexed_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
            .runtime_indexed_constructor_move_enabled = true,
        }
    );
    assert(!runtime_indexed_cleanup_narrow_drop_surface.has_errors());
    assert(
        !has_planned_drop_declaration(
            runtime_indexed_cleanup_narrow_drop_surface,
            "__orison_drop.Inner"
        )
    );

    auto runtime_indexed_cleanup_module_drop_surface = pipeline.emit_llvm(
        runtime_indexed_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
            .runtime_indexed_cleanup_module_ir_insertion_enabled = true,
            .runtime_indexed_constructor_move_enabled = true,
        }
    );
    assert(!runtime_indexed_cleanup_module_drop_surface.has_errors());
    assert(
        has_planned_drop_declaration(
            runtime_indexed_cleanup_module_drop_surface,
            "__orison_drop.Inner"
        )
    );

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
            .gated_ir_slice_line_count == 20
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
    assert(runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_ir_render_state.rendered_ir_line_count == 20);
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
            .rendered_ir_line_count == 20
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
            .cleanup_slice_line_count == 20
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state
            .any_continuation_block_generated
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_function_cfg_rewrite_plan_state
            .candidate_cfg_line_count == 22
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
            .candidate_cfg_line_count == 22
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
            .inserted_cfg_line_count == 21
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
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_blocker_kind ==
        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::InsertionGate
    );
    assert(
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_diagnostic(
            runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_module_ir_production_readiness_state
        ) == "runtime-index cleanup blocked: module insertion gate disabled"
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
            .inserted_ir_line_count == 20
    );
    assert(
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .projected_module_line_count ==
        runtime_indexed_cleanup_insertion_gate_on.runtime_indexed_cleanup_module_ir_insertion_preview_state
            .original_module_line_count + 20
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
            .inserted_ir_line_count == 20
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
            .candidate_cfg_line_count == 22
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
            .composition_failure_count == 0
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .first_composition_failure ==
        orison::pipeline::RuntimeIndexedCleanupIrCompositionFailure::none
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .all_candidates_separate_from_module_ir
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .all_splice_ranges_available
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .same_function_splice_ranges_ordered
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .same_function_splice_ranges_non_overlapping
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .any_function_ir_changed
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .inserted_cfg_line_count == 22
    );
    auto const& runtime_indexed_cleanup_function_candidate =
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .candidates.front();
    assert(runtime_indexed_cleanup_function_candidate.candidate_available);
    assert(
        runtime_indexed_cleanup_function_candidate.composition_failure ==
        orison::pipeline::RuntimeIndexedCleanupIrCompositionFailure::none
    );
    assert(runtime_indexed_cleanup_function_candidate.separate_from_module_ir);
    assert(runtime_indexed_cleanup_function_candidate.function_ir_changed);
    assert(runtime_indexed_cleanup_function_candidate.predecessor_terminator_replaced);
    assert(runtime_indexed_cleanup_function_candidate.splice_range_available);
    assert(
        runtime_indexed_cleanup_function_candidate.splice_range.start_offset <
        runtime_indexed_cleanup_function_candidate.splice_range.end_offset
    );
    assert(
        runtime_indexed_cleanup_function_candidate.original_function_ir_text.substr(
            runtime_indexed_cleanup_function_candidate.splice_range.start_offset,
            runtime_indexed_cleanup_function_candidate.splice_range.end_offset -
                runtime_indexed_cleanup_function_candidate.splice_range.start_offset
        ) == "  " + runtime_indexed_cleanup_function_candidate.replaced_terminator_text + "\n"
    );
    assert(runtime_indexed_cleanup_function_candidate.original_function_line_count > 0);
    assert(
        runtime_indexed_cleanup_function_candidate.candidate_function_line_count ==
        runtime_indexed_cleanup_function_candidate.original_function_line_count + 22
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
            .all_splice_ranges_available
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .same_function_splice_ranges_ordered
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .same_function_splice_ranges_non_overlapping
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
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .composition_failure_count == 0
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .first_composition_failure ==
        orison::pipeline::RuntimeIndexedCleanupIrCompositionFailure::none
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
            .composition_failure_count == 0
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
            .first_composition_failure ==
        orison::pipeline::RuntimeIndexedCleanupIrCompositionFailure::none
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
        runtime_indexed_cleanup_function_module_candidate.candidate_module_ir_text ==
        runtime_indexed_cleanup_function_module_rewrite_on.ir_text
    );
    assert(
        runtime_indexed_cleanup_function_module_candidate.candidate_module_line_count ==
        runtime_indexed_cleanup_function_module_candidate.original_module_line_count + 22
    );
    assert(
        occurrence_count(
            runtime_indexed_cleanup_function_module_candidate.candidate_module_ir_text,
            runtime_indexed_cleanup_function_rewrite_candidate.candidate_function_ir_text
        ) == 1
    );
    assert(
        occurrence_count(
            runtime_indexed_cleanup_function_module_candidate.candidate_module_ir_text,
            runtime_indexed_cleanup_function_module_rewrite_on.runtime_indexed_cleanup_module_ir_artifact_state
                .rendered_ir_lines.front()
        ) == 1
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .any_llvm_verifier_ran
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .all_llvm_verifier_passed
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
            .llvm_verified_count == 1
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .llvm_verifier_diagnostic_count == 0
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .composition_failure_count == 0
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .first_composition_failure ==
        orison::pipeline::RuntimeIndexedCleanupIrCompositionFailure::none
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
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .verifications.front().llvm_verifier_ran
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .verifications.front().llvm_verifier_passed
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .verifications.front().llvm_verifier_diagnostic_count == 0
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .verifications.front().llvm_verifier_diagnostic_text.empty()
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .mutation_requested
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .candidate_verified
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .replacement_targets_unique
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .mutation_applied
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .module_matches_candidate
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .llvm_verifier_passed
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .llvm_verifier_diagnostic_count == 0
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .composition_failure ==
        orison::pipeline::RuntimeIndexedCleanupIrCompositionFailure::none
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .function_integration_ready
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .production_ready
    );
    assert(
        runtime_indexed_cleanup_function_module_rewrite_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_blocker_kind ==
        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::None
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
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_blocker_kind ==
        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::FunctionIntegration
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .blockers.size() == 1
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .blockers.front().kind ==
        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::FunctionIntegration
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_blocker_stage_name == "function integration"
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_function_symbol_name == runtime_indexed_cleanup_function_candidate.function_symbol_name
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_source_available
    );
    assert(
        runtime_indexed_cleanup_constructor_move_on.runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_source_line == runtime_indexed_cleanup_function_candidate.source_line
    );
    assert(
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_diagnostic(
            runtime_indexed_cleanup_constructor_move_on
                .runtime_indexed_cleanup_module_ir_production_readiness_state
        ) == "runtime-index cleanup blocked: function integration blocked"
    );
    assert(
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_report(
            runtime_indexed_cleanup_constructor_move_on
                .runtime_indexed_cleanup_module_ir_production_readiness_state
        ).find(
            "ir-shape ready production blocked blocker-count 1 blocker-kind function-integration function " +
            runtime_indexed_cleanup_function_candidate.function_symbol_name +
            " source-line " +
            std::to_string(runtime_indexed_cleanup_function_candidate.source_line) +
            " diagnostic runtime-index cleanup blocked: function integration blocked"
        ) != std::string::npos
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
            .ir_plan.live_check_block_name == "holder.items.runtime_cleanup.check_live"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .ir_plan.element_llvm_type_name == "%record.Inner"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .ir_plan.owner_llvm_type_name == "[2 x %record.Inner]"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .ir_plan.owner_address_name == "%holder.items.runtime_cleanup.owner.addr"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .ir_plan.static_length_value == "2"
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
        "  %holder.items.runtime_cleanup.owner.addr = getelementptr %record.Holder, ptr "
        "%holder.addr, i32 0, i32 0\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[2] ==
        "holder.items.runtime_cleanup.condition:\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[4] ==
        "  %holder.items.runtime_cleanup.more = icmp ult i64 %holder.items.runtime_cleanup.index, 2\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[5] ==
        "  br i1 %holder.items.runtime_cleanup.more, label %holder.items.runtime_cleanup.check_live, "
        "label %holder.items.runtime_cleanup.exit\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[6] ==
        "holder.items.runtime_cleanup.check_live:\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[7] ==
        "  %holder.items.runtime_cleanup.skip_moved = icmp eq i64 "
        "%holder.items.runtime_cleanup.index, %index\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[8] ==
        "  br i1 %holder.items.runtime_cleanup.skip_moved, label "
        "%holder.items.runtime_cleanup.skip, label %holder.items.runtime_cleanup.drop\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[11] ==
        "holder.items.runtime_cleanup.drop:\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[12] ==
        "  %holder.items.runtime_cleanup.element.addr = getelementptr [2 x %record.Inner], ptr "
        "%holder.items.runtime_cleanup.owner.addr, i64 0, i64 %holder.items.runtime_cleanup.index\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[13] ==
        "  call void @__orison_drop.Inner(ptr %holder.items.runtime_cleanup.element.addr)\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[14] ==
        "  store %record.Inner zeroinitializer, ptr %holder.items.runtime_cleanup.element.addr\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[16] ==
        "holder.items.runtime_cleanup.continue:\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[17] ==
        "  %holder.items.runtime_cleanup.next_index = add i64 "
        "%holder.items.runtime_cleanup.index, 1\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[18] ==
        "  br label %holder.items.runtime_cleanup.condition\n"
    );
    assert(
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front()
            .gated_ir_slice_lines[19] ==
        "holder.items.runtime_cleanup.exit:\n"
    );
    auto runtime_indexed_dynamic_array_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "runtime_indexed_dynamic_array_constructor_computed_index_member_path_move_rejected.or";
    auto runtime_indexed_dynamic_array_cleanup = pipeline.emit_llvm(
        runtime_indexed_dynamic_array_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
            .runtime_indexed_cleanup_module_ir_insertion_enabled = true,
            .runtime_indexed_cleanup_module_ir_mutation_enabled = true,
            .runtime_indexed_cleanup_function_ir_module_rewrite_enabled = true,
            .runtime_indexed_constructor_move_enabled = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_index_lowering_enabled = true,
            .dynamic_array_production_append_lowering_enabled = true,
        }
    );
    assert(!runtime_indexed_dynamic_array_cleanup.has_errors());
    assert(
        runtime_indexed_dynamic_array_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.size() == 1
    );
    auto runtime_indexed_fixed_array_same_shape_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "runtime_indexed_fixed_array_constructor_computed_index_move_rejected.or";
    auto runtime_indexed_fixed_array_same_shape_cleanup = pipeline.emit_llvm(
        runtime_indexed_fixed_array_same_shape_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
        }
    );
    assert(runtime_indexed_fixed_array_same_shape_cleanup.has_errors());
    assert(
        runtime_indexed_fixed_array_same_shape_cleanup
            .runtime_indexed_cleanup_emission_plan_state.plans.size() == 1
    );
    auto const& runtime_indexed_dynamic_array_plan =
        runtime_indexed_dynamic_array_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.front();
    auto const& runtime_indexed_fixed_array_plan =
        runtime_indexed_cleanup_gate_on.runtime_indexed_cleanup_emission_plan_state.plans.front();
    auto const& runtime_indexed_fixed_array_same_shape_plan =
        runtime_indexed_fixed_array_same_shape_cleanup
            .runtime_indexed_cleanup_emission_plan_state.plans.front();
    assert(runtime_indexed_dynamic_array_plan.owner_name == "items");
    assert(runtime_indexed_dynamic_array_plan.element_source_type_name == "Inner");
    assert(runtime_indexed_dynamic_array_plan.element_llvm_type_name == "%record.Inner");
    assert(runtime_indexed_dynamic_array_plan.owner_llvm_type_name == "{ ptr, i64, i64 }");
    assert(runtime_indexed_dynamic_array_plan.owner_address_name == "%items.addr");
    assert(runtime_indexed_dynamic_array_plan.element_size_value == "4");
    assert(runtime_indexed_dynamic_array_plan.gated_ir_slice_line_count == 23);
    assert(runtime_indexed_dynamic_array_plan.ir_plan.complete);
    assert(runtime_indexed_dynamic_array_plan.ir_plan.descriptor_owner_ready);
    assert(runtime_indexed_dynamic_array_plan.ir_plan.owner_deallocation_required);
    auto different_layout_parity = orison::pipeline::runtime_indexed_cleanup_plan_parity_summary(
        runtime_indexed_fixed_array_plan,
        runtime_indexed_dynamic_array_plan
    );
    assert(!different_layout_parity.shared_metadata_matches);
    assert(!different_layout_parity.element_size_matches);
    assert(different_layout_parity.index_expression_matches);
    assert(different_layout_parity.element_source_type_matches);
    assert(different_layout_parity.element_llvm_type_matches);
    assert(different_layout_parity.drop_callee_matches);
    assert(different_layout_parity.operation_sequence_matches);
    assert(runtime_indexed_fixed_array_plan.owner_llvm_type_name == "[2 x %record.Inner]");
    assert(runtime_indexed_fixed_array_plan.static_length_value == "2");
    assert(!runtime_indexed_fixed_array_plan.element_size_value.empty());
    assert(!runtime_indexed_fixed_array_plan.ir_plan.element_size_value.empty());
    assert(
        runtime_indexed_fixed_array_plan.element_size_value !=
        runtime_indexed_dynamic_array_plan.element_size_value
    );
    assert(
        runtime_indexed_fixed_array_plan.ir_plan.element_size_value !=
        runtime_indexed_dynamic_array_plan.ir_plan.element_size_value
    );
    assert(runtime_indexed_fixed_array_same_shape_plan.owner_name == "items");
    auto same_layout_parity = orison::pipeline::runtime_indexed_cleanup_plan_parity_summary(
        runtime_indexed_fixed_array_same_shape_plan,
        runtime_indexed_dynamic_array_plan
    );
    assert(same_layout_parity.shared_metadata_matches);
    assert(same_layout_parity.element_size_matches);
    assert(same_layout_parity.storage_metadata_differs);
    assert(same_layout_parity.owner_llvm_type_differs);
    assert(same_layout_parity.static_length_differs);
    assert(same_layout_parity.descriptor_owner_readiness_differs);
    assert(
        orison::pipeline::runtime_indexed_cleanup_plan_parity_summary_report(same_layout_parity) ==
        "runtime-index cleanup plan parity left-owner items right-owner items "
        "shared-metadata matched index matched element-source matched element-llvm matched "
        "element-size matched drop-callee matched operations matched storage-metadata distinct "
        "owner-llvm distinct static-length distinct descriptor-owner distinct"
    );
    auto same_layout_fixed_shape = orison::pipeline::runtime_indexed_cleanup_ir_shape_summary(
        runtime_indexed_fixed_array_same_shape_plan
    );
    auto same_layout_dynamic_shape = orison::pipeline::runtime_indexed_cleanup_ir_shape_summary(
        runtime_indexed_dynamic_array_plan
    );
    assert(same_layout_fixed_shape.common_loop_shape_ready);
    assert(same_layout_fixed_shape.inline_storage_shape_ready);
    assert(!same_layout_fixed_shape.descriptor_storage_shape_ready);
    assert(same_layout_dynamic_shape.common_loop_shape_ready);
    assert(same_layout_dynamic_shape.descriptor_storage_shape_ready);
    assert(!same_layout_dynamic_shape.inline_storage_shape_ready);
    auto same_layout_ir_shape_parity =
        orison::pipeline::runtime_indexed_cleanup_ir_shape_parity_summary(
            runtime_indexed_fixed_array_same_shape_plan,
            runtime_indexed_dynamic_array_plan
        );
    assert(same_layout_ir_shape_parity.common_loop_shape_matches);
    assert(same_layout_ir_shape_parity.drop_call_shape_matches);
    assert(same_layout_ir_shape_parity.storage_ir_shape_differs);
    assert(same_layout_ir_shape_parity.cleanup_tail_differs);
    assert(same_layout_ir_shape_parity.line_count_differs);
    assert(
        orison::pipeline::runtime_indexed_cleanup_ir_shape_parity_summary_report(
            same_layout_ir_shape_parity
        ) ==
        "runtime-index cleanup ir-shape parity left-owner items right-owner items "
        "common-loop matched drop-call matched storage-ir distinct cleanup-tail distinct "
        "line-count distinct"
    );
    auto same_shape_ir_shape_report_lines =
        orison::pipeline::runtime_indexed_constructor_move_ir_shape_report_lines(
            runtime_indexed_fixed_array_same_shape_cleanup
                .runtime_indexed_cleanup_emission_plan_state
        );
    assert(same_shape_ir_shape_report_lines.size() == 1);
    assert(
        same_shape_ir_shape_report_lines.front() ==
        "runtime-index cleanup constructor-move ir-shape owner items lines 19 "
        "common-loop ready condition-blocks 1 live-check-blocks 1 skip-blocks 1 "
        "drop-blocks 1 continue-blocks 1 exit-blocks 1 drop-call ready "
        "descriptor-storage blocked inline-storage ready descriptor-load absent "
        "descriptor-gep absent inline-gep present zero-store present deallocate absent"
    );
    assert_runtime_indexed_constructor_move_shape_faults(
        pipeline,
        runtime_indexed_dynamic_array_cleanup_path,
        {
            {
                .fault = orison::pipeline::RuntimeIndexedCleanupIrShapeFaultInjection::
                    OmitDescriptorDeallocateTail,
                .expected_shape_detail =
                    "runtime-index cleanup module-ir shape is blocked owner items "
                    "common-loop ready drop-call ready descriptor-storage blocked inline-storage blocked "
                    "descriptor-load present descriptor-gep present inline-gep absent zero-store absent "
                    "deallocate absent",
                .expected_ir_shape =
                    "runtime-index cleanup constructor-move ir-shape owner items lines 22 "
                    "common-loop ready condition-blocks 1 live-check-blocks 1 skip-blocks 1 "
                    "drop-blocks 1 continue-blocks 1 exit-blocks 1 drop-call ready "
                    "descriptor-storage blocked inline-storage blocked descriptor-load present descriptor-gep present "
                    "inline-gep absent zero-store absent deallocate absent",
            },
            {
                .fault = orison::pipeline::RuntimeIndexedCleanupIrShapeFaultInjection::OmitDropCall,
                .expected_shape_detail =
                    "runtime-index cleanup module-ir shape is blocked owner items "
                    "common-loop blocked drop-call blocked descriptor-storage ready inline-storage blocked "
                    "descriptor-load present descriptor-gep present inline-gep absent zero-store absent "
                    "deallocate present",
                .expected_ir_shape =
                    "runtime-index cleanup constructor-move ir-shape owner items lines 22 "
                    "common-loop blocked condition-blocks 1 live-check-blocks 1 skip-blocks 1 "
                    "drop-blocks 1 continue-blocks 1 exit-blocks 1 drop-call blocked "
                    "descriptor-storage ready inline-storage blocked descriptor-load present descriptor-gep present "
                    "inline-gep absent zero-store absent deallocate present",
            },
            {
                .fault = orison::pipeline::RuntimeIndexedCleanupIrShapeFaultInjection::OmitConditionBlock,
                .expected_shape_detail =
                    "runtime-index cleanup module-ir shape is blocked owner items "
                    "common-loop blocked drop-call ready descriptor-storage ready inline-storage blocked "
                    "descriptor-load present descriptor-gep present inline-gep absent zero-store absent "
                    "deallocate present",
                .expected_ir_shape =
                    "runtime-index cleanup constructor-move ir-shape owner items lines 22 "
                    "common-loop blocked condition-blocks 0 live-check-blocks 1 skip-blocks 1 "
                    "drop-blocks 1 continue-blocks 1 exit-blocks 1 drop-call ready "
                    "descriptor-storage ready inline-storage blocked descriptor-load present descriptor-gep present "
                    "inline-gep absent zero-store absent deallocate present",
            },
        }
    );
    assert_runtime_indexed_constructor_move_shape_faults(
        pipeline,
        runtime_indexed_fixed_array_same_shape_cleanup_path,
        {
            {
                .fault = orison::pipeline::RuntimeIndexedCleanupIrShapeFaultInjection::OmitInlineZeroStore,
                .expected_shape_detail =
                    "runtime-index cleanup module-ir shape is blocked owner items "
                    "common-loop ready drop-call ready descriptor-storage blocked inline-storage blocked "
                    "descriptor-load absent descriptor-gep absent inline-gep present zero-store absent "
                    "deallocate absent",
                .expected_ir_shape =
                    "runtime-index cleanup constructor-move ir-shape owner items lines 18 "
                    "common-loop ready condition-blocks 1 live-check-blocks 1 skip-blocks 1 "
                    "drop-blocks 1 continue-blocks 1 exit-blocks 1 drop-call ready "
                    "descriptor-storage blocked inline-storage blocked descriptor-load absent descriptor-gep absent "
                    "inline-gep present zero-store absent deallocate absent",
            },
        }
    );
    assert(
        orison::pipeline::runtime_indexed_constructor_move_plan_report(
            runtime_indexed_fixed_array_same_shape_plan
        ) ==
        "runtime-index cleanup constructor-move plan owner items index index element Inner "
        "element-llvm %record.Inner owner-llvm [2 x %record.Inner] static-length 2 "
        "element-size 4 drop-callee __orison_drop.Inner operation-count 5 "
        "descriptor-owner blocked static-length-ready true production enabled"
    );
    auto same_shape_plan_report_lines =
        orison::pipeline::runtime_indexed_constructor_move_plan_report_lines(
            runtime_indexed_fixed_array_same_shape_cleanup
                .runtime_indexed_cleanup_emission_plan_state
        );
    assert(same_shape_plan_report_lines.size() == 1);
    assert(
        same_shape_plan_report_lines.front() ==
        orison::pipeline::runtime_indexed_constructor_move_plan_report(
            runtime_indexed_fixed_array_same_shape_plan
        )
    );
    assert(runtime_indexed_fixed_array_same_shape_plan.owner_llvm_type_name == "[2 x %record.Inner]");
    assert(runtime_indexed_fixed_array_same_shape_plan.static_length_value == "2");
    assert(runtime_indexed_fixed_array_same_shape_plan.ir_plan.static_length_ready);
    assert(!runtime_indexed_fixed_array_same_shape_plan.ir_plan.descriptor_owner_ready);
    assert(!runtime_indexed_fixed_array_plan.ir_plan.descriptor_owner_ready);
    assert(runtime_indexed_fixed_array_plan.ir_plan.static_length_ready);
    assert(runtime_indexed_dynamic_array_plan.owner_llvm_type_name == "{ ptr, i64, i64 }");
    assert(runtime_indexed_dynamic_array_plan.static_length_value.empty());
    assert(runtime_indexed_dynamic_array_plan.ir_plan.descriptor_owner_ready);
    assert(!runtime_indexed_dynamic_array_plan.ir_plan.static_length_ready);
    assert(
        runtime_indexed_dynamic_array_plan.gated_ir_slice_lines[0] ==
        "  %items.runtime_cleanup.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
    );
    assert(
        runtime_indexed_dynamic_array_plan.gated_ir_slice_lines[1] ==
        "  %items.runtime_cleanup.data = extractvalue { ptr, i64, i64 } "
        "%items.runtime_cleanup.descriptor, 0\n"
    );
    assert(
        runtime_indexed_dynamic_array_plan.gated_ir_slice_lines[2] ==
        "  %items.runtime_cleanup.length = extractvalue { ptr, i64, i64 } "
        "%items.runtime_cleanup.descriptor, 1\n"
    );
    assert(
        runtime_indexed_dynamic_array_plan.gated_ir_slice_lines[3] ==
        "  %items.runtime_cleanup.capacity = extractvalue { ptr, i64, i64 } "
        "%items.runtime_cleanup.descriptor, 2\n"
    );
    assert(
        runtime_indexed_dynamic_array_plan.gated_ir_slice_lines[15] ==
        "  %items.runtime_cleanup.element.addr = getelementptr %record.Inner, ptr "
        "%items.runtime_cleanup.data, i64 %items.runtime_cleanup.index\n"
    );
    assert(
        runtime_indexed_dynamic_array_plan.gated_ir_slice_lines[22] ==
        "  call void @__orison_dynamic_array_deallocate(ptr %items.runtime_cleanup.data, i64 4, "
        "i64 %items.runtime_cleanup.capacity)\n"
    );
    assert(
        runtime_indexed_dynamic_array_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .verification_metadata_available
    );
    assert(
        runtime_indexed_dynamic_array_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .verification_count == 1
    );
    assert(
        runtime_indexed_dynamic_array_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .all_candidate_functions_found
    );
    assert(
        runtime_indexed_dynamic_array_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .all_replacement_targets_unique
    );
    assert(
        runtime_indexed_dynamic_array_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .any_llvm_verifier_ran
    );
    assert(
        runtime_indexed_dynamic_array_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .all_llvm_verifier_passed
    );
    assert(
        runtime_indexed_dynamic_array_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .llvm_verifier_diagnostic_count == 0
    );
    assert(
        runtime_indexed_dynamic_array_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .all_verified
    );
    assert(
        runtime_indexed_dynamic_array_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .mutation_requested
    );
    assert(
        runtime_indexed_dynamic_array_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .candidate_verified
    );
    assert(
        runtime_indexed_dynamic_array_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .replacement_targets_unique
    );
    assert(
        runtime_indexed_dynamic_array_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .mutation_applied
    );
    assert(
        runtime_indexed_dynamic_array_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .module_matches_candidate
    );
    assert(
        runtime_indexed_dynamic_array_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .llvm_verifier_passed
    );
    assert(
        runtime_indexed_dynamic_array_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .llvm_verifier_diagnostic_count == 0
    );
    assert(
        runtime_indexed_dynamic_array_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .function_integration_ready
    );
    assert(
        runtime_indexed_dynamic_array_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .production_ready
    );

    auto runtime_indexed_multi_candidate_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "runtime_indexed_cleanup_two_function_candidates.or";
    auto runtime_indexed_multi_candidate_cleanup = pipeline.emit_llvm(
        runtime_indexed_multi_candidate_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
            .runtime_indexed_cleanup_module_ir_insertion_enabled = true,
            .runtime_indexed_cleanup_module_ir_mutation_enabled = true,
            .runtime_indexed_cleanup_function_ir_module_rewrite_enabled = true,
            .runtime_indexed_constructor_move_enabled = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_index_lowering_enabled = true,
            .dynamic_array_production_append_lowering_enabled = true,
        }
    );
    assert(!runtime_indexed_multi_candidate_cleanup.has_errors());
    assert(
        runtime_indexed_multi_candidate_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.size() == 2
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .candidate_count == 2
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .all_splice_ranges_available
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .same_function_splice_ranges_ordered
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .same_function_splice_ranges_non_overlapping
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .all_verified
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .all_splice_ranges_available
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
            .candidate_count == 2
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
            .available_candidate_count == 2
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .verification_count == 2
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .all_verified
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .verified_count == 2
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .llvm_verified_count == 2
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .llvm_verifier_diagnostic_count == 0
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .mutation_requested
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .candidate_verified
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .replacement_targets_unique
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .candidate_count == 2
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .mutation_applied
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .module_matches_candidate
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .llvm_verifier_passed
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .llvm_verifier_diagnostic_count == 0
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .function_integration_ready
    );
    assert(
        runtime_indexed_multi_candidate_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .production_ready
    );

    auto runtime_indexed_same_function_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "runtime_indexed_cleanup_same_function_two_candidates.or";
    auto runtime_indexed_same_function_cleanup = pipeline.emit_llvm(
        runtime_indexed_same_function_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
            .runtime_indexed_cleanup_module_ir_insertion_enabled = true,
            .runtime_indexed_cleanup_module_ir_mutation_enabled = true,
            .runtime_indexed_cleanup_function_ir_module_rewrite_enabled = true,
            .runtime_indexed_constructor_move_enabled = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_index_lowering_enabled = true,
            .dynamic_array_production_append_lowering_enabled = true,
        }
    );
    assert(!runtime_indexed_same_function_cleanup.has_errors());
    assert(
        runtime_indexed_same_function_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.size() == 2
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .candidate_count == 2
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .all_splice_ranges_available
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .same_function_splice_ranges_ordered
    );
    assert(
        !runtime_indexed_same_function_cleanup
             .runtime_indexed_cleanup_function_ir_rewrite_candidate_state
             .same_function_splice_ranges_non_overlapping
    );
    auto const& same_function_first_candidate =
        runtime_indexed_same_function_cleanup.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .candidates[0];
    auto const& same_function_second_candidate =
        runtime_indexed_same_function_cleanup.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .candidates[1];
    assert(same_function_first_candidate.function_symbol_name == same_function_second_candidate.function_symbol_name);
    assert(same_function_first_candidate.splice_range_available);
    assert(same_function_second_candidate.splice_range_available);
    assert(
        same_function_first_candidate.splice_range.start_offset ==
        same_function_second_candidate.splice_range.start_offset
    );
    assert(
        same_function_first_candidate.splice_range.end_offset ==
        same_function_second_candidate.splice_range.end_offset
    );
    assert(same_function_first_candidate.source_line == 46);
    assert(same_function_second_candidate.source_line == 51);
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
            .candidate_count == 2
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
            .available_candidate_count == 2
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .verification_count == 2
    );
    assert(
        !runtime_indexed_same_function_cleanup
             .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
             .all_replacement_targets_unique
    );
    assert(
        !runtime_indexed_same_function_cleanup
             .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
             .same_function_splice_ranges_non_overlapping
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .splice_conflict_count == 1
    );
    auto const& same_function_splice_conflict =
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .splice_conflicts.front();
    assert(same_function_splice_conflict.function_symbol_name == same_function_first_candidate.function_symbol_name);
    assert(same_function_splice_conflict.left_candidate_index == 0);
    assert(same_function_splice_conflict.right_candidate_index == 1);
    assert(same_function_splice_conflict.left_source_line == 46);
    assert(same_function_splice_conflict.right_source_line == 51);
    assert(
        same_function_splice_conflict.left_splice_range.start_offset ==
        same_function_first_candidate.splice_range.start_offset
    );
    assert(
        same_function_splice_conflict.left_splice_range.end_offset ==
        same_function_first_candidate.splice_range.end_offset
    );
    assert(
        same_function_splice_conflict.right_splice_range.start_offset ==
        same_function_second_candidate.splice_range.start_offset
    );
    assert(
        same_function_splice_conflict.right_splice_range.end_offset ==
        same_function_second_candidate.splice_range.end_offset
    );
    assert(
        !runtime_indexed_same_function_cleanup
             .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
             .all_verified
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .verified_count == 0
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .llvm_verified_count == 2
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .mutation_requested
    );
    assert(
        !runtime_indexed_same_function_cleanup
             .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
             .candidate_verified
    );
    assert(
        !runtime_indexed_same_function_cleanup
             .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
             .replacement_targets_unique
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .candidate_count == 2
    );
    assert(
        !runtime_indexed_same_function_cleanup
             .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
             .mutation_applied
    );
    assert(
        !runtime_indexed_same_function_cleanup
             .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
             .module_matches_candidate
    );
    assert(
        !runtime_indexed_same_function_cleanup
             .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
             .llvm_verifier_passed
    );
    assert(
        !runtime_indexed_same_function_cleanup
             .runtime_indexed_cleanup_module_ir_production_readiness_state
             .function_integration_ready
    );
    assert(
        !runtime_indexed_same_function_cleanup
             .runtime_indexed_cleanup_module_ir_production_readiness_state
             .function_splice_conflict_free
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .function_splice_conflict_count == 1
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_blocker_kind ==
        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::FunctionSpliceConflict
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .blockers.size() == 2
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .blockers[0].kind ==
        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::FunctionSpliceConflict
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .blockers[1].kind ==
        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::FunctionIntegration
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_blocker_stage_name == "function splice conflict"
    );
    assert(
        orison::pipeline::runtime_indexed_cleanup_production_readiness_blocker_kind_name(
            runtime_indexed_same_function_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
                .diagnostic_blocker_kind
        ) == "function-splice-conflict"
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_function_symbol_name == "select_both"
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_left_candidate_index == 0
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_right_candidate_index == 1
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_left_source_line == 46
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_right_source_line == 51
    );
    assert(
        runtime_indexed_same_function_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_text ==
        "runtime-index cleanup blocked: overlapping same-function splice ranges left-line 46 right-line 51"
    );
    assert(
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_diagnostic(
            runtime_indexed_same_function_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
        ) ==
        runtime_indexed_same_function_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_text
    );
    assert(
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_report(
            runtime_indexed_same_function_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
        ).find(
            "ir-shape ready production blocked blocker-count 2 blocker-kind function-splice-conflict "
            "function select_both "
            "source-line 46 "
            "diagnostic runtime-index cleanup blocked: "
            "overlapping same-function splice ranges left-line 46 right-line 51"
        ) != std::string::npos
    );
    auto runtime_indexed_same_function_cleanup_readiness_blocker_report =
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_blocker_report(
            runtime_indexed_same_function_cleanup.runtime_indexed_cleanup_module_ir_production_readiness_state
        );
    assert(runtime_indexed_same_function_cleanup_readiness_blocker_report.size() == 2);
    assert(
        runtime_indexed_same_function_cleanup_readiness_blocker_report.front().find(
            "index 0 kind function-splice-conflict stage function splice conflict function select_both source-line 46"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_same_function_cleanup_readiness_blocker_report.back().find(
            "index 1 kind function-integration stage function integration function select_both source-line 46"
        ) != std::string::npos
    );
    assert(
        !runtime_indexed_same_function_cleanup
             .runtime_indexed_cleanup_module_ir_production_readiness_state
             .production_ready
    );

    auto runtime_indexed_same_function_non_overlap_cleanup_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
        "runtime_indexed_cleanup_same_function_non_overlapping_candidates.or";
    auto runtime_indexed_same_function_non_overlap_cleanup = pipeline.emit_llvm(
        runtime_indexed_same_function_non_overlap_cleanup_path,
        orison::pipeline::CompilePipelineOptions {
            .source_drop_lowering_enabled = true,
            .collect_runtime_indexed_cleanup_audit = true,
            .runtime_indexed_cleanup_emission_enabled = true,
            .runtime_indexed_cleanup_module_ir_insertion_enabled = true,
            .runtime_indexed_cleanup_module_ir_mutation_enabled = true,
            .runtime_indexed_cleanup_function_ir_module_rewrite_enabled = true,
            .runtime_indexed_constructor_move_enabled = true,
            .dynamic_array_production_construction_lowering_enabled = true,
            .dynamic_array_production_index_lowering_enabled = true,
            .dynamic_array_production_append_lowering_enabled = true,
        }
    );
    assert(!runtime_indexed_same_function_non_overlap_cleanup.has_errors());
    assert(
        runtime_indexed_same_function_non_overlap_cleanup.runtime_indexed_cleanup_emission_plan_state.plans.size() == 2
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .candidate_count == 2
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .all_splice_ranges_available
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .same_function_splice_ranges_ordered
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .same_function_splice_ranges_non_overlapping
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .composition_failure_count == 0
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .first_composition_failure ==
        orison::pipeline::RuntimeIndexedCleanupIrCompositionFailure::none
    );
    auto const& non_overlap_first_candidate =
        runtime_indexed_same_function_non_overlap_cleanup.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .candidates[0];
    auto const& non_overlap_second_candidate =
        runtime_indexed_same_function_non_overlap_cleanup.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
            .candidates[1];
    assert(non_overlap_first_candidate.function_symbol_name == non_overlap_second_candidate.function_symbol_name);
    assert(non_overlap_first_candidate.splice_range_available);
    assert(non_overlap_second_candidate.splice_range_available);
    assert(non_overlap_first_candidate.splice_range.end_offset <= non_overlap_second_candidate.splice_range.start_offset);
    assert(non_overlap_first_candidate.source_line == 55);
    assert(non_overlap_second_candidate.source_line == 63);
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .all_verified
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .composition_failure_count == 0
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
            .first_composition_failure ==
        orison::pipeline::RuntimeIndexedCleanupIrCompositionFailure::none
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
            .candidate_count == 2
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state
            .available_candidate_count == 2
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .all_replacement_targets_unique
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .same_function_splice_ranges_non_overlapping
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .splice_conflict_count == 0
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .splice_conflicts.empty()
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .composition_failure_count == 0
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .first_composition_failure ==
        orison::pipeline::RuntimeIndexedCleanupIrCompositionFailure::none
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .all_verified
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state
            .llvm_verified_count == 2
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .candidate_verified
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .replacement_targets_unique
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .mutation_applied
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .module_matches_candidate
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .llvm_verifier_passed
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
            .composition_failure ==
        orison::pipeline::RuntimeIndexedCleanupIrCompositionFailure::none
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup.ir_text.find(
            "  br label %first_holder.items.runtime_cleanup.entry\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup.ir_text.find(
            "  br label %second_holder.items.runtime_cleanup.entry\n"
        ) != std::string::npos
    );
    assert(
        occurrence_count(
            runtime_indexed_same_function_non_overlap_cleanup.ir_text,
            "first_holder.items.runtime_cleanup.condition:\n"
        ) == 1
    );
    assert(
        occurrence_count(
            runtime_indexed_same_function_non_overlap_cleanup.ir_text,
            "second_holder.items.runtime_cleanup.condition:\n"
        ) == 1
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup.ir_text.find(
            "  call void @__orison_drop.Inner(ptr %first_holder.items.runtime_cleanup.element.addr)\n"
        ) != std::string::npos
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup.ir_text.find(
            "  call void @__orison_drop.Inner(ptr %second_holder.items.runtime_cleanup.element.addr)\n"
        ) != std::string::npos
    );
    assert(
        occurrence_count(
            runtime_indexed_same_function_non_overlap_cleanup.ir_text,
            "store %record.Inner zeroinitializer"
        ) >= 2
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .function_integration_ready
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .function_splice_conflict_free
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .function_splice_conflict_count == 0
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_blocker_kind ==
        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::None
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .blockers.empty()
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_blocker_stage_name.empty()
    );
    assert(
        orison::pipeline::runtime_indexed_cleanup_production_readiness_blocker_kind_name(
            runtime_indexed_same_function_non_overlap_cleanup
                .runtime_indexed_cleanup_module_ir_production_readiness_state
                .diagnostic_blocker_kind
        ) == "none"
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_function_symbol_name.empty()
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_left_candidate_index == 0
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_right_candidate_index == 0
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_left_source_line == 0
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_right_source_line == 0
    );
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .diagnostic_text.empty()
    );
    assert(
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_diagnostic(
            runtime_indexed_same_function_non_overlap_cleanup
                .runtime_indexed_cleanup_module_ir_production_readiness_state
        ).empty()
    );
    auto const non_overlap_readiness_report =
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_report(
            runtime_indexed_same_function_non_overlap_cleanup
                .runtime_indexed_cleanup_module_ir_production_readiness_state
        );
    assert(
        non_overlap_readiness_report ==
        "runtime-index cleanup module-ir production-readiness insertion-gate ready "
        "insertion-preview ready candidate ready candidate-verification verified "
        "module-mutation enabled function-integration ready splice-conflicts 0 "
        "splice-conflict-check clear ir-shape ready production ready blocker-count 0 blocker-kind none"
    );
    assert(non_overlap_readiness_report.find("diagnostic runtime-index cleanup blocked") == std::string::npos);
    auto const non_overlap_blocker_report =
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_blocker_report(
            runtime_indexed_same_function_non_overlap_cleanup
                .runtime_indexed_cleanup_module_ir_production_readiness_state
        );
    assert(non_overlap_blocker_report.empty());
    assert(
        runtime_indexed_same_function_non_overlap_cleanup
            .runtime_indexed_cleanup_module_ir_production_readiness_state
            .production_ready
    );

    auto composition_failure_readiness =
        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessState {
            .insertion_gate_ready = true,
            .insertion_preview_ready = true,
            .candidate_ready = true,
            .candidate_verified = true,
            .module_mutation_enabled = false,
            .function_integration_ready = false,
            .function_splice_conflict_free = true,
            .production_ready = false,
            .diagnostic_blocker_kind =
                orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::ModuleMutation,
            .blockers = {
                orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlocker {
                    .kind =
                        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::ModuleMutation,
                    .stage_name = "module mutation",
                    .function_symbol_name = "main",
                    .composition_failure =
                        orison::pipeline::RuntimeIndexedCleanupIrCompositionFailure::missing_cleanup_cfg_tail,
                    .composition_failure_part_available = true,
                    .composition_failure_part_index = 2,
                    .composition_failure_splice_range =
                        orison::pipeline::RuntimeIndexedCleanupTextSpliceRange {
                            .start_offset = 144,
                            .end_offset = 188,
                        },
                    .rewrite_apply_stage_available = true,
                    .branch_replacements_applied = true,
                    .cleanup_cfg_appended = true,
                    .phi_predecessors_retargeted = false,
                    .source_available = true,
                    .source_line = 55,
                },
            },
            .function_splice_conflict_count = 0,
            .diagnostic_blocker_stage_name = "module mutation",
            .diagnostic_function_symbol_name = "main",
            .diagnostic_composition_failure =
                orison::pipeline::RuntimeIndexedCleanupIrCompositionFailure::missing_cleanup_cfg_tail,
            .diagnostic_composition_failure_part_available = true,
            .diagnostic_composition_failure_part_index = 2,
            .diagnostic_composition_failure_splice_range =
                orison::pipeline::RuntimeIndexedCleanupTextSpliceRange {
                    .start_offset = 144,
                    .end_offset = 188,
                },
            .diagnostic_rewrite_apply_stage_available = true,
            .diagnostic_branch_replacements_applied = true,
            .diagnostic_cleanup_cfg_appended = true,
            .diagnostic_phi_predecessors_retargeted = false,
            .diagnostic_source_available = true,
            .diagnostic_source_line = 55,
        };
    composition_failure_readiness.diagnostic_text =
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_diagnostic(
            composition_failure_readiness
        );
    assert(
        composition_failure_readiness.diagnostic_text ==
        "runtime-index cleanup blocked: module mutation disabled "
        "composition-failure missing-cleanup-cfg-tail composition-part 2 splice-range 144..188 "
        "apply-stages available branch-replacements true cleanup-cfg-appended true phi-retargeted false"
    );
    assert(
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_report(
            composition_failure_readiness
        ).find(
            "ir-shape ready production blocked blocker-count 1 blocker-kind module-mutation function main "
            "source-line 55 "
            "diagnostic runtime-index cleanup blocked: module mutation disabled "
            "composition-failure missing-cleanup-cfg-tail composition-part 2 splice-range 144..188 "
            "apply-stages available branch-replacements true cleanup-cfg-appended true phi-retargeted false"
        ) != std::string::npos
    );
    auto const composition_failure_blockers =
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_blocker_report(
            composition_failure_readiness
        );
    assert(composition_failure_blockers.size() == 1);
    assert(
        composition_failure_blockers.front().find(
            "index 0 kind module-mutation stage module mutation function main source-line 55 "
            "composition-failure missing-cleanup-cfg-tail composition-part 2 splice-range 144..188 "
            "apply-stages available branch-replacements true cleanup-cfg-appended true phi-retargeted false"
        ) != std::string::npos
    );

    auto ir_shape_blocked_readiness =
        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessState {
            .insertion_gate_ready = true,
            .insertion_preview_ready = true,
            .candidate_ready = true,
            .candidate_verified = true,
            .module_mutation_enabled = true,
            .function_integration_ready = true,
            .function_splice_conflict_free = true,
            .ir_shape_ready = false,
            .production_ready = false,
            .diagnostic_blocker_kind =
                orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::IrShape,
            .blockers = {
                orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlocker {
                    .kind =
                        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::IrShape,
                    .stage_name = "cleanup ir shape",
                    .function_symbol_name = "main",
                    .source_available = true,
                    .source_line = 44,
                },
            },
            .function_splice_conflict_count = 0,
            .diagnostic_blocker_stage_name = "cleanup ir shape",
            .diagnostic_function_symbol_name = "main",
            .diagnostic_source_available = true,
            .diagnostic_source_line = 44,
        };
    ir_shape_blocked_readiness.diagnostic_text =
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_diagnostic(
            ir_shape_blocked_readiness
        );
    assert(
        ir_shape_blocked_readiness.diagnostic_text ==
        "runtime-index cleanup blocked: cleanup ir shape blocked"
    );
    assert(
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_report(
            ir_shape_blocked_readiness
        ).find(
            "splice-conflict-check clear ir-shape blocked production blocked blocker-count 1 "
            "blocker-kind ir-shape function main source-line 44 "
            "diagnostic runtime-index cleanup blocked: cleanup ir shape blocked"
        ) != std::string::npos
    );
    auto const ir_shape_blockers =
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_blocker_report(
            ir_shape_blocked_readiness
        );
    assert(ir_shape_blockers.size() == 1);
    assert(
        ir_shape_blockers.front() ==
        "runtime-index cleanup module-ir production-readiness blocker index 0 "
        "kind ir-shape stage cleanup ir shape function main source-line 44"
    );

    auto malformed_ir_shape_builder_readiness =
        ready_runtime_indexed_cleanup_module_ir_production_readiness(
            orison::lowering::RuntimeIndexedCleanupEmissionPlan {
                .function_symbol_name = "main",
                .owner_name = "items",
                .gated_ir_slice_lines = {
                    "  call void @__orison_drop.Inner(ptr %items.runtime_cleanup.element.addr)\n",
                },
                .ir_plan = orison::lowering::RuntimeIndexedCleanupIrPlan {
                    .owner_name = "items",
                    .element_llvm_type_name = "%record.Inner",
                    .owner_llvm_type_name = "[2 x %record.Inner]",
                    .owner_address_name = "%items.addr",
                    .condition_block_name = "items.runtime_cleanup.condition",
                    .live_check_block_name = "items.runtime_cleanup.live",
                    .skip_block_name = "items.runtime_cleanup.skip",
                    .drop_block_name = "items.runtime_cleanup.drop",
                    .element_address_name = "%items.runtime_cleanup.element.addr",
                    .drop_callee_name = "__orison_drop.Inner",
                    .continue_block_name = "items.runtime_cleanup.continue",
                    .exit_block_name = "items.runtime_cleanup.exit",
                    .complete = true,
                },
            }
        );
    assert_runtime_indexed_cleanup_ir_shape_blocked_with_ready_upstream(
        malformed_ir_shape_builder_readiness
    );

    auto well_formed_ir_shape_builder_readiness =
        ready_runtime_indexed_cleanup_module_ir_production_readiness(
            inline_runtime_indexed_cleanup_ir_shape_plan()
        );
    assert(well_formed_ir_shape_builder_readiness.production_ready);
    assert(well_formed_ir_shape_builder_readiness.ir_shape_ready);
    assert(well_formed_ir_shape_builder_readiness.blockers.empty());
    assert(
        well_formed_ir_shape_builder_readiness.diagnostic_blocker_kind ==
        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::None
    );
    assert(well_formed_ir_shape_builder_readiness.diagnostic_text.empty());
    assert(
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_report(
            well_formed_ir_shape_builder_readiness
        ).find(
            "splice-conflict-check clear ir-shape ready production ready blocker-count 0 "
            "blocker-kind none"
        ) != std::string::npos
    );

    auto inline_missing_zero_store_readiness =
        ready_runtime_indexed_cleanup_module_ir_production_readiness(
            inline_runtime_indexed_cleanup_ir_shape_plan_without_zero_store()
        );
    assert_runtime_indexed_cleanup_ir_shape_blocked_with_ready_upstream(
        inline_missing_zero_store_readiness
    );

    auto well_formed_descriptor_ir_shape_builder_readiness =
        ready_runtime_indexed_cleanup_module_ir_production_readiness(
            descriptor_runtime_indexed_cleanup_ir_shape_plan()
        );
    assert(well_formed_descriptor_ir_shape_builder_readiness.production_ready);
    assert(well_formed_descriptor_ir_shape_builder_readiness.ir_shape_ready);
    assert(well_formed_descriptor_ir_shape_builder_readiness.blockers.empty());
    assert(
        well_formed_descriptor_ir_shape_builder_readiness.diagnostic_blocker_kind ==
        orison::pipeline::RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::None
    );
    assert(well_formed_descriptor_ir_shape_builder_readiness.diagnostic_text.empty());
    assert(
        orison::pipeline::format_runtime_indexed_cleanup_production_readiness_report(
            well_formed_descriptor_ir_shape_builder_readiness
        ).find(
            "splice-conflict-check clear ir-shape ready production ready blocker-count 0 "
            "blocker-kind none"
        ) != std::string::npos
    );

    auto descriptor_missing_deallocate_tail_readiness =
        ready_runtime_indexed_cleanup_module_ir_production_readiness(
            descriptor_runtime_indexed_cleanup_ir_shape_plan_without_deallocate_tail()
        );
    assert_runtime_indexed_cleanup_ir_shape_blocked_with_ready_upstream(
        descriptor_missing_deallocate_tail_readiness
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
