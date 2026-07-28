#include "computed_cleanup_proof_reports.hpp"

#include "computed_cleanup_proof_model.hpp"

#include <sstream>

namespace orison::pipeline {

namespace {

auto format_computed_cleanup_call_emission_gate(
    ComputedCleanupCallEmissionGateEvent const& event
) -> std::string {
    auto const& acquisition = event.acquisition;
    auto const& resumption = event.resumption;
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
    ComputedCleanupCallPlanEvent const& event
) -> std::string {
    auto const& acquisition = event.acquisition;
    auto const& resumption = event.resumption;
    auto const& operands = event.operands;
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
    ComputedCleanupCallRenderEvent const& event
) -> std::string {
    auto const& acquisition = event.acquisition;
    auto const& resumption = event.resumption;
    auto const& operands = event.operands;
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
    ComputedCleanupCallInsertionGateEvent const& event
) -> std::string {
    auto const& resumption = event.resumption;
    auto const& decision = event.decision;
    auto output = std::ostringstream {};
    output << "computed DynamicArray for cleanup call insertion gate ";
    output << (decision.insertion_ready ? "ready" : "blocked");
    output << " cleanup-operation " << resumption.operation_name << ".call";
    output << (decision.state_verified ? " [inserted state verified]" : " [inserted state blocked]");
    output << (decision.operands_proven ? " [cleanup operands proven]" : " [cleanup operands blocked]");
    output << (decision.cleanup_calls_authorized ? " [cleanup calls authorized]" :
        " [cleanup calls unauthorized]");
    output << (decision.insertion_ready ? " [cleanup call insertion ready]" :
        " [cleanup call insertion blocked]");
    output << " (inserted IR)";
    return output.str();
}

auto format_computed_cleanup_call_inserted(
    ComputedInsertedCleanupCallEvent const& event
) -> std::string {
    auto const& acquisition = event.acquisition;
    auto const& resumption = event.resumption;
    auto const& operands = event.operands;
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

auto format_computed_consumed_cleanup_descriptor(
    ComputedConsumedCleanupDescriptorEvent const& event
) -> std::string {
    auto const& resumption = event.resumption;
    auto output = std::ostringstream {};
    output << "computed DynamicArray for consumed cleanup descriptor";
    output << " cleanup-operation " << resumption.operation_name << ".call";
    output << " owner " << resumption.target_owner_name;
    output << " descriptor " << event.descriptor_storage_name;
    output << " [inserted cleanup call proven]";
    output << " [descriptor finalized]";
    output << " (inserted IR)";
    return output.str();
}

}  // namespace

auto format_inserted_cleanup_transition(
    InsertedCleanupTransitionEvent const& event
) -> std::string {
    auto output = std::ostringstream {};
    output << "computed DynamicArray for inserted cleanup transition";
    output << " acquire-from " << event.acquisition.source_owner_name;
    output << " acquire-to " << event.acquisition.target_owner_name;
    output << " acquire-operation " << event.acquisition.operation_name;
    output << " resume-from " << event.resumption.source_owner_name;
    output << " resume-to " << event.resumption.target_owner_name;
    output << " resume-operation " << event.resumption.operation_name;
    output << " (inserted IR)";
    return output.str();
}

auto format_inserted_cleanup_state_verification(
    InsertedCleanupStateVerificationEvent const& event
) -> std::string {
    if (event.kind == InsertedCleanupStateVerificationKind::blocked || !event.acquisition.has_value()) {
        auto output = std::ostringstream {};
        output << "computed DynamicArray for inserted cleanup state verification blocked";
        output << " reason " << event.reason;
        output << " operation " << event.operation.operation_name;
        output << " from " << event.operation.source_owner_name;
        output << " to " << event.operation.target_owner_name;
        output << " (inserted IR)";
        return output.str();
    }
    auto const& acquisition = *event.acquisition;
    auto const& resumption = event.operation;
    auto output = std::ostringstream {};
    output << "computed DynamicArray for inserted cleanup state verification";
    output << " acquire-operation " << acquisition.operation_name;
    output << " resume-operation " << resumption.operation_name;
    output << " acquire-from " << acquisition.source_owner_name;
    output << " acquire-to " << acquisition.target_owner_name;
    output << " resume-from " << resumption.source_owner_name;
    output << " resume-to " << resumption.target_owner_name;
    output << " [handoff paired]";
    output << (acquisition.cleanup_calls_enabled && resumption.cleanup_calls_enabled ?
        " [cleanup calls enabled]" : " [cleanup calls disabled]");
    output << " (inserted IR)";
    return output.str();
}

auto build_computed_cleanup_proof_report_bundle(
    ComputedCleanupProofModel const& model
) -> ComputedCleanupProofReportBundle {
    auto reports = ComputedCleanupProofReportBundle {};
    for (auto const& event : model.inserted_cleanup_state.transition_events) {
        reports.inserted_cleanup_transition_report.push_back(format_inserted_cleanup_transition(event));
    }
    for (auto const& event : model.inserted_cleanup_state.verification_events) {
        reports.inserted_cleanup_state_verification_report.push_back(
            format_inserted_cleanup_state_verification(event)
        );
    }
    for (auto const& event : model.cleanup_call_report_events.emission_gate_events) {
        reports.cleanup_call_emission_gate_report.push_back(
            format_computed_cleanup_call_emission_gate(event)
        );
    }
    for (auto const& event : model.cleanup_call_report_events.plan_events) {
        reports.cleanup_call_plan_report.push_back(format_computed_cleanup_call_plan(event));
    }
    for (auto const& event : model.cleanup_call_report_events.render_events) {
        reports.cleanup_call_render_report.push_back(format_computed_cleanup_call_render(event));
    }
    for (auto const& event : model.cleanup_call_report_events.insertion_gate_events) {
        reports.cleanup_call_insertion_gate_report.push_back(format_computed_cleanup_call_insertion_gate(event));
    }
    for (auto const& event : model.cleanup_call_report_events.inserted_call_events) {
        reports.inserted_cleanup_call_report.push_back(format_computed_cleanup_call_inserted(event));
    }
    for (auto const& event : model.cleanup_call_report_events.consumed_descriptor_events) {
        reports.consumed_cleanup_descriptor_report.push_back(format_computed_consumed_cleanup_descriptor(event));
    }
    return reports;
}

}  // namespace orison::pipeline
