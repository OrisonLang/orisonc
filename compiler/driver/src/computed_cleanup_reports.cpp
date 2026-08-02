#include "computed_cleanup_reports.hpp"

#include "orison/lowering/aggregate_path.hpp"
#include "orison/lowering/dynamic_array_cleanup_plan.hpp"

#include <cstddef>
#include <set>
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

auto dynamic_array_descriptor_cleanup_plan_state_report(
    pipeline::DynamicArrayDescriptorCleanupPlanState const& state
) -> std::vector<std::string> {
    return lowering::format_dynamic_array_descriptor_cleanup_plan_report(state.plans);
}

auto dynamic_array_cleanup_obligation_state_report(
    pipeline::DynamicArrayCleanupObligationState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    lines.reserve(state.obligations.size());
    for (auto index = std::size_t {0}; index < state.obligations.size(); ++index) {
        auto line = lowering::format_dynamic_array_cleanup_obligation(state.obligations[index]);
        if (index < state.function_symbol_names.size() && !state.function_symbol_names[index].empty()) {
            line = "function " + state.function_symbol_names[index] + " " + line;
        }
        lines.push_back(std::move(line));
    }
    return lines;
}

auto dynamic_array_cleanup_sequence_plan_state_report(
    pipeline::DynamicArrayCleanupSequencePlanState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    lines.reserve(state.plans.size());
    for (auto index = std::size_t {0}; index < state.plans.size(); ++index) {
        auto line = lowering::format_dynamic_array_cleanup_sequence_plan(state.plans[index]);
        if (index < state.function_symbol_names.size() && !state.function_symbol_names[index].empty()) {
            line = "function " + state.function_symbol_names[index] + " " + line;
        }
        lines.push_back(std::move(line));
    }
    return lines;
}

auto dynamic_array_cleanup_sequence_verification_state_report(
    pipeline::DynamicArrayCleanupSequenceVerificationState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    lines.reserve(state.verifications.size());
    for (auto index = std::size_t {0}; index < state.verifications.size(); ++index) {
        auto line = lowering::format_dynamic_array_cleanup_sequence_verification(state.verifications[index]);
        if (index < state.function_symbol_names.size() && !state.function_symbol_names[index].empty()) {
            line = "function " + state.function_symbol_names[index] + " " + line;
        }
        lines.push_back(std::move(line));
    }
    return lines;
}

auto dynamic_array_cleanup_emission_gate_state_report(
    pipeline::DynamicArrayCleanupSequenceVerificationState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    lines.reserve(state.verifications.size());
    for (auto index = std::size_t {0}; index < state.verifications.size(); ++index) {
        auto line = lowering::format_dynamic_array_cleanup_emission_gate(state.verifications[index]);
        if (index < state.function_symbol_names.size() && !state.function_symbol_names[index].empty()) {
            line = "function " + state.function_symbol_names[index] + " " + line;
        }
        lines.push_back(std::move(line));
    }
    return lines;
}

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
    counts << " cleanup-blockers " << state.cleanup_call_blocker_count;
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
        auto const blocked_reason = indexed_name_or_unknown(state.cleanup_calls_blocked_reasons, index);
        if (blocked_reason != "<unknown>" && !blocked_reason.empty()) {
            fields << " cleanup-blocked-reason " << blocked_reason;
        }
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

auto computed_dynamic_array_for_descriptor_render_state_report(
    pipeline::ComputedDynamicArrayForDescriptorRenderState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "renders " << state.render_count;
    counts << " snippets " << state.rendered_ir_snippet_count;
    counts << (state.render_metadata_available ? " [metadata available]" : " [metadata missing]");
    counts << (state.all_descriptor_projections_ready ?
        " [descriptor projections ready]" : " [descriptor projections blocked]");
    append_computed_cleanup_summary(
        lines,
        "descriptor render",
        state.render_metadata_available ? "planned" : "absent",
        counts.str(),
        "(metadata only)"
    );

    for (auto index = std::size_t {0}; index < state.cleanup_owner_names.size(); ++index) {
        auto fields = std::ostringstream {};
        fields << "function " << indexed_name_or_unknown(state.enclosing_function_names, index);
        fields << " source " << indexed_name_or_unknown(state.source_type_names, index);
        fields << " element " << indexed_name_or_unknown(state.element_source_type_names, index);
        fields << " descriptor " << indexed_name_or_unknown(state.descriptor_storage_names, index);
        fields << " value " << indexed_name_or_unknown(state.descriptor_value_names, index);
        fields << " data " << indexed_name_or_unknown(state.data_pointer_names, index);
        fields << " length " << indexed_name_or_unknown(state.length_names, index);
        fields << " capacity " << indexed_name_or_unknown(state.capacity_names, index);
        append_computed_cleanup_detail(
            lines,
            "descriptor render",
            state.cleanup_owner_names[index],
            fields.str(),
            "(metadata only)"
        );
    }

    return lines;
}

auto computed_dynamic_array_for_loop_control_render_state_report(
    pipeline::ComputedDynamicArrayForLoopControlRenderState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "renders " << state.render_count;
    counts << " snippets " << state.rendered_ir_snippet_count;
    counts << (state.render_metadata_available ? " [metadata available]" : " [metadata missing]");
    counts << (state.all_control_flow_names_ready ? " [control flow ready]" : " [control flow blocked]");
    append_computed_cleanup_summary(
        lines,
        "loop control render",
        state.render_metadata_available ? "planned" : "absent",
        counts.str(),
        "(metadata only)"
    );

    for (auto index = std::size_t {0}; index < state.cleanup_owner_names.size(); ++index) {
        auto fields = std::ostringstream {};
        fields << "function " << indexed_name_or_unknown(state.enclosing_function_names, index);
        fields << " source " << indexed_name_or_unknown(state.source_type_names, index);
        fields << " element " << indexed_name_or_unknown(state.element_source_type_names, index);
        fields << " condition " << indexed_name_or_unknown(state.condition_block_names, index);
        fields << " body " << indexed_name_or_unknown(state.body_block_names, index);
        fields << " continue " << indexed_name_or_unknown(state.continue_block_names, index);
        fields << " exit " << indexed_name_or_unknown(state.exit_block_names, index);
        fields << " index " << indexed_name_or_unknown(state.index_names, index);
        fields << " next " << indexed_name_or_unknown(state.next_index_names, index);
        fields << " bounds " << indexed_name_or_unknown(state.bounds_check_names, index);
        append_computed_cleanup_detail(
            lines,
            "loop control render",
            state.cleanup_owner_names[index],
            fields.str(),
            "(metadata only)"
        );
    }

    return lines;
}

auto computed_dynamic_array_for_element_address_render_state_report(
    pipeline::ComputedDynamicArrayForElementAddressRenderState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "renders " << state.render_count;
    counts << " snippets " << state.rendered_ir_snippet_count;
    counts << (state.render_metadata_available ? " [metadata available]" : " [metadata missing]");
    counts << (state.all_element_address_inputs_ready ? " [element address ready]" : " [element address blocked]");
    append_computed_cleanup_summary(
        lines,
        "element address render",
        state.render_metadata_available ? "planned" : "absent",
        counts.str(),
        "(metadata only)"
    );

    for (auto index = std::size_t {0}; index < state.cleanup_owner_names.size(); ++index) {
        auto fields = std::ostringstream {};
        fields << "function " << indexed_name_or_unknown(state.enclosing_function_names, index);
        fields << " source " << indexed_name_or_unknown(state.source_type_names, index);
        fields << " element " << indexed_name_or_unknown(state.element_source_type_names, index);
        fields << " lowers-to " << indexed_name_or_unknown(state.element_llvm_type_names, index);
        fields << " data " << indexed_name_or_unknown(state.data_pointer_names, index);
        fields << " index " << indexed_name_or_unknown(state.index_names, index);
        fields << " address " << indexed_name_or_unknown(state.element_address_names, index);
        append_computed_cleanup_detail(
            lines,
            "element address render",
            state.cleanup_owner_names[index],
            fields.str(),
            "(metadata only)"
        );
    }

    return lines;
}

auto computed_dynamic_array_for_element_load_render_state_report(
    pipeline::ComputedDynamicArrayForElementLoadRenderState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "renders " << state.render_count;
    counts << " snippets " << state.rendered_ir_snippet_count;
    counts << (state.render_metadata_available ? " [metadata available]" : " [metadata missing]");
    counts << (state.all_element_load_inputs_ready ? " [element load ready]" : " [element load blocked]");
    append_computed_cleanup_summary(
        lines,
        "element load render",
        state.render_metadata_available ? "planned" : "absent",
        counts.str(),
        "(metadata only)"
    );

    for (auto index = std::size_t {0}; index < state.cleanup_owner_names.size(); ++index) {
        auto fields = std::ostringstream {};
        fields << "function " << indexed_name_or_unknown(state.enclosing_function_names, index);
        fields << " source " << indexed_name_or_unknown(state.source_type_names, index);
        fields << " element " << indexed_name_or_unknown(state.element_source_type_names, index);
        fields << " lowers-to " << indexed_name_or_unknown(state.element_llvm_type_names, index);
        fields << " address " << indexed_name_or_unknown(state.element_address_names, index);
        fields << " item " << indexed_name_or_unknown(state.item_value_names, index);
        append_computed_cleanup_detail(
            lines,
            "element load render",
            state.cleanup_owner_names[index],
            fields.str(),
            "(metadata only)"
        );
    }

    return lines;
}

auto computed_dynamic_array_for_loop_continue_render_state_report(
    pipeline::ComputedDynamicArrayForLoopContinueRenderState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "renders " << state.render_count;
    counts << " snippets " << state.rendered_ir_snippet_count;
    counts << (state.render_metadata_available ? " [metadata available]" : " [metadata missing]");
    counts << (state.all_loop_continue_inputs_ready ? " [loop continue ready]" : " [loop continue blocked]");
    append_computed_cleanup_summary(
        lines,
        "loop continue render",
        state.render_metadata_available ? "planned" : "absent",
        counts.str(),
        "(metadata only)"
    );

    for (auto index = std::size_t {0}; index < state.cleanup_owner_names.size(); ++index) {
        auto fields = std::ostringstream {};
        fields << "function " << indexed_name_or_unknown(state.enclosing_function_names, index);
        fields << " source " << indexed_name_or_unknown(state.source_type_names, index);
        fields << " element " << indexed_name_or_unknown(state.element_source_type_names, index);
        fields << " continue " << indexed_name_or_unknown(state.continue_block_names, index);
        fields << " condition " << indexed_name_or_unknown(state.condition_block_names, index);
        fields << " index " << indexed_name_or_unknown(state.index_names, index);
        fields << " next " << indexed_name_or_unknown(state.next_index_names, index);
        append_computed_cleanup_detail(
            lines,
            "loop continue render",
            state.cleanup_owner_names[index],
            fields.str(),
            "(metadata only)"
        );
    }

    return lines;
}

auto computed_dynamic_array_for_loop_render_sequence_state_report(
    pipeline::ComputedDynamicArrayForLoopRenderSequenceState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "sequences " << state.sequence_count;
    counts << " snippets " << state.rendered_ir_snippet_count;
    counts << (state.sequence_metadata_available ? " [metadata available]" : " [metadata missing]");
    counts << (state.all_body_blocks_ready ? " [body blocks ready]" : " [body blocks blocked]");
    append_computed_cleanup_summary(
        lines,
        "loop render sequence",
        state.sequence_metadata_available ? "planned" : "absent",
        counts.str(),
        "(metadata only)"
    );

    for (auto index = std::size_t {0}; index < state.cleanup_owner_names.size(); ++index) {
        auto fields = std::ostringstream {};
        fields << "function " << indexed_name_or_unknown(state.enclosing_function_names, index);
        fields << " source " << indexed_name_or_unknown(state.source_type_names, index);
        fields << " element " << indexed_name_or_unknown(state.element_source_type_names, index);
        fields << " body " << indexed_name_or_unknown(state.body_block_names, index);
        append_computed_cleanup_detail(
            lines,
            "loop render sequence",
            state.cleanup_owner_names[index],
            fields.str(),
            "(metadata only)"
        );
    }

    return lines;
}

auto computed_dynamic_array_for_loop_exit_cleanup_state_report(
    pipeline::ComputedDynamicArrayForLoopExitCleanupState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "cleanups " << state.cleanup_count;
    counts << " snippets " << state.rendered_ir_snippet_count;
    counts << (state.cleanup_metadata_available ? " [metadata available]" : " [metadata missing]");
    counts << (state.all_cleanup_resumptions_ready ? " [cleanup resumptions ready]" :
        " [cleanup resumptions blocked]");
    append_computed_cleanup_summary(
        lines,
        "loop exit cleanup",
        state.cleanup_metadata_available ? "planned" : "absent",
        counts.str(),
        "(metadata only)"
    );

    for (auto index = std::size_t {0}; index < state.cleanup_owner_names.size(); ++index) {
        auto fields = std::ostringstream {};
        fields << "function " << indexed_name_or_unknown(state.enclosing_function_names, index);
        fields << " source " << indexed_name_or_unknown(state.source_type_names, index);
        fields << " element " << indexed_name_or_unknown(state.element_source_type_names, index);
        fields << " exit " << indexed_name_or_unknown(state.exit_block_names, index);
        fields << " from " << indexed_name_or_unknown(state.loop_entry_cleanup_owner_names, index);
        fields << " to " << indexed_name_or_unknown(state.loop_exit_cleanup_owner_names, index);
        fields << " operation " << indexed_name_or_unknown(state.cleanup_resumption_operation_names, index);
        append_computed_cleanup_detail(
            lines,
            "loop exit cleanup",
            state.cleanup_owner_names[index],
            fields.str(),
            "(metadata only)"
        );
    }

    return lines;
}

auto computed_dynamic_array_for_cleanup_transition_state_report(
    pipeline::ComputedDynamicArrayForCleanupTransitionState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "transitions " << state.transition_count;
    counts << (state.transition_metadata_available ? " [metadata available]" : " [metadata missing]");
    counts << (state.all_transitions_paired ? " [transitions paired]" : " [transitions blocked]");
    append_computed_cleanup_summary(
        lines,
        "cleanup transition",
        state.transition_metadata_available ? "planned" : "absent",
        counts.str(),
        "(metadata only)"
    );

    for (auto index = std::size_t {0}; index < state.cleanup_owner_names.size(); ++index) {
        auto fields = std::ostringstream {};
        fields << "function " << indexed_name_or_unknown(state.enclosing_function_names, index);
        fields << " source " << indexed_name_or_unknown(state.source_type_names, index);
        fields << " element " << indexed_name_or_unknown(state.element_source_type_names, index);
        fields << " acquire-from " << indexed_name_or_unknown(state.acquisition_source_owner_names, index);
        fields << " acquire-to " << indexed_name_or_unknown(state.acquisition_target_owner_names, index);
        fields << " acquire-operation " << indexed_name_or_unknown(state.acquisition_operation_names, index);
        fields << " resume-from " << indexed_name_or_unknown(state.resumption_source_owner_names, index);
        fields << " resume-to " << indexed_name_or_unknown(state.resumption_target_owner_names, index);
        fields << " resume-operation " << indexed_name_or_unknown(state.resumption_operation_names, index);
        append_computed_cleanup_detail(
            lines,
            "cleanup transition",
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
    counts << " cleanup-blockers " << state.cleanup_call_blocker_count;
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
        auto const blocked_reason = indexed_name_or_unknown(state.cleanup_calls_blocked_reasons, index);
        if (blocked_reason != "<unknown>" && !blocked_reason.empty()) {
            fields << " cleanup-blocked-reason " << blocked_reason;
        }
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

auto computed_inserted_cleanup_state_verification_report(
    pipeline::ComputedInsertedCleanupStateVerificationState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "verifications " << state.verification_count;
    counts << " paired " << state.paired_count;
    counts << " blocked " << state.blocked_count;
    counts << (state.from_metadata ? " [metadata-backed]" : " [metadata-missing]");
    counts << (state.all_paired ? " [handoff paired]" : " [handoff blocked]");
    counts << (state.all_cleanup_calls_enabled ? " [cleanup calls enabled]" : " [cleanup calls disabled]");
    append_computed_cleanup_summary(
        lines,
        "inserted cleanup state verification",
        state.all_paired ? "paired" : "blocked",
        counts.str(),
        "(inserted IR)"
    );

    for (auto index = std::size_t {0}; index < state.acquire_operation_names.size(); ++index) {
        auto fields = std::ostringstream {};
        fields << "acquire " << indexed_name_or_unknown(state.acquire_operation_names, index);
        fields << " resume " << indexed_name_or_unknown(state.resume_operation_names, index);
        fields << " acquire-from " << indexed_name_or_unknown(state.acquire_source_owner_names, index);
        fields << " acquire-to " << indexed_name_or_unknown(state.acquire_target_owner_names, index);
        fields << " resume-from " << indexed_name_or_unknown(state.resume_source_owner_names, index);
        fields << " resume-to " << indexed_name_or_unknown(state.resume_target_owner_names, index);
        append_computed_cleanup_detail(
            lines,
            "inserted cleanup state verification",
            indexed_name_or_unknown(state.resume_target_owner_names, index),
            fields.str(),
            "(inserted IR)"
        );
    }

    for (auto const& reason : state.blocked_reasons) {
        auto fields = std::ostringstream {};
        fields << "blocked-reason " << (reason.empty() ? "<unknown>" : reason);
        append_computed_cleanup_detail(
            lines,
            "inserted cleanup state verification",
            "<unknown>",
            fields.str(),
            "(inserted IR)"
        );
    }

    return lines;
}

auto computed_cleanup_call_blocker_summary_report(
    pipeline::ComputedInsertedCleanupHandoffState const& state
) -> std::vector<std::string> {
    if (state.cleanup_call_blocker_count == 0) {
        return {};
    }

    auto unique_reasons = std::set<std::string_view> {};
    for (auto const& reason : state.cleanup_calls_blocked_reasons) {
        if (!reason.empty()) {
            unique_reasons.insert(reason);
        }
    }

    auto output = std::ostringstream {};
    output << "computed DynamicArray cleanup call blockers blocked";
    output << " cleanup-blockers " << state.cleanup_call_blocker_count;
    if (!unique_reasons.empty()) {
        output << " blocker-reasons";
        for (auto reason : unique_reasons) {
            output << " [" << reason << "]";
        }
    }
    output << " (metadata only)";
    return {output.str()};
}

auto computed_cleanup_call_emission_gate_state_report(
    pipeline::ComputedCleanupCallEmissionGateState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "gates " << state.gate_count;
    counts << " ready " << state.ready_count;
    counts << " blocked " << state.blocked_count;
    counts << (state.all_state_verified ? " [inserted state verified]" : " [inserted state unverified]");
    counts << (state.all_cleanup_calls_enabled ? " [cleanup calls enabled]" : " [cleanup calls disabled]");
    append_computed_cleanup_summary(
        lines,
        "cleanup call emission gate",
        state.all_ready ? "ready" : "blocked",
        counts.str(),
        "(inserted IR)"
    );

    for (auto index = std::size_t {0}; index < state.cleanup_owner_names.size(); ++index) {
        auto fields = std::ostringstream {};
        fields << "acquire " << indexed_name_or_unknown(state.acquire_operation_names, index);
        fields << " resume " << indexed_name_or_unknown(state.resume_operation_names, index);
        append_computed_cleanup_detail(
            lines,
            "cleanup call emission gate",
            state.cleanup_owner_names[index],
            fields.str(),
            "(inserted IR)"
        );
    }

    return lines;
}

auto computed_cleanup_call_plan_state_report(
    pipeline::ComputedCleanupCallPlanRenderState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "plans " << state.plan_count;
    counts << " planned " << state.planned_count;
    counts << " renderable " << state.renderable_count;
    counts << " renders " << state.render_count;
    counts << (state.all_state_verified ? " [inserted state verified]" : " [inserted state unverified]");
    counts << (state.all_operands_proven ? " [cleanup operands proven]" : " [cleanup operands missing]");
    counts << (state.all_cleanup_calls_enabled ? " [cleanup calls enabled]" : " [cleanup calls disabled]");
    append_computed_cleanup_summary(
        lines,
        "cleanup call plan",
        state.planned_count > 0 ? "planned" : "blocked",
        counts.str(),
        "(inserted IR)"
    );

    for (auto index = std::size_t {0}; index < state.cleanup_owner_names.size(); ++index) {
        auto fields = std::ostringstream {};
        fields << "cleanup-operation " << indexed_name_or_unknown(state.cleanup_operation_names, index);
        fields << " data " << indexed_name_or_unknown(state.data_pointer_names, index);
        fields << " element-size " << indexed_name_or_unknown(state.element_size_bytes, index);
        fields << " capacity " << indexed_name_or_unknown(state.capacity_names, index);
        append_computed_cleanup_detail(
            lines,
            "cleanup call plan",
            state.cleanup_owner_names[index],
            fields.str(),
            "(inserted IR)"
        );
    }

    return lines;
}

auto computed_cleanup_call_render_state_report(
    pipeline::ComputedCleanupCallPlanRenderState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "renders " << state.render_count;
    counts << " renderable " << state.renderable_count;
    counts << " plans " << state.plan_count;
    counts << (state.all_state_verified ? " [inserted state verified]" : " [inserted state unverified]");
    counts << (state.all_operands_proven ? " [cleanup operands proven]" : " [cleanup operands missing]");
    counts << (state.all_renderable ? " [renderable]" : " [render blocked]");
    append_computed_cleanup_summary(
        lines,
        "cleanup call render",
        state.all_renderable ? "rendered" : "blocked",
        counts.str(),
        "(inserted IR)"
    );

    for (auto index = std::size_t {0}; index < state.cleanup_owner_names.size(); ++index) {
        auto fields = std::ostringstream {};
        fields << "cleanup-operation " << indexed_name_or_unknown(state.cleanup_operation_names, index);
        fields << " data " << indexed_name_or_unknown(state.data_pointer_names, index);
        fields << " element-size " << indexed_name_or_unknown(state.element_size_bytes, index);
        fields << " capacity " << indexed_name_or_unknown(state.capacity_names, index);
        append_computed_cleanup_detail(
            lines,
            "cleanup call render",
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

auto consumed_descriptor_finalization_state_report(
    pipeline::ConsumedDescriptorFinalizationState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "computed-descriptor-plans " << state.computed_descriptor_plan_count;
    counts << " emitted-finalization-plans " << state.emitted_finalization_plan_count;
    counts << " ready " << state.ready_plan_count;
    counts << " blocked " << state.blocked_plan_count;
    append_computed_cleanup_summary(
        lines,
        "consumed descriptor finalization plans",
        state.all_ready ? "ready" : "blocked",
        counts.str(),
        "(metadata only)"
    );

    for (auto index = std::size_t {0}; index < state.cleanup_owner_names.size(); ++index) {
        auto fields = std::ostringstream {};
        fields << "descriptor " << indexed_name_or_unknown(state.descriptor_storage_names, index);
        append_computed_cleanup_detail(
            lines,
            "consumed descriptor finalization plan",
            state.cleanup_owner_names[index],
            fields.str(),
            "(metadata only)"
        );
    }

    return lines;
}

auto computed_consumed_cleanup_descriptor_model_state_report(
    pipeline::ComputedConsumedCleanupDescriptorModelState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "models " << state.descriptor_model_count;
    counts << " ready " << state.ready_model_count;
    counts << " blocked " << state.blocked_model_count;
    counts << (state.all_finalization_ready ? " [finalization ready]" : " [finalization blocked]");
    append_computed_cleanup_summary(
        lines,
        "consumed cleanup descriptor models",
        state.all_finalization_ready ? "ready" : "blocked",
        counts.str(),
        "(metadata only)"
    );

    for (auto index = std::size_t {0}; index < state.cleanup_owner_names.size(); ++index) {
        auto fields = std::ostringstream {};
        fields << "function " << indexed_name_or_unknown(state.enclosing_function_names, index);
        fields << " source " << indexed_name_or_unknown(state.source_type_names, index);
        fields << " element " << indexed_name_or_unknown(state.element_source_type_names, index);
        fields << " descriptor " << indexed_name_or_unknown(state.descriptor_storage_names, index);
        fields << " cleanup-operation " << indexed_name_or_unknown(state.cleanup_operation_names, index);
        append_computed_cleanup_detail(
            lines,
            "consumed cleanup descriptor model",
            state.cleanup_owner_names[index],
            fields.str(),
            "(metadata only)"
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

auto computed_dynamic_array_for_production_emission_gate_state_report(
    pipeline::ComputedDynamicArrayForProductionEmissionGateState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "gates " << state.gate_count;
    counts << " snippets " << state.rendered_ir_snippet_count;
    counts << (state.gate_metadata_available ? " [metadata available]" : " [metadata missing]");
    counts << (state.all_ownership_ready ? " [ownership ready]" : " [ownership blocked]");
    counts << (state.all_loop_render_ready ? " [loop render ready]" : " [loop render blocked]");
    counts << (state.all_loop_cleanup_ownership_ready ?
        " [loop cleanup ownership ready]" : " [loop cleanup ownership blocked]");
    counts << (state.all_function_cleanup_resumption_ready ?
        " [function cleanup resumption ready]" : " [function cleanup resumption blocked]");
    counts << (state.all_exit_cleanup_ready ? " [exit cleanup ready]" : " [exit cleanup blocked]");
    counts << (state.all_production_sequences_planned ?
        " [production sequence planned]" : " [production sequence blocked]");
    counts << (state.any_production_emission_enabled ?
        " [production emission enabled]" : " [production emission disabled]");
    append_computed_cleanup_summary(
        lines,
        "production emission gate",
        state.gate_metadata_available ? "planned" : "absent",
        counts.str(),
        "(metadata only)"
    );

    for (auto const& owner_name : state.cleanup_owner_names) {
        append_computed_cleanup_detail(
            lines,
            "production emission gate",
            owner_name,
            {},
            "(metadata only)"
        );
    }

    return lines;
}

auto computed_dynamic_array_for_production_sequence_state_report(
    pipeline::ComputedDynamicArrayForProductionSequenceState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto counts = std::ostringstream {};
    counts << "sequences " << state.sequence_count;
    counts << " snippets " << state.rendered_ir_snippet_count;
    counts << " module-comments " << state.module_comment_line_count;
    counts << (state.sequence_metadata_available ? " [metadata available]" : " [metadata missing]");
    counts << (state.module_comments_emitted ? " [module comments emitted]" : " [module comments absent]");
    append_computed_cleanup_summary(
        lines,
        "production sequence",
        state.sequence_metadata_available ? "planned" : "absent",
        counts.str(),
        "(metadata only)"
    );

    for (auto const& owner_name : state.cleanup_owner_names) {
        append_computed_cleanup_detail(
            lines,
            "production sequence",
            owner_name,
            {},
            "(metadata only)"
        );
    }

    return lines;
}

auto dynamic_array_cleanup_production_readiness_state_report(
    pipeline::DynamicArrayCleanupProductionReadiness const& state
) -> std::vector<std::string> {
    return {pipeline::format_dynamic_array_cleanup_production_readiness(state)};
}

auto dynamic_array_cleanup_emission_capability_state_report(
    pipeline::DynamicArrayCleanupEmissionCapabilityState const& state
) -> std::vector<std::string> {
    if (!state.capability_metadata_available) {
        return {};
    }
    auto const status = [](bool value) {
        return value ? "ok" : "missing";
    };
    auto details = std::ostringstream {};
    details << "dynamic array cleanup emission capability ";
    details << (state.proven ? "proven" : "blocked");
    if (!state.cleanup_pairs.empty()) {
        details << " cleanup-pairs";
        for (auto const& cleanup_pair : state.cleanup_pairs) {
            details << " [" << cleanup_pair << "]";
        }
    }
    if (!state.cleanup_operation_names.empty()) {
        details << " cleanup-operations";
        for (auto const& cleanup_operation_name : state.cleanup_operation_names) {
            details << " [" << cleanup_operation_name << "]";
        }
    }
    if (!state.cleanup_owner_names.empty()) {
        details << " cleanup-owners";
        for (auto const& cleanup_owner_name : state.cleanup_owner_names) {
            details << " [" << cleanup_owner_name << "]";
        }
    }
    if (!state.element_drop_pairs.empty()) {
        details << " element-drop-pairs";
        for (auto const& element_drop_pair : state.element_drop_pairs) {
            details << " [" << element_drop_pair << "]";
        }
    }
    if (!state.missing_element_drop_pairs.empty()) {
        details << " missing-element-drop-pairs";
        for (auto const& missing_element_drop_pair : state.missing_element_drop_pairs) {
            details << " [" << missing_element_drop_pair << "]";
        }
    }
    details << " [emission " << status(state.emission_enabled) << "]";
    details << " [descriptor storage " << status(state.descriptor_storage_bound) << "]";
    details << " [sequence verification " << status(state.sequence_verified) << "]";
    details << " [element cleanup " << status(state.element_cleanup_authorized_or_not_required) << "]";
    details << " [descriptor deallocation " << status(state.descriptor_deallocation_authorized) << "]";
    details << " (metadata only)";

    auto lines = std::vector<std::string> {};
    if (state.function_symbol_names.empty()) {
        lines.push_back(details.str());
        return lines;
    }
    for (auto const& function_symbol_name : state.function_symbol_names) {
        auto line = std::ostringstream {};
        line << "function " << function_symbol_name << ' ' << details.str();
        lines.push_back(line.str());
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

auto aggregate_projection_access_plan_state_report(
    pipeline::AggregateProjectionAccessPlanState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    for (auto index = std::size_t {0}; index < state.plan_count; ++index) {
        auto line = std::ostringstream {};
        line << "function " << indexed_name_or_unknown(state.function_symbol_names, index);
        line << " aggregate projection access intent ";
        if (index < state.intents.size()) {
            line << lowering::render_aggregate_projection_access_intent(state.intents[index]);
        } else {
            line << "<unknown>";
        }
        line << " status ";
        if (index < state.statuses.size()) {
            line << lowering::render_aggregate_projection_access_status(state.statuses[index]);
        } else {
            line << "<unknown>";
        }
        line << " binding " << indexed_name_or_unknown(state.binding_names, index);
        line << " source " << indexed_name_or_unknown(state.source_type_names, index);
        line << " receiver ";
        if (index < state.receiver_projections.size()) {
            line << (state.receiver_projections[index] ? "true" : "false");
        } else {
            line << "<unknown>";
        }
        auto const diagnostic = indexed_name_or_unknown(state.diagnostics, index);
        if (diagnostic != "<unknown>" && !diagnostic.empty()) {
            line << " diagnostic " << diagnostic;
        }
        lines.push_back(line.str());
    }
    return lines;
}

}  // namespace orison::driver
