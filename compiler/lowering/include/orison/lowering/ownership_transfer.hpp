#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace orison::syntax {
struct ExpressionSyntax;
}

namespace orison::lowering {

struct LoweringContext;
struct FunctionLoweringState;

struct RuntimeIndexedPartialOwner {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string element_llvm_type_name;
    std::string owner_llvm_type_name;
    std::string owner_address_name;
    std::vector<std::string> owner_address_ir_lines;
    std::string static_length_value;
    std::string element_size_value;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::string cleanup_strategy;
    bool constructor_move_enabled = false;
    std::size_t source_line = 0;

    auto operator==(RuntimeIndexedPartialOwner const&) const -> bool = default;
};

struct RuntimeIndexedCleanupSkipPlan {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string element_llvm_type_name;
    std::string owner_llvm_type_name;
    std::string owner_address_name;
    std::vector<std::string> owner_address_ir_lines;
    std::string static_length_value;
    std::string element_size_value;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::string cleanup_operation;
    bool production_cleanup_enabled = false;
    std::size_t source_line = 0;

    auto operator==(RuntimeIndexedCleanupSkipPlan const&) const -> bool = default;
};

struct RuntimeIndexedCleanupProofGate {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string element_llvm_type_name;
    std::string owner_llvm_type_name;
    std::string owner_address_name;
    std::vector<std::string> owner_address_ir_lines;
    std::string static_length_value;
    std::string element_size_value;
    std::string moved_source_type_name;
    std::string cleanup_operation;
    bool owner_known = false;
    bool index_known = false;
    bool type_match = false;
    bool member_cleanup_proof_ready = false;
    bool member_cleanup_blocks_whole_element = false;
    bool operation_supported = false;
    bool prerequisites_met = false;
    bool lowering_enabled = false;
    std::size_t source_line = 0;

    auto operator==(RuntimeIndexedCleanupProofGate const&) const -> bool = default;
};

struct RuntimeIndexedCleanupEmissionSketch {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string element_llvm_type_name;
    std::string owner_llvm_type_name;
    std::string owner_address_name;
    std::vector<std::string> owner_address_ir_lines;
    std::string static_length_value;
    std::string element_size_value;
    std::vector<std::string> snippets;
    bool report_only = true;
    bool production_emission_enabled = false;
    std::size_t source_line = 0;

    auto operator==(RuntimeIndexedCleanupEmissionSketch const&) const -> bool = default;
};

struct RuntimeIndexedCleanupCapability {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string element_llvm_type_name;
    std::string owner_llvm_type_name;
    std::string owner_address_name;
    std::vector<std::string> owner_address_ir_lines;
    std::string static_length_value;
    std::string element_size_value;
    bool proof_ready = false;
    bool sketch_ready = false;
    bool prerequisites_ready = false;
    bool production_enabled = false;
    std::size_t source_line = 0;

    auto operator==(RuntimeIndexedCleanupCapability const&) const -> bool = default;
};

struct RuntimeIndexedCleanupIrPlan {
    std::string owner_name;
    std::string index_expression_text;
    std::string index_operand_value;
    std::string element_source_type_name;
    std::string element_llvm_type_name;
    std::string owner_llvm_type_name;
    std::string owner_address_name;
    std::vector<std::string> owner_address_ir_lines;
    std::string static_length_value;
    std::string element_size_value;
    std::size_t source_line = 0;
    std::string descriptor_value_name;
    std::string descriptor_data_value_name;
    std::string descriptor_capacity_value_name;
    std::string entry_block_name;
    std::string length_value_name;
    std::string condition_block_name;
    std::string cleanup_index_name;
    std::string bounds_check_name;
    std::string live_check_block_name;
    std::string skip_check_name;
    std::string skip_block_name;
    std::string drop_block_name;
    std::string element_address_name;
    std::string drop_callee_name;
    std::string continue_block_name;
    std::string next_index_name;
    std::string exit_block_name;
    std::string deallocate_callee_name;
    bool owner_address_ready = false;
    bool static_length_ready = false;
    bool descriptor_owner_ready = false;
    bool owner_deallocation_required = true;
    bool labels_ready = false;
    bool operands_ready = false;
    bool calls_ready = false;
    bool complete = false;

    auto operator==(RuntimeIndexedCleanupIrPlan const&) const -> bool = default;
};

struct RuntimeIndexedCleanupEmissionPlan {
    std::string function_symbol_name;
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string element_llvm_type_name;
    std::string owner_llvm_type_name;
    std::string owner_address_name;
    std::vector<std::string> owner_address_ir_lines;
    std::string static_length_value;
    std::string element_size_value;
    std::size_t source_line = 0;
    std::string function_insertion_block_name;
    std::string function_predecessor_block_name;
    std::string function_continuation_block_name;
    std::vector<std::string> operation_names;
    std::vector<std::string> comment_ir_preview_lines;
    std::vector<std::string> gated_ir_slice_lines;
    RuntimeIndexedCleanupIrPlan ir_plan;
    bool prerequisites_ready = false;
    bool production_gate_requested = false;
    bool production_enabled = false;
    bool length_load_planned = false;
    bool length_load_slice_lowerable = false;
    bool loop_planned = false;
    bool loop_block_slice_lowerable = false;
    bool skip_planned = false;
    bool skip_branch_slice_lowerable = false;
    bool live_element_drop_planned = false;
    bool live_element_drop_slice_lowerable = false;
    bool owner_deallocation_planned = false;
    bool cleanup_tail_slice_lowerable = false;
    bool function_insertion_target_known = false;
    bool function_insertion_planned = false;
    std::size_t operation_count = 0;
    std::size_t comment_ir_preview_line_count = 0;
    std::size_t gated_ir_slice_line_count = 0;

    auto operator==(RuntimeIndexedCleanupEmissionPlan const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupPlan {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    bool owner_known = false;
    bool index_known = false;
    bool element_type_known = false;
    bool moved_type_known = false;
    bool moved_member_path_known = false;
    bool cleanup_element_matches_move = false;
    bool member_granular_cleanup_required = false;
    bool prerequisites_met = false;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupPlan const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupProof {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    bool plan_ready = false;
    bool whole_element_cleanup_matches_move = false;
    bool member_cleanup_required = false;
    bool member_scope_proven = false;
    bool whole_element_cleanup_blocked = false;
    bool prerequisites_met = false;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupProof const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupEmissionSketch {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::vector<std::string> snippets;
    bool proof_ready = false;
    bool report_only = true;
    bool production_emission_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupEmissionSketch const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupTarget {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::string cleanup_operation;
    std::string drop_metadata_symbol_name;
    bool metadata_ready = false;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupTarget const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupSiblingField {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::vector<std::string> field_path;
    std::vector<std::size_t> field_indices;
    std::vector<std::string> container_llvm_type_names;
    std::string field_name;
    std::string field_source_type_name;
    std::string field_llvm_type_name;
    std::string drop_symbol_name;
    std::size_t field_index = 0;
    bool drop_definition_available = false;

    auto operator==(RuntimeIndexedMemberCleanupSiblingField const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupHelperDropBindings {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::string helper_symbol_name;
    std::size_t sibling_binding_count = 0;
    bool all_drop_definitions_available = false;
    bool nested_member_path = false;
    bool helper_definition_ready = false;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupHelperDropBindings const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupEmissionGate {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::vector<std::string> blockers;
    bool sketch_ready = false;
    bool member_drop_metadata_ready = false;
    bool ir_insertion_ready = false;
    bool prerequisites_met = false;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupEmissionGate const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupIrInsertionPlan {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::string insertion_anchor;
    std::string entry_block_name;
    std::string skip_block_name;
    std::string sibling_drop_block_name;
    std::string preserve_block_name;
    std::string exit_block_name;
    std::vector<std::string> preview_operations;
    bool target_metadata_ready = false;
    bool insertion_points_named = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupIrInsertionPlan const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupIrCompositionPlan {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::string insertion_anchor;
    std::string entry_block_name;
    std::string skip_block_name;
    std::string sibling_drop_block_name;
    std::string preserve_block_name;
    std::string exit_block_name;
    std::string member_cleanup_target_symbol_name;
    std::vector<std::string> topology_edges;
    bool insertion_plan_ready = false;
    bool block_topology_ready = false;
    bool preview_operations_ready = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupIrCompositionPlan const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupCfgSlice {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::string insertion_anchor;
    std::string entry_block_name;
    std::string skip_block_name;
    std::string sibling_drop_block_name;
    std::string preserve_block_name;
    std::string exit_block_name;
    std::string member_cleanup_target_symbol_name;
    std::vector<std::string> cfg_lines;
    bool composition_ready = false;
    bool slice_rendered = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupCfgSlice const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupFunctionRewriteCandidate {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::string insertion_anchor;
    std::string entry_block_name;
    std::string skip_block_name;
    std::string sibling_drop_block_name;
    std::string preserve_block_name;
    std::string exit_block_name;
    std::string member_cleanup_target_symbol_name;
    std::string replaced_terminator_text;
    std::string replacement_branch_text;
    std::vector<std::string> appended_cfg_preview_lines;
    bool cfg_slice_ready = false;
    bool anchor_ready = false;
    bool branch_rewrite_planned = false;
    bool cfg_append_planned = false;
    bool candidate_available = false;
    bool candidate_verified = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupFunctionRewriteCandidate const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan {
    std::string function_symbol_name;
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::string insertion_anchor;
    std::string entry_block_name;
    std::string sibling_drop_block_name;
    std::string preserve_block_name;
    std::string exit_block_name;
    std::string member_cleanup_target_symbol_name;
    std::string expected_branch_text;
    std::string replacement_branch_text;
    std::string cleanup_cfg_append_placement;
    std::string expected_closing_text;
    std::vector<std::string> appended_cfg_preview_lines;
    std::string phi_old_predecessor_block_name;
    std::string phi_new_predecessor_block_name;
    bool candidate_verified = false;
    bool branch_replacement_ready = false;
    bool cleanup_cfg_append_ready = false;
    bool phi_retarget_ready = false;
    bool edit_script_ready = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupFunctionRewriteEditScriptValidation {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::string insertion_anchor;
    std::string entry_block_name;
    std::string exit_block_name;
    std::vector<std::string> blockers;
    bool edit_script_ready = false;
    bool branch_replacement_valid = false;
    bool cleanup_cfg_append_valid = false;
    bool phi_retarget_valid = false;
    bool validation_ready = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupFunctionRewriteEditScriptValidation const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupFunctionRewriteStagedApplyPlan {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::string insertion_anchor;
    std::string entry_block_name;
    std::string exit_block_name;
    std::vector<std::string> blockers;
    bool validation_ready = false;
    bool branch_replacement_planned = false;
    bool cleanup_cfg_append_planned = false;
    bool phi_retarget_planned = false;
    bool staged_apply_ready = false;
    bool branch_replacement_applied = false;
    bool cleanup_cfg_appended = false;
    bool phi_retarget_applied = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupFunctionRewriteStagedApplyPlan const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupModuleMutationGate {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::string insertion_anchor;
    std::string entry_block_name;
    std::string skip_block_name;
    std::string sibling_drop_block_name;
    std::string preserve_block_name;
    std::string exit_block_name;
    std::vector<std::string> blockers;
    bool cfg_slice_ready = false;
    bool edit_script_validation_ready = false;
    bool staged_apply_ready = false;
    bool module_mutation_enabled = false;
    bool production_member_cleanup_enabled = false;
    bool prerequisites_met = false;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupModuleMutationGate const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupProductionReadiness {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::vector<std::string> blockers;
    bool proof_ready = false;
    bool target_metadata_ready = false;
    bool helper_drop_bindings_ready = false;
    bool cfg_slice_ready = false;
    bool module_mutation_ready = false;
    bool production_member_cleanup_ready = false;
    bool production_gate_ready = false;
    bool production_enabled = false;
    bool production_ready = false;

    auto operator==(RuntimeIndexedMemberCleanupProductionReadiness const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupPromotionChecklist {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::vector<std::string> blockers;
    bool rewrite_candidate_ready = false;
    bool edit_script_ready = false;
    bool validation_ready = false;
    bool staged_apply_ready = false;
    bool module_mutation_ready = false;
    bool production_readiness_ready = false;
    bool promotion_ready = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupPromotionChecklist const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupPromotionSeam {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::vector<std::string> blockers;
    bool checklist_ready = false;
    bool mutation_seam_selected = false;
    bool ir_mutation_enabled = false;
    bool production_gate_enabled = false;
    bool promotion_ready = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupPromotionSeam const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupTypedPromotionGate {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::vector<std::string> blockers;
    bool checklist_ready = false;
    bool ir_mutation_requested = false;
    bool production_gate_requested = false;
    bool ir_mutation_enabled = false;
    bool production_gate_enabled = false;
    bool gate_ready = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupTypedPromotionGate const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupMutationOperation {
    std::string kind;
    std::string anchor;
    std::string expected_text;
    std::string replacement_text;
    std::string placement;
    std::string old_predecessor;
    std::string new_predecessor;
    bool ready = false;
    bool applied = false;

    auto operator==(RuntimeIndexedMemberCleanupMutationOperation const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupMutationOperationPlan {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::vector<RuntimeIndexedMemberCleanupMutationOperation> operations;
    std::vector<std::string> blockers;
    bool seam_selected = false;
    bool operations_ready = false;
    bool operations_applied = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupMutationOperationPlan const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupMutationOperationValidation {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::vector<std::string> blockers;
    bool seam_selected = false;
    bool operation_count_valid = false;
    bool operation_order_valid = false;
    bool branch_replacement_fields_valid = false;
    bool cfg_append_fields_valid = false;
    bool phi_retarget_fields_valid = false;
    bool operations_ready = false;
    bool no_operations_applied = false;
    bool validation_ready = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupMutationOperationValidation const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupMutationConflictDetection {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::vector<std::string> blockers;
    bool validation_ready = false;
    int branch_anchor_match_count = 0;
    int closing_anchor_match_count = 0;
    int phi_predecessor_match_count = 0;
    bool branch_anchor_unique = false;
    bool closing_anchor_unique = false;
    bool phi_predecessor_unique = false;
    bool conflict_free = false;
    bool apply_allowed = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupMutationConflictDetection const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupMutationApplyAuthorization {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::vector<std::string> blockers;
    bool validation_ready = false;
    bool conflict_free = false;
    bool ir_mutation_requested = false;
    bool production_gate_enabled = false;
    bool apply_authorization_requested = false;
    bool authorization_ready = false;
    bool apply_authorized = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupMutationApplyAuthorization const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupMutationApplyPreviewAction {
    std::string kind;
    std::string anchor;
    std::string detail;
    bool ready = false;
    bool applied = false;

    auto operator==(RuntimeIndexedMemberCleanupMutationApplyPreviewAction const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupMutationApplyPreview {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::vector<RuntimeIndexedMemberCleanupMutationApplyPreviewAction> actions;
    std::vector<std::string> blockers;
    bool authorization_ready = false;
    bool apply_authorized = false;
    bool preview_ready = false;
    bool actions_applied = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupMutationApplyPreview const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupMutationPostApplyVerification {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::vector<std::string> expected_checks;
    std::vector<std::string> blockers;
    bool preview_ready = false;
    bool apply_authorized = false;
    bool actions_applied = false;
    bool expected_checks_ready = false;
    bool verification_ready = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupMutationPostApplyVerification const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupMutationPromotionSummary {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::vector<std::string> blockers;
    int operation_count = 0;
    int action_count = 0;
    int expected_check_count = 0;
    bool operations_ready = false;
    bool validation_ready = false;
    bool conflict_free = false;
    bool authorization_ready = false;
    bool ir_mutation_requested = false;
    bool production_gate_enabled = false;
    bool apply_authorized = false;
    bool preview_ready = false;
    bool post_apply_verification_ready = false;
    bool promotion_ready = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupMutationPromotionSummary const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupMutationProductionReadiness {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::vector<std::string> blockers;
    bool promotion_ready = false;
    bool post_apply_verification_ready = false;
    bool authorization_ready = false;
    bool ir_mutation_requested = false;
    bool production_gate_enabled = false;
    bool readiness_ready = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupMutationProductionReadiness const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupMutationReadinessVerdict {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    int blocker_count = 0;
    int diagnostic_count = 0;
    bool readiness_ready = false;
    bool guarded_rewrite_ready = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupMutationReadinessVerdict const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupMutationRewriteAuthorization {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::vector<std::string> blockers;
    bool verdict_ready = false;
    bool guarded_rewrite_ready = false;
    bool authorization_ready = false;
    bool rewrite_authorization_requested = false;
    bool rewrite_authorized = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupMutationRewriteAuthorization const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupMutationRewriteExecutionPlan {
    std::string function_symbol_name;
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    std::vector<std::string> blockers;
    bool authorization_ready = false;
    bool rewrite_authorized = false;
    bool execution_plan_ready = false;
    bool execution_requested = false;
    bool execution_enabled = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupMutationRewriteExecutionPlan const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupMutationRewriteExecutionVerdict {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    int blocker_count = 0;
    int diagnostic_count = 0;
    bool execution_plan_ready = false;
    bool execution_enabled = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupMutationRewriteExecutionVerdict const&) const -> bool = default;
};

struct RuntimeIndexedMemberCleanupMutationRewritePromotionStatus {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::vector<std::string> moved_member_path;
    int blocker_count = 0;
    int diagnostic_count = 0;
    bool authorization_ready = false;
    bool execution_plan_ready = false;
    bool execution_verdict_ready = false;
    bool promotion_ready = false;
    bool report_only = true;
    bool production_enabled = false;

    auto operator==(RuntimeIndexedMemberCleanupMutationRewritePromotionStatus const&) const -> bool = default;
};

struct OwnershipTransferState {
    std::unordered_set<std::string> consumed_owned_bindings;
    std::vector<RuntimeIndexedPartialOwner> runtime_indexed_partial_owners;
    std::vector<RuntimeIndexedCleanupSkipPlan> runtime_indexed_cleanup_skip_plans;
    std::vector<RuntimeIndexedCleanupProofGate> runtime_indexed_cleanup_proof_gates;
    std::vector<RuntimeIndexedCleanupEmissionSketch> runtime_indexed_cleanup_emission_sketches;
    std::vector<RuntimeIndexedCleanupCapability> runtime_indexed_cleanup_capabilities;
    std::vector<RuntimeIndexedCleanupEmissionPlan> runtime_indexed_cleanup_emission_plans;
    std::vector<RuntimeIndexedMemberCleanupPlan> runtime_indexed_member_cleanup_plans;
    std::vector<RuntimeIndexedMemberCleanupProof> runtime_indexed_member_cleanup_proofs;
    std::vector<RuntimeIndexedMemberCleanupEmissionSketch> runtime_indexed_member_cleanup_emission_sketches;
    std::vector<std::vector<RuntimeIndexedMemberCleanupTarget>> runtime_indexed_member_cleanup_targets;
    std::vector<RuntimeIndexedMemberCleanupEmissionGate> runtime_indexed_member_cleanup_emission_gates;
    std::vector<RuntimeIndexedMemberCleanupIrInsertionPlan> runtime_indexed_member_cleanup_ir_insertion_plans;
    std::vector<RuntimeIndexedMemberCleanupIrCompositionPlan> runtime_indexed_member_cleanup_ir_composition_plans;
    std::vector<RuntimeIndexedMemberCleanupCfgSlice> runtime_indexed_member_cleanup_cfg_slices;
    std::vector<RuntimeIndexedMemberCleanupFunctionRewriteCandidate>
        runtime_indexed_member_cleanup_function_rewrite_candidates;
    std::vector<RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan>
        runtime_indexed_member_cleanup_function_rewrite_edit_script_plans;
    std::vector<RuntimeIndexedMemberCleanupFunctionRewriteEditScriptValidation>
        runtime_indexed_member_cleanup_function_rewrite_edit_script_validations;
    std::vector<RuntimeIndexedMemberCleanupFunctionRewriteStagedApplyPlan>
        runtime_indexed_member_cleanup_function_rewrite_staged_apply_plans;
    std::vector<RuntimeIndexedMemberCleanupModuleMutationGate> runtime_indexed_member_cleanup_module_mutation_gates;
    std::vector<RuntimeIndexedMemberCleanupProductionReadiness> runtime_indexed_member_cleanup_production_readiness;
    std::vector<RuntimeIndexedMemberCleanupPromotionChecklist> runtime_indexed_member_cleanup_promotion_checklists;
    std::vector<RuntimeIndexedMemberCleanupTypedPromotionGate> runtime_indexed_member_cleanup_typed_promotion_gates;
    std::vector<RuntimeIndexedMemberCleanupPromotionSeam> runtime_indexed_member_cleanup_promotion_seams;
    std::vector<RuntimeIndexedMemberCleanupMutationOperationPlan>
        runtime_indexed_member_cleanup_mutation_operation_plans;
    std::vector<RuntimeIndexedMemberCleanupMutationOperationValidation>
        runtime_indexed_member_cleanup_mutation_operation_validations;
    std::vector<RuntimeIndexedMemberCleanupMutationConflictDetection>
        runtime_indexed_member_cleanup_mutation_conflict_detections;
    std::vector<RuntimeIndexedMemberCleanupMutationApplyAuthorization>
        runtime_indexed_member_cleanup_mutation_apply_authorizations;
    std::vector<RuntimeIndexedMemberCleanupMutationApplyPreview>
        runtime_indexed_member_cleanup_mutation_apply_previews;
    std::vector<RuntimeIndexedMemberCleanupMutationPostApplyVerification>
        runtime_indexed_member_cleanup_mutation_post_apply_verifications;
    std::vector<RuntimeIndexedMemberCleanupMutationPromotionSummary>
        runtime_indexed_member_cleanup_mutation_promotion_summaries;
    std::vector<RuntimeIndexedMemberCleanupMutationProductionReadiness>
        runtime_indexed_member_cleanup_mutation_production_readiness;
    std::vector<RuntimeIndexedMemberCleanupMutationReadinessVerdict>
        runtime_indexed_member_cleanup_mutation_readiness_verdicts;
    std::vector<RuntimeIndexedMemberCleanupMutationRewriteAuthorization>
        runtime_indexed_member_cleanup_mutation_rewrite_authorizations;
    std::vector<RuntimeIndexedMemberCleanupMutationRewriteExecutionPlan>
        runtime_indexed_member_cleanup_mutation_rewrite_execution_plans;
    std::vector<RuntimeIndexedMemberCleanupMutationRewriteExecutionVerdict>
        runtime_indexed_member_cleanup_mutation_rewrite_execution_verdicts;
    std::vector<RuntimeIndexedMemberCleanupMutationRewritePromotionStatus>
        runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses;
};

struct OwnedAggregateMemberTransfer {
    std::string binding_name;
    std::string owner_name;
    std::string member_name;
    std::string source_type_name;
};

auto mark_owned_binding_consumed(
    OwnershipTransferState& state,
    std::string binding_name
) -> void;

auto record_runtime_indexed_partial_owner(
    OwnershipTransferState& state,
    RuntimeIndexedPartialOwner owner,
    std::string function_predecessor_block_name = {},
    bool production_cleanup_emission_enabled = false,
    bool member_cleanup_ir_mutation_requested = false,
    bool member_cleanup_production_gate_requested = false,
    bool member_cleanup_apply_authorization_requested = false,
    bool member_cleanup_rewrite_execution_requested = false
) -> void;

auto runtime_indexed_partial_owner_for_constructor_argument(
    syntax::ExpressionSyntax const& argument,
    std::string_view expected_source_type,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<RuntimeIndexedPartialOwner>;

auto runtime_indexed_partial_owner_report(
    RuntimeIndexedPartialOwner const& owner
) -> std::string;

auto runtime_indexed_cleanup_skip_plan(
    RuntimeIndexedPartialOwner const& owner
) -> RuntimeIndexedCleanupSkipPlan;

auto runtime_indexed_cleanup_skip_plan_report(
    RuntimeIndexedCleanupSkipPlan const& plan
) -> std::string;

auto runtime_indexed_cleanup_proof_gate(
    RuntimeIndexedCleanupSkipPlan const& plan
) -> RuntimeIndexedCleanupProofGate;

auto runtime_indexed_cleanup_proof_gate_report(
    RuntimeIndexedCleanupProofGate const& gate
) -> std::string;

auto runtime_indexed_cleanup_emission_sketch(
    RuntimeIndexedCleanupProofGate const& gate
) -> RuntimeIndexedCleanupEmissionSketch;

auto runtime_indexed_cleanup_emission_sketch_report(
    RuntimeIndexedCleanupEmissionSketch const& sketch
) -> std::string;

auto runtime_indexed_cleanup_capability(
    RuntimeIndexedCleanupProofGate const& gate,
    RuntimeIndexedCleanupEmissionSketch const& sketch,
    bool production_cleanup_emission_enabled = false
) -> RuntimeIndexedCleanupCapability;

auto runtime_indexed_cleanup_capability_report(
    RuntimeIndexedCleanupCapability const& capability
) -> std::string;

auto runtime_indexed_cleanup_emission_plan(
    RuntimeIndexedCleanupCapability const& capability,
    RuntimeIndexedCleanupEmissionSketch const& sketch,
    bool production_cleanup_emission_enabled = false
) -> RuntimeIndexedCleanupEmissionPlan;

auto runtime_indexed_member_cleanup_plan(
    RuntimeIndexedPartialOwner const& owner
) -> RuntimeIndexedMemberCleanupPlan;

auto runtime_indexed_member_cleanup_plan_report(
    RuntimeIndexedMemberCleanupPlan const& plan
) -> std::string;

auto runtime_indexed_member_cleanup_proof(
    RuntimeIndexedMemberCleanupPlan const& plan
) -> RuntimeIndexedMemberCleanupProof;

auto runtime_indexed_member_cleanup_proof_report(
    RuntimeIndexedMemberCleanupProof const& proof
) -> std::string;

auto runtime_indexed_member_cleanup_emission_sketch(
    RuntimeIndexedMemberCleanupProof const& proof
) -> RuntimeIndexedMemberCleanupEmissionSketch;

auto runtime_indexed_member_cleanup_emission_sketch_report(
    RuntimeIndexedMemberCleanupEmissionSketch const& sketch
) -> std::string;

auto runtime_indexed_member_cleanup_targets(
    RuntimeIndexedMemberCleanupEmissionSketch const& sketch
) -> std::vector<RuntimeIndexedMemberCleanupTarget>;

auto runtime_indexed_member_cleanup_target_report(
    RuntimeIndexedMemberCleanupTarget const& target
) -> std::string;

auto runtime_indexed_member_cleanup_emission_gate(
    RuntimeIndexedMemberCleanupEmissionSketch const& sketch,
    std::vector<RuntimeIndexedMemberCleanupTarget> const& targets = {}
) -> RuntimeIndexedMemberCleanupEmissionGate;

auto runtime_indexed_member_cleanup_emission_gate_report(
    RuntimeIndexedMemberCleanupEmissionGate const& gate
) -> std::string;

auto runtime_indexed_member_cleanup_ir_insertion_plan(
    RuntimeIndexedMemberCleanupEmissionGate const& gate,
    std::vector<RuntimeIndexedMemberCleanupTarget> const& targets = {}
) -> RuntimeIndexedMemberCleanupIrInsertionPlan;

auto runtime_indexed_member_cleanup_ir_insertion_plan_report(
    RuntimeIndexedMemberCleanupIrInsertionPlan const& plan
) -> std::string;

auto runtime_indexed_member_cleanup_ir_composition_plan(
    RuntimeIndexedMemberCleanupIrInsertionPlan const& plan
) -> RuntimeIndexedMemberCleanupIrCompositionPlan;

auto runtime_indexed_member_cleanup_ir_composition_plan_report(
    RuntimeIndexedMemberCleanupIrCompositionPlan const& plan
) -> std::string;

auto runtime_indexed_member_cleanup_cfg_slice(
    RuntimeIndexedMemberCleanupIrCompositionPlan const& plan
) -> RuntimeIndexedMemberCleanupCfgSlice;

auto runtime_indexed_member_cleanup_cfg_slice_report(
    RuntimeIndexedMemberCleanupCfgSlice const& slice
) -> std::string;

auto runtime_indexed_member_cleanup_function_rewrite_candidate(
    RuntimeIndexedMemberCleanupCfgSlice const& slice
) -> RuntimeIndexedMemberCleanupFunctionRewriteCandidate;

auto runtime_indexed_member_cleanup_function_rewrite_candidate_report(
    RuntimeIndexedMemberCleanupFunctionRewriteCandidate const& candidate
) -> std::string;

auto runtime_indexed_member_cleanup_function_rewrite_edit_script_plan(
    RuntimeIndexedMemberCleanupFunctionRewriteCandidate const& candidate
) -> RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan;

auto runtime_indexed_member_cleanup_function_rewrite_edit_script_plan_report(
    RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan const& plan
) -> std::string;

auto runtime_indexed_member_cleanup_function_rewrite_edit_script_validation(
    RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan const& plan
) -> RuntimeIndexedMemberCleanupFunctionRewriteEditScriptValidation;

auto runtime_indexed_member_cleanup_function_rewrite_edit_script_validation_report(
    RuntimeIndexedMemberCleanupFunctionRewriteEditScriptValidation const& validation
) -> std::string;

auto runtime_indexed_member_cleanup_function_rewrite_edit_script_validation_diagnostics(
    RuntimeIndexedMemberCleanupFunctionRewriteEditScriptValidation const& validation
) -> std::vector<std::string>;

auto runtime_indexed_member_cleanup_function_rewrite_staged_apply_plan(
    RuntimeIndexedMemberCleanupFunctionRewriteEditScriptValidation const& validation
) -> RuntimeIndexedMemberCleanupFunctionRewriteStagedApplyPlan;

auto runtime_indexed_member_cleanup_function_rewrite_staged_apply_plan_report(
    RuntimeIndexedMemberCleanupFunctionRewriteStagedApplyPlan const& plan
) -> std::string;

auto runtime_indexed_member_cleanup_function_rewrite_staged_apply_plan_diagnostics(
    RuntimeIndexedMemberCleanupFunctionRewriteStagedApplyPlan const& plan
) -> std::vector<std::string>;

auto runtime_indexed_member_cleanup_module_mutation_gate(
    RuntimeIndexedMemberCleanupCfgSlice const& slice,
    RuntimeIndexedMemberCleanupFunctionRewriteEditScriptValidation const& validation,
    RuntimeIndexedMemberCleanupFunctionRewriteStagedApplyPlan const& staged_apply_plan
) -> RuntimeIndexedMemberCleanupModuleMutationGate;

auto runtime_indexed_member_cleanup_module_mutation_gate_report(
    RuntimeIndexedMemberCleanupModuleMutationGate const& gate
) -> std::string;

auto runtime_indexed_member_cleanup_module_mutation_gate_diagnostics(
    RuntimeIndexedMemberCleanupModuleMutationGate const& gate
) -> std::vector<std::string>;

auto runtime_indexed_member_cleanup_production_readiness(
    RuntimeIndexedMemberCleanupProof const& proof,
    std::vector<RuntimeIndexedMemberCleanupTarget> const& targets,
    std::vector<RuntimeIndexedMemberCleanupHelperDropBindings> const& helper_drop_bindings,
    RuntimeIndexedMemberCleanupCfgSlice const& slice,
    RuntimeIndexedMemberCleanupModuleMutationGate const& gate
) -> RuntimeIndexedMemberCleanupProductionReadiness;

auto runtime_indexed_member_cleanup_production_readiness_report(
    RuntimeIndexedMemberCleanupProductionReadiness const& readiness
) -> std::string;

auto runtime_indexed_member_cleanup_production_blocker_diagnostics(
    RuntimeIndexedMemberCleanupProductionReadiness const& readiness
) -> std::vector<std::string>;

auto runtime_indexed_member_cleanup_helper_drop_bindings_report(
    RuntimeIndexedMemberCleanupHelperDropBindings const& bindings
) -> std::string;

auto runtime_indexed_member_cleanup_promotion_checklist(
    RuntimeIndexedMemberCleanupFunctionRewriteCandidate const& candidate,
    RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan const& edit_script_plan,
    RuntimeIndexedMemberCleanupFunctionRewriteEditScriptValidation const& validation,
    RuntimeIndexedMemberCleanupFunctionRewriteStagedApplyPlan const& staged_apply_plan,
    RuntimeIndexedMemberCleanupModuleMutationGate const& mutation_gate,
    RuntimeIndexedMemberCleanupProductionReadiness const& production_readiness
) -> RuntimeIndexedMemberCleanupPromotionChecklist;

auto runtime_indexed_member_cleanup_promotion_checklist_report(
    RuntimeIndexedMemberCleanupPromotionChecklist const& checklist
) -> std::string;

auto runtime_indexed_member_cleanup_typed_promotion_gate(
    RuntimeIndexedMemberCleanupPromotionChecklist const& checklist,
    bool ir_mutation_requested = false,
    bool production_gate_requested = false
) -> RuntimeIndexedMemberCleanupTypedPromotionGate;

auto runtime_indexed_member_cleanup_typed_promotion_gate_report(
    RuntimeIndexedMemberCleanupTypedPromotionGate const& gate
) -> std::string;

auto runtime_indexed_member_cleanup_promotion_seam(
    RuntimeIndexedMemberCleanupTypedPromotionGate const& gate
) -> RuntimeIndexedMemberCleanupPromotionSeam;

auto runtime_indexed_member_cleanup_promotion_seam(
    RuntimeIndexedMemberCleanupPromotionChecklist const& checklist
) -> RuntimeIndexedMemberCleanupPromotionSeam;

auto runtime_indexed_member_cleanup_promotion_seam_report(
    RuntimeIndexedMemberCleanupPromotionSeam const& seam
) -> std::string;

auto runtime_indexed_member_cleanup_mutation_operation_plan(
    RuntimeIndexedMemberCleanupPromotionSeam const& seam,
    RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan const& edit_script_plan
) -> RuntimeIndexedMemberCleanupMutationOperationPlan;

auto runtime_indexed_member_cleanup_mutation_operation_plan_report(
    RuntimeIndexedMemberCleanupMutationOperationPlan const& plan
) -> std::string;

auto runtime_indexed_member_cleanup_mutation_operation_validation(
    RuntimeIndexedMemberCleanupMutationOperationPlan const& plan
) -> RuntimeIndexedMemberCleanupMutationOperationValidation;

auto runtime_indexed_member_cleanup_mutation_operation_validation_report(
    RuntimeIndexedMemberCleanupMutationOperationValidation const& validation
) -> std::string;

auto runtime_indexed_member_cleanup_mutation_conflict_detection(
    RuntimeIndexedMemberCleanupMutationOperationPlan const& plan,
    RuntimeIndexedMemberCleanupMutationOperationValidation const& validation
) -> RuntimeIndexedMemberCleanupMutationConflictDetection;

auto runtime_indexed_member_cleanup_mutation_conflict_detection_report(
    RuntimeIndexedMemberCleanupMutationConflictDetection const& detection
) -> std::string;

auto runtime_indexed_member_cleanup_mutation_apply_authorization(
    RuntimeIndexedMemberCleanupMutationOperationValidation const& validation,
    RuntimeIndexedMemberCleanupMutationConflictDetection const& detection,
    bool ir_mutation_requested = false,
    bool production_gate_enabled = false,
    bool apply_authorization_requested = false
) -> RuntimeIndexedMemberCleanupMutationApplyAuthorization;

auto runtime_indexed_member_cleanup_mutation_apply_authorization_report(
    RuntimeIndexedMemberCleanupMutationApplyAuthorization const& authorization
) -> std::string;

auto runtime_indexed_member_cleanup_mutation_apply_preview(
    RuntimeIndexedMemberCleanupMutationOperationPlan const& plan,
    RuntimeIndexedMemberCleanupMutationApplyAuthorization const& authorization
) -> RuntimeIndexedMemberCleanupMutationApplyPreview;

auto runtime_indexed_member_cleanup_mutation_apply_preview_report(
    RuntimeIndexedMemberCleanupMutationApplyPreview const& preview
) -> std::string;

auto runtime_indexed_member_cleanup_mutation_post_apply_verification(
    RuntimeIndexedMemberCleanupMutationApplyPreview const& preview
) -> RuntimeIndexedMemberCleanupMutationPostApplyVerification;

auto runtime_indexed_member_cleanup_mutation_post_apply_verification_report(
    RuntimeIndexedMemberCleanupMutationPostApplyVerification const& verification
) -> std::string;

auto runtime_indexed_member_cleanup_mutation_promotion_summary(
    RuntimeIndexedMemberCleanupMutationOperationPlan const& plan,
    RuntimeIndexedMemberCleanupMutationOperationValidation const& validation,
    RuntimeIndexedMemberCleanupMutationConflictDetection const& detection,
    RuntimeIndexedMemberCleanupMutationApplyAuthorization const& authorization,
    RuntimeIndexedMemberCleanupMutationApplyPreview const& preview,
    RuntimeIndexedMemberCleanupMutationPostApplyVerification const& verification
) -> RuntimeIndexedMemberCleanupMutationPromotionSummary;

auto runtime_indexed_member_cleanup_mutation_promotion_summary_report(
    RuntimeIndexedMemberCleanupMutationPromotionSummary const& summary
) -> std::string;

auto runtime_indexed_member_cleanup_mutation_production_readiness(
    RuntimeIndexedMemberCleanupMutationPromotionSummary const& summary
) -> RuntimeIndexedMemberCleanupMutationProductionReadiness;

auto runtime_indexed_member_cleanup_mutation_production_readiness_report(
    RuntimeIndexedMemberCleanupMutationProductionReadiness const& readiness
) -> std::string;

auto runtime_indexed_member_cleanup_mutation_production_readiness_diagnostics(
    RuntimeIndexedMemberCleanupMutationProductionReadiness const& readiness
) -> std::vector<std::string>;

auto runtime_indexed_member_cleanup_mutation_readiness_verdict(
    RuntimeIndexedMemberCleanupMutationProductionReadiness const& readiness
) -> RuntimeIndexedMemberCleanupMutationReadinessVerdict;

auto runtime_indexed_member_cleanup_mutation_readiness_verdict_report(
    RuntimeIndexedMemberCleanupMutationReadinessVerdict const& verdict
) -> std::string;

auto runtime_indexed_member_cleanup_mutation_rewrite_authorization(
    RuntimeIndexedMemberCleanupMutationReadinessVerdict const& verdict,
    bool rewrite_authorization_requested = false
) -> RuntimeIndexedMemberCleanupMutationRewriteAuthorization;

auto runtime_indexed_member_cleanup_mutation_rewrite_authorization_report(
    RuntimeIndexedMemberCleanupMutationRewriteAuthorization const& authorization
) -> std::string;

auto runtime_indexed_member_cleanup_mutation_rewrite_authorization_diagnostics(
    RuntimeIndexedMemberCleanupMutationRewriteAuthorization const& authorization
) -> std::vector<std::string>;

auto runtime_indexed_member_cleanup_mutation_rewrite_execution_plan(
    RuntimeIndexedMemberCleanupMutationRewriteAuthorization const& authorization,
    bool execution_requested = false
) -> RuntimeIndexedMemberCleanupMutationRewriteExecutionPlan;

auto runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_report(
    RuntimeIndexedMemberCleanupMutationRewriteExecutionPlan const& plan
) -> std::string;

auto runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_diagnostics(
    RuntimeIndexedMemberCleanupMutationRewriteExecutionPlan const& plan
) -> std::vector<std::string>;

auto runtime_indexed_member_cleanup_mutation_rewrite_execution_verdict(
    RuntimeIndexedMemberCleanupMutationRewriteExecutionPlan const& plan
) -> RuntimeIndexedMemberCleanupMutationRewriteExecutionVerdict;

auto runtime_indexed_member_cleanup_mutation_rewrite_execution_verdict_report(
    RuntimeIndexedMemberCleanupMutationRewriteExecutionVerdict const& verdict
) -> std::string;

auto runtime_indexed_member_cleanup_mutation_rewrite_promotion_status(
    RuntimeIndexedMemberCleanupMutationRewriteAuthorization const& authorization,
    RuntimeIndexedMemberCleanupMutationRewriteExecutionPlan const& plan,
    RuntimeIndexedMemberCleanupMutationRewriteExecutionVerdict const& verdict
) -> RuntimeIndexedMemberCleanupMutationRewritePromotionStatus;

auto runtime_indexed_member_cleanup_mutation_rewrite_promotion_status_report(
    RuntimeIndexedMemberCleanupMutationRewritePromotionStatus const& status
) -> std::string;

auto render_runtime_indexed_cleanup_ir_plan(
    RuntimeIndexedCleanupIrPlan const& plan
) -> std::vector<std::string>;

auto runtime_indexed_cleanup_emission_plan_report(
    RuntimeIndexedCleanupEmissionPlan const& plan
) -> std::string;

auto runtime_indexed_cleanup_audit_report(
    OwnershipTransferState const& state
) -> std::vector<std::string>;

auto is_owned_binding_consumed(
    OwnershipTransferState const& state,
    std::string_view binding_name
) -> bool;

auto consumed_owned_binding_or_descendant_name(
    OwnershipTransferState const& state,
    std::string_view binding_name
) -> std::optional<std::string>;

auto consumed_owned_descendant_names(
    std::vector<OwnershipTransferState> const& states,
    std::string_view owner_name
) -> std::vector<std::string>;

auto normalize_consumed_owned_descendants(
    std::vector<OwnershipTransferState>& states,
    std::vector<std::string> const& consumed_descendant_names
) -> void;

auto merge_ownership_transfer_states(
    std::vector<OwnershipTransferState> const& branch_states
) -> std::optional<OwnershipTransferState>;

auto owned_binding_member_name(
    std::string_view owner_name,
    std::string_view member_name
) -> std::string;

auto is_owned_transfer_source_type(
    std::string_view source_type_name,
    LoweringContext const& context
) -> bool;

auto owned_record_field_transfer(
    std::string_view owner_name,
    std::string_view owner_source_type_name,
    std::string_view field_name,
    LoweringContext const& context
) -> std::optional<OwnedAggregateMemberTransfer>;

auto owned_record_member_path_transfer(
    std::string_view owner_name,
    std::string_view owner_source_type_name,
    std::span<std::string const> field_names,
    LoweringContext const& context
) -> std::optional<OwnedAggregateMemberTransfer>;

auto owned_choice_payload_transfer(
    std::string_view owner_name,
    std::string_view choice_source_type_name,
    std::string_view variant_name,
    std::string_view payload_name,
    LoweringContext const& context
) -> std::optional<OwnedAggregateMemberTransfer>;

}  // namespace orison::lowering
