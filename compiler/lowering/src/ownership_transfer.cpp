#include "orison/lowering/ownership_transfer.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/source_type_queries.hpp"

namespace orison::lowering {
namespace {

auto is_owned_transfer_source_type_impl(
    std::string_view source_type_name,
    LoweringContext const& context,
    std::unordered_set<std::string>& visiting
) -> bool {
    if (source_type_name.empty() || is_scalar_or_nonowning_source_type(source_type_name)) {
        return false;
    }

    auto source_type_key = std::string {source_type_name};
    if (!visiting.insert(source_type_key).second) {
        return true;
    }

    if (dynamic_array_element_source_type_name(source_type_name).has_value()) {
        visiting.erase(source_type_key);
        return true;
    }

    if (auto array_element_type = array_element_source_type_name(source_type_name)) {
        auto owns = is_owned_transfer_source_type_impl(*array_element_type, context, visiting);
        visiting.erase(source_type_key);
        return owns;
    }

    if (auto maybe_payload_type = maybe_payload_source_type_name(source_type_name)) {
        auto owns = is_owned_transfer_source_type_impl(*maybe_payload_type, context, visiting);
        visiting.erase(source_type_key);
        return owns;
    }

    if (context.records.contains(source_type_key)) {
        visiting.erase(source_type_key);
        return true;
    }

    if (context.choices.contains(source_type_key)) {
        visiting.erase(source_type_key);
        return true;
    }

    visiting.erase(source_type_key);
    return true;
}

}  // namespace

auto mark_owned_binding_consumed(
    OwnershipTransferState& state,
    std::string binding_name
) -> void {
    state.consumed_owned_bindings.insert(std::move(binding_name));
}

auto record_runtime_indexed_partial_owner(
    OwnershipTransferState& state,
    RuntimeIndexedPartialOwner owner,
    std::string function_predecessor_block_name,
    bool production_cleanup_emission_enabled
) -> void {
    auto plan = runtime_indexed_cleanup_skip_plan(owner);
    auto gate = runtime_indexed_cleanup_proof_gate(plan);
    auto sketch = runtime_indexed_cleanup_emission_sketch(gate);
    auto capability = runtime_indexed_cleanup_capability(
        gate,
        sketch,
        production_cleanup_emission_enabled
    );
    auto emission_plan = runtime_indexed_cleanup_emission_plan(
        capability,
        sketch,
        production_cleanup_emission_enabled
    );
    emission_plan.function_predecessor_block_name = std::move(function_predecessor_block_name);
    state.runtime_indexed_partial_owners.push_back(std::move(owner));
    state.runtime_indexed_cleanup_skip_plans.push_back(std::move(plan));
    state.runtime_indexed_cleanup_proof_gates.push_back(std::move(gate));
    state.runtime_indexed_cleanup_emission_sketches.push_back(std::move(sketch));
    state.runtime_indexed_cleanup_capabilities.push_back(std::move(capability));
    state.runtime_indexed_cleanup_emission_plans.push_back(std::move(emission_plan));
}

auto runtime_indexed_partial_owner_report(
    RuntimeIndexedPartialOwner const& owner
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index partial owner owner " << owner.owner_name
           << " index " << owner.index_expression_text
           << " element " << owner.element_source_type_name
           << " moved " << owner.moved_source_type_name
           << " cleanup " << owner.cleanup_strategy
           << " constructor-move " << (owner.constructor_move_enabled ? "enabled" : "disabled");
    return report.str();
}

auto runtime_indexed_cleanup_skip_plan(
    RuntimeIndexedPartialOwner const& owner
) -> RuntimeIndexedCleanupSkipPlan {
    return RuntimeIndexedCleanupSkipPlan {
        .owner_name = owner.owner_name,
        .index_expression_text = owner.index_expression_text,
        .element_source_type_name = owner.element_source_type_name,
        .element_llvm_type_name = owner.element_llvm_type_name,
        .owner_llvm_type_name = owner.owner_llvm_type_name,
        .owner_address_name = owner.owner_address_name,
        .owner_address_ir_lines = owner.owner_address_ir_lines,
        .static_length_value = owner.static_length_value,
        .element_size_value = owner.element_size_value,
        .moved_source_type_name = owner.moved_source_type_name,
        .cleanup_operation = owner.cleanup_strategy,
        .production_cleanup_enabled = false,
        .source_line = owner.source_line,
    };
}

auto runtime_indexed_cleanup_skip_plan_report(
    RuntimeIndexedCleanupSkipPlan const& plan
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index cleanup-skip plan owner " << plan.owner_name
           << " index " << plan.index_expression_text
           << " element " << plan.element_source_type_name
           << " moved " << plan.moved_source_type_name
           << " operation " << plan.cleanup_operation
           << " production-cleanup " << (plan.production_cleanup_enabled ? "enabled" : "disabled");
    return report.str();
}

auto runtime_indexed_cleanup_proof_gate(
    RuntimeIndexedCleanupSkipPlan const& plan
) -> RuntimeIndexedCleanupProofGate {
    auto owner_known = !plan.owner_name.empty();
    auto index_known = !plan.index_expression_text.empty() && plan.index_expression_text != "<computed>";
    auto type_match = !plan.element_source_type_name.empty() &&
        !plan.element_llvm_type_name.empty() &&
        plan.element_source_type_name == plan.moved_source_type_name;
    auto operation_supported = plan.cleanup_operation == "skip-moved-element";
    return RuntimeIndexedCleanupProofGate {
        .owner_name = plan.owner_name,
        .index_expression_text = plan.index_expression_text,
        .element_source_type_name = plan.element_source_type_name,
        .element_llvm_type_name = plan.element_llvm_type_name,
        .owner_llvm_type_name = plan.owner_llvm_type_name,
        .owner_address_name = plan.owner_address_name,
        .owner_address_ir_lines = plan.owner_address_ir_lines,
        .static_length_value = plan.static_length_value,
        .element_size_value = plan.element_size_value,
        .moved_source_type_name = plan.moved_source_type_name,
        .cleanup_operation = plan.cleanup_operation,
        .owner_known = owner_known,
        .index_known = index_known,
        .type_match = type_match,
        .operation_supported = operation_supported,
        .prerequisites_met = owner_known && index_known && type_match && operation_supported,
        .lowering_enabled = false,
        .source_line = plan.source_line,
    };
}

auto runtime_indexed_cleanup_proof_gate_report(
    RuntimeIndexedCleanupProofGate const& gate
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index cleanup proof owner " << gate.owner_name
           << " index " << gate.index_expression_text
           << " element " << gate.element_source_type_name
           << " moved " << gate.moved_source_type_name
           << " operation " << gate.cleanup_operation
           << " owner-known " << (gate.owner_known ? "true" : "false")
           << " index-known " << (gate.index_known ? "true" : "false")
           << " type-match " << (gate.type_match ? "true" : "false")
           << " operation-supported " << (gate.operation_supported ? "true" : "false")
           << " prerequisites " << (gate.prerequisites_met ? "met" : "missing")
           << " lowering " << (gate.lowering_enabled ? "enabled" : "disabled");
    return report.str();
}

auto runtime_indexed_cleanup_emission_sketch(
    RuntimeIndexedCleanupProofGate const& gate
) -> RuntimeIndexedCleanupEmissionSketch {
    auto snippets = std::vector<std::string> {};
    if (gate.prerequisites_met) {
        snippets.push_back("load-length " + gate.owner_name);
        snippets.push_back("loop-cleanup-index 0..<length");
        snippets.push_back("skip-cleanup-index " + gate.index_expression_text);
        snippets.push_back(
            "drop-live-element " + gate.owner_name + "[cleanup_index] as " + gate.element_source_type_name
        );
        snippets.push_back("deallocate-owner " + gate.owner_name);
    }
    return RuntimeIndexedCleanupEmissionSketch {
        .owner_name = gate.owner_name,
        .index_expression_text = gate.index_expression_text,
        .element_source_type_name = gate.element_source_type_name,
        .element_llvm_type_name = gate.element_llvm_type_name,
        .owner_llvm_type_name = gate.owner_llvm_type_name,
        .owner_address_name = gate.owner_address_name,
        .owner_address_ir_lines = gate.owner_address_ir_lines,
        .static_length_value = gate.static_length_value,
        .element_size_value = gate.element_size_value,
        .snippets = std::move(snippets),
        .report_only = true,
        .production_emission_enabled = false,
        .source_line = gate.source_line,
    };
}

auto runtime_indexed_cleanup_emission_sketch_report(
    RuntimeIndexedCleanupEmissionSketch const& sketch
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index cleanup emission-sketch owner " << sketch.owner_name
           << " index " << sketch.index_expression_text
           << " element " << sketch.element_source_type_name
           << " snippets " << sketch.snippets.size()
           << " report-only " << (sketch.report_only ? "true" : "false")
           << " production-emission " << (sketch.production_emission_enabled ? "enabled" : "disabled");
    for (auto const& snippet : sketch.snippets) {
        report << " snippet " << snippet;
    }
    return report.str();
}

auto runtime_indexed_cleanup_capability(
    RuntimeIndexedCleanupProofGate const& gate,
    RuntimeIndexedCleanupEmissionSketch const& sketch,
    bool production_cleanup_emission_enabled
) -> RuntimeIndexedCleanupCapability {
    auto proof_ready = gate.prerequisites_met && !gate.lowering_enabled;
    auto sketch_ready = sketch.report_only &&
        !sketch.production_emission_enabled &&
        !sketch.snippets.empty();
    return RuntimeIndexedCleanupCapability {
        .owner_name = gate.owner_name,
        .index_expression_text = gate.index_expression_text,
        .element_source_type_name = gate.element_source_type_name,
        .element_llvm_type_name = gate.element_llvm_type_name,
        .owner_llvm_type_name = gate.owner_llvm_type_name,
        .owner_address_name = gate.owner_address_name,
        .owner_address_ir_lines = gate.owner_address_ir_lines,
        .static_length_value = gate.static_length_value,
        .element_size_value = gate.element_size_value,
        .proof_ready = proof_ready,
        .sketch_ready = sketch_ready,
        .prerequisites_ready = proof_ready && sketch_ready,
        .production_enabled = production_cleanup_emission_enabled && proof_ready && sketch_ready,
        .source_line = gate.source_line,
    };
}

auto runtime_indexed_cleanup_capability_report(
    RuntimeIndexedCleanupCapability const& capability
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index cleanup capability owner " << capability.owner_name
           << " index " << capability.index_expression_text
           << " element " << capability.element_source_type_name
           << " proof-ready " << (capability.proof_ready ? "true" : "false")
           << " sketch-ready " << (capability.sketch_ready ? "true" : "false")
           << " prerequisites " << (capability.prerequisites_ready ? "ready" : "blocked")
           << " production " << (capability.production_enabled ? "enabled" : "disabled");
    return report.str();
}

auto runtime_indexed_cleanup_emission_plan(
    RuntimeIndexedCleanupCapability const& capability,
    RuntimeIndexedCleanupEmissionSketch const& sketch,
    bool production_cleanup_emission_enabled
) -> RuntimeIndexedCleanupEmissionPlan {
    auto plan = RuntimeIndexedCleanupEmissionPlan {
        .owner_name = capability.owner_name,
        .index_expression_text = capability.index_expression_text,
        .element_source_type_name = capability.element_source_type_name,
        .element_llvm_type_name = capability.element_llvm_type_name,
        .owner_llvm_type_name = capability.owner_llvm_type_name,
        .owner_address_name = capability.owner_address_name,
        .owner_address_ir_lines = capability.owner_address_ir_lines,
        .static_length_value = capability.static_length_value,
        .element_size_value = capability.element_size_value,
        .source_line = capability.source_line,
        .prerequisites_ready = capability.prerequisites_ready && sketch.snippets.size() == 5,
        .production_gate_requested = production_cleanup_emission_enabled,
        .production_enabled = production_cleanup_emission_enabled && capability.production_enabled,
    };
    if (!plan.prerequisites_ready) {
        return plan;
    }

    plan.operation_names = {
        "load-length",
        "loop-cleanup-index",
        "skip-cleanup-index",
        "drop-live-element",
        "deallocate-owner",
    };
    plan.length_load_planned = true;
    plan.loop_planned = true;
    plan.skip_planned = true;
    plan.live_element_drop_planned = true;
    plan.owner_deallocation_planned = true;
    plan.operation_count = plan.operation_names.size();
    plan.comment_ir_preview_lines = {
        "; runtime-index cleanup preview load-length owner " + plan.owner_name + "\n",
        "; runtime-index cleanup preview loop-cleanup-index owner " + plan.owner_name + "\n",
        "; runtime-index cleanup preview skip-cleanup-index " + plan.index_expression_text + "\n",
        "; runtime-index cleanup preview drop-live-element owner " + plan.owner_name +
            " element " + plan.element_source_type_name + "\n",
        "; runtime-index cleanup preview deallocate-owner " + plan.owner_name + "\n",
    };
    plan.comment_ir_preview_line_count = plan.comment_ir_preview_lines.size();
    auto fixed_array_owner_ready = !plan.owner_llvm_type_name.empty() &&
        !plan.owner_address_name.empty() &&
        !plan.static_length_value.empty();
    auto descriptor_owner_ready =
        plan.owner_llvm_type_name == dynamic_array_descriptor_llvm_type() &&
        !plan.owner_address_name.empty() &&
        !plan.element_size_value.empty();
    if (plan.production_enabled && (fixed_array_owner_ready || descriptor_owner_ready)) {
        plan.ir_plan = RuntimeIndexedCleanupIrPlan {
            .owner_name = plan.owner_name,
            .index_expression_text = plan.index_expression_text,
            .element_source_type_name = plan.element_source_type_name,
            .element_llvm_type_name = plan.element_llvm_type_name,
            .owner_llvm_type_name = plan.owner_llvm_type_name,
            .owner_address_name = plan.owner_address_name,
            .owner_address_ir_lines = plan.owner_address_ir_lines,
            .static_length_value = plan.static_length_value,
            .element_size_value = plan.element_size_value,
            .source_line = plan.source_line,
            .descriptor_value_name = "%" + plan.owner_name + ".runtime_cleanup.descriptor",
            .descriptor_data_value_name = "%" + plan.owner_name + ".runtime_cleanup.data",
            .descriptor_capacity_value_name = "%" + plan.owner_name + ".runtime_cleanup.capacity",
            .entry_block_name = plan.owner_name + ".runtime_cleanup.entry",
            .length_value_name = "%" + plan.owner_name + ".runtime_cleanup.length",
            .condition_block_name = plan.owner_name + ".runtime_cleanup.condition",
            .cleanup_index_name = "%" + plan.owner_name + ".runtime_cleanup.index",
            .bounds_check_name = "%" + plan.owner_name + ".runtime_cleanup.more",
            .live_check_block_name = plan.owner_name + ".runtime_cleanup.check_live",
            .skip_check_name = "%" + plan.owner_name + ".runtime_cleanup.skip_moved",
            .skip_block_name = plan.owner_name + ".runtime_cleanup.skip",
            .drop_block_name = plan.owner_name + ".runtime_cleanup.drop",
            .element_address_name = "%" + plan.owner_name + ".runtime_cleanup.element.addr",
            .drop_callee_name = "__orison_drop." + plan.element_source_type_name,
            .continue_block_name = plan.owner_name + ".runtime_cleanup.continue",
            .next_index_name = "%" + plan.owner_name + ".runtime_cleanup.next_index",
            .exit_block_name = plan.owner_name + ".runtime_cleanup.exit",
            .deallocate_callee_name = "__orison_dynamic_array_deallocate",
            .owner_address_ready = true,
            .static_length_ready = fixed_array_owner_ready,
            .descriptor_owner_ready = descriptor_owner_ready,
            .owner_deallocation_required = descriptor_owner_ready,
            .labels_ready = true,
            .operands_ready = true,
            .calls_ready = true,
            .complete = true,
        };
        plan.gated_ir_slice_lines = render_runtime_indexed_cleanup_ir_plan(plan.ir_plan);
        plan.length_load_slice_lowerable = true;
        plan.loop_block_slice_lowerable = true;
        plan.skip_branch_slice_lowerable = true;
        plan.live_element_drop_slice_lowerable = true;
        plan.cleanup_tail_slice_lowerable = true;
        plan.gated_ir_slice_line_count = plan.gated_ir_slice_lines.size();
    }
    return plan;
}

auto render_runtime_indexed_cleanup_ir_plan(
    RuntimeIndexedCleanupIrPlan const& plan
) -> std::vector<std::string> {
    if (!plan.complete) {
        return {};
    }

    auto lines = std::vector<std::string> {};
    lines.insert(
        lines.end(),
        plan.owner_address_ir_lines.begin(),
        plan.owner_address_ir_lines.end()
    );
    if (plan.descriptor_owner_ready) {
        lines.insert(lines.end(), {
            "  " + plan.descriptor_value_name + " = load " + plan.owner_llvm_type_name +
                ", ptr " + plan.owner_address_name + "\n",
            "  " + plan.descriptor_data_value_name + " = extractvalue " + plan.owner_llvm_type_name +
                " " + plan.descriptor_value_name + ", 0\n",
            "  " + plan.length_value_name + " = extractvalue " + plan.owner_llvm_type_name +
                " " + plan.descriptor_value_name + ", 1\n",
            "  " + plan.descriptor_capacity_value_name + " = extractvalue " + plan.owner_llvm_type_name +
                " " + plan.descriptor_value_name + ", 2\n",
        });
    } else if (!plan.static_length_ready) {
        lines.push_back("  " + plan.length_value_name + " = load i64, ptr %" + plan.owner_name + ".length\n");
    }
    auto const length_operand = plan.static_length_ready
        ? plan.static_length_value
        : plan.length_value_name;
    lines.insert(lines.end(), {
        "  br label %" + plan.condition_block_name + "\n",
        plan.condition_block_name + ":\n",
        "  " + plan.cleanup_index_name + " = phi i64 [ 0, %" + plan.entry_block_name +
            " ], [ " + plan.next_index_name + ", %" +
            plan.continue_block_name + " ]\n",
        "  " + plan.bounds_check_name + " = icmp ult i64 " + plan.cleanup_index_name +
            ", " + length_operand + "\n",
        "  br i1 " + plan.bounds_check_name + ", label %" + plan.live_check_block_name +
            ", label %" + plan.exit_block_name + "\n",
        plan.live_check_block_name + ":\n",
        "  " + plan.skip_check_name + " = icmp eq i64 " + plan.cleanup_index_name +
            ", %" + plan.index_expression_text + "\n",
        "  br i1 " + plan.skip_check_name + ", label %" + plan.skip_block_name +
            ", label %" + plan.drop_block_name + "\n",
        plan.skip_block_name + ":\n",
        "  br label %" + plan.continue_block_name + "\n",
        plan.drop_block_name + ":\n",
        plan.descriptor_owner_ready
            ? "  " + plan.element_address_name + " = getelementptr " +
                plan.element_llvm_type_name + ", ptr " + plan.descriptor_data_value_name +
                ", i64 " + plan.cleanup_index_name + "\n"
            : "  " + plan.element_address_name + " = getelementptr " +
                plan.owner_llvm_type_name + ", ptr " + plan.owner_address_name +
                ", i64 0, i64 " + plan.cleanup_index_name + "\n",
        "  call void @" + plan.drop_callee_name + "(ptr " + plan.element_address_name + ")\n",
        "  br label %" + plan.continue_block_name + "\n",
        plan.continue_block_name + ":\n",
        "  " + plan.next_index_name + " = add i64 " + plan.cleanup_index_name + ", 1\n",
        "  br label %" + plan.condition_block_name + "\n",
        plan.exit_block_name + ":\n",
    });
    if (plan.owner_deallocation_required) {
        lines.push_back(
            "  call void @" + plan.deallocate_callee_name + "(ptr " +
            plan.descriptor_data_value_name + ", i64 " + plan.element_size_value +
            ", i64 " + plan.descriptor_capacity_value_name + ")\n"
        );
    }
    return lines;
}

auto runtime_indexed_cleanup_emission_plan_report(
    RuntimeIndexedCleanupEmissionPlan const& plan
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index cleanup emission-plan owner " << plan.owner_name
           << " index " << plan.index_expression_text
           << " element " << plan.element_source_type_name
           << " operations " << plan.operation_count
           << " prerequisites " << (plan.prerequisites_ready ? "ready" : "blocked")
           << " production-gate " << (plan.production_gate_requested ? "requested" : "blocked")
           << " production " << (plan.production_enabled ? "enabled" : "disabled")
           << " length-load " << (plan.length_load_planned ? "planned" : "missing")
           << " length-load-slice " << (plan.length_load_slice_lowerable ? "lowerable" : "blocked")
           << " loop " << (plan.loop_planned ? "planned" : "missing")
           << " loop-block-slice " << (plan.loop_block_slice_lowerable ? "lowerable" : "blocked")
           << " skip " << (plan.skip_planned ? "planned" : "missing")
           << " skip-branch-slice " << (plan.skip_branch_slice_lowerable ? "lowerable" : "blocked")
           << " live-drop " << (plan.live_element_drop_planned ? "planned" : "missing")
           << " live-drop-slice " << (plan.live_element_drop_slice_lowerable ? "lowerable" : "blocked")
           << " deallocate " << (plan.owner_deallocation_planned ? "planned" : "missing")
           << " cleanup-tail-slice " << (plan.cleanup_tail_slice_lowerable ? "lowerable" : "blocked")
           << " structured-ir-plan " << (plan.ir_plan.complete ? "complete" : "blocked")
           << " comment-ir-preview-lines " << plan.comment_ir_preview_line_count
           << " gated-ir-slice-lines " << plan.gated_ir_slice_line_count;
    for (auto const& operation_name : plan.operation_names) {
        report << " operation " << operation_name;
    }
    return report.str();
}

auto runtime_indexed_cleanup_audit_report(
    OwnershipTransferState const& state
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    if (state.runtime_indexed_partial_owners.empty() &&
        state.runtime_indexed_cleanup_skip_plans.empty() &&
        state.runtime_indexed_cleanup_proof_gates.empty() &&
        state.runtime_indexed_cleanup_emission_sketches.empty() &&
        state.runtime_indexed_cleanup_capabilities.empty() &&
        state.runtime_indexed_cleanup_emission_plans.empty()) {
        return {"runtime-index cleanup audit: no runtime-index cleanup metadata"};
    }

    report.push_back(
        "runtime-index cleanup audit entries " +
        std::to_string(state.runtime_indexed_cleanup_capabilities.size())
    );
    for (auto const& owner : state.runtime_indexed_partial_owners) {
        report.push_back(runtime_indexed_partial_owner_report(owner));
    }
    for (auto const& plan : state.runtime_indexed_cleanup_skip_plans) {
        report.push_back(runtime_indexed_cleanup_skip_plan_report(plan));
    }
    for (auto const& gate : state.runtime_indexed_cleanup_proof_gates) {
        report.push_back(runtime_indexed_cleanup_proof_gate_report(gate));
    }
    for (auto const& sketch : state.runtime_indexed_cleanup_emission_sketches) {
        report.push_back(runtime_indexed_cleanup_emission_sketch_report(sketch));
    }
    for (auto const& capability : state.runtime_indexed_cleanup_capabilities) {
        report.push_back(runtime_indexed_cleanup_capability_report(capability));
    }
    for (auto const& plan : state.runtime_indexed_cleanup_emission_plans) {
        report.push_back(runtime_indexed_cleanup_emission_plan_report(plan));
    }
    return report;
}

auto is_owned_binding_consumed(
    OwnershipTransferState const& state,
    std::string_view binding_name
) -> bool {
    return state.consumed_owned_bindings.contains(std::string(binding_name));
}

auto consumed_owned_binding_or_descendant_name(
    OwnershipTransferState const& state,
    std::string_view binding_name
) -> std::optional<std::string> {
    if (is_owned_binding_consumed(state, binding_name)) {
        return std::string {binding_name};
    }

    auto descendant_prefix = std::string {binding_name};
    descendant_prefix += ".";
    auto matches = std::vector<std::string> {};
    for (auto const& consumed_name : state.consumed_owned_bindings) {
        if (consumed_name.starts_with(descendant_prefix)) {
            matches.push_back(consumed_name);
        }
    }
    if (matches.empty()) {
        return std::nullopt;
    }
    std::ranges::sort(matches);
    return matches.front();
}

auto consumed_owned_descendant_names(
    std::vector<OwnershipTransferState> const& states,
    std::string_view owner_name
) -> std::vector<std::string> {
    if (owner_name.empty()) {
        return {};
    }

    auto descendant_prefix = std::string {owner_name};
    descendant_prefix += ".";
    auto names = std::vector<std::string> {};
    auto seen = std::unordered_set<std::string> {};
    for (auto const& state : states) {
        for (auto const& consumed_name : state.consumed_owned_bindings) {
            if (consumed_name.starts_with(descendant_prefix) && seen.insert(consumed_name).second) {
                names.push_back(consumed_name);
            }
        }
    }
    std::ranges::sort(names);
    return names;
}

auto normalize_consumed_owned_descendants(
    std::vector<OwnershipTransferState>& states,
    std::vector<std::string> const& consumed_descendant_names
) -> void {
    for (auto& state : states) {
        for (auto const& name : consumed_descendant_names) {
            mark_owned_binding_consumed(state, name);
        }
    }
}

auto merge_ownership_transfer_states(
    std::vector<OwnershipTransferState> const& branch_states
) -> std::optional<OwnershipTransferState> {
    if (branch_states.empty()) {
        return OwnershipTransferState {};
    }

    auto merged = branch_states.front();
    for (auto index = std::size_t {1}; index < branch_states.size(); ++index) {
        if (branch_states[index].consumed_owned_bindings != merged.consumed_owned_bindings ||
            branch_states[index].runtime_indexed_partial_owners != merged.runtime_indexed_partial_owners ||
            branch_states[index].runtime_indexed_cleanup_skip_plans !=
                merged.runtime_indexed_cleanup_skip_plans ||
            branch_states[index].runtime_indexed_cleanup_proof_gates !=
                merged.runtime_indexed_cleanup_proof_gates ||
            branch_states[index].runtime_indexed_cleanup_emission_sketches !=
                merged.runtime_indexed_cleanup_emission_sketches ||
            branch_states[index].runtime_indexed_cleanup_capabilities !=
                merged.runtime_indexed_cleanup_capabilities ||
            branch_states[index].runtime_indexed_cleanup_emission_plans !=
                merged.runtime_indexed_cleanup_emission_plans) {
            return std::nullopt;
        }
    }
    return merged;
}

auto owned_binding_member_name(
    std::string_view owner_name,
    std::string_view member_name
) -> std::string {
    auto binding_name = std::string {owner_name};
    binding_name += ".";
    binding_name += member_name;
    return binding_name;
}

auto is_owned_transfer_source_type(
    std::string_view source_type_name,
    LoweringContext const& context
) -> bool {
    auto visiting = std::unordered_set<std::string> {};
    return is_owned_transfer_source_type_impl(source_type_name, context, visiting);
}

auto owned_record_field_transfer(
    std::string_view owner_name,
    std::string_view owner_source_type_name,
    std::string_view field_name,
    LoweringContext const& context
) -> std::optional<OwnedAggregateMemberTransfer> {
    auto field_names = std::vector<std::string> {std::string {field_name}};
    return owned_record_member_path_transfer(owner_name, owner_source_type_name, field_names, context);
}

auto owned_record_member_path_transfer(
    std::string_view owner_name,
    std::string_view owner_source_type_name,
    std::span<std::string const> field_names,
    LoweringContext const& context
) -> std::optional<OwnedAggregateMemberTransfer> {
    if (field_names.empty()) {
        return std::nullopt;
    }

    auto current_source_type_name = std::string {owner_source_type_name};
    auto member_name = std::string {};
    for (auto const& field_name : field_names) {
        auto record = context.records.find(current_source_type_name);
        if (record == context.records.end()) {
            return std::nullopt;
        }

        auto const* field = find_record_field(record->second, field_name);
        if (field == nullptr) {
            return std::nullopt;
        }

        if (!member_name.empty()) {
            member_name += ".";
        }
        member_name += field_name;
        current_source_type_name = field->source_type_name;
    }

    if (!is_owned_transfer_source_type(current_source_type_name, context)) {
        return std::nullopt;
    }

    return OwnedAggregateMemberTransfer {
        .binding_name = owned_binding_member_name(owner_name, member_name),
        .owner_name = std::string {owner_name},
        .member_name = std::move(member_name),
        .source_type_name = std::move(current_source_type_name),
    };
}

auto owned_choice_payload_transfer(
    std::string_view owner_name,
    std::string_view choice_source_type_name,
    std::string_view variant_name,
    std::string_view payload_name,
    LoweringContext const& context
) -> std::optional<OwnedAggregateMemberTransfer> {
    auto choice = context.choices.find(std::string {choice_source_type_name});
    if (choice == context.choices.end()) {
        return std::nullopt;
    }

    for (auto const& variant : choice->second.variants) {
        if (variant.name != variant_name) {
            continue;
        }
        for (auto const& payload : variant.payloads) {
            if (payload.name != payload_name ||
                !is_owned_transfer_source_type(payload.source_type_name, context)) {
                continue;
            }
            auto member_name = std::string {variant_name};
            member_name += ".";
            member_name += payload_name;
            return OwnedAggregateMemberTransfer {
                .binding_name = owned_binding_member_name(owner_name, member_name),
                .owner_name = std::string {owner_name},
                .member_name = std::move(member_name),
                .source_type_name = payload.source_type_name,
            };
        }
    }
    return std::nullopt;
}

}  // namespace orison::lowering
