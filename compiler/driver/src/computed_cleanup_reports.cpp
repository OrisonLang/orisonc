#include "computed_cleanup_reports.hpp"

#include <cstddef>
#include <sstream>
#include <string_view>

namespace orison::driver {
namespace {

auto indexed_name_or_unknown(std::vector<std::string> const& names, std::size_t index) -> std::string_view {
    if (index < names.size()) {
        return names[index];
    }
    return "<unknown>";
}

void append_computed_cleanup_summary(
    std::vector<std::string>& lines,
    std::string_view subject,
    std::string_view status,
    std::string_view counts,
    std::string_view suffix
) {
    auto summary = std::ostringstream {};
    summary << "computed DynamicArray " << subject << ' ' << status << ' ' << counts << ' ' << suffix;
    lines.push_back(summary.str());
}

void append_computed_cleanup_detail(
    std::vector<std::string>& lines,
    std::string_view subject,
    std::string_view owner_name,
    std::string_view fields,
    std::string_view suffix
) {
    auto detail = std::ostringstream {};
    detail << "computed DynamicArray " << subject << " detail owner " << owner_name;
    if (!fields.empty()) {
        detail << ' ' << fields;
    }
    detail << ' ' << suffix;
    lines.push_back(detail.str());
}

}  // namespace

auto computed_cleanup_call_insertion_capability_report(
    pipeline::ComputedCleanupCallInsertionCapabilityState const& state
) -> std::vector<std::string> {
    auto output = std::ostringstream {};
    output << "computed DynamicArray cleanup call insertion capability ";
    output << (state.enabled ? "enabled" : "disabled");
    output << (state.cleanup_call_authorization_enabled ?
        " [cleanup call authorization enabled]" : " [cleanup call authorization disabled]");
    output << (state.cleanup_call_insertion_enabled ?
        " [cleanup call insertion enabled]" : " [cleanup call insertion disabled]");
    output << " (metadata only)";
    return {output.str()};
}

auto computed_cleanup_call_insertion_readiness_report(
    pipeline::ComputedCleanupCallInsertionGateState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "gates " << state.gate_count;
    counts << " ready " << state.ready_count;
    counts << " blocked " << state.blocked_count;
    counts << (state.all_state_verified ? " [inserted state verified]" : " [inserted state unverified]");
    counts << (state.all_operands_proven ? " [cleanup operands proven]" : " [cleanup operands missing]");
    counts << (state.all_cleanup_calls_authorized ? " [cleanup calls authorized]" : " [cleanup calls unauthorized]");
    append_computed_cleanup_summary(
        lines,
        "cleanup call insertion readiness",
        state.all_ready ? "ready" : "blocked",
        counts.str(),
        "(metadata only)"
    );

    for (auto index = std::size_t {0}; index < state.cleanup_owner_names.size(); ++index) {
        auto fields = std::ostringstream {};
        fields << "cleanup-operation " << indexed_name_or_unknown(state.cleanup_operation_names, index);
        append_computed_cleanup_detail(
            lines,
            "cleanup call insertion readiness",
            state.cleanup_owner_names[index],
            fields.str(),
            "(metadata only)"
        );
    }

    return lines;
}

auto computed_inserted_cleanup_handoff_state_report(
    pipeline::ComputedInsertedCleanupHandoffState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "transitions " << state.transition_count;
    counts << " verifications " << state.verification_count;
    counts << " paired " << state.paired_count;
    counts << " blocked " << state.blocked_count;
    counts << (state.from_metadata ? " [metadata-backed]" : " [metadata-missing]");
    counts << (state.all_paired ? " [handoffs paired]" : " [handoffs blocked]");
    counts << (state.all_cleanup_calls_enabled ? " [cleanup calls enabled]" : " [cleanup calls disabled]");
    append_computed_cleanup_summary(
        lines,
        "inserted cleanup handoffs",
        state.all_paired ? "paired" : "blocked",
        counts.str(),
        "(inserted IR)"
    );

    for (auto index = std::size_t {0}; index < state.cleanup_owner_names.size(); ++index) {
        auto fields = std::ostringstream {};
        fields << "acquire " << indexed_name_or_unknown(state.acquire_operation_names, index);
        fields << " resume " << indexed_name_or_unknown(state.resume_operation_names, index);
        append_computed_cleanup_detail(
            lines,
            "inserted cleanup handoff",
            state.cleanup_owner_names[index],
            fields.str(),
            "(inserted IR)"
        );
    }

    return lines;
}

auto computed_inserted_cleanup_call_state_report(
    pipeline::ComputedInsertedCleanupCallState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "calls " << state.call_count;
    counts << " structured-proofs " << state.structured_proof_count;
    counts << " ir-fallback-proofs " << state.ir_fallback_proof_count;
    append_computed_cleanup_summary(
        lines,
        "inserted cleanup calls",
        state.all_inserted ? "inserted" : "absent",
        counts.str(),
        "(inserted IR)"
    );

    for (auto index = std::size_t {0}; index < state.cleanup_owner_names.size(); ++index) {
        auto fields = std::ostringstream {};
        fields << "data " << indexed_name_or_unknown(state.data_pointer_names, index);
        fields << " capacity " << indexed_name_or_unknown(state.capacity_names, index);
        append_computed_cleanup_detail(
            lines,
            "inserted cleanup call",
            state.cleanup_owner_names[index],
            fields.str(),
            "(inserted IR)"
        );
    }

    return lines;
}

auto computed_consumed_cleanup_descriptor_state_report(
    pipeline::ComputedConsumedCleanupDescriptorState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "descriptors " << state.descriptor_count;
    counts << " structured-proofs " << state.structured_proof_count;
    counts << " ir-fallback-proofs " << state.ir_fallback_proof_count;
    append_computed_cleanup_summary(
        lines,
        "consumed cleanup descriptors",
        state.all_finalized ? "finalized" : "absent",
        counts.str(),
        "(inserted IR)"
    );

    for (auto index = std::size_t {0}; index < state.cleanup_owner_names.size(); ++index) {
        auto fields = std::ostringstream {};
        fields << "descriptor " << indexed_name_or_unknown(state.descriptor_storage_names, index);
        append_computed_cleanup_detail(
            lines,
            "consumed cleanup descriptor",
            state.cleanup_owner_names[index],
            fields.str(),
            "(inserted IR)"
        );
    }

    return lines;
}

auto computed_cleanup_proof_summary_state_report(
    pipeline::ComputedCleanupProofSummaryState const& state
) -> std::vector<std::string> {
    auto counts = std::ostringstream {};
    counts << "models " << state.cleanup_proof_model_count;
    counts << " verified-pairs " << state.verified_inserted_cleanup_pair_count;
    counts << " structured-handoffs " << state.structured_inserted_cleanup_handoff_count;
    counts << " structured-handoff-uses " << state.structured_inserted_cleanup_handoff_use_count;
    counts << " ir-handoff-fallbacks " << state.ir_inserted_cleanup_handoff_fallback_count;
    counts << " structured-operands " << state.structured_cleanup_operand_count;
    counts << " structured-operand-uses " << state.structured_cleanup_operand_use_count;
    counts << " ir-operand-fallbacks " << state.ir_cleanup_operand_fallback_count;
    counts << " structured-inserted-calls " << state.structured_inserted_cleanup_call_count;
    counts << " ir-inserted-call-fallbacks " << state.ir_inserted_cleanup_call_fallback_count;
    counts << " structured-consumed-descriptors " << state.structured_consumed_cleanup_descriptor_count;
    counts << " ir-consumed-descriptor-fallbacks " << state.ir_consumed_cleanup_descriptor_fallback_count;

    auto lines = std::vector<std::string> {};
    append_computed_cleanup_summary(
        lines,
        "cleanup proof summary",
        state.cleanup_proof_model_count > 0 ? "available" : "empty",
        counts.str(),
        "(inserted IR)"
    );
    return lines;
}

}  // namespace orison::driver
