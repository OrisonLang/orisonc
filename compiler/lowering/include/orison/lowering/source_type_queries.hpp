#pragma once

#include "orison/lowering/function_lowering_state.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace orison::lowering {

struct ParsedLlvmArrayType {
    std::string element_type;
    std::size_t length = 0;
};

enum class DynamicSequenceKind {
    dynamic_array,
    shared_view,
    exclusive_view,
    view,
};

struct DynamicSequenceSourceType {
    DynamicSequenceKind kind = DynamicSequenceKind::view;
    std::string element_source_type_name;
    bool owns_storage = false;
    bool permits_element_mutation = false;
};

struct DynamicArrayLoweringInvariants {
    std::string_view descriptor_llvm_type;
    bool unique_owner_required = true;
    bool allocator_required = true;
    bool length_capacity_invariant_required = true;
    bool element_drop_walk_required = true;
    bool lowered_signatures_enabled = false;
};

enum class DynamicArrayIterableDescriptorPlanKind {
    not_dynamic_array,
    named_descriptor_owner,
    missing_named_descriptor_storage,
    computed_owner_unproven,
};

enum class DynamicArrayIterableCleanupOwnerProofStatus {
    not_dynamic_array,
    missing_cleanup_plan,
    predicted_owner_local,
    audit_parameter_descriptor,
    proven_bound_parameter_descriptor,
    proven_lowered_local_descriptor,
};

struct DynamicArrayIterableDescriptorPlan {
    DynamicArrayIterableDescriptorPlanKind kind = DynamicArrayIterableDescriptorPlanKind::not_dynamic_array;
    DynamicArrayIterableCleanupOwnerProofStatus cleanup_owner_proof_status =
        DynamicArrayIterableCleanupOwnerProofStatus::not_dynamic_array;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string owner_name;
    std::string descriptor_storage;
    bool can_lower_now = false;
    bool cleanup_owner_proven = false;
};

enum class ComputedDynamicArrayIterableOwnershipPlanKind {
    not_computed_dynamic_array,
    unsupported_computed_shape,
    ternary_branch_owner_mismatch,
    ternary_single_owner_unproven,
    ternary_single_owner_proven,
};

struct ComputedDynamicArrayIterableOwnershipPlan {
    ComputedDynamicArrayIterableOwnershipPlanKind kind =
        ComputedDynamicArrayIterableOwnershipPlanKind::not_computed_dynamic_array;
    std::string source_type_name;
    std::string element_source_type_name;
    std::vector<std::string> branch_owner_names;
    std::vector<DynamicArrayIterableCleanupOwnerProofStatus> branch_cleanup_owner_proof_statuses;
    OwnershipTransferState merged_transfers;
    bool ownership_join_matches = false;
    bool cleanup_owner_proven = false;
};

enum class ComputedDynamicArrayIterableDescriptorHandoffPlanKind {
    not_computed_dynamic_array,
    unsupported_computed_shape,
    ownership_join_blocked,
    cleanup_owner_unproven,
    single_cleanup_owner_handoff_planned,
};

struct ComputedDynamicArrayIterableDescriptorHandoffPlan {
    ComputedDynamicArrayIterableDescriptorHandoffPlanKind kind =
        ComputedDynamicArrayIterableDescriptorHandoffPlanKind::not_computed_dynamic_array;
    ComputedDynamicArrayIterableOwnershipPlan ownership_plan;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string source_owner_name;
    std::string handoff_owner_name;
    std::string descriptor_storage_name;
    bool descriptor_storage_available = false;
    bool cleanup_owner_proven = false;
    bool lowering_enabled = false;
};

enum class ComputedDynamicArrayIterableCleanupSequencePlanKind {
    not_computed_dynamic_array,
    unsupported_computed_shape,
    ownership_join_blocked,
    cleanup_owner_unproven,
    loop_cleanup_sequence_planned,
};

struct ComputedDynamicArrayIterableCleanupSequencePlan {
    ComputedDynamicArrayIterableCleanupSequencePlanKind kind =
        ComputedDynamicArrayIterableCleanupSequencePlanKind::not_computed_dynamic_array;
    ComputedDynamicArrayIterableDescriptorHandoffPlan handoff_plan;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string cleanup_owner_name;
    std::string descriptor_storage_name;
    std::string loop_entry_cleanup_owner_name;
    std::string loop_exit_cleanup_owner_name;
    bool loop_body_has_cleanup_responsibility = false;
    bool function_cleanup_resumes_after_loop = false;
    bool cleanup_sequence_enabled = false;
};

enum class ComputedDynamicArrayIterableDescriptorRenderPlanKind {
    not_computed_dynamic_array,
    unsupported_computed_shape,
    ownership_join_blocked,
    cleanup_owner_unproven,
    descriptor_render_planned,
};

struct ComputedDynamicArrayIterableDescriptorRenderPlan {
    ComputedDynamicArrayIterableDescriptorRenderPlanKind kind =
        ComputedDynamicArrayIterableDescriptorRenderPlanKind::not_computed_dynamic_array;
    ComputedDynamicArrayIterableCleanupSequencePlan cleanup_sequence_plan;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string cleanup_owner_name;
    std::string descriptor_storage_name;
    std::string descriptor_value_name;
    std::string data_pointer_name;
    std::string length_name;
    std::vector<std::string> rendered_ir;
    bool descriptor_load_planned = false;
    bool data_projection_planned = false;
    bool length_projection_planned = false;
    bool render_enabled = false;
};

enum class ComputedDynamicArrayIterableLoopControlRenderPlanKind {
    not_computed_dynamic_array,
    unsupported_computed_shape,
    ownership_join_blocked,
    cleanup_owner_unproven,
    loop_control_render_planned,
};

struct ComputedDynamicArrayIterableLoopControlRenderPlan {
    ComputedDynamicArrayIterableLoopControlRenderPlanKind kind =
        ComputedDynamicArrayIterableLoopControlRenderPlanKind::not_computed_dynamic_array;
    ComputedDynamicArrayIterableDescriptorRenderPlan descriptor_render_plan;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string cleanup_owner_name;
    std::string condition_block_name;
    std::string body_block_name;
    std::string continue_block_name;
    std::string exit_block_name;
    std::string incoming_block_name;
    std::string index_name;
    std::string next_index_name;
    std::string bounds_check_name;
    std::vector<std::string> rendered_ir;
    bool entry_branch_planned = false;
    bool index_phi_planned = false;
    bool bounds_check_planned = false;
    bool conditional_branch_planned = false;
    bool render_enabled = false;
};

enum class ComputedDynamicArrayIterableElementAddressRenderPlanKind {
    not_computed_dynamic_array,
    unsupported_computed_shape,
    ownership_join_blocked,
    cleanup_owner_unproven,
    element_type_unlowerable,
    element_address_render_planned,
};

struct ComputedDynamicArrayIterableElementAddressRenderPlan {
    ComputedDynamicArrayIterableElementAddressRenderPlanKind kind =
        ComputedDynamicArrayIterableElementAddressRenderPlanKind::not_computed_dynamic_array;
    ComputedDynamicArrayIterableLoopControlRenderPlan loop_control_render_plan;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string element_llvm_type_name;
    std::string cleanup_owner_name;
    std::string data_pointer_name;
    std::string index_name;
    std::string element_address_name;
    std::vector<std::string> rendered_ir;
    bool data_pointer_available = false;
    bool index_available = false;
    bool element_address_planned = false;
    bool render_enabled = false;
};

enum class ComputedDynamicArrayIterableElementLoadRenderPlanKind {
    not_computed_dynamic_array,
    unsupported_computed_shape,
    ownership_join_blocked,
    cleanup_owner_unproven,
    element_type_unlowerable,
    element_address_unplanned,
    element_load_render_planned,
};

struct ComputedDynamicArrayIterableElementLoadRenderPlan {
    ComputedDynamicArrayIterableElementLoadRenderPlanKind kind =
        ComputedDynamicArrayIterableElementLoadRenderPlanKind::not_computed_dynamic_array;
    ComputedDynamicArrayIterableElementAddressRenderPlan element_address_render_plan;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string element_llvm_type_name;
    std::string cleanup_owner_name;
    std::string element_address_name;
    std::string item_value_name;
    std::vector<std::string> rendered_ir;
    bool element_address_available = false;
    bool item_value_planned = false;
    bool render_enabled = false;
};

enum class ComputedDynamicArrayIterableLoopContinueRenderPlanKind {
    not_computed_dynamic_array,
    unsupported_computed_shape,
    ownership_join_blocked,
    cleanup_owner_unproven,
    element_type_unlowerable,
    element_address_unplanned,
    element_load_unplanned,
    loop_continue_render_planned,
};

struct ComputedDynamicArrayIterableLoopContinueRenderPlan {
    ComputedDynamicArrayIterableLoopContinueRenderPlanKind kind =
        ComputedDynamicArrayIterableLoopContinueRenderPlanKind::not_computed_dynamic_array;
    ComputedDynamicArrayIterableElementLoadRenderPlan element_load_render_plan;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string cleanup_owner_name;
    std::string continue_block_name;
    std::string condition_block_name;
    std::string index_name;
    std::string next_index_name;
    std::vector<std::string> rendered_ir;
    bool continue_block_planned = false;
    bool next_index_planned = false;
    bool backedge_branch_planned = false;
    bool render_enabled = false;
};

enum class ComputedDynamicArrayIterableLoopRenderSequencePlanKind {
    not_computed_dynamic_array,
    unsupported_computed_shape,
    ownership_join_blocked,
    cleanup_owner_unproven,
    element_type_unlowerable,
    element_address_unplanned,
    element_load_unplanned,
    loop_continue_unplanned,
    loop_render_sequence_planned,
};

struct ComputedDynamicArrayIterableLoopRenderSequencePlan {
    ComputedDynamicArrayIterableLoopRenderSequencePlanKind kind =
        ComputedDynamicArrayIterableLoopRenderSequencePlanKind::not_computed_dynamic_array;
    ComputedDynamicArrayIterableLoopContinueRenderPlan loop_continue_render_plan;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string cleanup_owner_name;
    std::string body_block_name;
    std::vector<std::string> rendered_ir;
    bool descriptor_render_planned = false;
    bool loop_control_render_planned = false;
    bool body_block_planned = false;
    bool element_address_render_planned = false;
    bool element_load_render_planned = false;
    bool loop_continue_render_planned = false;
    bool render_enabled = false;
};

enum class ComputedDynamicArrayIterableLoopExitCleanupPlanKind {
    not_computed_dynamic_array,
    unsupported_computed_shape,
    ownership_join_blocked,
    cleanup_owner_unproven,
    element_type_unlowerable,
    element_address_unplanned,
    element_load_unplanned,
    loop_continue_unplanned,
    loop_render_sequence_unplanned,
    loop_exit_cleanup_planned,
};

struct ComputedDynamicArrayIterableLoopExitCleanupPlan {
    ComputedDynamicArrayIterableLoopExitCleanupPlanKind kind =
        ComputedDynamicArrayIterableLoopExitCleanupPlanKind::not_computed_dynamic_array;
    ComputedDynamicArrayIterableLoopRenderSequencePlan loop_render_sequence_plan;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string cleanup_owner_name;
    std::string exit_block_name;
    std::string loop_entry_cleanup_owner_name;
    std::string loop_exit_cleanup_owner_name;
    std::string cleanup_resumption_operation_name;
    std::vector<std::string> rendered_ir;
    bool exit_block_planned = false;
    bool cleanup_resumption_planned = false;
    bool cleanup_sequence_enabled = false;
    bool render_enabled = false;
};

enum class ComputedDynamicArrayIterableProductionEmissionGatePlanKind {
    not_computed_dynamic_array,
    unsupported_computed_shape,
    ownership_join_blocked,
    cleanup_owner_unproven,
    element_type_unlowerable,
    element_address_unplanned,
    element_load_unplanned,
    loop_continue_unplanned,
    loop_render_sequence_unplanned,
    loop_exit_cleanup_unplanned,
    production_emission_gate_planned,
};

struct ComputedDynamicArrayIterableProductionEmissionGatePlan {
    ComputedDynamicArrayIterableProductionEmissionGatePlanKind kind =
        ComputedDynamicArrayIterableProductionEmissionGatePlanKind::not_computed_dynamic_array;
    ComputedDynamicArrayIterableLoopExitCleanupPlan loop_exit_cleanup_plan;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string cleanup_owner_name;
    std::vector<std::string> rendered_ir;
    bool ownership_ready = false;
    bool loop_render_ready = false;
    bool loop_cleanup_ownership_ready = false;
    bool function_cleanup_resumption_ready = false;
    bool exit_cleanup_ready = false;
    bool production_sequence_render_planned = false;
    bool production_emission_enabled = false;
};

auto split_top_level_generic_arguments(std::string_view text) -> std::vector<std::string>;

auto parse_llvm_array_type(std::string_view type) -> std::optional<ParsedLlvmArrayType>;

auto array_element_source_type_name(std::string_view type_name) -> std::optional<std::string>;

auto dynamic_array_element_source_type_name(std::string_view type_name) -> std::optional<std::string>;

auto view_element_source_type_name(std::string_view type_name) -> std::optional<std::string>;

auto dynamic_sequence_source_type(std::string_view type_name) -> std::optional<DynamicSequenceSourceType>;

auto view_descriptor_llvm_type() -> std::string_view;

auto dynamic_array_descriptor_llvm_type() -> std::string_view;

auto dynamic_array_lowering_invariants() -> DynamicArrayLoweringInvariants;

auto plan_dynamic_array_iterable_descriptor(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> DynamicArrayIterableDescriptorPlan;

auto dynamic_array_iterable_descriptor_plan_report(
    DynamicArrayIterableDescriptorPlan const& plan
) -> std::string;

auto plan_computed_dynamic_array_iterable_ownership_transfer(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> ComputedDynamicArrayIterableOwnershipPlan;

auto computed_dynamic_array_iterable_ownership_plan_report(
    ComputedDynamicArrayIterableOwnershipPlan const& plan
) -> std::string;

auto plan_computed_dynamic_array_iterable_descriptor_handoff(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> ComputedDynamicArrayIterableDescriptorHandoffPlan;

auto computed_dynamic_array_iterable_descriptor_handoff_plan_report(
    ComputedDynamicArrayIterableDescriptorHandoffPlan const& plan
) -> std::string;

auto plan_computed_dynamic_array_iterable_cleanup_sequence(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> ComputedDynamicArrayIterableCleanupSequencePlan;

auto computed_dynamic_array_iterable_cleanup_sequence_plan_report(
    ComputedDynamicArrayIterableCleanupSequencePlan const& plan
) -> std::string;

auto plan_computed_dynamic_array_iterable_descriptor_render(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> ComputedDynamicArrayIterableDescriptorRenderPlan;

auto computed_dynamic_array_iterable_descriptor_render_plan_report(
    ComputedDynamicArrayIterableDescriptorRenderPlan const& plan
) -> std::string;

auto plan_computed_dynamic_array_iterable_loop_control_render(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> ComputedDynamicArrayIterableLoopControlRenderPlan;

auto computed_dynamic_array_iterable_loop_control_render_plan_report(
    ComputedDynamicArrayIterableLoopControlRenderPlan const& plan
) -> std::string;

auto plan_computed_dynamic_array_iterable_element_address_render(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> ComputedDynamicArrayIterableElementAddressRenderPlan;

auto computed_dynamic_array_iterable_element_address_render_plan_report(
    ComputedDynamicArrayIterableElementAddressRenderPlan const& plan
) -> std::string;

auto plan_computed_dynamic_array_iterable_element_load_render(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> ComputedDynamicArrayIterableElementLoadRenderPlan;

auto computed_dynamic_array_iterable_element_load_render_plan_report(
    ComputedDynamicArrayIterableElementLoadRenderPlan const& plan
) -> std::string;

auto plan_computed_dynamic_array_iterable_loop_continue_render(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> ComputedDynamicArrayIterableLoopContinueRenderPlan;

auto computed_dynamic_array_iterable_loop_continue_render_plan_report(
    ComputedDynamicArrayIterableLoopContinueRenderPlan const& plan
) -> std::string;

auto plan_computed_dynamic_array_iterable_loop_render_sequence(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> ComputedDynamicArrayIterableLoopRenderSequencePlan;

auto computed_dynamic_array_iterable_loop_render_sequence_plan_report(
    ComputedDynamicArrayIterableLoopRenderSequencePlan const& plan
) -> std::string;

auto plan_computed_dynamic_array_iterable_loop_exit_cleanup(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> ComputedDynamicArrayIterableLoopExitCleanupPlan;

auto computed_dynamic_array_iterable_loop_exit_cleanup_plan_report(
    ComputedDynamicArrayIterableLoopExitCleanupPlan const& plan
) -> std::string;

auto plan_computed_dynamic_array_iterable_production_emission_gate(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> ComputedDynamicArrayIterableProductionEmissionGatePlan;

auto computed_dynamic_array_iterable_production_emission_gate_plan_report(
    ComputedDynamicArrayIterableProductionEmissionGatePlan const& plan
) -> std::string;

auto is_scalar_or_nonowning_source_type(std::string_view source_type_name) -> bool;

auto pointer_pointee_source_type_name(std::string_view type_name) -> std::optional<std::string>;

auto maybe_payload_source_type_name(std::string_view type_name) -> std::optional<std::string>;

auto source_type_name_for_llvm_type(
    std::string_view llvm_type,
    LoweringContext const& context
) -> std::optional<std::string>;

auto find_record_field(
    LoweredRecordLayout const& layout,
    std::string_view field_name
) -> LoweredRecordField const*;

auto generic_record_constructor_inference_failure_detail(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string>;

auto lowered_type_for_source_type_name(
    std::string_view type_name,
    LoweringContext const& context
) -> std::optional<LoweredType>;

auto llvm_type_for_source_type_name(
    std::string_view type_name,
    LoweringContext const& context
) -> std::optional<std::string>;

auto source_type_name_for_expression(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string>;

auto source_type_name_for_initializer(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state,
    std::string_view lowered_llvm_type
) -> std::optional<std::string>;

auto source_type_name_for_value_statement(
    syntax::StatementSyntax const& statement,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string>;

auto source_type_name_for_value_statement_block(
    std::vector<syntax::StatementSyntax> const& statements,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string>;

auto source_type_name_for_value_statement_block(
    std::vector<std::unique_ptr<syntax::StatementSyntax>> const& statements,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string>;

}  // namespace orison::lowering
