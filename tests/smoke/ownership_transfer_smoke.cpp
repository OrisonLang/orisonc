#include "orison/lowering/ownership_transfer.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/maybe_switch_lowering.hpp"
#include "orison/lowering/string_constants.hpp"

#include <cassert>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

int main() {
    auto transfers = orison::lowering::OwnershipTransferState {};
    assert(!orison::lowering::is_owned_binding_consumed(transfers, "items"));

    orison::lowering::mark_owned_binding_consumed(transfers, "items");
    assert(orison::lowering::is_owned_binding_consumed(transfers, "items"));
    assert(!orison::lowering::is_owned_binding_consumed(transfers, "other"));
    assert(orison::lowering::consumed_owned_binding_or_descendant_name(transfers, "items") == "items");
    assert(!orison::lowering::consumed_owned_binding_or_descendant_name(transfers, "other").has_value());

    orison::lowering::mark_owned_binding_consumed(transfers, "maybe.Some.value");
    assert(
        orison::lowering::consumed_owned_binding_or_descendant_name(transfers, "maybe") ==
        "maybe.Some.value"
    );

    auto matching = orison::lowering::merge_ownership_transfer_states({
        transfers,
        transfers,
    });
    assert(matching.has_value());
    assert(orison::lowering::is_owned_binding_consumed(*matching, "items"));

    auto different = orison::lowering::OwnershipTransferState {};
    orison::lowering::mark_owned_binding_consumed(different, "other");
    auto mismatched = orison::lowering::merge_ownership_transfer_states({
        std::move(transfers),
        std::move(different),
    });
    assert(!mismatched.has_value());

    auto empty = orison::lowering::merge_ownership_transfer_states({});
    assert(empty.has_value());
    assert(!orison::lowering::is_owned_binding_consumed(*empty, "items"));
    auto empty_runtime_indexed_audit = orison::lowering::runtime_indexed_cleanup_audit_report(*empty);
    assert(empty_runtime_indexed_audit.size() == 1);
    assert(
        empty_runtime_indexed_audit.front() ==
        "runtime-index cleanup audit: no runtime-index cleanup metadata"
    );

    auto runtime_indexed = orison::lowering::OwnershipTransferState {};
    orison::lowering::record_runtime_indexed_partial_owner(
        runtime_indexed,
        orison::lowering::RuntimeIndexedPartialOwner {
            .owner_name = "holder.items",
            .index_expression_text = "index",
            .element_source_type_name = "Inner",
            .element_llvm_type_name = "%record.Inner",
            .owner_llvm_type_name = "[2 x %record.Inner]",
            .owner_address_name = "%holder.items.runtime_cleanup.owner.addr",
            .owner_address_ir_lines = {
                "  %holder.items.runtime_cleanup.owner.addr = getelementptr %record.Holder, ptr "
                "%holder.addr, i32 0, i32 0\n",
            },
            .static_length_value = "2",
            .moved_source_type_name = "Inner",
            .cleanup_strategy = "skip-moved-element",
            .constructor_move_enabled = false,
        }
    );
    assert(runtime_indexed.runtime_indexed_partial_owners.size() == 1);
    assert(runtime_indexed.runtime_indexed_cleanup_skip_plans.size() == 1);
    assert(runtime_indexed.runtime_indexed_cleanup_proof_gates.size() == 1);
    assert(runtime_indexed.runtime_indexed_cleanup_emission_sketches.size() == 1);
    assert(runtime_indexed.runtime_indexed_cleanup_capabilities.size() == 1);
    assert(runtime_indexed.runtime_indexed_cleanup_emission_plans.size() == 1);
    assert(
        orison::lowering::runtime_indexed_partial_owner_report(
            runtime_indexed.runtime_indexed_partial_owners.front()
        ) ==
        "runtime-index partial owner owner holder.items index index element Inner moved Inner "
        "cleanup skip-moved-element constructor-move disabled"
    );
    assert(
        orison::lowering::runtime_indexed_cleanup_skip_plan_report(
            runtime_indexed.runtime_indexed_cleanup_skip_plans.front()
        ) ==
        "runtime-index cleanup-skip plan owner holder.items index index element Inner moved Inner "
        "operation skip-moved-element production-cleanup disabled"
    );
    assert(runtime_indexed.runtime_indexed_cleanup_proof_gates.front().prerequisites_met);
    assert(!runtime_indexed.runtime_indexed_cleanup_proof_gates.front().lowering_enabled);
    assert(
        orison::lowering::runtime_indexed_cleanup_proof_gate_report(
            runtime_indexed.runtime_indexed_cleanup_proof_gates.front()
        ) ==
        "runtime-index cleanup proof owner holder.items index index element Inner moved Inner "
        "operation skip-moved-element owner-known true index-known true type-match true "
        "operation-supported true prerequisites met lowering disabled"
    );
    auto missing_index_plan = runtime_indexed.runtime_indexed_cleanup_skip_plans.front();
    missing_index_plan.index_expression_text = "<computed>";
    auto missing_index_gate = orison::lowering::runtime_indexed_cleanup_proof_gate(missing_index_plan);
    assert(!missing_index_gate.prerequisites_met);
    assert(!missing_index_gate.index_known);
    auto missing_index_sketch = orison::lowering::runtime_indexed_cleanup_emission_sketch(
        missing_index_gate
    );
    assert(missing_index_sketch.snippets.empty());
    assert(!missing_index_sketch.production_emission_enabled);
    assert(
        runtime_indexed.runtime_indexed_cleanup_emission_sketches.front().snippets.size() == 5
    );
    assert(
        orison::lowering::runtime_indexed_cleanup_emission_sketch_report(
            runtime_indexed.runtime_indexed_cleanup_emission_sketches.front()
        ) ==
        "runtime-index cleanup emission-sketch owner holder.items index index element Inner snippets 5 "
        "report-only true production-emission disabled snippet load-length holder.items "
        "snippet loop-cleanup-index 0..<length snippet skip-cleanup-index index "
        "snippet drop-live-element holder.items[cleanup_index] as Inner "
        "snippet deallocate-owner holder.items"
    );
    assert(runtime_indexed.runtime_indexed_cleanup_capabilities.front().prerequisites_ready);
    assert(!runtime_indexed.runtime_indexed_cleanup_capabilities.front().production_enabled);
    assert(
        orison::lowering::runtime_indexed_cleanup_capability_report(
            runtime_indexed.runtime_indexed_cleanup_capabilities.front()
        ) ==
        "runtime-index cleanup capability owner holder.items index index element Inner "
        "proof-ready true sketch-ready true prerequisites ready production disabled"
    );
    assert(runtime_indexed.runtime_indexed_cleanup_emission_plans.front().prerequisites_ready);
    assert(!runtime_indexed.runtime_indexed_cleanup_emission_plans.front().production_gate_requested);
    assert(!runtime_indexed.runtime_indexed_cleanup_emission_plans.front().production_enabled);
    assert(runtime_indexed.runtime_indexed_cleanup_emission_plans.front().operation_count == 5);
    assert(runtime_indexed.runtime_indexed_cleanup_emission_plans.front().operation_names.size() == 5);
    assert(
        runtime_indexed.runtime_indexed_cleanup_emission_plans.front()
            .comment_ir_preview_line_count == 5
    );
    assert(
        runtime_indexed.runtime_indexed_cleanup_emission_plans.front()
            .comment_ir_preview_lines.size() == 5
    );
    assert(
        runtime_indexed.runtime_indexed_cleanup_emission_plans.front()
            .comment_ir_preview_lines[0] ==
        "; runtime-index cleanup preview load-length owner holder.items\n"
    );
    assert(
        runtime_indexed.runtime_indexed_cleanup_emission_plans.front()
            .comment_ir_preview_lines[2] ==
        "; runtime-index cleanup preview skip-cleanup-index index\n"
    );
    assert(
        runtime_indexed.runtime_indexed_cleanup_emission_plans.front()
            .comment_ir_preview_lines[4] ==
        "; runtime-index cleanup preview deallocate-owner holder.items\n"
    );
    assert(runtime_indexed.runtime_indexed_cleanup_emission_plans.front().length_load_planned);
    assert(!runtime_indexed.runtime_indexed_cleanup_emission_plans.front().length_load_slice_lowerable);
    assert(runtime_indexed.runtime_indexed_cleanup_emission_plans.front().loop_planned);
    assert(!runtime_indexed.runtime_indexed_cleanup_emission_plans.front().loop_block_slice_lowerable);
    assert(runtime_indexed.runtime_indexed_cleanup_emission_plans.front().skip_planned);
    assert(!runtime_indexed.runtime_indexed_cleanup_emission_plans.front().skip_branch_slice_lowerable);
    assert(runtime_indexed.runtime_indexed_cleanup_emission_plans.front().live_element_drop_planned);
    assert(!runtime_indexed.runtime_indexed_cleanup_emission_plans.front().live_element_drop_slice_lowerable);
    assert(runtime_indexed.runtime_indexed_cleanup_emission_plans.front().owner_deallocation_planned);
    assert(!runtime_indexed.runtime_indexed_cleanup_emission_plans.front().cleanup_tail_slice_lowerable);
    assert(!runtime_indexed.runtime_indexed_cleanup_emission_plans.front().ir_plan.complete);
    assert(runtime_indexed.runtime_indexed_cleanup_emission_plans.front().gated_ir_slice_lines.empty());
    assert(
        orison::lowering::runtime_indexed_cleanup_emission_plan_report(
            runtime_indexed.runtime_indexed_cleanup_emission_plans.front()
        ) ==
        "runtime-index cleanup emission-plan owner holder.items index index element Inner "
        "operations 5 prerequisites ready production-gate blocked production disabled "
        "length-load planned length-load-slice blocked loop planned loop-block-slice blocked "
        "skip planned skip-branch-slice blocked live-drop planned live-drop-slice blocked "
        "deallocate planned cleanup-tail-slice blocked structured-ir-plan blocked "
        "comment-ir-preview-lines 5 gated-ir-slice-lines 0 "
        "operation load-length operation loop-cleanup-index "
        "operation skip-cleanup-index operation drop-live-element operation deallocate-owner"
    );
    auto runtime_indexed_audit = orison::lowering::runtime_indexed_cleanup_audit_report(runtime_indexed);
    assert(runtime_indexed_audit.size() == 7);
    assert(runtime_indexed_audit[0] == "runtime-index cleanup audit entries 1");
    assert(runtime_indexed_audit[1] == orison::lowering::runtime_indexed_partial_owner_report(
        runtime_indexed.runtime_indexed_partial_owners.front()
    ));
    assert(runtime_indexed_audit[2] == orison::lowering::runtime_indexed_cleanup_skip_plan_report(
        runtime_indexed.runtime_indexed_cleanup_skip_plans.front()
    ));
    assert(runtime_indexed_audit[3] == orison::lowering::runtime_indexed_cleanup_proof_gate_report(
        runtime_indexed.runtime_indexed_cleanup_proof_gates.front()
    ));
    assert(runtime_indexed_audit[4] == orison::lowering::runtime_indexed_cleanup_emission_sketch_report(
        runtime_indexed.runtime_indexed_cleanup_emission_sketches.front()
    ));
    assert(runtime_indexed_audit[5] == orison::lowering::runtime_indexed_cleanup_capability_report(
        runtime_indexed.runtime_indexed_cleanup_capabilities.front()
    ));
    assert(runtime_indexed_audit[6] == orison::lowering::runtime_indexed_cleanup_emission_plan_report(
        runtime_indexed.runtime_indexed_cleanup_emission_plans.front()
    ));
    auto missing_index_capability = orison::lowering::runtime_indexed_cleanup_capability(
        missing_index_gate,
        missing_index_sketch
    );
    assert(!missing_index_capability.prerequisites_ready);
    assert(!missing_index_capability.production_enabled);
    auto missing_index_emission_plan = orison::lowering::runtime_indexed_cleanup_emission_plan(
        missing_index_capability,
        missing_index_sketch
    );
    assert(!missing_index_emission_plan.prerequisites_ready);
    assert(!missing_index_emission_plan.production_gate_requested);
    assert(!missing_index_emission_plan.production_enabled);
    assert(missing_index_emission_plan.operation_names.empty());
    assert(missing_index_emission_plan.comment_ir_preview_lines.empty());
    assert(
        orison::lowering::render_runtime_indexed_cleanup_ir_plan(
            missing_index_emission_plan.ir_plan
        ).empty()
    );
    auto enabled_capability = orison::lowering::runtime_indexed_cleanup_capability(
        runtime_indexed.runtime_indexed_cleanup_proof_gates.front(),
        runtime_indexed.runtime_indexed_cleanup_emission_sketches.front(),
        true
    );
    auto enabled_emission_plan = orison::lowering::runtime_indexed_cleanup_emission_plan(
        enabled_capability,
        runtime_indexed.runtime_indexed_cleanup_emission_sketches.front(),
        true
    );
    assert(enabled_emission_plan.production_gate_requested);
    assert(enabled_emission_plan.production_enabled);
    assert(enabled_emission_plan.comment_ir_preview_line_count == 5);
    assert(enabled_emission_plan.length_load_slice_lowerable);
    assert(enabled_emission_plan.loop_block_slice_lowerable);
    assert(enabled_emission_plan.skip_branch_slice_lowerable);
    assert(enabled_emission_plan.live_element_drop_slice_lowerable);
    assert(enabled_emission_plan.cleanup_tail_slice_lowerable);
    assert(enabled_emission_plan.ir_plan.complete);
    assert(enabled_emission_plan.ir_plan.labels_ready);
    assert(enabled_emission_plan.ir_plan.operands_ready);
    assert(enabled_emission_plan.ir_plan.calls_ready);
    assert(enabled_emission_plan.ir_plan.owner_name == "holder.items");
    assert(enabled_emission_plan.ir_plan.index_expression_text == "index");
    assert(enabled_emission_plan.ir_plan.element_source_type_name == "Inner");
    assert(enabled_emission_plan.ir_plan.element_llvm_type_name == "%record.Inner");
    assert(enabled_emission_plan.ir_plan.owner_llvm_type_name == "[2 x %record.Inner]");
    assert(enabled_emission_plan.ir_plan.owner_address_name == "%holder.items.runtime_cleanup.owner.addr");
    assert(enabled_emission_plan.ir_plan.static_length_value == "2");
    assert(enabled_emission_plan.ir_plan.entry_block_name == "holder.items.runtime_cleanup.entry");
    assert(enabled_emission_plan.ir_plan.length_value_name == "%holder.items.runtime_cleanup.length");
    assert(enabled_emission_plan.ir_plan.condition_block_name == "holder.items.runtime_cleanup.condition");
    assert(enabled_emission_plan.ir_plan.cleanup_index_name == "%holder.items.runtime_cleanup.index");
    assert(enabled_emission_plan.ir_plan.live_check_block_name == "holder.items.runtime_cleanup.check_live");
    assert(enabled_emission_plan.ir_plan.skip_block_name == "holder.items.runtime_cleanup.skip");
    assert(enabled_emission_plan.ir_plan.drop_block_name == "holder.items.runtime_cleanup.drop");
    assert(enabled_emission_plan.ir_plan.element_address_name == "%holder.items.runtime_cleanup.element.addr");
    assert(enabled_emission_plan.ir_plan.drop_callee_name == "__orison_drop.Inner");
    assert(enabled_emission_plan.ir_plan.continue_block_name == "holder.items.runtime_cleanup.continue");
    assert(enabled_emission_plan.ir_plan.exit_block_name == "holder.items.runtime_cleanup.exit");
    assert(enabled_emission_plan.ir_plan.deallocate_callee_name == "__orison_dynamic_array_deallocate");
    assert(enabled_emission_plan.gated_ir_slice_line_count == 20);
    assert(
        orison::lowering::render_runtime_indexed_cleanup_ir_plan(
            enabled_emission_plan.ir_plan
        ) == enabled_emission_plan.gated_ir_slice_lines
    );
    assert(
        enabled_emission_plan.gated_ir_slice_lines.front() ==
        "  %holder.items.runtime_cleanup.owner.addr = getelementptr %record.Holder, ptr "
        "%holder.addr, i32 0, i32 0\n"
    );
    assert(
        enabled_emission_plan.gated_ir_slice_lines[1] ==
        "  br label %holder.items.runtime_cleanup.condition\n"
    );
    assert(
        enabled_emission_plan.gated_ir_slice_lines[2] ==
        "holder.items.runtime_cleanup.condition:\n"
    );
    assert(
        enabled_emission_plan.gated_ir_slice_lines[4] ==
        "  %holder.items.runtime_cleanup.more = icmp ult i64 %holder.items.runtime_cleanup.index, 2\n"
    );
    assert(
        enabled_emission_plan.gated_ir_slice_lines[5] ==
        "  br i1 %holder.items.runtime_cleanup.more, label %holder.items.runtime_cleanup.check_live, "
        "label %holder.items.runtime_cleanup.exit\n"
    );
    assert(
        enabled_emission_plan.gated_ir_slice_lines[6] ==
        "holder.items.runtime_cleanup.check_live:\n"
    );
    assert(
        enabled_emission_plan.gated_ir_slice_lines[7] ==
        "  %holder.items.runtime_cleanup.skip_moved = icmp eq i64 "
        "%holder.items.runtime_cleanup.index, %index\n"
    );
    assert(
        enabled_emission_plan.gated_ir_slice_lines[8] ==
        "  br i1 %holder.items.runtime_cleanup.skip_moved, label "
        "%holder.items.runtime_cleanup.skip, label %holder.items.runtime_cleanup.drop\n"
    );
    assert(
        enabled_emission_plan.gated_ir_slice_lines[10] ==
        "  br label %holder.items.runtime_cleanup.continue\n"
    );
    assert(
        enabled_emission_plan.gated_ir_slice_lines[11] ==
        "holder.items.runtime_cleanup.drop:\n"
    );
    assert(
        enabled_emission_plan.gated_ir_slice_lines[12] ==
        "  %holder.items.runtime_cleanup.element.addr = getelementptr [2 x %record.Inner], ptr "
        "%holder.items.runtime_cleanup.owner.addr, i64 0, i64 %holder.items.runtime_cleanup.index\n"
    );
    assert(
        enabled_emission_plan.gated_ir_slice_lines[13] ==
        "  call void @__orison_drop.Inner(ptr %holder.items.runtime_cleanup.element.addr)\n"
    );
    assert(
        enabled_emission_plan.gated_ir_slice_lines[14] ==
        "  store %record.Inner zeroinitializer, ptr %holder.items.runtime_cleanup.element.addr\n"
    );
    assert(
        enabled_emission_plan.gated_ir_slice_lines[15] ==
        "  br label %holder.items.runtime_cleanup.continue\n"
    );
    assert(
        enabled_emission_plan.gated_ir_slice_lines[16] ==
        "holder.items.runtime_cleanup.continue:\n"
    );
    assert(
        enabled_emission_plan.gated_ir_slice_lines[17] ==
        "  %holder.items.runtime_cleanup.next_index = add i64 "
        "%holder.items.runtime_cleanup.index, 1\n"
    );
    assert(
        enabled_emission_plan.gated_ir_slice_lines[18] ==
        "  br label %holder.items.runtime_cleanup.condition\n"
    );
    assert(
        enabled_emission_plan.gated_ir_slice_lines[19] ==
        "holder.items.runtime_cleanup.exit:\n"
    );
    auto descriptor_owner = orison::lowering::RuntimeIndexedPartialOwner {
        .owner_name = "items",
        .index_expression_text = "index",
        .element_source_type_name = "Inner",
        .element_llvm_type_name = "%record.Inner",
        .owner_llvm_type_name = "{ ptr, i64, i64 }",
        .owner_address_name = "%items.addr",
        .element_size_value = "4",
        .moved_source_type_name = "Inner",
        .cleanup_strategy = "skip-moved-element",
        .constructor_move_enabled = false,
    };
    auto descriptor_skip = orison::lowering::runtime_indexed_cleanup_skip_plan(descriptor_owner);
    auto descriptor_gate = orison::lowering::runtime_indexed_cleanup_proof_gate(descriptor_skip);
    auto descriptor_sketch = orison::lowering::runtime_indexed_cleanup_emission_sketch(descriptor_gate);
    auto descriptor_capability = orison::lowering::runtime_indexed_cleanup_capability(
        descriptor_gate,
        descriptor_sketch,
        true
    );
    auto descriptor_emission_plan = orison::lowering::runtime_indexed_cleanup_emission_plan(
        descriptor_capability,
        descriptor_sketch,
        true
    );
    assert(descriptor_emission_plan.production_enabled);
    assert(descriptor_emission_plan.ir_plan.complete);
    assert(descriptor_emission_plan.ir_plan.descriptor_owner_ready);
    assert(!descriptor_emission_plan.ir_plan.static_length_ready);
    assert(descriptor_emission_plan.ir_plan.owner_deallocation_required);
    assert(descriptor_emission_plan.ir_plan.descriptor_value_name == "%items.runtime_cleanup.descriptor");
    assert(descriptor_emission_plan.ir_plan.descriptor_data_value_name == "%items.runtime_cleanup.data");
    assert(descriptor_emission_plan.ir_plan.length_value_name == "%items.runtime_cleanup.length");
    assert(descriptor_emission_plan.ir_plan.descriptor_capacity_value_name == "%items.runtime_cleanup.capacity");
    assert(descriptor_emission_plan.gated_ir_slice_line_count == 23);
    assert(
        descriptor_emission_plan.gated_ir_slice_lines[0] ==
        "  %items.runtime_cleanup.descriptor = load { ptr, i64, i64 }, ptr %items.addr\n"
    );
    assert(
        descriptor_emission_plan.gated_ir_slice_lines[1] ==
        "  %items.runtime_cleanup.data = extractvalue { ptr, i64, i64 } "
        "%items.runtime_cleanup.descriptor, 0\n"
    );
    assert(
        descriptor_emission_plan.gated_ir_slice_lines[2] ==
        "  %items.runtime_cleanup.length = extractvalue { ptr, i64, i64 } "
        "%items.runtime_cleanup.descriptor, 1\n"
    );
    assert(
        descriptor_emission_plan.gated_ir_slice_lines[3] ==
        "  %items.runtime_cleanup.capacity = extractvalue { ptr, i64, i64 } "
        "%items.runtime_cleanup.descriptor, 2\n"
    );
    assert(
        descriptor_emission_plan.gated_ir_slice_lines[7] ==
        "  %items.runtime_cleanup.more = icmp ult i64 %items.runtime_cleanup.index, "
        "%items.runtime_cleanup.length\n"
    );
    assert(
        descriptor_emission_plan.gated_ir_slice_lines[15] ==
        "  %items.runtime_cleanup.element.addr = getelementptr %record.Inner, ptr "
        "%items.runtime_cleanup.data, i64 %items.runtime_cleanup.index\n"
    );
    assert(
        descriptor_emission_plan.gated_ir_slice_lines[22] ==
        "  call void @__orison_dynamic_array_deallocate(ptr %items.runtime_cleanup.data, i64 4, "
        "i64 %items.runtime_cleanup.capacity)\n"
    );
    auto member_transfer_owner = orison::lowering::RuntimeIndexedPartialOwner {
        .owner_name = "items",
        .index_expression_text = "(index + zero)",
        .element_source_type_name = "Box",
        .element_llvm_type_name = "%record.Box",
        .owner_llvm_type_name = "{ ptr, i64, i64 }",
        .owner_address_name = "%items.addr",
        .element_size_value = "4",
        .moved_source_type_name = "Inner",
        .moved_member_path = {"item"},
        .cleanup_strategy = "skip-moved-element",
        .constructor_move_enabled = true,
    };
    auto member_transfer_skip = orison::lowering::runtime_indexed_cleanup_skip_plan(member_transfer_owner);
    auto member_transfer_gate = orison::lowering::runtime_indexed_cleanup_proof_gate(member_transfer_skip);
    assert(member_transfer_gate.owner_known);
    assert(member_transfer_gate.index_known);
    assert(!member_transfer_gate.type_match);
    assert(member_transfer_gate.operation_supported);
    assert(!member_transfer_gate.prerequisites_met);
    assert(!member_transfer_gate.lowering_enabled);
    assert(
        orison::lowering::runtime_indexed_cleanup_proof_gate_report(member_transfer_gate) ==
        "runtime-index cleanup proof owner items index (index + zero) element Box moved Inner "
        "operation skip-moved-element owner-known true index-known true type-match false "
        "operation-supported true prerequisites missing lowering disabled"
    );
    auto member_transfer_sketch =
        orison::lowering::runtime_indexed_cleanup_emission_sketch(member_transfer_gate);
    assert(member_transfer_sketch.snippets.empty());
    auto member_transfer_capability = orison::lowering::runtime_indexed_cleanup_capability(
        member_transfer_gate,
        member_transfer_sketch,
        true
    );
    assert(!member_transfer_capability.proof_ready);
    assert(!member_transfer_capability.sketch_ready);
    assert(!member_transfer_capability.prerequisites_ready);
    assert(!member_transfer_capability.production_enabled);
    auto member_transfer_emission_plan = orison::lowering::runtime_indexed_cleanup_emission_plan(
        member_transfer_capability,
        member_transfer_sketch,
        true
    );
    assert(!member_transfer_emission_plan.prerequisites_ready);
    assert(member_transfer_emission_plan.production_gate_requested);
    assert(!member_transfer_emission_plan.production_enabled);
    assert(member_transfer_emission_plan.operation_names.empty());
    assert(member_transfer_emission_plan.gated_ir_slice_lines.empty());
    auto member_cleanup_plan = orison::lowering::runtime_indexed_member_cleanup_plan(member_transfer_owner);
    assert(member_cleanup_plan.owner_known);
    assert(member_cleanup_plan.index_known);
    assert(member_cleanup_plan.element_type_known);
    assert(member_cleanup_plan.moved_type_known);
    assert(member_cleanup_plan.moved_member_path_known);
    assert(!member_cleanup_plan.cleanup_element_matches_move);
    assert(member_cleanup_plan.member_granular_cleanup_required);
    assert(!member_cleanup_plan.prerequisites_met);
    assert(!member_cleanup_plan.production_enabled);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_plan_report(member_cleanup_plan) ==
        "runtime-index member cleanup owner items index (index + zero) element Box moved Inner "
        "member-path item owner-known true index-known true element-type-known true "
        "moved-type-known true member-path-known true cleanup-element-matches-move false "
        "member-granular-required true prerequisites missing production disabled"
    );
    auto matching_runtime_indexed = orison::lowering::merge_ownership_transfer_states({
        runtime_indexed,
        runtime_indexed,
    });
    assert(matching_runtime_indexed.has_value());
    assert(matching_runtime_indexed->runtime_indexed_partial_owners.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_cleanup_skip_plans.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_cleanup_proof_gates.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_cleanup_emission_sketches.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_cleanup_capabilities.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_cleanup_emission_plans.size() == 1);
    auto different_runtime_indexed = runtime_indexed;
    different_runtime_indexed.runtime_indexed_partial_owners.front().index_expression_text = "other_index";
    auto mismatched_runtime_indexed = orison::lowering::merge_ownership_transfer_states({
        runtime_indexed,
        different_runtime_indexed,
    });
    assert(!mismatched_runtime_indexed.has_value());
    auto different_cleanup_plan = runtime_indexed;
    different_cleanup_plan.runtime_indexed_cleanup_skip_plans.front().cleanup_operation = "other-operation";
    auto mismatched_cleanup_plan = orison::lowering::merge_ownership_transfer_states({
        runtime_indexed,
        different_cleanup_plan,
    });
    assert(!mismatched_cleanup_plan.has_value());
    auto different_proof_gate = runtime_indexed;
    different_proof_gate.runtime_indexed_cleanup_proof_gates.front().lowering_enabled = true;
    auto mismatched_proof_gate = orison::lowering::merge_ownership_transfer_states({
        runtime_indexed,
        different_proof_gate,
    });
    assert(!mismatched_proof_gate.has_value());
    auto different_emission_sketch = runtime_indexed;
    different_emission_sketch.runtime_indexed_cleanup_emission_sketches.front()
        .production_emission_enabled = true;
    auto mismatched_emission_sketch = orison::lowering::merge_ownership_transfer_states({
        runtime_indexed,
        different_emission_sketch,
    });
    assert(!mismatched_emission_sketch.has_value());
    auto different_capability = runtime_indexed;
    different_capability.runtime_indexed_cleanup_capabilities.front().production_enabled = true;
    auto mismatched_capability = orison::lowering::merge_ownership_transfer_states({
        runtime_indexed,
        different_capability,
    });
    assert(!mismatched_capability.has_value());
    auto different_emission_plan = runtime_indexed;
    different_emission_plan.runtime_indexed_cleanup_emission_plans.front().production_enabled = true;
    auto mismatched_emission_plan = orison::lowering::merge_ownership_transfer_states({
        runtime_indexed,
        different_emission_plan,
    });
    assert(!mismatched_emission_plan.has_value());

    auto context = orison::lowering::LoweringContext {};
    context.records.emplace(
        "Payload",
        orison::lowering::LoweredRecordLayout {
            .name = "Payload",
            .llvm_type_name = "%record.Payload",
            .fields = {
                orison::lowering::LoweredRecordField {
                    .name = "items",
                    .source_type_name = "DynamicArray<UInt32>",
                    .llvm_type = "{ ptr, i64, i64 }",
                    .index = 0,
                },
            },
        }
    );
    context.records.emplace(
        "Box",
        orison::lowering::LoweredRecordLayout {
            .name = "Box",
            .llvm_type_name = "%record.Box",
            .fields = {
                orison::lowering::LoweredRecordField {
                    .name = "count",
                    .source_type_name = "UInt32",
                    .llvm_type = "i32",
                    .index = 0,
                },
                orison::lowering::LoweredRecordField {
                    .name = "payload",
                    .source_type_name = "Payload",
                    .llvm_type = "%record.Payload",
                    .index = 1,
                },
            },
        }
    );
    context.records.emplace(
        "NestedBox",
        orison::lowering::LoweredRecordLayout {
            .name = "NestedBox",
            .llvm_type_name = "%record.NestedBox",
            .fields = {
                orison::lowering::LoweredRecordField {
                    .name = "box",
                    .source_type_name = "Box",
                    .llvm_type = "%record.Box",
                    .index = 0,
                },
            },
        }
    );
    context.choices.emplace(
        "MaybePayload",
        orison::lowering::LoweredChoiceLayout {
            .name = "MaybePayload",
            .source_type_name = "MaybePayload",
            .llvm_type_name = "{ i32, %record.Payload }",
            .variants = {
                orison::lowering::LoweredChoiceVariant {
                    .name = "Some",
                    .lowered_payload_type = "%record.Payload",
                    .tag = 0,
                    .payloads = {
                        orison::lowering::LoweredChoicePayload {
                            .name = "value",
                            .source_type_name = "Payload",
                            .llvm_type = "%record.Payload",
                            .index = 0,
                        },
                    },
                },
                orison::lowering::LoweredChoiceVariant {
                    .name = "Empty",
                    .tag = 1,
                },
            },
        }
    );

    assert(!orison::lowering::is_owned_transfer_source_type("UInt32", context));
    assert(!orison::lowering::is_owned_transfer_source_type("Array<UInt32, 4>", context));
    assert(!orison::lowering::is_owned_transfer_source_type("Maybe<UInt32>", context));
    assert(orison::lowering::is_owned_transfer_source_type("DynamicArray<UInt32>", context));
    assert(orison::lowering::is_owned_transfer_source_type("Payload", context));
    assert(orison::lowering::is_owned_transfer_source_type("Box", context));
    assert(orison::lowering::is_owned_transfer_source_type("MaybePayload", context));

    auto scalar_field = orison::lowering::owned_record_field_transfer("box", "Box", "count", context);
    assert(!scalar_field.has_value());

    auto owned_field = orison::lowering::owned_record_field_transfer("box", "Box", "payload", context);
    assert(owned_field.has_value());
    assert(owned_field->binding_name == "box.payload");
    assert(owned_field->source_type_name == "Payload");

    auto nested_field_names = std::vector<std::string> {"box", "payload"};
    auto nested_field = orison::lowering::owned_record_member_path_transfer(
        "nested",
        "NestedBox",
        nested_field_names,
        context
    );
    assert(nested_field.has_value());
    assert(nested_field->binding_name == "nested.box.payload");
    assert(nested_field->source_type_name == "Payload");

    auto missing_outer_field_names = std::vector<std::string> {"missing", "payload"};
    auto missing_outer_field = orison::lowering::owned_record_member_path_transfer(
        "nested",
        "NestedBox",
        missing_outer_field_names,
        context
    );
    assert(!missing_outer_field.has_value());

    auto missing_inner_field_names = std::vector<std::string> {"box", "missing"};
    auto missing_inner_field = orison::lowering::owned_record_member_path_transfer(
        "nested",
        "NestedBox",
        missing_inner_field_names,
        context
    );
    assert(!missing_inner_field.has_value());

    auto nested_scalar_field_names = std::vector<std::string> {"box", "count"};
    auto nested_scalar_field = orison::lowering::owned_record_member_path_transfer(
        "nested",
        "NestedBox",
        nested_scalar_field_names,
        context
    );
    assert(!nested_scalar_field.has_value());

    auto nested_cross_scalar_field_names = std::vector<std::string> {"box", "count", "payload"};
    auto nested_cross_scalar_field = orison::lowering::owned_record_member_path_transfer(
        "nested",
        "NestedBox",
        nested_cross_scalar_field_names,
        context
    );
    assert(!nested_cross_scalar_field.has_value());

    auto owned_payload = orison::lowering::owned_choice_payload_transfer(
        "maybe",
        "MaybePayload",
        "Some",
        "value",
        context
    );
    assert(owned_payload.has_value());
    assert(owned_payload->binding_name == "maybe.Some.value");
    assert(owned_payload->source_type_name == "Payload");

    auto payload_name = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "payload",
    };
    auto some_pattern = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::call,
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::name,
                .text = "Some",
            }
        ),
    };
    some_pattern.arguments.push_back(std::move(payload_name));
    auto switch_case = orison::syntax::SwitchCaseSyntax {
        .pattern = std::move(some_pattern),
    };
    auto planned_case = orison::lowering::LoweredSwitchCasePlan {
        .syntax = &switch_case,
        .block = "switch.case.0",
    };
    auto subject_expression = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "maybe",
    };
    auto switch_state = orison::lowering::FunctionLoweringState {};
    switch_state.source_type_names.emplace("maybe", "MaybePayload");
    auto switch_failures = orison::lowering::LoweringFailures {};
    auto switch_session = orison::lowering::FunctionLoweringSession {
        .state = switch_state,
        .failures = switch_failures,
    };
    auto strings = orison::lowering::StringConstantTable {};
    auto emission_context = orison::lowering::LoweringEmissionContext {
        .lowering = context,
        .string_constants = strings,
        .options = {},
    };
    auto output = std::ostringstream {};
    orison::lowering::bind_switch_payload(
        planned_case,
        subject_expression,
        orison::lowering::LoweredExpression {
            .type = "{ i32, %record.Payload }",
            .value = "%maybe",
            .signedness = orison::lowering::IntegerSignedness::not_integer,
        },
        emission_context,
        switch_session,
        output,
        std::string_view {"MaybePayload"}
    );
    assert(orison::lowering::is_owned_binding_consumed(
        switch_state.ownership_transfers,
        "maybe.Some.value"
    ));
    assert(switch_state.source_type_names.at("payload") == "Payload");
    assert(switch_state.immutable_bindings.at("payload").type == "%record.Payload");
    assert(
        output.str() ==
        "  %tmp0 = extractvalue { i32, %record.Payload } %maybe, 1\n"
        "  %payload.addr = alloca %record.Payload\n"
        "  store %record.Payload %tmp0, ptr %payload.addr\n"
    );

    auto consumed_case_state = orison::lowering::OwnershipTransferState {};
    orison::lowering::mark_owned_binding_consumed(consumed_case_state, "holder.Loaded.payload");
    auto empty_case_state = orison::lowering::OwnershipTransferState {};
    auto nonvalue_switch_case_states = std::vector<orison::lowering::OwnershipTransferState> {
        consumed_case_state,
        empty_case_state,
    };
    auto holder_subject = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "holder",
    };
    auto consumed_descendants = orison::lowering::consumed_owned_descendant_names(
        nonvalue_switch_case_states,
        holder_subject.text
    );
    assert(consumed_descendants.size() == 1);
    assert(consumed_descendants.front() == "holder.Loaded.payload");

    orison::lowering::normalize_consumed_owned_descendants(
        nonvalue_switch_case_states,
        consumed_descendants
    );
    auto normalized_merge = orison::lowering::merge_ownership_transfer_states(nonvalue_switch_case_states);
    assert(normalized_merge.has_value());
    assert(orison::lowering::is_owned_binding_consumed(*normalized_merge, "holder.Loaded.payload"));

    auto other_subject = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "other",
    };
    auto unrelated_descendants = orison::lowering::consumed_owned_descendant_names(
        nonvalue_switch_case_states,
        other_subject.text
    );
    assert(unrelated_descendants.empty());
    return 0;
}
