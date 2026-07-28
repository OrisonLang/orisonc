#include "computed_cleanup_proof_reports.hpp"

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
    InsertedCleanupOperation const& resumption,
    ComputedCleanupCallInsertionDecision const& decision
) -> std::string {
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

}  // namespace

auto format_inserted_cleanup_transition(
    InsertedCleanupOperation const& acquisition,
    InsertedCleanupOperation const& resumption
) -> std::string {
    auto output = std::ostringstream {};
    output << "computed DynamicArray for inserted cleanup transition";
    output << " acquire-from " << acquisition.source_owner_name;
    output << " acquire-to " << acquisition.target_owner_name;
    output << " acquire-operation " << acquisition.operation_name;
    output << " resume-from " << resumption.source_owner_name;
    output << " resume-to " << resumption.target_owner_name;
    output << " resume-operation " << resumption.operation_name;
    output << " (inserted IR)";
    return output.str();
}

auto format_inserted_cleanup_state_verification(
    InsertedCleanupOperation const& acquisition,
    InsertedCleanupOperation const& resumption
) -> std::string {
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

auto format_inserted_cleanup_state_verification_blocked(
    std::string_view reason,
    InsertedCleanupOperation const& operation
) -> std::string {
    auto output = std::ostringstream {};
    output << "computed DynamicArray for inserted cleanup state verification blocked";
    output << " reason " << reason;
    output << " operation " << operation.operation_name;
    output << " from " << operation.source_owner_name;
    output << " to " << operation.target_owner_name;
    output << " (inserted IR)";
    return output.str();
}

auto build_computed_cleanup_proof_report_bundle(
    ComputedCleanupProofModel const& model
) -> ComputedCleanupProofReportBundle {
    auto reports = ComputedCleanupProofReportBundle {};
    for (auto const& [acquisition, resumption] : model.inserted_cleanup_state.verified_pairs) {
        reports.cleanup_call_emission_gate_report.push_back(
            format_computed_cleanup_call_emission_gate(acquisition, resumption)
        );
    }
    for (auto const& call : model.verified_cleanup_calls) {
        reports.cleanup_call_plan_report.push_back(format_computed_cleanup_call_plan(
            call.acquisition,
            call.resumption,
            call.operands
        ));
        reports.cleanup_call_render_report.push_back(format_computed_cleanup_call_render(
            call.acquisition,
            call.resumption,
            call.operands
        ));
        reports.cleanup_call_insertion_gate_report.push_back(format_computed_cleanup_call_insertion_gate(
            call.resumption,
            call.insertion_decision
        ));
        if (call.inserted_call_decision.operands_proven && call.inserted_call_decision.inserted) {
            reports.inserted_cleanup_call_report.push_back(format_computed_cleanup_call_inserted(
                call.acquisition,
                call.resumption,
                call.operands
            ));
        }
        if (call.consumed_descriptor_decision.operands_proven &&
            call.consumed_descriptor_decision.finalized &&
            call.consumed_descriptor_decision.descriptor_storage_name.has_value()) {
            reports.consumed_cleanup_descriptor_report.push_back(format_computed_consumed_cleanup_descriptor(
                call.resumption,
                *call.consumed_descriptor_decision.descriptor_storage_name
            ));
        }
    }
    return reports;
}

}  // namespace orison::pipeline
