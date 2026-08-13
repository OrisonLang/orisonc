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

auto dotted_path(std::vector<std::string> const& path) -> std::string {
    if (path.empty()) {
        return "none";
    }
    auto output = std::ostringstream {};
    for (auto index = std::size_t {0}; index < path.size(); ++index) {
        if (index != 0) {
            output << ".";
        }
        output << path[index];
    }
    return output.str();
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
    auto member_plan = runtime_indexed_member_cleanup_plan(owner);
    auto member_proof = runtime_indexed_member_cleanup_proof(member_plan);
    auto member_sketch = runtime_indexed_member_cleanup_emission_sketch(member_proof);
    auto member_targets = runtime_indexed_member_cleanup_targets(member_sketch);
    auto member_gate = runtime_indexed_member_cleanup_emission_gate(member_sketch, member_targets);
    auto member_insertion_plan =
        runtime_indexed_member_cleanup_ir_insertion_plan(member_gate, member_targets);
    auto member_composition_plan =
        runtime_indexed_member_cleanup_ir_composition_plan(member_insertion_plan);
    auto member_cfg_slice = runtime_indexed_member_cleanup_cfg_slice(member_composition_plan);
    emission_plan.function_predecessor_block_name = std::move(function_predecessor_block_name);
    state.runtime_indexed_partial_owners.push_back(std::move(owner));
    state.runtime_indexed_cleanup_skip_plans.push_back(std::move(plan));
    state.runtime_indexed_cleanup_proof_gates.push_back(std::move(gate));
    state.runtime_indexed_cleanup_emission_sketches.push_back(std::move(sketch));
    state.runtime_indexed_cleanup_capabilities.push_back(std::move(capability));
    state.runtime_indexed_cleanup_emission_plans.push_back(std::move(emission_plan));
    state.runtime_indexed_member_cleanup_plans.push_back(std::move(member_plan));
    state.runtime_indexed_member_cleanup_proofs.push_back(std::move(member_proof));
    state.runtime_indexed_member_cleanup_emission_sketches.push_back(std::move(member_sketch));
    state.runtime_indexed_member_cleanup_targets.push_back(std::move(member_targets));
    state.runtime_indexed_member_cleanup_emission_gates.push_back(std::move(member_gate));
    state.runtime_indexed_member_cleanup_ir_insertion_plans.push_back(std::move(member_insertion_plan));
    state.runtime_indexed_member_cleanup_ir_composition_plans.push_back(std::move(member_composition_plan));
    state.runtime_indexed_member_cleanup_cfg_slices.push_back(std::move(member_cfg_slice));
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
        .moved_member_path = owner.moved_member_path,
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
    auto member_cleanup_plan = runtime_indexed_member_cleanup_plan(
        RuntimeIndexedPartialOwner {
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
            .moved_member_path = plan.moved_member_path,
            .cleanup_strategy = plan.cleanup_operation,
            .source_line = plan.source_line,
        }
    );
    auto member_cleanup_proof = runtime_indexed_member_cleanup_proof(member_cleanup_plan);
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
        .member_cleanup_proof_ready = member_cleanup_proof.prerequisites_met,
        .member_cleanup_blocks_whole_element = member_cleanup_proof.whole_element_cleanup_blocked,
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
           << " member-proof-ready " << (gate.member_cleanup_proof_ready ? "true" : "false")
           << " member-blocks-whole-element "
           << (gate.member_cleanup_blocks_whole_element ? "true" : "false")
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

auto runtime_indexed_member_cleanup_plan(
    RuntimeIndexedPartialOwner const& owner
) -> RuntimeIndexedMemberCleanupPlan {
    auto owner_known = !owner.owner_name.empty();
    auto index_known = !owner.index_expression_text.empty() && owner.index_expression_text != "<computed>";
    auto element_type_known = !owner.element_source_type_name.empty();
    auto moved_type_known = !owner.moved_source_type_name.empty();
    auto moved_member_path_known = !owner.moved_member_path.empty();
    auto cleanup_element_matches_move = element_type_known &&
        moved_type_known &&
        owner.element_source_type_name == owner.moved_source_type_name;
    auto member_granular_cleanup_required =
        moved_member_path_known && !cleanup_element_matches_move;
    return RuntimeIndexedMemberCleanupPlan {
        .owner_name = owner.owner_name,
        .index_expression_text = owner.index_expression_text,
        .element_source_type_name = owner.element_source_type_name,
        .moved_source_type_name = owner.moved_source_type_name,
        .moved_member_path = owner.moved_member_path,
        .owner_known = owner_known,
        .index_known = index_known,
        .element_type_known = element_type_known,
        .moved_type_known = moved_type_known,
        .moved_member_path_known = moved_member_path_known,
        .cleanup_element_matches_move = cleanup_element_matches_move,
        .member_granular_cleanup_required = member_granular_cleanup_required,
        .prerequisites_met = owner_known &&
            index_known &&
            element_type_known &&
            moved_type_known &&
            moved_member_path_known &&
            cleanup_element_matches_move,
        .production_enabled = false,
    };
}

auto runtime_indexed_member_cleanup_plan_report(
    RuntimeIndexedMemberCleanupPlan const& plan
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup owner " << plan.owner_name
           << " index " << plan.index_expression_text
           << " element " << plan.element_source_type_name
           << " moved " << plan.moved_source_type_name
           << " member-path " << dotted_path(plan.moved_member_path)
           << " owner-known " << (plan.owner_known ? "true" : "false")
           << " index-known " << (plan.index_known ? "true" : "false")
           << " element-type-known " << (plan.element_type_known ? "true" : "false")
           << " moved-type-known " << (plan.moved_type_known ? "true" : "false")
           << " member-path-known " << (plan.moved_member_path_known ? "true" : "false")
           << " cleanup-element-matches-move "
           << (plan.cleanup_element_matches_move ? "true" : "false")
           << " member-granular-required "
           << (plan.member_granular_cleanup_required ? "true" : "false")
           << " prerequisites " << (plan.prerequisites_met ? "met" : "missing")
           << " production " << (plan.production_enabled ? "enabled" : "disabled");
    return report.str();
}

auto runtime_indexed_member_cleanup_proof(
    RuntimeIndexedMemberCleanupPlan const& plan
) -> RuntimeIndexedMemberCleanupProof {
    auto plan_ready = plan.owner_known &&
        plan.index_known &&
        plan.element_type_known &&
        plan.moved_type_known &&
        plan.moved_member_path_known;
    auto member_scope_proven = plan_ready && plan.member_granular_cleanup_required;
    auto whole_element_cleanup_blocked = member_scope_proven && !plan.cleanup_element_matches_move;
    return RuntimeIndexedMemberCleanupProof {
        .owner_name = plan.owner_name,
        .index_expression_text = plan.index_expression_text,
        .element_source_type_name = plan.element_source_type_name,
        .moved_source_type_name = plan.moved_source_type_name,
        .moved_member_path = plan.moved_member_path,
        .plan_ready = plan_ready,
        .whole_element_cleanup_matches_move = plan.cleanup_element_matches_move,
        .member_cleanup_required = plan.member_granular_cleanup_required,
        .member_scope_proven = member_scope_proven,
        .whole_element_cleanup_blocked = whole_element_cleanup_blocked,
        .prerequisites_met = member_scope_proven && whole_element_cleanup_blocked,
        .production_enabled = false,
    };
}

auto runtime_indexed_member_cleanup_proof_report(
    RuntimeIndexedMemberCleanupProof const& proof
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup proof owner " << proof.owner_name
           << " index " << proof.index_expression_text
           << " element " << proof.element_source_type_name
           << " moved " << proof.moved_source_type_name
           << " member-path " << dotted_path(proof.moved_member_path)
           << " plan-ready " << (proof.plan_ready ? "true" : "false")
           << " whole-element-cleanup-matches-move "
           << (proof.whole_element_cleanup_matches_move ? "true" : "false")
           << " member-cleanup-required " << (proof.member_cleanup_required ? "true" : "false")
           << " member-scope-proven " << (proof.member_scope_proven ? "true" : "false")
           << " whole-element-cleanup-blocked "
           << (proof.whole_element_cleanup_blocked ? "true" : "false")
           << " prerequisites " << (proof.prerequisites_met ? "met" : "missing")
           << " production " << (proof.production_enabled ? "enabled" : "disabled");
    return report.str();
}

auto runtime_indexed_member_cleanup_emission_sketch(
    RuntimeIndexedMemberCleanupProof const& proof
) -> RuntimeIndexedMemberCleanupEmissionSketch {
    auto snippets = std::vector<std::string> {};
    if (proof.prerequisites_met) {
        auto member_path = dotted_path(proof.moved_member_path);
        snippets.push_back("load-length " + proof.owner_name);
        snippets.push_back("loop-cleanup-index 0..<length");
        snippets.push_back("skip-cleanup-index " + proof.index_expression_text);
        snippets.push_back(
            "drop-live-member-siblings " + proof.owner_name + "[cleanup_index] except " + member_path
        );
        snippets.push_back(
            "preserve-moved-member " + proof.owner_name + "[" + proof.index_expression_text + "]." + member_path
        );
        snippets.push_back("deallocate-owner " + proof.owner_name);
    }
    return RuntimeIndexedMemberCleanupEmissionSketch {
        .owner_name = proof.owner_name,
        .index_expression_text = proof.index_expression_text,
        .element_source_type_name = proof.element_source_type_name,
        .moved_source_type_name = proof.moved_source_type_name,
        .moved_member_path = proof.moved_member_path,
        .snippets = std::move(snippets),
        .proof_ready = proof.prerequisites_met,
        .report_only = true,
        .production_emission_enabled = false,
    };
}

auto runtime_indexed_member_cleanup_emission_sketch_report(
    RuntimeIndexedMemberCleanupEmissionSketch const& sketch
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup emission-sketch owner " << sketch.owner_name
           << " index " << sketch.index_expression_text
           << " element " << sketch.element_source_type_name
           << " moved " << sketch.moved_source_type_name
           << " member-path " << dotted_path(sketch.moved_member_path)
           << " snippets " << sketch.snippets.size()
           << " proof-ready " << (sketch.proof_ready ? "true" : "false")
           << " report-only " << (sketch.report_only ? "true" : "false")
           << " production-emission "
           << (sketch.production_emission_enabled ? "enabled" : "disabled");
    for (auto const& snippet : sketch.snippets) {
        report << " snippet " << snippet;
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_targets(
    RuntimeIndexedMemberCleanupEmissionSketch const& sketch
) -> std::vector<RuntimeIndexedMemberCleanupTarget> {
    if (!sketch.proof_ready || sketch.snippets.size() != 6 || sketch.moved_member_path.empty()) {
        return {};
    }

    auto member_path = dotted_path(sketch.moved_member_path);
    return {
        RuntimeIndexedMemberCleanupTarget {
            .owner_name = sketch.owner_name,
            .index_expression_text = sketch.index_expression_text,
            .element_source_type_name = sketch.element_source_type_name,
            .moved_source_type_name = sketch.moved_source_type_name,
            .moved_member_path = sketch.moved_member_path,
            .cleanup_operation = "drop-live-member-siblings",
            .drop_metadata_symbol_name = "__orison_member_cleanup." +
                sketch.element_source_type_name + ".except." + member_path,
            .metadata_ready = true,
            .production_enabled = false,
        },
    };
}

auto runtime_indexed_member_cleanup_target_report(
    RuntimeIndexedMemberCleanupTarget const& target
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup target owner " << target.owner_name
           << " index " << target.index_expression_text
           << " element " << target.element_source_type_name
           << " moved " << target.moved_source_type_name
           << " member-path " << dotted_path(target.moved_member_path)
           << " operation " << target.cleanup_operation
           << " drop-metadata " << target.drop_metadata_symbol_name
           << " metadata " << (target.metadata_ready ? "ready" : "missing")
           << " production " << (target.production_enabled ? "enabled" : "disabled");
    return report.str();
}

auto runtime_indexed_member_cleanup_emission_gate(
    RuntimeIndexedMemberCleanupEmissionSketch const& sketch,
    std::vector<RuntimeIndexedMemberCleanupTarget> const& targets
) -> RuntimeIndexedMemberCleanupEmissionGate {
    auto sketch_ready = sketch.proof_ready &&
        sketch.report_only &&
        !sketch.production_emission_enabled &&
        sketch.snippets.size() == 6;
    auto member_drop_metadata_ready = sketch_ready &&
        !targets.empty() &&
        std::ranges::all_of(
            targets,
            [](RuntimeIndexedMemberCleanupTarget const& target) {
                return target.metadata_ready &&
                    !target.cleanup_operation.empty() &&
                    !target.drop_metadata_symbol_name.empty();
            }
        );
    auto blockers = std::vector<std::string> {};
    if (!sketch_ready) {
        blockers.push_back("member-cleanup-sketch");
    }
    if (!member_drop_metadata_ready) {
        blockers.push_back("member-drop-metadata");
    }
    blockers.push_back("member-cleanup-ir-insertion");
    return RuntimeIndexedMemberCleanupEmissionGate {
        .owner_name = sketch.owner_name,
        .index_expression_text = sketch.index_expression_text,
        .element_source_type_name = sketch.element_source_type_name,
        .moved_source_type_name = sketch.moved_source_type_name,
        .moved_member_path = sketch.moved_member_path,
        .blockers = std::move(blockers),
        .sketch_ready = sketch_ready,
        .member_drop_metadata_ready = member_drop_metadata_ready,
        .ir_insertion_ready = false,
        .prerequisites_met = false,
        .production_enabled = false,
    };
}

auto runtime_indexed_member_cleanup_emission_gate_report(
    RuntimeIndexedMemberCleanupEmissionGate const& gate
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup emission-gate owner " << gate.owner_name
           << " index " << gate.index_expression_text
           << " element " << gate.element_source_type_name
           << " moved " << gate.moved_source_type_name
           << " member-path " << dotted_path(gate.moved_member_path)
           << " sketch-ready " << (gate.sketch_ready ? "true" : "false")
           << " member-drop-metadata "
           << (gate.member_drop_metadata_ready ? "ready" : "missing")
           << " ir-insertion " << (gate.ir_insertion_ready ? "ready" : "missing")
           << " prerequisites " << (gate.prerequisites_met ? "met" : "missing")
           << " production " << (gate.production_enabled ? "enabled" : "disabled")
           << " blockers " << gate.blockers.size();
    for (auto const& blocker : gate.blockers) {
        report << " blocker " << blocker;
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_ir_insertion_plan(
    RuntimeIndexedMemberCleanupEmissionGate const& gate,
    std::vector<RuntimeIndexedMemberCleanupTarget> const& targets
) -> RuntimeIndexedMemberCleanupIrInsertionPlan {
    auto target_metadata_ready = gate.member_drop_metadata_ready && !targets.empty();
    auto insertion_points_named = target_metadata_ready && !gate.owner_name.empty();
    auto preview_operations = std::vector<std::string> {};
    if (insertion_points_named) {
        preview_operations = {
            "anchor-owner-final-cleanup " + gate.owner_name,
            "split-member-cleanup-entry " + gate.owner_name + ".member_cleanup.entry",
            "branch-skip-moved-index " + gate.index_expression_text,
            "call-member-cleanup-target " + targets.front().drop_metadata_symbol_name,
            "preserve-moved-member-path " + dotted_path(gate.moved_member_path),
            "resume-owner-deallocation " + gate.owner_name + ".member_cleanup.exit",
        };
    }
    return RuntimeIndexedMemberCleanupIrInsertionPlan {
        .owner_name = gate.owner_name,
        .index_expression_text = gate.index_expression_text,
        .element_source_type_name = gate.element_source_type_name,
        .moved_source_type_name = gate.moved_source_type_name,
        .moved_member_path = gate.moved_member_path,
        .insertion_anchor = insertion_points_named ? gate.owner_name + ".final-cleanup" : "",
        .entry_block_name = insertion_points_named ? gate.owner_name + ".member_cleanup.entry" : "",
        .skip_block_name = insertion_points_named ? gate.owner_name + ".member_cleanup.skip_moved" : "",
        .sibling_drop_block_name = insertion_points_named ? gate.owner_name + ".member_cleanup.drop_siblings" : "",
        .preserve_block_name = insertion_points_named ? gate.owner_name + ".member_cleanup.preserve_moved" : "",
        .exit_block_name = insertion_points_named ? gate.owner_name + ".member_cleanup.exit" : "",
        .preview_operations = std::move(preview_operations),
        .target_metadata_ready = target_metadata_ready,
        .insertion_points_named = insertion_points_named,
        .report_only = true,
        .production_enabled = false,
    };
}

auto runtime_indexed_member_cleanup_ir_insertion_plan_report(
    RuntimeIndexedMemberCleanupIrInsertionPlan const& plan
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup ir-insertion-plan owner " << plan.owner_name
           << " index " << plan.index_expression_text
           << " element " << plan.element_source_type_name
           << " moved " << plan.moved_source_type_name
           << " member-path " << dotted_path(plan.moved_member_path)
           << " anchor " << (plan.insertion_anchor.empty() ? "missing" : plan.insertion_anchor)
           << " entry " << (plan.entry_block_name.empty() ? "missing" : plan.entry_block_name)
           << " skip " << (plan.skip_block_name.empty() ? "missing" : plan.skip_block_name)
           << " sibling-drop "
           << (plan.sibling_drop_block_name.empty() ? "missing" : plan.sibling_drop_block_name)
           << " preserve " << (plan.preserve_block_name.empty() ? "missing" : plan.preserve_block_name)
           << " exit " << (plan.exit_block_name.empty() ? "missing" : plan.exit_block_name)
           << " target-metadata " << (plan.target_metadata_ready ? "ready" : "missing")
           << " insertion-points " << (plan.insertion_points_named ? "named" : "missing")
           << " report-only " << (plan.report_only ? "true" : "false")
           << " production " << (plan.production_enabled ? "enabled" : "disabled")
           << " preview-operations " << plan.preview_operations.size();
    for (auto const& operation : plan.preview_operations) {
        report << " operation " << operation;
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_ir_composition_plan(
    RuntimeIndexedMemberCleanupIrInsertionPlan const& plan
) -> RuntimeIndexedMemberCleanupIrCompositionPlan {
    auto insertion_plan_ready = plan.insertion_points_named && plan.target_metadata_ready;
    auto block_topology_ready =
        insertion_plan_ready &&
        !plan.insertion_anchor.empty() &&
        !plan.entry_block_name.empty() &&
        !plan.skip_block_name.empty() &&
        !plan.sibling_drop_block_name.empty() &&
        !plan.preserve_block_name.empty() &&
        !plan.exit_block_name.empty() &&
        plan.entry_block_name != plan.skip_block_name &&
        plan.entry_block_name != plan.sibling_drop_block_name &&
        plan.entry_block_name != plan.preserve_block_name &&
        plan.entry_block_name != plan.exit_block_name &&
        plan.skip_block_name != plan.sibling_drop_block_name &&
        plan.skip_block_name != plan.preserve_block_name &&
        plan.skip_block_name != plan.exit_block_name &&
        plan.sibling_drop_block_name != plan.preserve_block_name &&
        plan.sibling_drop_block_name != plan.exit_block_name &&
        plan.preserve_block_name != plan.exit_block_name;
    auto preview_operations_ready = block_topology_ready && plan.preview_operations.size() == 6;
    auto topology_edges = std::vector<std::string> {};
    if (block_topology_ready) {
        topology_edges = {
            plan.insertion_anchor + " -> " + plan.entry_block_name,
            plan.entry_block_name + " -> " + plan.skip_block_name,
            plan.entry_block_name + " -> " + plan.sibling_drop_block_name,
            plan.skip_block_name + " -> " + plan.preserve_block_name,
            plan.sibling_drop_block_name + " -> " + plan.preserve_block_name,
            plan.preserve_block_name + " -> " + plan.exit_block_name,
        };
    }

    return RuntimeIndexedMemberCleanupIrCompositionPlan {
        .owner_name = plan.owner_name,
        .index_expression_text = plan.index_expression_text,
        .element_source_type_name = plan.element_source_type_name,
        .moved_source_type_name = plan.moved_source_type_name,
        .moved_member_path = plan.moved_member_path,
        .insertion_anchor = plan.insertion_anchor,
        .entry_block_name = plan.entry_block_name,
        .skip_block_name = plan.skip_block_name,
        .sibling_drop_block_name = plan.sibling_drop_block_name,
        .preserve_block_name = plan.preserve_block_name,
        .exit_block_name = plan.exit_block_name,
        .topology_edges = std::move(topology_edges),
        .insertion_plan_ready = insertion_plan_ready,
        .block_topology_ready = block_topology_ready,
        .preview_operations_ready = preview_operations_ready,
        .report_only = true,
        .production_enabled = false,
    };
}

auto runtime_indexed_member_cleanup_ir_composition_plan_report(
    RuntimeIndexedMemberCleanupIrCompositionPlan const& plan
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup ir-composition-plan owner " << plan.owner_name
           << " index " << plan.index_expression_text
           << " element " << plan.element_source_type_name
           << " moved " << plan.moved_source_type_name
           << " member-path " << dotted_path(plan.moved_member_path)
           << " anchor " << (plan.insertion_anchor.empty() ? "missing" : plan.insertion_anchor)
           << " entry " << (plan.entry_block_name.empty() ? "missing" : plan.entry_block_name)
           << " skip " << (plan.skip_block_name.empty() ? "missing" : plan.skip_block_name)
           << " sibling-drop "
           << (plan.sibling_drop_block_name.empty() ? "missing" : plan.sibling_drop_block_name)
           << " preserve " << (plan.preserve_block_name.empty() ? "missing" : plan.preserve_block_name)
           << " exit " << (plan.exit_block_name.empty() ? "missing" : plan.exit_block_name)
           << " insertion-plan " << (plan.insertion_plan_ready ? "ready" : "missing")
           << " block-topology " << (plan.block_topology_ready ? "ready" : "missing")
           << " preview-operations " << (plan.preview_operations_ready ? "ready" : "missing")
           << " report-only " << (plan.report_only ? "true" : "false")
           << " production " << (plan.production_enabled ? "enabled" : "disabled")
           << " topology-edges " << plan.topology_edges.size();
    for (auto const& edge : plan.topology_edges) {
        report << " edge " << edge;
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_cfg_slice(
    RuntimeIndexedMemberCleanupIrCompositionPlan const& plan
) -> RuntimeIndexedMemberCleanupCfgSlice {
    auto composition_ready =
        plan.block_topology_ready && plan.preview_operations_ready && !plan.topology_edges.empty();
    auto cfg_lines = std::vector<std::string> {};
    if (composition_ready) {
        cfg_lines = {
            "; report-only runtime-index member cleanup anchor " + plan.insertion_anchor + "\n",
            plan.entry_block_name + ":\n",
            "; report-only compare cleanup_index with " + plan.index_expression_text + "\n",
            "  ; br moved index -> " + plan.skip_block_name + "\n",
            "  ; br live sibling -> " + plan.sibling_drop_block_name + "\n",
            plan.skip_block_name + ":\n",
            "  ; preserve moved member " + plan.owner_name + "[" + plan.index_expression_text +
                "]." + dotted_path(plan.moved_member_path) + "\n",
            "  ; br label %" + plan.preserve_block_name + "\n",
            plan.sibling_drop_block_name + ":\n",
            "  ; call member cleanup for " + plan.element_source_type_name +
                " except " + dotted_path(plan.moved_member_path) + "\n",
            "  ; br label %" + plan.preserve_block_name + "\n",
            plan.preserve_block_name + ":\n",
            "  ; br label %" + plan.exit_block_name + "\n",
            plan.exit_block_name + ":\n",
            "  ; resume owner cleanup " + plan.owner_name + "\n",
        };
    }

    return RuntimeIndexedMemberCleanupCfgSlice {
        .owner_name = plan.owner_name,
        .index_expression_text = plan.index_expression_text,
        .element_source_type_name = plan.element_source_type_name,
        .moved_source_type_name = plan.moved_source_type_name,
        .moved_member_path = plan.moved_member_path,
        .insertion_anchor = plan.insertion_anchor,
        .entry_block_name = plan.entry_block_name,
        .skip_block_name = plan.skip_block_name,
        .sibling_drop_block_name = plan.sibling_drop_block_name,
        .preserve_block_name = plan.preserve_block_name,
        .exit_block_name = plan.exit_block_name,
        .cfg_lines = std::move(cfg_lines),
        .composition_ready = composition_ready,
        .slice_rendered = composition_ready,
        .report_only = true,
        .production_enabled = false,
    };
}

auto runtime_indexed_member_cleanup_cfg_slice_report(
    RuntimeIndexedMemberCleanupCfgSlice const& slice
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup cfg-slice owner " << slice.owner_name
           << " index " << slice.index_expression_text
           << " element " << slice.element_source_type_name
           << " moved " << slice.moved_source_type_name
           << " member-path " << dotted_path(slice.moved_member_path)
           << " anchor " << (slice.insertion_anchor.empty() ? "missing" : slice.insertion_anchor)
           << " entry " << (slice.entry_block_name.empty() ? "missing" : slice.entry_block_name)
           << " skip " << (slice.skip_block_name.empty() ? "missing" : slice.skip_block_name)
           << " sibling-drop "
           << (slice.sibling_drop_block_name.empty() ? "missing" : slice.sibling_drop_block_name)
           << " preserve " << (slice.preserve_block_name.empty() ? "missing" : slice.preserve_block_name)
           << " exit " << (slice.exit_block_name.empty() ? "missing" : slice.exit_block_name)
           << " composition " << (slice.composition_ready ? "ready" : "missing")
           << " slice " << (slice.slice_rendered ? "rendered" : "missing")
           << " report-only " << (slice.report_only ? "true" : "false")
           << " production " << (slice.production_enabled ? "enabled" : "disabled")
           << " cfg-lines " << slice.cfg_lines.size();
    for (auto const& line : slice.cfg_lines) {
        auto trimmed_line = line;
        if (!trimmed_line.empty() && trimmed_line.back() == '\n') {
            trimmed_line.pop_back();
        }
        report << " line " << trimmed_line;
    }
    return report.str();
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
    auto const index_operand = plan.index_operand_value.empty()
        ? "%" + plan.index_expression_text
        : plan.index_operand_value;
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
            ", " + index_operand + "\n",
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
    });
    if (!plan.descriptor_owner_ready) {
        lines.push_back(
            "  store " + plan.element_llvm_type_name + " zeroinitializer, ptr " +
            plan.element_address_name + "\n"
        );
    }
    lines.insert(lines.end(), {
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
        state.runtime_indexed_cleanup_emission_plans.empty() &&
        state.runtime_indexed_member_cleanup_plans.empty() &&
        state.runtime_indexed_member_cleanup_proofs.empty() &&
        state.runtime_indexed_member_cleanup_emission_sketches.empty() &&
        state.runtime_indexed_member_cleanup_targets.empty() &&
        state.runtime_indexed_member_cleanup_emission_gates.empty() &&
        state.runtime_indexed_member_cleanup_ir_insertion_plans.empty() &&
        state.runtime_indexed_member_cleanup_ir_composition_plans.empty() &&
        state.runtime_indexed_member_cleanup_cfg_slices.empty()) {
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
    for (auto const& plan : state.runtime_indexed_member_cleanup_plans) {
        report.push_back(runtime_indexed_member_cleanup_plan_report(plan));
    }
    for (auto const& proof : state.runtime_indexed_member_cleanup_proofs) {
        report.push_back(runtime_indexed_member_cleanup_proof_report(proof));
    }
    for (auto const& sketch : state.runtime_indexed_member_cleanup_emission_sketches) {
        report.push_back(runtime_indexed_member_cleanup_emission_sketch_report(sketch));
    }
    for (auto const& targets : state.runtime_indexed_member_cleanup_targets) {
        for (auto const& target : targets) {
            report.push_back(runtime_indexed_member_cleanup_target_report(target));
        }
    }
    for (auto const& gate : state.runtime_indexed_member_cleanup_emission_gates) {
        report.push_back(runtime_indexed_member_cleanup_emission_gate_report(gate));
    }
    for (auto const& plan : state.runtime_indexed_member_cleanup_ir_insertion_plans) {
        report.push_back(runtime_indexed_member_cleanup_ir_insertion_plan_report(plan));
    }
    for (auto const& plan : state.runtime_indexed_member_cleanup_ir_composition_plans) {
        report.push_back(runtime_indexed_member_cleanup_ir_composition_plan_report(plan));
    }
    for (auto const& slice : state.runtime_indexed_member_cleanup_cfg_slices) {
        report.push_back(runtime_indexed_member_cleanup_cfg_slice_report(slice));
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
                merged.runtime_indexed_cleanup_emission_plans ||
            branch_states[index].runtime_indexed_member_cleanup_plans !=
                merged.runtime_indexed_member_cleanup_plans ||
            branch_states[index].runtime_indexed_member_cleanup_proofs !=
                merged.runtime_indexed_member_cleanup_proofs ||
            branch_states[index].runtime_indexed_member_cleanup_emission_sketches !=
                merged.runtime_indexed_member_cleanup_emission_sketches ||
            branch_states[index].runtime_indexed_member_cleanup_targets !=
                merged.runtime_indexed_member_cleanup_targets ||
            branch_states[index].runtime_indexed_member_cleanup_emission_gates !=
                merged.runtime_indexed_member_cleanup_emission_gates ||
            branch_states[index].runtime_indexed_member_cleanup_ir_insertion_plans !=
                merged.runtime_indexed_member_cleanup_ir_insertion_plans ||
            branch_states[index].runtime_indexed_member_cleanup_ir_composition_plans !=
                merged.runtime_indexed_member_cleanup_ir_composition_plans ||
            branch_states[index].runtime_indexed_member_cleanup_cfg_slices !=
                merged.runtime_indexed_member_cleanup_cfg_slices) {
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
