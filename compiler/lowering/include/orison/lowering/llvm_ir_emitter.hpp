#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/dynamic_array_cleanup_plan.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/drop_metadata.hpp"
#include "orison/lowering/lowering_options.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/syntax/module_parser.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace orison::lowering {

struct ComputedDynamicArrayForProductionSequenceMetadata {
    std::string enclosing_function_name;
    std::size_t source_line = 0;
    std::string cleanup_owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    std::vector<std::string> rendered_ir;
};

struct ComputedDynamicArrayForDescriptorRenderMetadata {
    std::string enclosing_function_name;
    std::size_t source_line = 0;
    std::string cleanup_owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string descriptor_storage_name;
    std::string descriptor_value_name;
    std::string data_pointer_name;
    std::string length_name;
    std::vector<std::string> rendered_ir;
};

struct ComputedDynamicArrayForLoopControlRenderMetadata {
    std::string enclosing_function_name;
    std::size_t source_line = 0;
    std::string cleanup_owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string condition_block_name;
    std::string body_block_name;
    std::string continue_block_name;
    std::string exit_block_name;
    std::string index_name;
    std::string next_index_name;
    std::string bounds_check_name;
    std::vector<std::string> rendered_ir;
};

struct ComputedDynamicArrayForElementAddressRenderMetadata {
    std::string enclosing_function_name;
    std::size_t source_line = 0;
    std::string cleanup_owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string element_llvm_type_name;
    std::string data_pointer_name;
    std::string index_name;
    std::string element_address_name;
    std::vector<std::string> rendered_ir;
};

struct ComputedDynamicArrayForElementLoadRenderMetadata {
    std::string enclosing_function_name;
    std::size_t source_line = 0;
    std::string cleanup_owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string element_llvm_type_name;
    std::string element_address_name;
    std::string item_value_name;
    std::vector<std::string> rendered_ir;
};

struct ComputedDynamicArrayForLoopContinueRenderMetadata {
    std::string enclosing_function_name;
    std::size_t source_line = 0;
    std::string cleanup_owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string continue_block_name;
    std::string condition_block_name;
    std::string index_name;
    std::string next_index_name;
    std::vector<std::string> rendered_ir;
};

auto format_computed_dynamic_array_for_production_sequence_metadata(
    ComputedDynamicArrayForProductionSequenceMetadata const& metadata
) -> std::string;

auto format_computed_dynamic_array_for_production_sequence_metadata_report(
    std::vector<ComputedDynamicArrayForProductionSequenceMetadata> const& metadata
) -> std::vector<std::string>;

auto format_computed_dynamic_array_for_descriptor_render_metadata(
    ComputedDynamicArrayForDescriptorRenderMetadata const& metadata
) -> std::string;

auto format_computed_dynamic_array_for_descriptor_render_metadata_report(
    std::vector<ComputedDynamicArrayForDescriptorRenderMetadata> const& metadata
) -> std::vector<std::string>;

auto format_computed_dynamic_array_for_loop_control_render_metadata(
    ComputedDynamicArrayForLoopControlRenderMetadata const& metadata
) -> std::string;

auto format_computed_dynamic_array_for_loop_control_render_metadata_report(
    std::vector<ComputedDynamicArrayForLoopControlRenderMetadata> const& metadata
) -> std::vector<std::string>;

auto format_computed_dynamic_array_for_element_address_render_metadata(
    ComputedDynamicArrayForElementAddressRenderMetadata const& metadata
) -> std::string;

auto format_computed_dynamic_array_for_element_address_render_metadata_report(
    std::vector<ComputedDynamicArrayForElementAddressRenderMetadata> const& metadata
) -> std::vector<std::string>;

auto format_computed_dynamic_array_for_element_load_render_metadata(
    ComputedDynamicArrayForElementLoadRenderMetadata const& metadata
) -> std::string;

auto format_computed_dynamic_array_for_element_load_render_metadata_report(
    std::vector<ComputedDynamicArrayForElementLoadRenderMetadata> const& metadata
) -> std::vector<std::string>;

auto format_computed_dynamic_array_for_loop_continue_render_metadata(
    ComputedDynamicArrayForLoopContinueRenderMetadata const& metadata
) -> std::string;

auto format_computed_dynamic_array_for_loop_continue_render_metadata_report(
    std::vector<ComputedDynamicArrayForLoopContinueRenderMetadata> const& metadata
) -> std::vector<std::string>;

struct LlvmIrEmissionResult {
    diagnostics::DiagnosticBag diagnostics;
    std::string ir_text;
    std::vector<ConcurrencyDropCleanupPlan> drop_cleanups;
    std::vector<PlannedDropAction> planned_drop_actions;
    std::vector<PlannedDropDeclaration> planned_drop_declarations;
    std::vector<DynamicArrayRuntimeOperation> dynamic_array_runtime_operations;
    std::vector<DynamicArrayConstructionPlan> dynamic_array_construction_plans;
    std::vector<DynamicArrayDescriptorCleanupPlan> dynamic_array_descriptor_cleanup_plans;
    std::vector<DynamicArrayCleanupObligation> dynamic_array_cleanup_obligations;
    std::vector<DynamicArrayCleanupSequencePlan> dynamic_array_cleanup_sequence_plans;
    std::vector<DynamicArrayCleanupSequenceVerification> dynamic_array_cleanup_sequence_verifications;
    std::optional<DynamicArrayCleanupEmissionCapability> dynamic_array_cleanup_emission_capability;
    std::vector<std::string> dynamic_array_allocation_call_ir;
    std::vector<std::string> test_only_dynamic_array_allocation_call_ir;
    std::vector<std::string> test_only_dynamic_array_grow_call_ir;
    std::vector<std::string> test_only_dynamic_array_deallocation_call_ir;
    std::vector<std::string> test_only_dynamic_array_descriptor_binding_ir;
    std::vector<std::string> test_only_dynamic_array_descriptor_projection_ir;
    std::vector<std::string> test_only_dynamic_array_bounds_check_ir;
    std::vector<std::string> test_only_dynamic_array_element_address_ir;
    std::vector<std::string> test_only_dynamic_array_element_load_ir;
    std::vector<std::string> test_only_dynamic_array_element_store_ir;
    std::vector<std::string> test_only_dynamic_array_descriptor_length_update_ir;
    std::vector<std::string> test_only_dynamic_array_descriptor_write_back_ir;
    std::vector<std::string> test_only_dynamic_array_append_sequence_ir;
    std::vector<std::string> test_only_dynamic_array_grow_sequence_ir;
    std::vector<std::string> test_only_dynamic_array_append_with_grow_sequence_ir;
    std::vector<std::string> test_only_dynamic_array_cleanup_sequence_ir;
    std::vector<std::string> test_only_dynamic_array_descriptor_load_cleanup_sequence_ir;
    std::vector<std::string> test_only_dynamic_array_element_drop_walk_ir;
    std::vector<ComputedDynamicArrayForProductionSequenceMetadata>
        test_only_computed_dynamic_array_for_production_sequences;
    std::vector<std::string> test_only_computed_dynamic_array_for_production_sequence_ir;
    std::vector<ComputedDynamicArrayForDescriptorRenderMetadata>
        test_only_computed_dynamic_array_for_descriptor_renders;
    std::vector<std::string> test_only_computed_dynamic_array_for_descriptor_render_ir;
    std::vector<ComputedDynamicArrayForLoopControlRenderMetadata>
        test_only_computed_dynamic_array_for_loop_control_renders;
    std::vector<std::string> test_only_computed_dynamic_array_for_loop_control_render_ir;
    std::vector<ComputedDynamicArrayForElementAddressRenderMetadata>
        test_only_computed_dynamic_array_for_element_address_renders;
    std::vector<std::string> test_only_computed_dynamic_array_for_element_address_render_ir;
    std::vector<ComputedDynamicArrayForElementLoadRenderMetadata>
        test_only_computed_dynamic_array_for_element_load_renders;
    std::vector<std::string> test_only_computed_dynamic_array_for_element_load_render_ir;
    std::vector<ComputedDynamicArrayForLoopContinueRenderMetadata>
        test_only_computed_dynamic_array_for_loop_continue_renders;
    std::vector<std::string> test_only_computed_dynamic_array_for_loop_continue_render_ir;
    std::vector<semantics::DropLoweringAuthorization> semantic_drop_lowering_authorizations;

    auto has_errors() const -> bool;
    auto render(std::string_view path) const -> std::string;
    auto planned_drop_report() const -> std::vector<std::string>;
    auto dynamic_array_construction_plan_report() const -> std::vector<std::string>;
    auto dynamic_array_descriptor_cleanup_plan_report() const -> std::vector<std::string>;
    auto dynamic_array_cleanup_obligation_report() const -> std::vector<std::string>;
    auto dynamic_array_cleanup_sequence_plan_report() const -> std::vector<std::string>;
    auto dynamic_array_cleanup_sequence_verification_report() const -> std::vector<std::string>;
    auto dynamic_array_cleanup_emission_gate_report() const -> std::vector<std::string>;
    auto dynamic_array_cleanup_emission_capability_report() const -> std::vector<std::string>;
    auto computed_dynamic_array_for_production_sequence_report() const -> std::vector<std::string>;
    auto computed_dynamic_array_for_descriptor_render_report() const -> std::vector<std::string>;
    auto computed_dynamic_array_for_loop_control_render_report() const -> std::vector<std::string>;
    auto computed_dynamic_array_for_element_address_render_report() const -> std::vector<std::string>;
    auto computed_dynamic_array_for_element_load_render_report() const -> std::vector<std::string>;
    auto computed_dynamic_array_for_loop_continue_render_report() const -> std::vector<std::string>;
    auto dynamic_array_runtime_request_report() const -> std::vector<std::string>;
    auto emitted_drop_declaration_report() const -> std::vector<std::string>;
    auto planned_drop_action_report() const -> std::vector<std::string>;
    auto drop_cleanup_authorization_report() const -> std::vector<std::string>;
    auto drop_readiness_snapshot() const -> DropReadinessSnapshot;
    auto drop_readiness_snapshot_report() const -> std::vector<std::string>;
    auto drop_readiness_summary() const -> DropReadinessSummary;
    auto drop_readiness_summary_report() const -> std::vector<std::string>;
    auto drop_readiness_relation_report() const -> std::vector<std::string>;
};

class LlvmIrEmitter {
public:
    auto emit(
        syntax::ModuleSyntax const& module,
        semantics::SemanticAnalysisResult const& semantic_result,
        LlvmIrEmissionOptions const& options = {}
    ) const -> LlvmIrEmissionResult;
    auto emit_metadata(
        syntax::ModuleSyntax const& module,
        semantics::SemanticAnalysisResult const& semantic_result,
        LlvmIrEmissionOptions const& options = {}
    ) const -> LlvmIrEmissionResult;
};

}  // namespace orison::lowering
