#include "orison/lowering/ownership_transfer.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/maybe_switch_lowering.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/syntax/module_parser.hpp"

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
        "member-proof-ready false member-blocks-whole-element false "
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
    assert(runtime_indexed_audit.size() == 76);
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
    assert(runtime_indexed_audit[7] == orison::lowering::runtime_indexed_member_cleanup_plan_report(
        runtime_indexed.runtime_indexed_member_cleanup_plans.front()
    ));
    assert(runtime_indexed_audit[8] == orison::lowering::runtime_indexed_member_cleanup_proof_report(
        runtime_indexed.runtime_indexed_member_cleanup_proofs.front()
    ));
    assert(runtime_indexed_audit[9] == orison::lowering::runtime_indexed_member_cleanup_emission_sketch_report(
        runtime_indexed.runtime_indexed_member_cleanup_emission_sketches.front()
    ));
    assert(runtime_indexed_audit[10] == orison::lowering::runtime_indexed_member_cleanup_emission_gate_report(
        runtime_indexed.runtime_indexed_member_cleanup_emission_gates.front()
    ));
    assert(runtime_indexed_audit[11] == orison::lowering::runtime_indexed_member_cleanup_ir_insertion_plan_report(
        runtime_indexed.runtime_indexed_member_cleanup_ir_insertion_plans.front()
    ));
    assert(runtime_indexed_audit[12] == orison::lowering::runtime_indexed_member_cleanup_ir_composition_plan_report(
        runtime_indexed.runtime_indexed_member_cleanup_ir_composition_plans.front()
    ));
    assert(runtime_indexed_audit[13] == orison::lowering::runtime_indexed_member_cleanup_cfg_slice_report(
        runtime_indexed.runtime_indexed_member_cleanup_cfg_slices.front()
    ));
    assert(
        runtime_indexed_audit[14] ==
        orison::lowering::runtime_indexed_member_cleanup_function_rewrite_candidate_report(
            runtime_indexed.runtime_indexed_member_cleanup_function_rewrite_candidates.front()
        )
    );
    assert(
        runtime_indexed_audit[15] ==
        orison::lowering::runtime_indexed_member_cleanup_function_rewrite_edit_script_plan_report(
            runtime_indexed.runtime_indexed_member_cleanup_function_rewrite_edit_script_plans.front()
        )
    );
    assert(
        runtime_indexed_audit[16] ==
        orison::lowering::runtime_indexed_member_cleanup_function_rewrite_edit_script_validation_report(
            runtime_indexed.runtime_indexed_member_cleanup_function_rewrite_edit_script_validations.front()
        )
    );
    auto runtime_indexed_validation_diagnostics =
        orison::lowering::runtime_indexed_member_cleanup_function_rewrite_edit_script_validation_diagnostics(
            runtime_indexed.runtime_indexed_member_cleanup_function_rewrite_edit_script_validations.front()
        );
    assert(runtime_indexed_validation_diagnostics.size() == 5);
    assert(runtime_indexed_audit[17] == runtime_indexed_validation_diagnostics[0]);
    assert(runtime_indexed_audit[18] == runtime_indexed_validation_diagnostics[1]);
    assert(runtime_indexed_audit[19] == runtime_indexed_validation_diagnostics[2]);
    assert(runtime_indexed_audit[20] == runtime_indexed_validation_diagnostics[3]);
    assert(runtime_indexed_audit[21] == runtime_indexed_validation_diagnostics[4]);
    assert(
        runtime_indexed_audit[22] ==
        orison::lowering::runtime_indexed_member_cleanup_function_rewrite_staged_apply_plan_report(
            runtime_indexed.runtime_indexed_member_cleanup_function_rewrite_staged_apply_plans.front()
        )
    );
    auto runtime_indexed_staged_apply_diagnostics =
        orison::lowering::runtime_indexed_member_cleanup_function_rewrite_staged_apply_plan_diagnostics(
            runtime_indexed.runtime_indexed_member_cleanup_function_rewrite_staged_apply_plans.front()
        );
    assert(runtime_indexed_staged_apply_diagnostics.size() == 2);
    assert(runtime_indexed_audit[23] == runtime_indexed_staged_apply_diagnostics[0]);
    assert(runtime_indexed_audit[24] == runtime_indexed_staged_apply_diagnostics[1]);
    assert(runtime_indexed_audit[25] == orison::lowering::runtime_indexed_member_cleanup_module_mutation_gate_report(
        runtime_indexed.runtime_indexed_member_cleanup_module_mutation_gates.front()
    ));
    auto runtime_indexed_module_mutation_diagnostics =
        orison::lowering::runtime_indexed_member_cleanup_module_mutation_gate_diagnostics(
            runtime_indexed.runtime_indexed_member_cleanup_module_mutation_gates.front()
        );
    assert(runtime_indexed_module_mutation_diagnostics.size() == 5);
    assert(runtime_indexed_audit[26] == runtime_indexed_module_mutation_diagnostics[0]);
    assert(runtime_indexed_audit[27] == runtime_indexed_module_mutation_diagnostics[1]);
    assert(runtime_indexed_audit[28] == runtime_indexed_module_mutation_diagnostics[2]);
    assert(runtime_indexed_audit[29] == runtime_indexed_module_mutation_diagnostics[3]);
    assert(runtime_indexed_audit[30] == runtime_indexed_module_mutation_diagnostics[4]);
    assert(runtime_indexed_audit[31] == orison::lowering::runtime_indexed_member_cleanup_production_readiness_report(
        runtime_indexed.runtime_indexed_member_cleanup_production_readiness.front()
    ));
    auto runtime_indexed_member_diagnostics =
        orison::lowering::runtime_indexed_member_cleanup_production_blocker_diagnostics(
            runtime_indexed.runtime_indexed_member_cleanup_production_readiness.front()
    );
    assert(runtime_indexed_member_diagnostics.size() == 5);
    assert(runtime_indexed_audit[32] == runtime_indexed_member_diagnostics[0]);
    assert(runtime_indexed_audit[33] == runtime_indexed_member_diagnostics[1]);
    assert(runtime_indexed_audit[34] == runtime_indexed_member_diagnostics[2]);
    assert(runtime_indexed_audit[35] == runtime_indexed_member_diagnostics[3]);
    assert(runtime_indexed_audit[36] == runtime_indexed_member_diagnostics[4]);
    assert(runtime_indexed_audit[37] == orison::lowering::runtime_indexed_member_cleanup_promotion_checklist_report(
        runtime_indexed.runtime_indexed_member_cleanup_promotion_checklists.front()
    ));
    assert(runtime_indexed_audit[38] == orison::lowering::runtime_indexed_member_cleanup_promotion_seam_report(
        runtime_indexed.runtime_indexed_member_cleanup_promotion_seams.front()
    ));
    assert(
        runtime_indexed_audit[39] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_operation_plan_report(
            runtime_indexed.runtime_indexed_member_cleanup_mutation_operation_plans.front()
        )
    );
    assert(
        runtime_indexed_audit[40] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_operation_validation_report(
            runtime_indexed.runtime_indexed_member_cleanup_mutation_operation_validations.front()
        )
    );
    assert(
        runtime_indexed_audit[41] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_conflict_detection_report(
            runtime_indexed.runtime_indexed_member_cleanup_mutation_conflict_detections.front()
        )
    );
    assert(
        runtime_indexed_audit[42] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_apply_authorization_report(
            runtime_indexed.runtime_indexed_member_cleanup_mutation_apply_authorizations.front()
        )
    );
    assert(
        runtime_indexed_audit[43] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_apply_preview_report(
            runtime_indexed.runtime_indexed_member_cleanup_mutation_apply_previews.front()
        )
    );
    assert(
        runtime_indexed_audit[44] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_post_apply_verification_report(
            runtime_indexed.runtime_indexed_member_cleanup_mutation_post_apply_verifications.front()
        )
    );
    assert(
        runtime_indexed_audit[45] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_promotion_summary_report(
            runtime_indexed.runtime_indexed_member_cleanup_mutation_promotion_summaries.front()
        )
    );
    assert(
        runtime_indexed_audit[46] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_production_readiness_report(
            runtime_indexed.runtime_indexed_member_cleanup_mutation_production_readiness.front()
        )
    );
    auto runtime_indexed_mutation_production_diagnostics =
        orison::lowering::runtime_indexed_member_cleanup_mutation_production_readiness_diagnostics(
            runtime_indexed.runtime_indexed_member_cleanup_mutation_production_readiness.front()
        );
    assert(runtime_indexed_mutation_production_diagnostics.size() == 19);
    for (auto index = std::size_t {0}; index < runtime_indexed_mutation_production_diagnostics.size(); ++index) {
        assert(runtime_indexed_audit[47 + index] == runtime_indexed_mutation_production_diagnostics[index]);
    }
    assert(
        runtime_indexed_audit[66] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_readiness_verdict_report(
            runtime_indexed.runtime_indexed_member_cleanup_mutation_readiness_verdicts.front()
        )
    );
    assert(
        runtime_indexed_audit[67] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_authorization_report(
            runtime_indexed.runtime_indexed_member_cleanup_mutation_rewrite_authorizations.front()
        )
    );
    auto runtime_indexed_rewrite_authorization_diagnostics =
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_authorization_diagnostics(
            runtime_indexed.runtime_indexed_member_cleanup_mutation_rewrite_authorizations.front()
        );
    assert(runtime_indexed_rewrite_authorization_diagnostics.size() == 2);
    assert(runtime_indexed_audit[68] == runtime_indexed_rewrite_authorization_diagnostics[0]);
    assert(runtime_indexed_audit[69] == runtime_indexed_rewrite_authorization_diagnostics[1]);
    assert(
        runtime_indexed_audit[70] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_report(
            runtime_indexed.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans.front()
        )
    );
    auto runtime_indexed_rewrite_execution_plan_diagnostics =
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_diagnostics(
            runtime_indexed.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans.front()
        );
    assert(runtime_indexed_rewrite_execution_plan_diagnostics.size() == 2);
    assert(runtime_indexed_audit[71] == runtime_indexed_rewrite_execution_plan_diagnostics[0]);
    assert(runtime_indexed_audit[72] == runtime_indexed_rewrite_execution_plan_diagnostics[1]);
    assert(
        runtime_indexed_audit[73] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_verdict_report(
            runtime_indexed.runtime_indexed_member_cleanup_mutation_rewrite_execution_verdicts.front()
        )
    );
    assert(
        runtime_indexed_audit[74] ==
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_promotion_status_report(
            runtime_indexed.runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses.front()
        )
    );
    assert(
        runtime_indexed_audit[75] ==
        orison::lowering::runtime_indexed_member_cleanup_typed_promotion_gate_report(
            orison::lowering::runtime_indexed_member_cleanup_typed_promotion_gate(
                runtime_indexed.runtime_indexed_member_cleanup_promotion_checklists.front()
            )
        )
    );
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
    assert(member_transfer_gate.member_cleanup_proof_ready);
    assert(member_transfer_gate.member_cleanup_blocks_whole_element);
    assert(member_transfer_gate.operation_supported);
    assert(!member_transfer_gate.prerequisites_met);
    assert(!member_transfer_gate.lowering_enabled);
    assert(
        orison::lowering::runtime_indexed_cleanup_proof_gate_report(member_transfer_gate) ==
        "runtime-index cleanup proof owner items index (index + zero) element Box moved Inner "
        "operation skip-moved-element owner-known true index-known true type-match false "
        "member-proof-ready true member-blocks-whole-element true "
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
    auto member_cleanup_proof = orison::lowering::runtime_indexed_member_cleanup_proof(member_cleanup_plan);
    assert(member_cleanup_proof.plan_ready);
    assert(!member_cleanup_proof.whole_element_cleanup_matches_move);
    assert(member_cleanup_proof.member_cleanup_required);
    assert(member_cleanup_proof.member_scope_proven);
    assert(member_cleanup_proof.whole_element_cleanup_blocked);
    assert(member_cleanup_proof.prerequisites_met);
    assert(!member_cleanup_proof.production_enabled);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_proof_report(member_cleanup_proof) ==
        "runtime-index member cleanup proof owner items index (index + zero) element Box moved Inner "
        "member-path item plan-ready true whole-element-cleanup-matches-move false "
        "member-cleanup-required true member-scope-proven true whole-element-cleanup-blocked true "
        "prerequisites met production disabled"
    );
    auto member_cleanup_sketch =
        orison::lowering::runtime_indexed_member_cleanup_emission_sketch(member_cleanup_proof);
    assert(member_cleanup_sketch.proof_ready);
    assert(member_cleanup_sketch.report_only);
    assert(!member_cleanup_sketch.production_emission_enabled);
    assert(member_cleanup_sketch.snippets.size() == 6);
    assert(member_cleanup_sketch.snippets[3] == "drop-live-member-siblings items[cleanup_index] except item");
    assert(member_cleanup_sketch.snippets[4] == "preserve-moved-member items[(index + zero)].item");
    assert(
        orison::lowering::runtime_indexed_member_cleanup_emission_sketch_report(member_cleanup_sketch) ==
        "runtime-index member cleanup emission-sketch owner items index (index + zero) element Box "
        "moved Inner member-path item snippets 6 proof-ready true report-only true "
        "production-emission disabled snippet load-length items snippet loop-cleanup-index 0..<length "
        "snippet skip-cleanup-index (index + zero) "
        "snippet drop-live-member-siblings items[cleanup_index] except item "
        "snippet preserve-moved-member items[(index + zero)].item snippet deallocate-owner items"
    );
    auto member_cleanup_targets =
        orison::lowering::runtime_indexed_member_cleanup_targets(member_cleanup_sketch);
    assert(member_cleanup_targets.size() == 1);
    assert(member_cleanup_targets.front().metadata_ready);
    assert(!member_cleanup_targets.front().production_enabled);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_target_report(member_cleanup_targets.front()) ==
        "runtime-index member cleanup target owner items index (index + zero) element Box moved Inner "
        "member-path item operation drop-live-member-siblings "
        "drop-metadata __orison_member_cleanup.Box.except.item metadata ready production disabled"
    );
    auto member_cleanup_gate =
        orison::lowering::runtime_indexed_member_cleanup_emission_gate(
            member_cleanup_sketch,
            member_cleanup_targets
        );
    assert(member_cleanup_gate.sketch_ready);
    assert(member_cleanup_gate.member_drop_metadata_ready);
    assert(!member_cleanup_gate.ir_insertion_ready);
    assert(!member_cleanup_gate.prerequisites_met);
    assert(!member_cleanup_gate.production_enabled);
    assert(member_cleanup_gate.blockers.size() == 1);
    assert(member_cleanup_gate.blockers[0] == "member-cleanup-ir-insertion");
    assert(
        orison::lowering::runtime_indexed_member_cleanup_emission_gate_report(member_cleanup_gate) ==
        "runtime-index member cleanup emission-gate owner items index (index + zero) element Box "
        "moved Inner member-path item sketch-ready true member-drop-metadata ready "
        "ir-insertion missing prerequisites missing production disabled blockers 1 "
        "blocker member-cleanup-ir-insertion"
    );
    auto member_insertion_plan = orison::lowering::runtime_indexed_member_cleanup_ir_insertion_plan(
        member_cleanup_gate,
        member_cleanup_targets
    );
    assert(member_insertion_plan.target_metadata_ready);
    assert(member_insertion_plan.insertion_points_named);
    assert(member_insertion_plan.report_only);
    assert(!member_insertion_plan.production_enabled);
    assert(member_insertion_plan.preview_operations.size() == 6);
    assert(member_insertion_plan.insertion_anchor == "items.final-cleanup");
    assert(member_insertion_plan.entry_block_name == "items.member_cleanup.entry");
    assert(member_insertion_plan.sibling_drop_block_name == "items.member_cleanup.drop_siblings");
    assert(
        orison::lowering::runtime_indexed_member_cleanup_ir_insertion_plan_report(member_insertion_plan) ==
        "runtime-index member cleanup ir-insertion-plan owner items index (index + zero) element Box "
        "moved Inner member-path item anchor items.final-cleanup entry items.member_cleanup.entry "
        "skip items.member_cleanup.skip_moved sibling-drop items.member_cleanup.drop_siblings "
        "preserve items.member_cleanup.preserve_moved exit items.member_cleanup.exit "
        "target-metadata ready insertion-points named report-only true production disabled "
        "preview-operations 6 operation anchor-owner-final-cleanup items "
        "operation split-member-cleanup-entry items.member_cleanup.entry "
        "operation branch-skip-moved-index (index + zero) "
        "operation call-member-cleanup-target __orison_member_cleanup.Box.except.item "
        "operation preserve-moved-member-path item operation resume-owner-deallocation "
        "items.member_cleanup.exit"
    );
    auto member_composition_plan =
        orison::lowering::runtime_indexed_member_cleanup_ir_composition_plan(member_insertion_plan);
    assert(member_composition_plan.insertion_plan_ready);
    assert(member_composition_plan.block_topology_ready);
    assert(member_composition_plan.preview_operations_ready);
    assert(
        member_composition_plan.member_cleanup_target_symbol_name ==
        "__orison_member_cleanup.Box.except.item"
    );
    assert(member_composition_plan.report_only);
    assert(!member_composition_plan.production_enabled);
    assert(member_composition_plan.topology_edges.size() == 6);
    assert(member_composition_plan.topology_edges[0] == "items.final-cleanup -> items.member_cleanup.entry");
    assert(
        member_composition_plan.topology_edges[3] ==
        "items.member_cleanup.skip_moved -> items.member_cleanup.preserve_moved"
    );
    assert(
        member_composition_plan.topology_edges[5] ==
        "items.member_cleanup.preserve_moved -> items.member_cleanup.exit"
    );
    assert(
        orison::lowering::runtime_indexed_member_cleanup_ir_composition_plan_report(
            member_composition_plan
        ) ==
        "runtime-index member cleanup ir-composition-plan owner items index (index + zero) "
        "element Box moved Inner member-path item anchor items.final-cleanup "
        "entry items.member_cleanup.entry skip items.member_cleanup.skip_moved "
        "sibling-drop items.member_cleanup.drop_siblings preserve items.member_cleanup.preserve_moved "
        "exit items.member_cleanup.exit cleanup-target __orison_member_cleanup.Box.except.item "
        "insertion-plan ready block-topology ready "
        "preview-operations ready report-only true production disabled topology-edges 6 "
        "edge items.final-cleanup -> items.member_cleanup.entry "
        "edge items.member_cleanup.entry -> items.member_cleanup.skip_moved "
        "edge items.member_cleanup.entry -> items.member_cleanup.drop_siblings "
        "edge items.member_cleanup.skip_moved -> items.member_cleanup.preserve_moved "
        "edge items.member_cleanup.drop_siblings -> items.member_cleanup.preserve_moved "
        "edge items.member_cleanup.preserve_moved -> items.member_cleanup.exit"
    );
    auto member_cfg_slice =
        orison::lowering::runtime_indexed_member_cleanup_cfg_slice(member_composition_plan);
    assert(member_cfg_slice.composition_ready);
    assert(member_cfg_slice.slice_rendered);
    assert(member_cfg_slice.report_only);
    assert(!member_cfg_slice.production_enabled);
    assert(member_cfg_slice.member_cleanup_target_symbol_name == "__orison_member_cleanup.Box.except.item");
    assert(member_cfg_slice.cfg_lines.size() == 16);
    assert(member_cfg_slice.cfg_lines[0] == "; report-only runtime-index member cleanup anchor items.final-cleanup\n");
    assert(member_cfg_slice.cfg_lines[1] == "items.member_cleanup.entry:\n");
    assert(
        member_cfg_slice.cfg_lines[6] ==
        "  ; preserve moved member items[(index + zero)].item\n"
    );
    assert(
        member_cfg_slice.cfg_lines[10] ==
        "  ; member cleanup target __orison_member_cleanup.Box.except.item\n"
    );
    assert(member_cfg_slice.cfg_lines[14] == "items.member_cleanup.exit:\n");
    assert(
        orison::lowering::runtime_indexed_member_cleanup_cfg_slice_report(member_cfg_slice) ==
        "runtime-index member cleanup cfg-slice owner items index (index + zero) element Box "
        "moved Inner member-path item anchor items.final-cleanup entry items.member_cleanup.entry "
        "skip items.member_cleanup.skip_moved sibling-drop items.member_cleanup.drop_siblings "
        "preserve items.member_cleanup.preserve_moved exit items.member_cleanup.exit "
        "cleanup-target __orison_member_cleanup.Box.except.item composition ready slice rendered "
        "report-only true production disabled cfg-lines 16 "
        "line ; report-only runtime-index member cleanup anchor items.final-cleanup "
        "line items.member_cleanup.entry: line ; report-only compare cleanup_index with (index + zero) "
        "line   ; br moved index -> items.member_cleanup.skip_moved "
        "line   ; br live sibling -> items.member_cleanup.drop_siblings "
        "line items.member_cleanup.skip_moved: "
        "line   ; preserve moved member items[(index + zero)].item "
        "line   ; br label %items.member_cleanup.preserve_moved "
        "line items.member_cleanup.drop_siblings: "
        "line   ; call member cleanup for Box except item "
        "line   ; member cleanup target __orison_member_cleanup.Box.except.item "
        "line   ; br label %items.member_cleanup.preserve_moved "
        "line items.member_cleanup.preserve_moved: "
        "line   ; br label %items.member_cleanup.exit "
        "line items.member_cleanup.exit: line   ; resume owner cleanup items"
    );
    auto member_rewrite_candidate =
        orison::lowering::runtime_indexed_member_cleanup_function_rewrite_candidate(member_cfg_slice);
    assert(member_rewrite_candidate.cfg_slice_ready);
    assert(member_rewrite_candidate.anchor_ready);
    assert(member_rewrite_candidate.branch_rewrite_planned);
    assert(member_rewrite_candidate.cfg_append_planned);
    assert(member_rewrite_candidate.candidate_available);
    assert(member_rewrite_candidate.candidate_verified);
    assert(member_rewrite_candidate.sibling_drop_block_name == "items.member_cleanup.drop_siblings");
    assert(member_rewrite_candidate.preserve_block_name == "items.member_cleanup.preserve_moved");
    assert(
        member_rewrite_candidate.member_cleanup_target_symbol_name ==
        "__orison_member_cleanup.Box.except.item"
    );
    assert(member_rewrite_candidate.report_only);
    assert(!member_rewrite_candidate.production_enabled);
    assert(member_rewrite_candidate.replaced_terminator_text == "br label %items.final-cleanup");
    assert(member_rewrite_candidate.replacement_branch_text == "br label %items.member_cleanup.entry");
    assert(member_rewrite_candidate.appended_cfg_preview_lines.size() == 16);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_function_rewrite_candidate_report(
            member_rewrite_candidate
        ) ==
        "runtime-index member cleanup function-rewrite-candidate owner items index (index + zero) "
        "element Box moved Inner member-path item anchor items.final-cleanup "
        "entry items.member_cleanup.entry sibling-drop items.member_cleanup.drop_siblings "
        "preserve items.member_cleanup.preserve_moved exit items.member_cleanup.exit "
        "cleanup-target __orison_member_cleanup.Box.except.item cfg-slice ready "
        "anchor-state ready branch-rewrite planned cfg-append planned candidate available "
        "verification verified report-only true production disabled replaced-terminator "
        "br label %items.final-cleanup replacement-branch br label %items.member_cleanup.entry "
        "appended-cfg-lines 16"
    );
    auto member_edit_script_plan =
        orison::lowering::runtime_indexed_member_cleanup_function_rewrite_edit_script_plan(
            member_rewrite_candidate
        );
    assert(member_edit_script_plan.candidate_verified);
    assert(member_edit_script_plan.branch_replacement_ready);
    assert(member_edit_script_plan.cleanup_cfg_append_ready);
    assert(member_edit_script_plan.phi_retarget_ready);
    assert(member_edit_script_plan.edit_script_ready);
    assert(member_edit_script_plan.sibling_drop_block_name == "items.member_cleanup.drop_siblings");
    assert(member_edit_script_plan.preserve_block_name == "items.member_cleanup.preserve_moved");
    assert(
        member_edit_script_plan.member_cleanup_target_symbol_name ==
        "__orison_member_cleanup.Box.except.item"
    );
    assert(member_edit_script_plan.report_only);
    assert(!member_edit_script_plan.production_enabled);
    assert(member_edit_script_plan.expected_branch_text == "br label %items.final-cleanup");
    assert(member_edit_script_plan.replacement_branch_text == "br label %items.member_cleanup.entry");
    assert(member_edit_script_plan.cleanup_cfg_append_placement == "before-function-closing-brace");
    assert(member_edit_script_plan.expected_closing_text == "\\n}\\n");
    assert(member_edit_script_plan.phi_old_predecessor_block_name == "items.final-cleanup");
    assert(member_edit_script_plan.phi_new_predecessor_block_name == "items.member_cleanup.exit");
    assert(member_edit_script_plan.appended_cfg_preview_lines.size() == 16);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_function_rewrite_edit_script_plan_report(
            member_edit_script_plan
        ) ==
        "runtime-index member cleanup function-rewrite-edit-script-plan owner items "
        "index (index + zero) element Box moved Inner member-path item anchor items.final-cleanup "
        "entry items.member_cleanup.entry sibling-drop items.member_cleanup.drop_siblings "
        "preserve items.member_cleanup.preserve_moved exit items.member_cleanup.exit "
        "cleanup-target __orison_member_cleanup.Box.except.item candidate verified "
        "branch-replacement ready cleanup-cfg-append ready phi-retarget ready edit-script ready "
        "report-only true production disabled expected-branch br label %items.final-cleanup "
        "replacement-branch br label %items.member_cleanup.entry append-placement "
        "before-function-closing-brace expected-closing \\n}\\n phi-old items.final-cleanup "
        "phi-new items.member_cleanup.exit appended-cfg-lines 16"
    );
    auto member_edit_script_validation =
        orison::lowering::runtime_indexed_member_cleanup_function_rewrite_edit_script_validation(
            member_edit_script_plan
        );
    assert(member_edit_script_validation.edit_script_ready);
    assert(member_edit_script_validation.branch_replacement_valid);
    assert(member_edit_script_validation.cleanup_cfg_append_valid);
    assert(member_edit_script_validation.phi_retarget_valid);
    assert(member_edit_script_validation.validation_ready);
    assert(member_edit_script_validation.report_only);
    assert(!member_edit_script_validation.production_enabled);
    assert(member_edit_script_validation.blockers.size() == 1);
    assert(
        member_edit_script_validation.blockers[0] ==
        "production-member-cleanup-module-mutation"
    );
    assert(
        orison::lowering::runtime_indexed_member_cleanup_function_rewrite_edit_script_validation_report(
            member_edit_script_validation
        ) ==
        "runtime-index member cleanup function-rewrite-edit-script-validation owner items "
        "index (index + zero) element Box moved Inner member-path item anchor items.final-cleanup "
        "entry items.member_cleanup.entry exit items.member_cleanup.exit edit-script ready "
        "branch-replacement valid cleanup-cfg-append valid phi-retarget valid validation ready "
        "report-only true production disabled blockers 1 blocker production-member-cleanup-module-mutation"
    );
    auto member_edit_script_validation_diagnostics =
        orison::lowering::runtime_indexed_member_cleanup_function_rewrite_edit_script_validation_diagnostics(
            member_edit_script_validation
        );
    assert(member_edit_script_validation_diagnostics.size() == 1);
    assert(
        member_edit_script_validation_diagnostics[0] ==
        "runtime-index member cleanup edit-script validation diagnostic owner items "
        "index (index + zero) element Box moved Inner member-path item "
        "blocker production-member-cleanup-module-mutation detail "
        "member cleanup edit script is validated but production module mutation is disabled"
    );
    auto member_staged_apply_plan =
        orison::lowering::runtime_indexed_member_cleanup_function_rewrite_staged_apply_plan(
            member_edit_script_validation
        );
    assert(member_staged_apply_plan.validation_ready);
    assert(member_staged_apply_plan.branch_replacement_planned);
    assert(member_staged_apply_plan.cleanup_cfg_append_planned);
    assert(member_staged_apply_plan.phi_retarget_planned);
    assert(member_staged_apply_plan.staged_apply_ready);
    assert(!member_staged_apply_plan.branch_replacement_applied);
    assert(!member_staged_apply_plan.cleanup_cfg_appended);
    assert(!member_staged_apply_plan.phi_retarget_applied);
    assert(member_staged_apply_plan.report_only);
    assert(!member_staged_apply_plan.production_enabled);
    assert(member_staged_apply_plan.blockers.size() == 1);
    assert(member_staged_apply_plan.blockers[0] == "production-member-cleanup-module-mutation");
    assert(
        orison::lowering::runtime_indexed_member_cleanup_function_rewrite_staged_apply_plan_report(
            member_staged_apply_plan
        ) ==
        "runtime-index member cleanup function-rewrite-staged-apply-plan owner items "
        "index (index + zero) element Box moved Inner member-path item anchor items.final-cleanup "
        "entry items.member_cleanup.entry exit items.member_cleanup.exit validation ready "
        "branch-replacement planned cleanup-cfg-append planned phi-retarget planned "
        "staged-apply ready branch-applied false cfg-appended false phi-applied false "
        "report-only true production disabled blockers 1 blocker production-member-cleanup-module-mutation"
    );
    auto member_staged_apply_diagnostics =
        orison::lowering::runtime_indexed_member_cleanup_function_rewrite_staged_apply_plan_diagnostics(
            member_staged_apply_plan
        );
    assert(member_staged_apply_diagnostics.size() == 1);
    assert(
        member_staged_apply_diagnostics[0] ==
        "runtime-index member cleanup staged-apply diagnostic owner items "
        "index (index + zero) element Box moved Inner member-path item "
        "blocker production-member-cleanup-module-mutation detail "
        "member cleanup staged plan is ready but production module mutation is disabled"
    );
    auto member_mutation_gate =
        orison::lowering::runtime_indexed_member_cleanup_module_mutation_gate(
            member_cfg_slice,
            member_edit_script_validation,
            member_staged_apply_plan
        );
    assert(member_mutation_gate.cfg_slice_ready);
    assert(member_mutation_gate.edit_script_validation_ready);
    assert(member_mutation_gate.staged_apply_ready);
    assert(!member_mutation_gate.module_mutation_enabled);
    assert(!member_mutation_gate.production_member_cleanup_enabled);
    assert(!member_mutation_gate.prerequisites_met);
    assert(!member_mutation_gate.production_enabled);
    assert(member_mutation_gate.blockers.size() == 2);
    assert(member_mutation_gate.blockers[0] == "member-cleanup-module-mutation");
    assert(member_mutation_gate.blockers[1] == "production-member-cleanup");
    assert(
        orison::lowering::runtime_indexed_member_cleanup_module_mutation_gate_report(
            member_mutation_gate
        ) ==
        "runtime-index member cleanup module-mutation-gate owner items index (index + zero) "
        "element Box moved Inner member-path item anchor items.final-cleanup "
        "entry items.member_cleanup.entry skip items.member_cleanup.skip_moved "
        "sibling-drop items.member_cleanup.drop_siblings preserve items.member_cleanup.preserve_moved "
        "exit items.member_cleanup.exit cfg-slice ready edit-script-validation ready staged-apply ready "
        "module-mutation disabled production-member-cleanup disabled prerequisites missing production disabled blockers 2 "
        "blocker member-cleanup-module-mutation blocker production-member-cleanup"
    );
    auto member_module_mutation_diagnostics =
        orison::lowering::runtime_indexed_member_cleanup_module_mutation_gate_diagnostics(
            member_mutation_gate
        );
    assert(member_module_mutation_diagnostics.size() == 2);
    assert(
        member_module_mutation_diagnostics[0] ==
        "runtime-index member cleanup module-mutation diagnostic owner items "
        "index (index + zero) element Box moved Inner member-path item "
        "blocker member-cleanup-module-mutation detail member cleanup module mutation is disabled"
    );
    assert(
        member_module_mutation_diagnostics[1] ==
        "runtime-index member cleanup module-mutation diagnostic owner items "
        "index (index + zero) element Box moved Inner member-path item "
        "blocker production-member-cleanup detail production member cleanup is disabled"
    );
    auto member_production_readiness = orison::lowering::runtime_indexed_member_cleanup_production_readiness(
        member_cleanup_proof,
        member_cleanup_targets,
        std::vector<orison::lowering::RuntimeIndexedMemberCleanupHelperDropBindings> {
            orison::lowering::RuntimeIndexedMemberCleanupHelperDropBindings {
                .owner_name = "items",
                .index_expression_text = "(index + zero)",
                .element_source_type_name = "Box",
                .moved_source_type_name = "Inner",
                .moved_member_path = std::vector<std::string> {"item"},
                .helper_symbol_name = "__orison_member_cleanup.Box.except.item",
                .sibling_binding_count = 2,
                .all_drop_definitions_available = true,
                .nested_member_path = false,
                .helper_definition_ready = true,
                .production_enabled = false,
            },
        },
        member_cfg_slice,
        member_mutation_gate
    );
    assert(member_production_readiness.proof_ready);
    assert(member_production_readiness.target_metadata_ready);
    assert(member_production_readiness.helper_drop_bindings_ready);
    assert(member_production_readiness.cfg_slice_ready);
    assert(!member_production_readiness.module_mutation_ready);
    assert(!member_production_readiness.production_member_cleanup_ready);
    assert(!member_production_readiness.production_gate_ready);
    assert(!member_production_readiness.production_enabled);
    assert(!member_production_readiness.production_ready);
    assert(member_production_readiness.blockers.size() == 2);
    assert(member_production_readiness.blockers[0] == "member-cleanup-module-mutation");
    assert(member_production_readiness.blockers[1] == "production-member-cleanup");
    assert(
        orison::lowering::runtime_indexed_member_cleanup_production_readiness_report(
            member_production_readiness
        ) ==
        "runtime-index member cleanup production-readiness owner items index (index + zero) "
        "element Box moved Inner member-path item proof ready target-metadata ready helper-drop-bindings ready "
        "cfg-slice ready module-mutation blocked production-member-cleanup blocked "
        "production-gate blocked production-enabled false production blocked blockers 2 "
        "blocker member-cleanup-module-mutation "
        "blocker production-member-cleanup"
    );
    auto member_production_diagnostics =
        orison::lowering::runtime_indexed_member_cleanup_production_blocker_diagnostics(
            member_production_readiness
        );
    assert(member_production_diagnostics.size() == 2);
    assert(
        member_production_diagnostics[0] ==
        "runtime-index member cleanup production blocker owner items index (index + zero) "
        "element Box moved Inner member-path item blocker member-cleanup-module-mutation "
        "detail member cleanup module mutation is disabled"
    );
    assert(
        member_production_diagnostics[1] ==
        "runtime-index member cleanup production blocker owner items index (index + zero) "
        "element Box moved Inner member-path item blocker production-member-cleanup "
        "detail production member cleanup is disabled"
    );
    auto member_promotion_checklist = orison::lowering::runtime_indexed_member_cleanup_promotion_checklist(
        member_rewrite_candidate,
        member_edit_script_plan,
        member_edit_script_validation,
        member_staged_apply_plan,
        member_mutation_gate,
        member_production_readiness
    );
    assert(member_promotion_checklist.rewrite_candidate_ready);
    assert(member_promotion_checklist.edit_script_ready);
    assert(member_promotion_checklist.validation_ready);
    assert(member_promotion_checklist.staged_apply_ready);
    assert(!member_promotion_checklist.module_mutation_ready);
    assert(!member_promotion_checklist.production_readiness_ready);
    assert(!member_promotion_checklist.promotion_ready);
    assert(member_promotion_checklist.report_only);
    assert(!member_promotion_checklist.production_enabled);
    assert(member_promotion_checklist.blockers.size() == 2);
    assert(member_promotion_checklist.blockers[0] == "member-cleanup-module-mutation");
    assert(member_promotion_checklist.blockers[1] == "production-member-cleanup");
    assert(
        orison::lowering::runtime_indexed_member_cleanup_promotion_checklist_report(member_promotion_checklist) ==
        "runtime-index member cleanup promotion-checklist owner items index (index + zero) "
        "element Box moved Inner member-path item candidate ready edit-script ready "
        "validation ready staged-apply ready module-mutation blocked production-readiness blocked "
        "promotion blocked report-only true production disabled blockers 2 "
        "blocker member-cleanup-module-mutation blocker production-member-cleanup"
    );
    auto member_typed_promotion_gate =
        orison::lowering::runtime_indexed_member_cleanup_typed_promotion_gate(member_promotion_checklist);
    assert(member_typed_promotion_gate.checklist_ready);
    assert(!member_typed_promotion_gate.ir_mutation_requested);
    assert(!member_typed_promotion_gate.production_gate_requested);
    assert(!member_typed_promotion_gate.ir_mutation_enabled);
    assert(!member_typed_promotion_gate.production_gate_enabled);
    assert(!member_typed_promotion_gate.gate_ready);
    assert(member_typed_promotion_gate.report_only);
    assert(!member_typed_promotion_gate.production_enabled);
    assert(member_typed_promotion_gate.blockers.size() == 4);
    assert(member_typed_promotion_gate.blockers[0] == "member-cleanup-module-mutation");
    assert(member_typed_promotion_gate.blockers[1] == "production-member-cleanup");
    assert(member_typed_promotion_gate.blockers[2] == "member-cleanup-ir-mutation");
    assert(member_typed_promotion_gate.blockers[3] == "production-member-cleanup-ir-mutation");
    assert(
        orison::lowering::runtime_indexed_member_cleanup_typed_promotion_gate_report(
            member_typed_promotion_gate
        ) ==
        "runtime-index member cleanup typed-promotion-gate owner items index (index + zero) "
        "element Box moved Inner member-path item checklist ready ir-mutation-requested false "
        "production-gate-requested false ir-mutation disabled production-gate disabled "
        "gate blocked report-only true production disabled blockers 4 "
        "blocker member-cleanup-module-mutation blocker production-member-cleanup "
        "blocker member-cleanup-ir-mutation blocker production-member-cleanup-ir-mutation"
    );
    auto member_promotion_seam =
        orison::lowering::runtime_indexed_member_cleanup_promotion_seam(member_typed_promotion_gate);
    assert(member_promotion_seam.checklist_ready);
    assert(member_promotion_seam.mutation_seam_selected);
    assert(!member_promotion_seam.ir_mutation_enabled);
    assert(!member_promotion_seam.production_gate_enabled);
    assert(!member_promotion_seam.promotion_ready);
    assert(member_promotion_seam.report_only);
    assert(!member_promotion_seam.production_enabled);
    assert(member_promotion_seam.blockers.size() == 4);
    assert(member_promotion_seam.blockers[0] == "member-cleanup-module-mutation");
    assert(member_promotion_seam.blockers[1] == "production-member-cleanup");
    assert(member_promotion_seam.blockers[2] == "member-cleanup-ir-mutation");
    assert(member_promotion_seam.blockers[3] == "production-member-cleanup-ir-mutation");
    assert(
        orison::lowering::runtime_indexed_member_cleanup_promotion_seam_report(member_promotion_seam) ==
        "runtime-index member cleanup promotion-seam owner items index (index + zero) "
        "element Box moved Inner member-path item checklist ready mutation-seam selected "
        "ir-mutation disabled production-gate disabled promotion blocked report-only true "
        "production disabled blockers 4 blocker member-cleanup-module-mutation "
        "blocker production-member-cleanup blocker member-cleanup-ir-mutation "
        "blocker production-member-cleanup-ir-mutation"
    );
    auto requested_member_typed_promotion_gate =
        orison::lowering::runtime_indexed_member_cleanup_typed_promotion_gate(
            member_promotion_checklist,
            true,
            true
        );
    assert(requested_member_typed_promotion_gate.checklist_ready);
    assert(requested_member_typed_promotion_gate.ir_mutation_requested);
    assert(requested_member_typed_promotion_gate.production_gate_requested);
    assert(requested_member_typed_promotion_gate.ir_mutation_enabled);
    assert(requested_member_typed_promotion_gate.production_gate_enabled);
    assert(requested_member_typed_promotion_gate.gate_ready);
    assert(!requested_member_typed_promotion_gate.report_only);
    assert(requested_member_typed_promotion_gate.production_enabled);
    assert(requested_member_typed_promotion_gate.blockers.empty());
    assert(
        orison::lowering::runtime_indexed_member_cleanup_typed_promotion_gate_report(
            requested_member_typed_promotion_gate
        ) ==
        "runtime-index member cleanup typed-promotion-gate owner items index (index + zero) "
        "element Box moved Inner member-path item checklist ready ir-mutation-requested true "
        "production-gate-requested true ir-mutation enabled production-gate enabled "
        "gate ready report-only false production enabled blockers 0"
    );
    auto member_mutation_operation_plan =
        orison::lowering::runtime_indexed_member_cleanup_mutation_operation_plan(
            member_promotion_seam,
            member_edit_script_plan
        );
    assert(member_mutation_operation_plan.seam_selected);
    assert(member_mutation_operation_plan.operations_ready);
    assert(!member_mutation_operation_plan.operations_applied);
    assert(member_mutation_operation_plan.report_only);
    assert(!member_mutation_operation_plan.production_enabled);
    assert(member_mutation_operation_plan.operations.size() == 3);
    assert(member_mutation_operation_plan.operations[0].kind == "branch-replacement");
    assert(member_mutation_operation_plan.operations[0].ready);
    assert(!member_mutation_operation_plan.operations[0].applied);
    assert(member_mutation_operation_plan.operations[1].kind == "cfg-append");
    assert(member_mutation_operation_plan.operations[1].ready);
    assert(!member_mutation_operation_plan.operations[1].applied);
    assert(member_mutation_operation_plan.operations[2].kind == "phi-retarget");
    assert(member_mutation_operation_plan.operations[2].ready);
    assert(!member_mutation_operation_plan.operations[2].applied);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_operation_plan_report(
            member_mutation_operation_plan
        ) ==
        "runtime-index member cleanup mutation-operation-plan owner items index (index + zero) "
        "element Box moved Inner member-path item seam selected operations 3 operations-ready ready "
        "operations-applied false report-only true production disabled blockers 4 "
        "blocker member-cleanup-module-mutation blocker production-member-cleanup "
        "blocker member-cleanup-ir-mutation blocker production-member-cleanup-ir-mutation "
        "operation branch-replacement ready true applied false anchor items.final-cleanup "
        "expected br label %items.final-cleanup replacement br label %items.member_cleanup.entry "
        "placement missing old-pred missing new-pred missing operation cfg-append ready true "
        "applied false anchor items.member_cleanup.exit expected \\n}\\n replacement missing "
        "placement before-function-closing-brace old-pred missing new-pred missing operation "
        "phi-retarget ready true applied false anchor items.member_cleanup.exit expected missing "
        "replacement missing placement missing old-pred items.final-cleanup new-pred items.member_cleanup.exit"
    );
    auto member_mutation_operation_validation =
        orison::lowering::runtime_indexed_member_cleanup_mutation_operation_validation(
            member_mutation_operation_plan
        );
    assert(member_mutation_operation_validation.seam_selected);
    assert(member_mutation_operation_validation.operation_count_valid);
    assert(member_mutation_operation_validation.operation_order_valid);
    assert(member_mutation_operation_validation.branch_replacement_fields_valid);
    assert(member_mutation_operation_validation.cfg_append_fields_valid);
    assert(member_mutation_operation_validation.phi_retarget_fields_valid);
    assert(member_mutation_operation_validation.operations_ready);
    assert(member_mutation_operation_validation.no_operations_applied);
    assert(member_mutation_operation_validation.validation_ready);
    assert(member_mutation_operation_validation.report_only);
    assert(!member_mutation_operation_validation.production_enabled);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_operation_validation_report(
            member_mutation_operation_validation
        ) ==
        "runtime-index member cleanup mutation-operation-validation owner items index (index + zero) "
        "element Box moved Inner member-path item seam selected count valid order valid "
        "branch-replacement-fields valid cfg-append-fields valid phi-retarget-fields valid "
        "operations-ready ready no-operations-applied true validation ready report-only true "
        "production disabled blockers 4 blocker member-cleanup-module-mutation "
        "blocker production-member-cleanup blocker member-cleanup-ir-mutation "
        "blocker production-member-cleanup-ir-mutation"
    );
    auto member_mutation_conflict_detection =
        orison::lowering::runtime_indexed_member_cleanup_mutation_conflict_detection(
            member_mutation_operation_plan,
            member_mutation_operation_validation
        );
    assert(member_mutation_conflict_detection.validation_ready);
    assert(member_mutation_conflict_detection.branch_anchor_match_count == 1);
    assert(member_mutation_conflict_detection.closing_anchor_match_count == 1);
    assert(member_mutation_conflict_detection.phi_predecessor_match_count == 1);
    assert(member_mutation_conflict_detection.branch_anchor_unique);
    assert(member_mutation_conflict_detection.closing_anchor_unique);
    assert(member_mutation_conflict_detection.phi_predecessor_unique);
    assert(member_mutation_conflict_detection.conflict_free);
    assert(!member_mutation_conflict_detection.apply_allowed);
    assert(member_mutation_conflict_detection.report_only);
    assert(!member_mutation_conflict_detection.production_enabled);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_conflict_detection_report(
            member_mutation_conflict_detection
        ) ==
        "runtime-index member cleanup mutation-conflict-detection owner items index (index + zero) "
        "element Box moved Inner member-path item validation ready branch-anchor-matches 1 "
        "branch-anchor unique closing-anchor-matches 1 closing-anchor unique "
        "phi-predecessor-matches 1 phi-predecessor unique conflict-free true "
        "apply-allowed false report-only true production disabled blockers 4 "
        "blocker member-cleanup-module-mutation blocker production-member-cleanup "
        "blocker member-cleanup-ir-mutation blocker production-member-cleanup-ir-mutation"
    );
    auto member_mutation_apply_authorization =
        orison::lowering::runtime_indexed_member_cleanup_mutation_apply_authorization(
            member_mutation_operation_validation,
            member_mutation_conflict_detection
        );
    assert(member_mutation_apply_authorization.validation_ready);
    assert(member_mutation_apply_authorization.conflict_free);
    assert(!member_mutation_apply_authorization.ir_mutation_requested);
    assert(!member_mutation_apply_authorization.production_gate_enabled);
    assert(!member_mutation_apply_authorization.authorization_ready);
    assert(!member_mutation_apply_authorization.apply_authorized);
    assert(member_mutation_apply_authorization.report_only);
    assert(!member_mutation_apply_authorization.production_enabled);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_apply_authorization_report(
            member_mutation_apply_authorization
        ) ==
        "runtime-index member cleanup mutation-apply-authorization owner items index (index + zero) "
        "element Box moved Inner member-path item validation ready conflict-free true "
        "ir-mutation blocked production-gate disabled apply-requested false authorization blocked apply-authorized false "
        "report-only true production disabled blockers 4 blocker member-cleanup-module-mutation "
        "blocker production-member-cleanup blocker member-cleanup-ir-mutation "
        "blocker production-member-cleanup-ir-mutation"
    );
    auto requested_member_mutation_apply_authorization =
        orison::lowering::runtime_indexed_member_cleanup_mutation_apply_authorization(
            member_mutation_operation_validation,
            member_mutation_conflict_detection,
            true
        );
    assert(requested_member_mutation_apply_authorization.validation_ready);
    assert(requested_member_mutation_apply_authorization.conflict_free);
    assert(requested_member_mutation_apply_authorization.ir_mutation_requested);
    assert(!requested_member_mutation_apply_authorization.production_gate_enabled);
    assert(!requested_member_mutation_apply_authorization.authorization_ready);
    assert(!requested_member_mutation_apply_authorization.apply_authorized);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_apply_authorization_report(
            requested_member_mutation_apply_authorization
        ) ==
        "runtime-index member cleanup mutation-apply-authorization owner items index (index + zero) "
        "element Box moved Inner member-path item validation ready conflict-free true "
        "ir-mutation requested production-gate disabled apply-requested false authorization blocked "
        "apply-authorized false "
        "report-only true production disabled blockers 3 blocker member-cleanup-module-mutation "
        "blocker production-member-cleanup blocker production-member-cleanup-ir-mutation"
    );
    auto gate_requested_member_mutation_apply_authorization =
        orison::lowering::runtime_indexed_member_cleanup_mutation_apply_authorization(
            member_mutation_operation_validation,
            member_mutation_conflict_detection,
            requested_member_typed_promotion_gate.ir_mutation_enabled,
            requested_member_typed_promotion_gate.production_gate_enabled
        );
    assert(gate_requested_member_mutation_apply_authorization.validation_ready);
    assert(gate_requested_member_mutation_apply_authorization.conflict_free);
    assert(gate_requested_member_mutation_apply_authorization.ir_mutation_requested);
    assert(gate_requested_member_mutation_apply_authorization.production_gate_enabled);
    assert(gate_requested_member_mutation_apply_authorization.authorization_ready);
    assert(!gate_requested_member_mutation_apply_authorization.apply_authorized);
    assert(gate_requested_member_mutation_apply_authorization.blockers.empty());
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_apply_authorization_report(
            gate_requested_member_mutation_apply_authorization
        ) ==
        "runtime-index member cleanup mutation-apply-authorization owner items index (index + zero) "
        "element Box moved Inner member-path item validation ready conflict-free true "
        "ir-mutation requested production-gate enabled apply-requested false authorization ready "
        "apply-authorized false "
        "report-only true production disabled blockers 0"
    );
    auto gate_requested_member_mutation_apply_preview =
        orison::lowering::runtime_indexed_member_cleanup_mutation_apply_preview(
            member_mutation_operation_plan,
            gate_requested_member_mutation_apply_authorization
        );
    assert(gate_requested_member_mutation_apply_preview.authorization_ready);
    assert(!gate_requested_member_mutation_apply_preview.apply_authorized);
    assert(gate_requested_member_mutation_apply_preview.preview_ready);
    assert(!gate_requested_member_mutation_apply_preview.actions_applied);
    assert(gate_requested_member_mutation_apply_preview.blockers.empty());
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_apply_preview_report(
            gate_requested_member_mutation_apply_preview
        ).starts_with(
            "runtime-index member cleanup mutation-apply-preview owner items index (index + zero) "
            "element Box moved Inner member-path item authorization ready apply-authorized false "
            "preview ready actions 3 actions-applied false report-only true production disabled blockers 0 "
        )
    );
    auto apply_requested_member_mutation_apply_authorization =
        orison::lowering::runtime_indexed_member_cleanup_mutation_apply_authorization(
            member_mutation_operation_validation,
            member_mutation_conflict_detection,
            requested_member_typed_promotion_gate.ir_mutation_enabled,
            requested_member_typed_promotion_gate.production_gate_enabled,
            true
        );
    assert(apply_requested_member_mutation_apply_authorization.validation_ready);
    assert(apply_requested_member_mutation_apply_authorization.conflict_free);
    assert(apply_requested_member_mutation_apply_authorization.ir_mutation_requested);
    assert(apply_requested_member_mutation_apply_authorization.production_gate_enabled);
    assert(apply_requested_member_mutation_apply_authorization.apply_authorization_requested);
    assert(apply_requested_member_mutation_apply_authorization.authorization_ready);
    assert(apply_requested_member_mutation_apply_authorization.apply_authorized);
    assert(!apply_requested_member_mutation_apply_authorization.report_only);
    assert(apply_requested_member_mutation_apply_authorization.production_enabled);
    assert(apply_requested_member_mutation_apply_authorization.blockers.empty());
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_apply_authorization_report(
            apply_requested_member_mutation_apply_authorization
        ) ==
        "runtime-index member cleanup mutation-apply-authorization owner items index (index + zero) "
        "element Box moved Inner member-path item validation ready conflict-free true "
        "ir-mutation requested production-gate enabled apply-requested true authorization ready "
        "apply-authorized true report-only false production enabled blockers 0"
    );
    auto apply_requested_member_mutation_apply_preview =
        orison::lowering::runtime_indexed_member_cleanup_mutation_apply_preview(
            member_mutation_operation_plan,
            apply_requested_member_mutation_apply_authorization
        );
    assert(apply_requested_member_mutation_apply_preview.authorization_ready);
    assert(apply_requested_member_mutation_apply_preview.apply_authorized);
    assert(apply_requested_member_mutation_apply_preview.preview_ready);
    assert(apply_requested_member_mutation_apply_preview.actions_applied);
    assert(!apply_requested_member_mutation_apply_preview.report_only);
    assert(apply_requested_member_mutation_apply_preview.production_enabled);
    assert(apply_requested_member_mutation_apply_preview.blockers.empty());
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_apply_preview_report(
            apply_requested_member_mutation_apply_preview
        ).starts_with(
            "runtime-index member cleanup mutation-apply-preview owner items index (index + zero) "
            "element Box moved Inner member-path item authorization ready apply-authorized true "
            "preview ready actions 3 actions-applied true report-only false production enabled blockers 0 "
        )
    );
    auto apply_requested_member_mutation_post_apply_verification =
        orison::lowering::runtime_indexed_member_cleanup_mutation_post_apply_verification(
            apply_requested_member_mutation_apply_preview
        );
    assert(apply_requested_member_mutation_post_apply_verification.preview_ready);
    assert(apply_requested_member_mutation_post_apply_verification.apply_authorized);
    assert(apply_requested_member_mutation_post_apply_verification.actions_applied);
    assert(apply_requested_member_mutation_post_apply_verification.expected_checks_ready);
    assert(apply_requested_member_mutation_post_apply_verification.verification_ready);
    assert(!apply_requested_member_mutation_post_apply_verification.report_only);
    assert(apply_requested_member_mutation_post_apply_verification.production_enabled);
    assert(apply_requested_member_mutation_post_apply_verification.blockers.empty());
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_post_apply_verification_report(
            apply_requested_member_mutation_post_apply_verification
        ) ==
        "runtime-index member cleanup mutation-post-apply-verification owner items index (index + zero) "
        "element Box moved Inner member-path item preview ready apply-authorized true "
        "actions-applied true expected-checks 3 expected-checks-ready true verification ready "
        "report-only false production enabled blockers 0 expected-check branch-target items.final-cleanup "
        "expected-check cfg-appended items.member_cleanup.exit expected-check phi-predecessor items.member_cleanup.exit"
    );
    auto member_mutation_apply_preview =
        orison::lowering::runtime_indexed_member_cleanup_mutation_apply_preview(
            member_mutation_operation_plan,
            member_mutation_apply_authorization
        );
    assert(!member_mutation_apply_preview.authorization_ready);
    assert(!member_mutation_apply_preview.apply_authorized);
    assert(member_mutation_apply_preview.preview_ready);
    assert(!member_mutation_apply_preview.actions_applied);
    assert(member_mutation_apply_preview.report_only);
    assert(!member_mutation_apply_preview.production_enabled);
    assert(member_mutation_apply_preview.actions.size() == 3);
    assert(member_mutation_apply_preview.actions[0].kind == "branch-replacement");
    assert(member_mutation_apply_preview.actions[1].kind == "cfg-append");
    assert(member_mutation_apply_preview.actions[2].kind == "phi-retarget");
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_apply_preview_report(
            member_mutation_apply_preview
        ) ==
        "runtime-index member cleanup mutation-apply-preview owner items index (index + zero) "
        "element Box moved Inner member-path item authorization blocked apply-authorized false "
        "preview ready actions 3 actions-applied false report-only true production disabled "
        "blockers 4 blocker member-cleanup-module-mutation blocker production-member-cleanup "
        "blocker member-cleanup-ir-mutation blocker production-member-cleanup-ir-mutation "
        "action branch-replacement ready true applied false anchor items.final-cleanup "
        "detail replace `br label %items.final-cleanup` with `br label %items.member_cleanup.entry` "
        "action cfg-append ready true applied false anchor items.member_cleanup.exit "
        "detail append cleanup CFG before-function-closing-brace before closing anchor "
        "action phi-retarget ready true applied false anchor items.member_cleanup.exit "
        "detail retarget PHI predecessor items.final-cleanup to items.member_cleanup.exit"
    );
    auto member_mutation_post_apply_verification =
        orison::lowering::runtime_indexed_member_cleanup_mutation_post_apply_verification(
            member_mutation_apply_preview
        );
    assert(member_mutation_post_apply_verification.preview_ready);
    assert(!member_mutation_post_apply_verification.apply_authorized);
    assert(!member_mutation_post_apply_verification.actions_applied);
    assert(member_mutation_post_apply_verification.expected_checks_ready);
    assert(!member_mutation_post_apply_verification.verification_ready);
    assert(member_mutation_post_apply_verification.report_only);
    assert(!member_mutation_post_apply_verification.production_enabled);
    assert(member_mutation_post_apply_verification.expected_checks.size() == 3);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_post_apply_verification_report(
            member_mutation_post_apply_verification
        ) ==
        "runtime-index member cleanup mutation-post-apply-verification owner items index (index + zero) "
        "element Box moved Inner member-path item preview ready apply-authorized false "
        "actions-applied false expected-checks 3 expected-checks-ready true verification blocked "
        "report-only true production disabled blockers 6 blocker member-cleanup-module-mutation "
        "blocker production-member-cleanup blocker member-cleanup-ir-mutation "
        "blocker production-member-cleanup-ir-mutation blocker member-cleanup-mutation-apply-authorization "
        "blocker member-cleanup-mutation-actions-applied expected-check branch-target items.final-cleanup "
        "expected-check cfg-appended items.member_cleanup.exit expected-check phi-predecessor items.member_cleanup.exit"
    );
    auto member_mutation_promotion_summary =
        orison::lowering::runtime_indexed_member_cleanup_mutation_promotion_summary(
            member_mutation_operation_plan,
            member_mutation_operation_validation,
            member_mutation_conflict_detection,
            member_mutation_apply_authorization,
            member_mutation_apply_preview,
            member_mutation_post_apply_verification
        );
    assert(member_mutation_promotion_summary.operation_count == 3);
    assert(member_mutation_promotion_summary.action_count == 3);
    assert(member_mutation_promotion_summary.expected_check_count == 3);
    assert(member_mutation_promotion_summary.operations_ready);
    assert(member_mutation_promotion_summary.validation_ready);
    assert(member_mutation_promotion_summary.conflict_free);
    assert(!member_mutation_promotion_summary.authorization_ready);
    assert(member_mutation_promotion_summary.preview_ready);
    assert(!member_mutation_promotion_summary.post_apply_verification_ready);
    assert(!member_mutation_promotion_summary.promotion_ready);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_promotion_summary_report(
            member_mutation_promotion_summary
        ) ==
        "runtime-index member cleanup mutation-promotion-summary owner items index (index + zero) "
        "element Box moved Inner member-path item operations 3 operations-ready ready "
        "validation ready conflict-free true authorization blocked preview ready actions 3 "
        "post-apply-verification blocked expected-checks 3 promotion blocked report-only true "
        "production disabled blockers 6 blocker member-cleanup-module-mutation "
        "blocker production-member-cleanup blocker member-cleanup-ir-mutation "
        "blocker production-member-cleanup-ir-mutation blocker member-cleanup-mutation-apply-authorization "
        "blocker member-cleanup-mutation-actions-applied"
    );
    auto member_mutation_production_readiness =
        orison::lowering::runtime_indexed_member_cleanup_mutation_production_readiness(
            member_mutation_promotion_summary
        );
    assert(!member_mutation_production_readiness.promotion_ready);
    assert(!member_mutation_production_readiness.post_apply_verification_ready);
    assert(!member_mutation_production_readiness.authorization_ready);
    assert(!member_mutation_production_readiness.ir_mutation_requested);
    assert(!member_mutation_production_readiness.production_gate_enabled);
    assert(!member_mutation_production_readiness.readiness_ready);
    assert(member_mutation_production_readiness.report_only);
    assert(!member_mutation_production_readiness.production_enabled);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_production_readiness_report(
            member_mutation_production_readiness
        ) ==
        "runtime-index member cleanup mutation-production-readiness owner items index (index + zero) "
        "element Box moved Inner member-path item promotion blocked post-apply-verification blocked "
        "authorization blocked ir-mutation blocked production-gate disabled readiness blocked report-only true "
        "production disabled blockers 8 blocker member-cleanup-module-mutation "
        "blocker production-member-cleanup blocker member-cleanup-ir-mutation "
        "blocker production-member-cleanup-ir-mutation blocker member-cleanup-mutation-apply-authorization "
        "blocker member-cleanup-mutation-actions-applied blocker member-cleanup-mutation-promotion "
        "blocker member-cleanup-mutation-post-apply-verification"
    );
    auto member_mutation_production_readiness_diagnostics =
        orison::lowering::runtime_indexed_member_cleanup_mutation_production_readiness_diagnostics(
            member_mutation_production_readiness
        );
    assert(member_mutation_production_readiness_diagnostics.size() == 8);
    assert(
        member_mutation_production_readiness_diagnostics[0] ==
        "runtime-index member cleanup mutation production blocker owner items index (index + zero) "
        "element Box moved Inner member-path item blocker member-cleanup-module-mutation "
        "detail member cleanup module mutation is disabled"
    );
    assert(
        member_mutation_production_readiness_diagnostics[1] ==
        "runtime-index member cleanup mutation production blocker owner items index (index + zero) "
        "element Box moved Inner member-path item blocker production-member-cleanup "
        "detail production member cleanup is disabled"
    );
    assert(
        member_mutation_production_readiness_diagnostics[2] ==
        "runtime-index member cleanup mutation production blocker owner items index (index + zero) "
        "element Box moved Inner member-path item blocker member-cleanup-ir-mutation "
        "detail member cleanup IR mutation is disabled"
    );
    assert(
        member_mutation_production_readiness_diagnostics[3] ==
        "runtime-index member cleanup mutation production blocker owner items index (index + zero) "
        "element Box moved Inner member-path item blocker production-member-cleanup-ir-mutation "
        "detail production member cleanup IR mutation is disabled"
    );
    assert(
        member_mutation_production_readiness_diagnostics[4] ==
        "runtime-index member cleanup mutation production blocker owner items index (index + zero) "
        "element Box moved Inner member-path item blocker member-cleanup-mutation-apply-authorization "
        "detail member cleanup mutation apply authorization is blocked"
    );
    assert(
        member_mutation_production_readiness_diagnostics[5] ==
        "runtime-index member cleanup mutation production blocker owner items index (index + zero) "
        "element Box moved Inner member-path item blocker member-cleanup-mutation-actions-applied "
        "detail member cleanup mutation actions are not applied"
    );
    assert(
        member_mutation_production_readiness_diagnostics[6] ==
        "runtime-index member cleanup mutation production blocker owner items index (index + zero) "
        "element Box moved Inner member-path item blocker member-cleanup-mutation-promotion "
        "detail member cleanup mutation promotion is blocked"
    );
    assert(
        member_mutation_production_readiness_diagnostics[7] ==
        "runtime-index member cleanup mutation production blocker owner items index (index + zero) "
        "element Box moved Inner member-path item blocker member-cleanup-mutation-post-apply-verification "
        "detail member cleanup mutation post-apply verification is blocked"
    );
    auto member_mutation_readiness_verdict =
        orison::lowering::runtime_indexed_member_cleanup_mutation_readiness_verdict(
            member_mutation_production_readiness
        );
    assert(member_mutation_readiness_verdict.blocker_count == 8);
    assert(member_mutation_readiness_verdict.diagnostic_count == 8);
    assert(!member_mutation_readiness_verdict.readiness_ready);
    assert(!member_mutation_readiness_verdict.guarded_rewrite_ready);
    assert(member_mutation_readiness_verdict.report_only);
    assert(!member_mutation_readiness_verdict.production_enabled);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_readiness_verdict_report(
            member_mutation_readiness_verdict
        ) ==
        "runtime-index member cleanup mutation readiness verdict owner items index (index + zero) "
        "element Box moved Inner member-path item readiness blocked guarded-rewrite blocked blockers 8 "
        "diagnostics 8 report-only true production disabled"
    );
    auto member_mutation_rewrite_authorization =
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_authorization(
            member_mutation_readiness_verdict
        );
    assert(!member_mutation_rewrite_authorization.verdict_ready);
    assert(!member_mutation_rewrite_authorization.guarded_rewrite_ready);
    assert(!member_mutation_rewrite_authorization.authorization_ready);
    assert(!member_mutation_rewrite_authorization.rewrite_authorization_requested);
    assert(!member_mutation_rewrite_authorization.rewrite_authorized);
    assert(member_mutation_rewrite_authorization.report_only);
    assert(!member_mutation_rewrite_authorization.production_enabled);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_authorization_report(
            member_mutation_rewrite_authorization
        ) ==
        "runtime-index member cleanup mutation rewrite authorization owner items index (index + zero) "
        "element Box moved Inner member-path item verdict blocked guarded-rewrite blocked "
        "authorization blocked rewrite-requested false rewrite-authorized false report-only true "
        "production disabled blockers 2 blocker member-cleanup-mutation-readiness-verdict "
        "blocker member-cleanup-mutation-guarded-rewrite"
    );
    auto member_mutation_rewrite_authorization_diagnostics =
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_authorization_diagnostics(
            member_mutation_rewrite_authorization
        );
    assert(member_mutation_rewrite_authorization_diagnostics.size() == 2);
    assert(
        member_mutation_rewrite_authorization_diagnostics[0] ==
        "runtime-index member cleanup mutation rewrite authorization blocker owner items index (index + zero) "
        "element Box moved Inner member-path item blocker member-cleanup-mutation-readiness-verdict "
        "detail member cleanup mutation readiness verdict is blocked"
    );
    assert(
        member_mutation_rewrite_authorization_diagnostics[1] ==
        "runtime-index member cleanup mutation rewrite authorization blocker owner items index (index + zero) "
        "element Box moved Inner member-path item blocker member-cleanup-mutation-guarded-rewrite "
        "detail member cleanup mutation guarded rewrite is blocked"
    );
    auto member_mutation_rewrite_execution_plan =
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_plan(
            member_mutation_rewrite_authorization
        );
    assert(!member_mutation_rewrite_execution_plan.authorization_ready);
    assert(!member_mutation_rewrite_execution_plan.rewrite_authorized);
    assert(!member_mutation_rewrite_execution_plan.execution_plan_ready);
    assert(!member_mutation_rewrite_execution_plan.execution_requested);
    assert(!member_mutation_rewrite_execution_plan.execution_enabled);
    assert(member_mutation_rewrite_execution_plan.report_only);
    assert(!member_mutation_rewrite_execution_plan.production_enabled);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_report(
            member_mutation_rewrite_execution_plan
        ) ==
        "runtime-index member cleanup mutation rewrite execution-plan owner items index (index + zero) "
        "element Box moved Inner member-path item authorization blocked rewrite-authorized false "
        "execution-plan blocked execution-requested false execution disabled report-only true "
        "production disabled blockers 2 blocker member-cleanup-mutation-rewrite-authorization "
        "blocker member-cleanup-mutation-rewrite-not-authorized"
    );
    auto member_mutation_rewrite_execution_plan_diagnostics =
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_diagnostics(
            member_mutation_rewrite_execution_plan
        );
    assert(member_mutation_rewrite_execution_plan_diagnostics.size() == 2);
    assert(
        member_mutation_rewrite_execution_plan_diagnostics[0] ==
        "runtime-index member cleanup mutation rewrite execution-plan blocker owner items index (index + zero) "
        "element Box moved Inner member-path item blocker member-cleanup-mutation-rewrite-authorization "
        "detail member cleanup mutation rewrite authorization is blocked"
    );
    assert(
        member_mutation_rewrite_execution_plan_diagnostics[1] ==
        "runtime-index member cleanup mutation rewrite execution-plan blocker owner items index (index + zero) "
        "element Box moved Inner member-path item blocker member-cleanup-mutation-rewrite-not-authorized "
        "detail member cleanup mutation rewrite is not authorized"
    );
    auto member_mutation_rewrite_execution_verdict =
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_verdict(
            member_mutation_rewrite_execution_plan
        );
    assert(member_mutation_rewrite_execution_verdict.blocker_count == 2);
    assert(member_mutation_rewrite_execution_verdict.diagnostic_count == 2);
    assert(!member_mutation_rewrite_execution_verdict.execution_plan_ready);
    assert(!member_mutation_rewrite_execution_verdict.execution_enabled);
    assert(member_mutation_rewrite_execution_verdict.report_only);
    assert(!member_mutation_rewrite_execution_verdict.production_enabled);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_verdict_report(
            member_mutation_rewrite_execution_verdict
        ) ==
        "runtime-index member cleanup mutation rewrite execution verdict owner items index (index + zero) "
        "element Box moved Inner member-path item execution-plan blocked execution disabled blockers 2 "
        "diagnostics 2 report-only true production disabled"
    );
    auto member_mutation_rewrite_promotion_status =
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_promotion_status(
            member_mutation_rewrite_authorization,
            member_mutation_rewrite_execution_plan,
            member_mutation_rewrite_execution_verdict
        );
    assert(member_mutation_rewrite_promotion_status.blocker_count == 2);
    assert(member_mutation_rewrite_promotion_status.diagnostic_count == 2);
    assert(!member_mutation_rewrite_promotion_status.authorization_ready);
    assert(!member_mutation_rewrite_promotion_status.execution_plan_ready);
    assert(!member_mutation_rewrite_promotion_status.execution_verdict_ready);
    assert(!member_mutation_rewrite_promotion_status.promotion_ready);
    assert(member_mutation_rewrite_promotion_status.report_only);
    assert(!member_mutation_rewrite_promotion_status.production_enabled);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_promotion_status_report(
            member_mutation_rewrite_promotion_status
        ) ==
        "runtime-index member cleanup mutation rewrite promotion-status owner items index (index + zero) "
        "element Box moved Inner member-path item authorization blocked execution-plan blocked "
        "execution-verdict blocked promotion blocked blockers 2 diagnostics 2 report-only true "
        "production disabled"
    );
    auto ready_member_mutation_readiness_verdict = member_mutation_readiness_verdict;
    ready_member_mutation_readiness_verdict.blocker_count = 0;
    ready_member_mutation_readiness_verdict.diagnostic_count = 0;
    ready_member_mutation_readiness_verdict.readiness_ready = true;
    ready_member_mutation_readiness_verdict.guarded_rewrite_ready = true;
    auto authorized_member_mutation_rewrite =
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_authorization(
            ready_member_mutation_readiness_verdict,
            true
    );
    assert(authorized_member_mutation_rewrite.authorization_ready);
    assert(authorized_member_mutation_rewrite.rewrite_authorization_requested);
    assert(authorized_member_mutation_rewrite.rewrite_authorized);
    assert(authorized_member_mutation_rewrite.blockers.empty());
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_authorization_report(
            authorized_member_mutation_rewrite
        ) ==
        "runtime-index member cleanup mutation rewrite authorization owner items index (index + zero) "
        "element Box moved Inner member-path item verdict ready guarded-rewrite ready authorization ready "
        "rewrite-requested true rewrite-authorized true report-only true production disabled blockers 0"
    );
    auto enabled_member_mutation_execution_plan =
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_plan(
            authorized_member_mutation_rewrite,
            true
        );
    assert(enabled_member_mutation_execution_plan.authorization_ready);
    assert(enabled_member_mutation_execution_plan.rewrite_authorized);
    assert(enabled_member_mutation_execution_plan.execution_plan_ready);
    assert(enabled_member_mutation_execution_plan.execution_requested);
    assert(enabled_member_mutation_execution_plan.execution_enabled);
    assert(enabled_member_mutation_execution_plan.blockers.empty());
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_report(
            enabled_member_mutation_execution_plan
        ) ==
        "runtime-index member cleanup mutation rewrite execution-plan owner items index (index + zero) "
        "element Box moved Inner member-path item authorization ready rewrite-authorized true "
        "execution-plan ready execution-requested true execution enabled report-only true "
        "production disabled blockers 0"
    );
    auto enabled_member_mutation_execution_verdict =
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_verdict(
            enabled_member_mutation_execution_plan
        );
    assert(enabled_member_mutation_execution_verdict.blocker_count == 0);
    assert(enabled_member_mutation_execution_verdict.diagnostic_count == 0);
    assert(enabled_member_mutation_execution_verdict.execution_plan_ready);
    assert(enabled_member_mutation_execution_verdict.execution_enabled);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_verdict_report(
            enabled_member_mutation_execution_verdict
        ) ==
        "runtime-index member cleanup mutation rewrite execution verdict owner items index (index + zero) "
        "element Box moved Inner member-path item execution-plan ready execution enabled blockers 0 "
        "diagnostics 0 report-only true production disabled"
    );
    auto enabled_member_mutation_promotion_status =
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_promotion_status(
            authorized_member_mutation_rewrite,
            enabled_member_mutation_execution_plan,
            enabled_member_mutation_execution_verdict
        );
    assert(enabled_member_mutation_promotion_status.authorization_ready);
    assert(enabled_member_mutation_promotion_status.execution_plan_ready);
    assert(enabled_member_mutation_promotion_status.execution_verdict_ready);
    assert(enabled_member_mutation_promotion_status.promotion_ready);
    assert(!enabled_member_mutation_promotion_status.production_enabled);
    assert(
        orison::lowering::runtime_indexed_member_cleanup_mutation_rewrite_promotion_status_report(
            enabled_member_mutation_promotion_status
        ) ==
        "runtime-index member cleanup mutation rewrite promotion-status owner items index (index + zero) "
        "element Box moved Inner member-path item authorization ready execution-plan ready "
        "execution-verdict ready promotion ready blockers 0 diagnostics 0 report-only true production disabled"
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
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_plans.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_proofs.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_emission_sketches.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_targets.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_emission_gates.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_ir_insertion_plans.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_ir_composition_plans.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_cfg_slices.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_function_rewrite_candidates.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_function_rewrite_edit_script_plans.size() == 1);
    assert(
        matching_runtime_indexed->runtime_indexed_member_cleanup_function_rewrite_edit_script_validations.size() == 1
    );
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_function_rewrite_staged_apply_plans.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_module_mutation_gates.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_production_readiness.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_promotion_checklists.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_promotion_seams.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_mutation_operation_plans.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_mutation_operation_validations.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_mutation_conflict_detections.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_mutation_apply_authorizations.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_mutation_apply_previews.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_mutation_post_apply_verifications.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_mutation_promotion_summaries.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_mutation_production_readiness.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_mutation_readiness_verdicts.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_mutation_rewrite_authorizations.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_mutation_rewrite_execution_plans.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_mutation_rewrite_execution_verdicts.size() == 1);
    assert(matching_runtime_indexed->runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses.size() == 1);
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

    auto make_runtime_indexed_member_argument = [](std::string owner_name) {
        auto index_argument = orison::syntax::ExpressionSyntax {
            .kind = orison::syntax::ExpressionKind::name,
            .line = 7,
            .text = "index",
        };
        auto index_access = orison::syntax::ExpressionSyntax {
            .kind = orison::syntax::ExpressionKind::index_access,
            .line = 7,
            .left = std::make_unique<orison::syntax::ExpressionSyntax>(
                orison::syntax::ExpressionSyntax {
                    .kind = orison::syntax::ExpressionKind::name,
                    .line = 7,
                    .text = std::move(owner_name),
                }
            ),
        };
        index_access.arguments.push_back(std::move(index_argument));
        return orison::syntax::ExpressionSyntax {
            .kind = orison::syntax::ExpressionKind::member_access,
            .line = 7,
            .text = "payload",
            .left = std::make_unique<orison::syntax::ExpressionSyntax>(
                std::move(index_access)
            ),
        };
    };

    auto runtime_indexed_state = orison::lowering::FunctionLoweringState {};
    runtime_indexed_state.source_type_names.emplace("items", "DynamicArray<Box>");
    auto runtime_indexed_argument = make_runtime_indexed_member_argument("items");
    auto runtime_indexed_owner = orison::lowering::runtime_indexed_partial_owner_for_constructor_argument(
        runtime_indexed_argument,
        "Payload",
        context,
        runtime_indexed_state
    );
    assert(runtime_indexed_owner.has_value());
    assert(runtime_indexed_owner->owner_name == "items");
    assert(runtime_indexed_owner->index_expression_text == "index");
    assert(runtime_indexed_owner->element_source_type_name == "Box");
    assert(runtime_indexed_owner->element_llvm_type_name == "%record.Box");
    assert(runtime_indexed_owner->owner_llvm_type_name == "{ ptr, i64, i64 }");
    assert(runtime_indexed_owner->static_length_value.empty());
    assert(runtime_indexed_owner->moved_source_type_name == "Payload");
    assert((runtime_indexed_owner->moved_member_path == std::vector<std::string> {"payload"}));
    assert(runtime_indexed_owner->cleanup_strategy == "skip-moved-element");
    assert(!runtime_indexed_owner->constructor_move_enabled);

    auto fixed_runtime_indexed_state = orison::lowering::FunctionLoweringState {};
    fixed_runtime_indexed_state.source_type_names.emplace("fixed_items", "Array<Box, 2>");
    auto fixed_runtime_indexed_argument = make_runtime_indexed_member_argument("fixed_items");
    auto fixed_runtime_indexed_owner =
        orison::lowering::runtime_indexed_partial_owner_for_constructor_argument(
            fixed_runtime_indexed_argument,
            "Payload",
            context,
            fixed_runtime_indexed_state
        );
    assert(fixed_runtime_indexed_owner.has_value());
    assert(fixed_runtime_indexed_owner->owner_name == "fixed_items");
    assert(fixed_runtime_indexed_owner->index_expression_text == runtime_indexed_owner->index_expression_text);
    assert(fixed_runtime_indexed_owner->element_source_type_name == runtime_indexed_owner->element_source_type_name);
    assert(fixed_runtime_indexed_owner->element_llvm_type_name == runtime_indexed_owner->element_llvm_type_name);
    assert(fixed_runtime_indexed_owner->moved_source_type_name == runtime_indexed_owner->moved_source_type_name);
    assert(fixed_runtime_indexed_owner->moved_member_path == runtime_indexed_owner->moved_member_path);
    assert(fixed_runtime_indexed_owner->cleanup_strategy == runtime_indexed_owner->cleanup_strategy);
    assert(fixed_runtime_indexed_owner->constructor_move_enabled == runtime_indexed_owner->constructor_move_enabled);
    assert(fixed_runtime_indexed_owner->owner_llvm_type_name == "[2 x %record.Box]");
    assert(fixed_runtime_indexed_owner->static_length_value == "2");
    assert(runtime_indexed_owner->static_length_value.empty());
    assert(fixed_runtime_indexed_owner->element_size_value == runtime_indexed_owner->element_size_value);

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
