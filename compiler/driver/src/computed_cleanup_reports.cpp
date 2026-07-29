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

}  // namespace orison::driver
