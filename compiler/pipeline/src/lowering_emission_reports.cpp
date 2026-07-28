#include "lowering_emission_reports.hpp"

#include "orison/lowering/dynamic_array_cleanup_plan.hpp"
#include "orison/pipeline/drop_readiness_source_correlation_report.hpp"

#include "dynamic_array_cleanup_readiness.hpp"
#include "computed_cleanup_proof_model.hpp"

#include <sstream>
#include <string_view>

namespace orison::pipeline {

namespace {

auto format_computed_cleanup_call_emission_gate(
    InsertedCleanupOperation const& acquisition,
    InsertedCleanupOperation const& resumption
) -> std::string {
    auto const state_verified =
        acquisition.target_owner_name == resumption.source_owner_name &&
        acquisition.source_owner_name == resumption.target_owner_name;
    auto const cleanup_calls_enabled =
        acquisition.cleanup_calls_enabled && resumption.cleanup_calls_enabled;
    auto output = std::ostringstream {};
    output << "computed DynamicArray for cleanup call emission gate ";
    output << (state_verified && cleanup_calls_enabled ? "ready" : "blocked");
    output << " acquire-operation " << acquisition.operation_name;
    output << " resume-operation " << resumption.operation_name;
    output << (state_verified ? " [inserted state verified]" : " [inserted state blocked]");
    output << (cleanup_calls_enabled ? " [cleanup calls enabled]" : " [cleanup calls disabled]");
    output << (state_verified && cleanup_calls_enabled ? " [cleanup call emission ready]" :
        " [cleanup call emission blocked]");
    output << " (inserted IR)";
    return output.str();
}

auto format_computed_cleanup_call_plan(
    InsertedCleanupOperation const& acquisition,
    InsertedCleanupOperation const& resumption,
    ComputedCleanupCallOperands const& operands
) -> std::string {
    auto const state_verified =
        acquisition.target_owner_name == resumption.source_owner_name &&
        acquisition.source_owner_name == resumption.target_owner_name;
    auto const cleanup_calls_enabled =
        acquisition.cleanup_calls_enabled && resumption.cleanup_calls_enabled;
    auto output = std::ostringstream {};
    output << "computed DynamicArray for cleanup call plan ";
    output << (state_verified ? "planned" : "blocked");
    output << " cleanup-operation " << resumption.operation_name << ".call";
    output << " after-resume-operation " << resumption.operation_name;
    output << " owner " << resumption.target_owner_name;
    if (!operands.data_pointer_name.empty()) {
        output << " data " << operands.data_pointer_name;
    }
    if (!operands.element_size_bytes.empty()) {
        output << " element-size " << operands.element_size_bytes;
    }
    if (!operands.capacity_name.empty()) {
        output << " capacity " << operands.capacity_name;
    }
    output << (state_verified ? " [inserted state verified]" : " [inserted state blocked]");
    output << (cleanup_calls_enabled ? " [cleanup calls enabled]" : " [cleanup calls disabled]");
    output << (operands.data_pointer_name.empty() ? " [data operand pending]" : " [data operand proven]");
    output << (operands.element_size_bytes.empty() ? " [element-size operand pending]" :
        " [element-size operand proven]");
    output << (operands.capacity_name.empty() ? " [capacity operand pending]" : " [capacity operand proven]");
    output << " [cleanup call disabled]";
    output << " snippets 1 (inserted IR)";
    return output.str();
}

auto format_computed_cleanup_call_render(
    InsertedCleanupOperation const& acquisition,
    InsertedCleanupOperation const& resumption,
    ComputedCleanupCallOperands const& operands
) -> std::string {
    auto const state_verified =
        acquisition.target_owner_name == resumption.source_owner_name &&
        acquisition.source_owner_name == resumption.target_owner_name;
    auto const operands_proven =
        !operands.data_pointer_name.empty() &&
        !operands.element_size_bytes.empty() &&
        !operands.capacity_name.empty();
    auto output = std::ostringstream {};
    output << "computed DynamicArray for cleanup call render ";
    output << (state_verified && operands_proven ? "rendered" : "blocked");
    output << " cleanup-operation " << resumption.operation_name << ".call";
    if (operands_proven) {
        output << " call \"call void @__orison_dynamic_array_deallocate(ptr ";
        output << operands.data_pointer_name;
        output << ", i64 " << operands.element_size_bytes;
        output << ", i64 " << operands.capacity_name << ")\"";
    }
    output << (state_verified ? " [inserted state verified]" : " [inserted state blocked]");
    output << (operands.data_pointer_name.empty() ? " [data operand pending]" : " [data operand proven]");
    output << (operands.element_size_bytes.empty() ? " [element-size operand pending]" :
        " [element-size operand proven]");
    output << (operands.capacity_name.empty() ? " [capacity operand pending]" : " [capacity operand proven]");
    output << " [render disabled]";
    output << " [module IR unchanged]";
    output << " snippets " << (state_verified && operands_proven ? 1 : 0);
    output << " (inserted IR)";
    return output.str();
}

auto format_computed_cleanup_call_insertion_gate(
    InsertedCleanupOperation const& acquisition,
    InsertedCleanupOperation const& resumption,
    ComputedCleanupCallOperands const& operands
) -> std::string {
    auto const state_verified =
        acquisition.target_owner_name == resumption.source_owner_name &&
        acquisition.source_owner_name == resumption.target_owner_name;
    auto const operands_proven =
        !operands.data_pointer_name.empty() &&
        !operands.element_size_bytes.empty() &&
        !operands.capacity_name.empty();
    auto const cleanup_calls_authorized =
        acquisition.cleanup_calls_enabled && resumption.cleanup_calls_enabled;
    auto const insertion_ready =
        state_verified && operands_proven && cleanup_calls_authorized;
    auto output = std::ostringstream {};
    output << "computed DynamicArray for cleanup call insertion gate ";
    output << (insertion_ready ? "ready" : "blocked");
    output << " cleanup-operation " << resumption.operation_name << ".call";
    output << (state_verified ? " [inserted state verified]" : " [inserted state blocked]");
    output << (operands_proven ? " [cleanup operands proven]" : " [cleanup operands blocked]");
    output << (cleanup_calls_authorized ? " [cleanup calls authorized]" : " [cleanup calls unauthorized]");
    output << (insertion_ready ? " [cleanup call insertion ready]" : " [cleanup call insertion blocked]");
    output << " (inserted IR)";
    return output.str();
}

auto format_computed_cleanup_call_emission_gate_report(
    std::vector<std::pair<InsertedCleanupOperation, InsertedCleanupOperation>> const& verified_pairs
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    for (auto const& [acquisition, resumption] : verified_pairs) {
        report.push_back(format_computed_cleanup_call_emission_gate(acquisition, resumption));
    }
    return report;
}

auto format_computed_cleanup_call_plan_report(
    std::vector<VerifiedComputedCleanupCall> const& verified_calls
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    for (auto const& call : verified_calls) {
        report.push_back(format_computed_cleanup_call_plan(
            call.acquisition,
            call.resumption,
            call.operands
        ));
    }
    return report;
}

auto format_computed_cleanup_call_render_report(
    std::vector<VerifiedComputedCleanupCall> const& verified_calls
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    for (auto const& call : verified_calls) {
        report.push_back(format_computed_cleanup_call_render(
            call.acquisition,
            call.resumption,
            call.operands
        ));
    }
    return report;
}

auto format_computed_cleanup_call_insertion_gate_report(
    std::vector<VerifiedComputedCleanupCall> const& verified_calls
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    for (auto const& call : verified_calls) {
        report.push_back(format_computed_cleanup_call_insertion_gate(
            call.acquisition,
            call.resumption,
            call.operands
        ));
    }
    return report;
}

auto format_computed_cleanup_call_inserted(
    InsertedCleanupOperation const& acquisition,
    InsertedCleanupOperation const& resumption,
    ComputedCleanupCallOperands const& operands
) -> std::string {
    auto output = std::ostringstream {};
    output << "computed DynamicArray for inserted cleanup call";
    output << " cleanup-operation " << resumption.operation_name << ".call";
    output << " call \"call void @__orison_dynamic_array_deallocate(ptr ";
    output << operands.data_pointer_name;
    output << ", i64 " << operands.element_size_bytes;
    output << ", i64 " << operands.capacity_name << ")\"";
    output << " [inserted state verified]";
    output << (acquisition.cleanup_calls_enabled && resumption.cleanup_calls_enabled ?
        " [cleanup calls authorized]" : " [cleanup calls unauthorized]");
    output << " (inserted IR)";
    return output.str();
}

auto format_computed_cleanup_call_inserted_report(
    std::string_view ir_text,
    std::vector<VerifiedComputedCleanupCall> const& verified_calls
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    for (auto const& call : verified_calls) {
        auto const& operands = call.operands;
        if (!computed_cleanup_call_operands_complete(operands)) {
            continue;
        }
        if (computed_cleanup_call_inserted_by_metadata(call)) {
            report.push_back(format_computed_cleanup_call_inserted(
                call.acquisition,
                call.resumption,
                operands
            ));
            continue;
        }
        if (!computed_cleanup_call_inserted_by_ir(ir_text, call)) {
            continue;
        }
        report.push_back(format_computed_cleanup_call_inserted(call.acquisition, call.resumption, operands));
    }
    return report;
}

auto format_computed_consumed_cleanup_descriptor(
    InsertedCleanupOperation const& resumption,
    std::string_view descriptor_storage_name
) -> std::string {
    auto output = std::ostringstream {};
    output << "computed DynamicArray for consumed cleanup descriptor";
    output << " cleanup-operation " << resumption.operation_name << ".call";
    output << " owner " << resumption.target_owner_name;
    output << " descriptor " << descriptor_storage_name;
    output << " [inserted cleanup call proven]";
    output << " [descriptor finalized]";
    output << " (inserted IR)";
    return output.str();
}

auto format_computed_consumed_cleanup_descriptor_report(
    std::string_view ir_text,
    std::vector<VerifiedComputedCleanupCall> const& verified_calls
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    for (auto const& call : verified_calls) {
        auto const& operands = call.operands;
        if (!computed_cleanup_call_operands_complete(operands)) {
            continue;
        }
        if (computed_consumed_cleanup_descriptor_by_metadata(call)) {
            report.push_back(
                format_computed_consumed_cleanup_descriptor(
                    call.resumption,
                    call.metadata->descriptor_storage_name
                )
            );
            continue;
        }
        if (auto descriptor_storage_name = computed_consumed_cleanup_descriptor_by_ir(ir_text, call)) {
            report.push_back(
                format_computed_consumed_cleanup_descriptor(call.resumption, *descriptor_storage_name)
            );
        }
    }
    return report;
}

}  // namespace

void populate_lowering_emission_reports(
    CompilePipelineResult& result,
    lowering::LlvmIrEmissionResult&& emission,
    CompilePipelineOptions const& options
) {
    result.ir_text = std::move(emission.ir_text);
    auto const cleanup_proof_model = build_computed_cleanup_proof_model(
        result.ir_text,
        emission.computed_dynamic_array_inserted_cleanup_handoffs,
        emission.computed_dynamic_array_cleanup_call_operands
    );
    result.dynamic_array_construction_plan_report =
        emission.dynamic_array_construction_plan_report();
    result.dynamic_array_runtime_request_report =
        emission.dynamic_array_runtime_request_report();
    result.dynamic_array_allocation_call_ir =
        std::move(emission.dynamic_array_allocation_call_ir);
    result.dynamic_array_descriptor_cleanup_plan_report =
        emission.dynamic_array_descriptor_cleanup_plan_report();
    result.dynamic_array_cleanup_obligation_report =
        emission.dynamic_array_cleanup_obligation_report();
    result.dynamic_array_cleanup_sequence_plan_report =
        emission.dynamic_array_cleanup_sequence_plan_report();
    result.dynamic_array_cleanup_sequence_verification_report =
        emission.dynamic_array_cleanup_sequence_verification_report();
    result.dynamic_array_cleanup_sequence_verification_passed =
        !emission.dynamic_array_cleanup_sequence_verifications.empty() &&
        lowering::dynamic_array_cleanup_sequence_verification_report_passed(
            emission.dynamic_array_cleanup_sequence_verifications
        );
    result.dynamic_array_cleanup_emission_gate_report =
        emission.dynamic_array_cleanup_emission_gate_report();
    if (emission.dynamic_array_cleanup_emission_capability.has_value()) {
        result.dynamic_array_cleanup_capability_proven =
            lowering::dynamic_array_cleanup_emission_capability_proven(
                *emission.dynamic_array_cleanup_emission_capability
            );
        result.dynamic_array_cleanup_missing_element_drop_pairs =
            emission.dynamic_array_cleanup_emission_capability->missing_element_drop_pairs;
    }
    result.dynamic_array_cleanup_emission_capability_report =
        emission.dynamic_array_cleanup_emission_capability_report();
    result.emitted_dynamic_array_cleanup_obligation_report =
        std::move(emission.emitted_dynamic_array_cleanup_obligation_report);
    result.emitted_dynamic_array_cleanup_sequence_plan_report =
        std::move(emission.emitted_dynamic_array_cleanup_sequence_plan_report);
    result.emitted_dynamic_array_cleanup_sequence_verification_report =
        std::move(emission.emitted_dynamic_array_cleanup_sequence_verification_report);
    result.emitted_dynamic_array_cleanup_emission_gate_report =
        std::move(emission.emitted_dynamic_array_cleanup_emission_gate_report);
    result.emitted_dynamic_array_cleanup_emission_capability_report =
        std::move(emission.emitted_dynamic_array_cleanup_emission_capability_report);
    result.computed_dynamic_array_for_descriptor_render_report =
        emission.computed_dynamic_array_for_descriptor_render_report();
    result.computed_dynamic_array_for_loop_control_render_report =
        emission.computed_dynamic_array_for_loop_control_render_report();
    result.computed_dynamic_array_for_element_address_render_report =
        emission.computed_dynamic_array_for_element_address_render_report();
    result.computed_dynamic_array_for_element_load_render_report =
        emission.computed_dynamic_array_for_element_load_render_report();
    result.computed_dynamic_array_for_loop_continue_render_report =
        emission.computed_dynamic_array_for_loop_continue_render_report();
    result.computed_dynamic_array_for_loop_render_sequence_report =
        emission.computed_dynamic_array_for_loop_render_sequence_report();
    result.computed_dynamic_array_for_loop_exit_cleanup_report =
        emission.computed_dynamic_array_for_loop_exit_cleanup_report();
    result.computed_dynamic_array_for_cleanup_transition_report =
        emission.computed_dynamic_array_for_cleanup_transition_report();
    result.computed_dynamic_array_for_inserted_cleanup_transition_report =
        cleanup_proof_model.inserted_cleanup_state.transition_report;
    result.computed_dynamic_array_for_inserted_cleanup_state_verification_report =
        cleanup_proof_model.inserted_cleanup_state.verification_report;
    result.computed_dynamic_array_for_cleanup_proof_model_count =
        cleanup_proof_model.verified_cleanup_calls.size();
    result.computed_dynamic_array_for_verified_inserted_cleanup_pair_count =
        cleanup_proof_model.inserted_cleanup_state.verified_pairs.size();
    result.computed_dynamic_array_for_structured_inserted_cleanup_handoff_count =
        emission.computed_dynamic_array_inserted_cleanup_handoffs.size();
    if (cleanup_proof_model.inserted_cleanup_state.from_metadata) {
        result.computed_dynamic_array_for_structured_inserted_cleanup_handoff_use_count =
            cleanup_proof_model.inserted_cleanup_state.verified_pairs.size() * 2;
    } else {
        result.computed_dynamic_array_for_ir_inserted_cleanup_handoff_fallback_count =
            cleanup_proof_model.inserted_cleanup_state.verified_pairs.size() * 2;
    }
    result.computed_dynamic_array_for_structured_cleanup_operand_count =
        emission.computed_dynamic_array_cleanup_call_operands.size();
    for (auto const& call : cleanup_proof_model.verified_cleanup_calls) {
        if (call.operands.from_metadata) {
            ++result.computed_dynamic_array_for_structured_cleanup_operand_use_count;
        } else {
            ++result.computed_dynamic_array_for_ir_cleanup_operand_fallback_count;
        }
    }
    for (auto const& operands : emission.computed_dynamic_array_cleanup_call_operands) {
        if (operands.cleanup_call_inserted) {
            ++result.computed_dynamic_array_for_structured_inserted_cleanup_call_count;
        }
        if (operands.cleanup_call_inserted &&
            operands.descriptor_finalized &&
            !operands.descriptor_storage_name.empty()) {
            ++result.computed_dynamic_array_for_structured_consumed_cleanup_descriptor_count;
        }
    }
    for (auto const& call : cleanup_proof_model.verified_cleanup_calls) {
        if (computed_cleanup_call_inserted_by_ir(result.ir_text, call)) {
            ++result.computed_dynamic_array_for_ir_inserted_cleanup_call_fallback_count;
        }
        if (computed_consumed_cleanup_descriptor_by_ir(result.ir_text, call).has_value()) {
            ++result.computed_dynamic_array_for_ir_consumed_cleanup_descriptor_fallback_count;
        }
    }
    result.computed_dynamic_array_for_cleanup_call_emission_gate_report =
        format_computed_cleanup_call_emission_gate_report(
            cleanup_proof_model.inserted_cleanup_state.verified_pairs
        );
    result.computed_dynamic_array_for_cleanup_call_plan_report =
        format_computed_cleanup_call_plan_report(cleanup_proof_model.verified_cleanup_calls);
    result.computed_dynamic_array_for_cleanup_call_render_report =
        format_computed_cleanup_call_render_report(cleanup_proof_model.verified_cleanup_calls);
    result.computed_dynamic_array_for_cleanup_call_insertion_gate_report =
        format_computed_cleanup_call_insertion_gate_report(cleanup_proof_model.verified_cleanup_calls);
    result.computed_dynamic_array_for_inserted_cleanup_call_report =
        format_computed_cleanup_call_inserted_report(
            result.ir_text,
            cleanup_proof_model.verified_cleanup_calls
        );
    result.consumed_descriptor_finalization_plan_report =
        emission.consumed_descriptor_finalization_plan_report();
    result.computed_dynamic_array_for_consumed_cleanup_descriptor_model_report =
        emission.computed_dynamic_array_for_consumed_cleanup_descriptor_model_report();
    result.computed_dynamic_array_for_consumed_cleanup_descriptor_report =
        format_computed_consumed_cleanup_descriptor_report(
            result.ir_text,
            cleanup_proof_model.verified_cleanup_calls
        );
    result.computed_dynamic_array_for_production_emission_gate_report =
        emission.computed_dynamic_array_for_production_emission_gate_report();
    result.computed_dynamic_array_for_production_sequence_report =
        emission.computed_dynamic_array_for_production_sequence_report();
    result.test_only_computed_dynamic_array_for_production_sequence_module_ir =
        std::move(emission.test_only_computed_dynamic_array_for_production_sequence_module_ir);
    result.dynamic_array_cleanup_production_readiness =
        plan_dynamic_array_cleanup_production_readiness(result, options);
    result.dynamic_array_cleanup_production_readiness_report = {
        format_dynamic_array_cleanup_production_readiness(result.dynamic_array_cleanup_production_readiness),
    };
    result.planned_drop_report = emission.planned_drop_report();
    result.emitted_drop_declaration_report =
        emission.emitted_drop_declaration_report();
    result.planned_drop_action_report =
        emission.planned_drop_action_report();
    result.drop_cleanup_authorization_report =
        emission.drop_cleanup_authorization_report();
    result.drop_readiness_snapshot = emission.drop_readiness_snapshot();
    result.drop_readiness_snapshot_report =
        emission.drop_readiness_snapshot_report();
    result.drop_readiness_summary = emission.drop_readiness_summary();
    result.drop_readiness_summary_report =
        emission.drop_readiness_summary_report();
    result.drop_readiness_relation_report =
        emission.drop_readiness_relation_report();
    result.drop_readiness_blocker_summary =
        lowering::summarize_drop_readiness_blockers(result.drop_readiness_snapshot);
    result.drop_readiness_blocker_report =
        lowering::format_drop_readiness_blocker_report(result.drop_readiness_blocker_summary);
    result.drop_readiness_source_correlation_report =
        format_drop_readiness_source_correlation_report(result.drop_readiness_snapshot);
    result.semantic_drop_lowering_authorizations = std::move(emission.semantic_drop_lowering_authorizations);
}

}  // namespace orison::pipeline
