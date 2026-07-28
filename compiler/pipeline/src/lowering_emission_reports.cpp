#include "lowering_emission_reports.hpp"

#include "orison/lowering/dynamic_array_cleanup_plan.hpp"
#include "orison/pipeline/drop_readiness_source_correlation_report.hpp"

#include "dynamic_array_cleanup_readiness.hpp"
#include "computed_cleanup_proof_model.hpp"

namespace orison::pipeline {

namespace {

auto build_computed_dynamic_array_for_production_sequence_state(
    lowering::LlvmIrEmissionResult const& emission
) -> ComputedDynamicArrayForProductionSequenceState {
    auto state = ComputedDynamicArrayForProductionSequenceState {
        .module_comments_emitted = !emission.test_only_computed_dynamic_array_for_production_sequence_module_ir.empty(),
        .sequence_count = emission.test_only_computed_dynamic_array_for_production_sequences.size(),
        .module_comment_line_count =
            emission.test_only_computed_dynamic_array_for_production_sequence_module_ir.size(),
    };
    state.sequence_metadata_available = state.sequence_count > 0;
    state.cleanup_owner_names.reserve(emission.test_only_computed_dynamic_array_for_production_sequences.size());
    for (auto const& sequence : emission.test_only_computed_dynamic_array_for_production_sequences) {
        state.cleanup_owner_names.push_back(sequence.cleanup_owner_name);
        state.rendered_ir_snippet_count += sequence.rendered_ir.size();
    }
    return state;
}

auto build_computed_dynamic_array_for_descriptor_render_state(
    lowering::LlvmIrEmissionResult const& emission
) -> ComputedDynamicArrayForDescriptorRenderState {
    auto const& renders = emission.test_only_computed_dynamic_array_for_descriptor_renders;
    auto state = ComputedDynamicArrayForDescriptorRenderState {
        .render_count = renders.size(),
    };
    state.render_metadata_available = state.render_count > 0;
    state.all_descriptor_projections_ready = state.render_metadata_available;
    state.enclosing_function_names.reserve(renders.size());
    state.cleanup_owner_names.reserve(renders.size());
    state.source_type_names.reserve(renders.size());
    state.element_source_type_names.reserve(renders.size());
    state.descriptor_storage_names.reserve(renders.size());
    state.descriptor_value_names.reserve(renders.size());
    state.data_pointer_names.reserve(renders.size());
    state.length_names.reserve(renders.size());
    state.capacity_names.reserve(renders.size());
    for (auto const& render : renders) {
        state.enclosing_function_names.push_back(render.enclosing_function_name);
        state.cleanup_owner_names.push_back(render.cleanup_owner_name);
        state.source_type_names.push_back(render.source_type_name);
        state.element_source_type_names.push_back(render.element_source_type_name);
        state.descriptor_storage_names.push_back(render.descriptor_storage_name);
        state.descriptor_value_names.push_back(render.descriptor_value_name);
        state.data_pointer_names.push_back(render.data_pointer_name);
        state.length_names.push_back(render.length_name);
        state.capacity_names.push_back(render.capacity_name);
        state.rendered_ir_snippet_count += render.rendered_ir.size();
        state.all_descriptor_projections_ready =
            state.all_descriptor_projections_ready &&
            !render.descriptor_storage_name.empty() &&
            !render.descriptor_value_name.empty() &&
            !render.data_pointer_name.empty() &&
            !render.length_name.empty() &&
            !render.capacity_name.empty();
    }
    return state;
}

auto build_computed_dynamic_array_for_loop_control_render_state(
    lowering::LlvmIrEmissionResult const& emission
) -> ComputedDynamicArrayForLoopControlRenderState {
    auto const& renders = emission.test_only_computed_dynamic_array_for_loop_control_renders;
    auto state = ComputedDynamicArrayForLoopControlRenderState {
        .render_count = renders.size(),
    };
    state.render_metadata_available = state.render_count > 0;
    state.all_control_flow_names_ready = state.render_metadata_available;
    state.enclosing_function_names.reserve(renders.size());
    state.cleanup_owner_names.reserve(renders.size());
    state.source_type_names.reserve(renders.size());
    state.element_source_type_names.reserve(renders.size());
    state.condition_block_names.reserve(renders.size());
    state.body_block_names.reserve(renders.size());
    state.continue_block_names.reserve(renders.size());
    state.exit_block_names.reserve(renders.size());
    state.index_names.reserve(renders.size());
    state.next_index_names.reserve(renders.size());
    state.bounds_check_names.reserve(renders.size());
    for (auto const& render : renders) {
        state.enclosing_function_names.push_back(render.enclosing_function_name);
        state.cleanup_owner_names.push_back(render.cleanup_owner_name);
        state.source_type_names.push_back(render.source_type_name);
        state.element_source_type_names.push_back(render.element_source_type_name);
        state.condition_block_names.push_back(render.condition_block_name);
        state.body_block_names.push_back(render.body_block_name);
        state.continue_block_names.push_back(render.continue_block_name);
        state.exit_block_names.push_back(render.exit_block_name);
        state.index_names.push_back(render.index_name);
        state.next_index_names.push_back(render.next_index_name);
        state.bounds_check_names.push_back(render.bounds_check_name);
        state.rendered_ir_snippet_count += render.rendered_ir.size();
        state.all_control_flow_names_ready =
            state.all_control_flow_names_ready &&
            !render.condition_block_name.empty() &&
            !render.body_block_name.empty() &&
            !render.continue_block_name.empty() &&
            !render.exit_block_name.empty() &&
            !render.index_name.empty() &&
            !render.next_index_name.empty() &&
            !render.bounds_check_name.empty();
    }
    return state;
}

auto build_computed_dynamic_array_for_element_address_render_state(
    lowering::LlvmIrEmissionResult const& emission
) -> ComputedDynamicArrayForElementAddressRenderState {
    auto const& renders = emission.test_only_computed_dynamic_array_for_element_address_renders;
    auto state = ComputedDynamicArrayForElementAddressRenderState {
        .render_count = renders.size(),
    };
    state.render_metadata_available = state.render_count > 0;
    state.all_element_address_inputs_ready = state.render_metadata_available;
    state.enclosing_function_names.reserve(renders.size());
    state.cleanup_owner_names.reserve(renders.size());
    state.source_type_names.reserve(renders.size());
    state.element_source_type_names.reserve(renders.size());
    state.element_llvm_type_names.reserve(renders.size());
    state.data_pointer_names.reserve(renders.size());
    state.index_names.reserve(renders.size());
    state.element_address_names.reserve(renders.size());
    for (auto const& render : renders) {
        state.enclosing_function_names.push_back(render.enclosing_function_name);
        state.cleanup_owner_names.push_back(render.cleanup_owner_name);
        state.source_type_names.push_back(render.source_type_name);
        state.element_source_type_names.push_back(render.element_source_type_name);
        state.element_llvm_type_names.push_back(render.element_llvm_type_name);
        state.data_pointer_names.push_back(render.data_pointer_name);
        state.index_names.push_back(render.index_name);
        state.element_address_names.push_back(render.element_address_name);
        state.rendered_ir_snippet_count += render.rendered_ir.size();
        state.all_element_address_inputs_ready =
            state.all_element_address_inputs_ready &&
            !render.element_llvm_type_name.empty() &&
            !render.data_pointer_name.empty() &&
            !render.index_name.empty() &&
            !render.element_address_name.empty();
    }
    return state;
}

auto build_computed_dynamic_array_for_element_load_render_state(
    lowering::LlvmIrEmissionResult const& emission
) -> ComputedDynamicArrayForElementLoadRenderState {
    auto const& renders = emission.test_only_computed_dynamic_array_for_element_load_renders;
    auto state = ComputedDynamicArrayForElementLoadRenderState {
        .render_count = renders.size(),
    };
    state.render_metadata_available = state.render_count > 0;
    state.all_element_load_inputs_ready = state.render_metadata_available;
    state.enclosing_function_names.reserve(renders.size());
    state.cleanup_owner_names.reserve(renders.size());
    state.source_type_names.reserve(renders.size());
    state.element_source_type_names.reserve(renders.size());
    state.element_llvm_type_names.reserve(renders.size());
    state.element_address_names.reserve(renders.size());
    state.item_value_names.reserve(renders.size());
    for (auto const& render : renders) {
        state.enclosing_function_names.push_back(render.enclosing_function_name);
        state.cleanup_owner_names.push_back(render.cleanup_owner_name);
        state.source_type_names.push_back(render.source_type_name);
        state.element_source_type_names.push_back(render.element_source_type_name);
        state.element_llvm_type_names.push_back(render.element_llvm_type_name);
        state.element_address_names.push_back(render.element_address_name);
        state.item_value_names.push_back(render.item_value_name);
        state.rendered_ir_snippet_count += render.rendered_ir.size();
        state.all_element_load_inputs_ready =
            state.all_element_load_inputs_ready &&
            !render.element_llvm_type_name.empty() &&
            !render.element_address_name.empty() &&
            !render.item_value_name.empty();
    }
    return state;
}

auto build_computed_dynamic_array_for_loop_continue_render_state(
    lowering::LlvmIrEmissionResult const& emission
) -> ComputedDynamicArrayForLoopContinueRenderState {
    auto const& renders = emission.test_only_computed_dynamic_array_for_loop_continue_renders;
    auto state = ComputedDynamicArrayForLoopContinueRenderState {
        .render_count = renders.size(),
    };
    state.render_metadata_available = state.render_count > 0;
    state.all_loop_continue_inputs_ready = state.render_metadata_available;
    state.enclosing_function_names.reserve(renders.size());
    state.cleanup_owner_names.reserve(renders.size());
    state.source_type_names.reserve(renders.size());
    state.element_source_type_names.reserve(renders.size());
    state.continue_block_names.reserve(renders.size());
    state.condition_block_names.reserve(renders.size());
    state.index_names.reserve(renders.size());
    state.next_index_names.reserve(renders.size());
    for (auto const& render : renders) {
        state.enclosing_function_names.push_back(render.enclosing_function_name);
        state.cleanup_owner_names.push_back(render.cleanup_owner_name);
        state.source_type_names.push_back(render.source_type_name);
        state.element_source_type_names.push_back(render.element_source_type_name);
        state.continue_block_names.push_back(render.continue_block_name);
        state.condition_block_names.push_back(render.condition_block_name);
        state.index_names.push_back(render.index_name);
        state.next_index_names.push_back(render.next_index_name);
        state.rendered_ir_snippet_count += render.rendered_ir.size();
        state.all_loop_continue_inputs_ready =
            state.all_loop_continue_inputs_ready &&
            !render.continue_block_name.empty() &&
            !render.condition_block_name.empty() &&
            !render.index_name.empty() &&
            !render.next_index_name.empty();
    }
    return state;
}

auto build_computed_dynamic_array_for_loop_render_sequence_state(
    lowering::LlvmIrEmissionResult const& emission
) -> ComputedDynamicArrayForLoopRenderSequenceState {
    auto const& sequences = emission.test_only_computed_dynamic_array_for_loop_render_sequences;
    auto state = ComputedDynamicArrayForLoopRenderSequenceState {
        .sequence_count = sequences.size(),
    };
    state.sequence_metadata_available = state.sequence_count > 0;
    state.all_body_blocks_ready = state.sequence_metadata_available;
    state.enclosing_function_names.reserve(sequences.size());
    state.cleanup_owner_names.reserve(sequences.size());
    state.source_type_names.reserve(sequences.size());
    state.element_source_type_names.reserve(sequences.size());
    state.body_block_names.reserve(sequences.size());
    for (auto const& sequence : sequences) {
        state.enclosing_function_names.push_back(sequence.enclosing_function_name);
        state.cleanup_owner_names.push_back(sequence.cleanup_owner_name);
        state.source_type_names.push_back(sequence.source_type_name);
        state.element_source_type_names.push_back(sequence.element_source_type_name);
        state.body_block_names.push_back(sequence.body_block_name);
        state.rendered_ir_snippet_count += sequence.rendered_ir.size();
        state.all_body_blocks_ready =
            state.all_body_blocks_ready &&
            !sequence.body_block_name.empty();
    }
    return state;
}

auto build_computed_dynamic_array_for_loop_exit_cleanup_state(
    lowering::LlvmIrEmissionResult const& emission
) -> ComputedDynamicArrayForLoopExitCleanupState {
    auto const& cleanups = emission.test_only_computed_dynamic_array_for_loop_exit_cleanups;
    auto state = ComputedDynamicArrayForLoopExitCleanupState {
        .cleanup_count = cleanups.size(),
    };
    state.cleanup_metadata_available = state.cleanup_count > 0;
    state.all_cleanup_resumptions_ready = state.cleanup_metadata_available;
    state.enclosing_function_names.reserve(cleanups.size());
    state.cleanup_owner_names.reserve(cleanups.size());
    state.source_type_names.reserve(cleanups.size());
    state.element_source_type_names.reserve(cleanups.size());
    state.exit_block_names.reserve(cleanups.size());
    state.loop_entry_cleanup_owner_names.reserve(cleanups.size());
    state.loop_exit_cleanup_owner_names.reserve(cleanups.size());
    state.cleanup_resumption_operation_names.reserve(cleanups.size());
    for (auto const& cleanup : cleanups) {
        state.enclosing_function_names.push_back(cleanup.enclosing_function_name);
        state.cleanup_owner_names.push_back(cleanup.cleanup_owner_name);
        state.source_type_names.push_back(cleanup.source_type_name);
        state.element_source_type_names.push_back(cleanup.element_source_type_name);
        state.exit_block_names.push_back(cleanup.exit_block_name);
        state.loop_entry_cleanup_owner_names.push_back(cleanup.loop_entry_cleanup_owner_name);
        state.loop_exit_cleanup_owner_names.push_back(cleanup.loop_exit_cleanup_owner_name);
        state.cleanup_resumption_operation_names.push_back(cleanup.cleanup_resumption_operation_name);
        state.rendered_ir_snippet_count += cleanup.rendered_ir.size();
        state.all_cleanup_resumptions_ready =
            state.all_cleanup_resumptions_ready &&
            !cleanup.exit_block_name.empty() &&
            !cleanup.loop_entry_cleanup_owner_name.empty() &&
            !cleanup.loop_exit_cleanup_owner_name.empty() &&
            !cleanup.cleanup_resumption_operation_name.empty();
    }
    return state;
}

auto build_computed_dynamic_array_for_cleanup_transition_state(
    lowering::LlvmIrEmissionResult const& emission
) -> ComputedDynamicArrayForCleanupTransitionState {
    auto const& transitions = emission.test_only_computed_dynamic_array_for_cleanup_transitions;
    auto state = ComputedDynamicArrayForCleanupTransitionState {
        .transition_count = transitions.size(),
    };
    state.transition_metadata_available = state.transition_count > 0;
    state.all_transitions_paired = state.transition_metadata_available;
    state.enclosing_function_names.reserve(transitions.size());
    state.cleanup_owner_names.reserve(transitions.size());
    state.source_type_names.reserve(transitions.size());
    state.element_source_type_names.reserve(transitions.size());
    state.acquisition_source_owner_names.reserve(transitions.size());
    state.acquisition_target_owner_names.reserve(transitions.size());
    state.acquisition_operation_names.reserve(transitions.size());
    state.resumption_source_owner_names.reserve(transitions.size());
    state.resumption_target_owner_names.reserve(transitions.size());
    state.resumption_operation_names.reserve(transitions.size());
    for (auto const& transition : transitions) {
        state.enclosing_function_names.push_back(transition.enclosing_function_name);
        state.cleanup_owner_names.push_back(transition.cleanup_owner_name);
        state.source_type_names.push_back(transition.source_type_name);
        state.element_source_type_names.push_back(transition.element_source_type_name);
        state.acquisition_source_owner_names.push_back(transition.acquisition_source_owner_name);
        state.acquisition_target_owner_names.push_back(transition.acquisition_target_owner_name);
        state.acquisition_operation_names.push_back(transition.acquisition_operation_name);
        state.resumption_source_owner_names.push_back(transition.resumption_source_owner_name);
        state.resumption_target_owner_names.push_back(transition.resumption_target_owner_name);
        state.resumption_operation_names.push_back(transition.resumption_operation_name);
        state.all_transitions_paired =
            state.all_transitions_paired &&
            transition.acquisition_target_owner_name == transition.resumption_source_owner_name &&
            transition.acquisition_source_owner_name == transition.resumption_target_owner_name &&
            !transition.acquisition_operation_name.empty() &&
            !transition.resumption_operation_name.empty();
    }
    return state;
}

auto build_computed_dynamic_array_for_production_emission_gate_state(
    lowering::LlvmIrEmissionResult const& emission
) -> ComputedDynamicArrayForProductionEmissionGateState {
    auto state = ComputedDynamicArrayForProductionEmissionGateState {
        .gate_count = emission.test_only_computed_dynamic_array_for_production_emission_gates.size(),
    };
    state.gate_metadata_available = state.gate_count > 0;
    state.all_ownership_ready = state.gate_metadata_available;
    state.all_loop_render_ready = state.gate_metadata_available;
    state.all_loop_cleanup_ownership_ready = state.gate_metadata_available;
    state.all_function_cleanup_resumption_ready = state.gate_metadata_available;
    state.all_exit_cleanup_ready = state.gate_metadata_available;
    state.all_production_sequences_planned = state.gate_metadata_available;
    state.cleanup_owner_names.reserve(emission.test_only_computed_dynamic_array_for_production_emission_gates.size());
    for (auto const& gate : emission.test_only_computed_dynamic_array_for_production_emission_gates) {
        state.cleanup_owner_names.push_back(gate.cleanup_owner_name);
        state.rendered_ir_snippet_count += gate.rendered_ir.size();
        state.all_ownership_ready = state.all_ownership_ready && gate.ownership_ready;
        state.all_loop_render_ready = state.all_loop_render_ready && gate.loop_render_ready;
        state.all_loop_cleanup_ownership_ready =
            state.all_loop_cleanup_ownership_ready && gate.loop_cleanup_ownership_ready;
        state.all_function_cleanup_resumption_ready =
            state.all_function_cleanup_resumption_ready && gate.function_cleanup_resumption_ready;
        state.all_exit_cleanup_ready = state.all_exit_cleanup_ready && gate.exit_cleanup_ready;
        state.all_production_sequences_planned =
            state.all_production_sequences_planned && gate.production_sequence_render_planned;
        state.any_production_emission_enabled =
            state.any_production_emission_enabled || gate.production_emission_enabled;
    }
    return state;
}

auto record_consumed_descriptor_finalization_plan(
    ConsumedDescriptorFinalizationState& state,
    lowering::ConsumedDescriptorFinalizationPlan const& plan
) {
    auto const readiness = lowering::plan_consumed_descriptor_finalization_readiness(plan);
    if (readiness.ready) {
        ++state.ready_plan_count;
    } else {
        ++state.blocked_plan_count;
    }
    if (!plan.cleanup_owner_name.empty()) {
        state.cleanup_owner_names.push_back(plan.cleanup_owner_name);
    }
    if (!plan.descriptor_storage_name.empty()) {
        state.descriptor_storage_names.push_back(plan.descriptor_storage_name);
    }
}

auto build_consumed_descriptor_finalization_state(
    lowering::LlvmIrEmissionResult const& emission
) -> ConsumedDescriptorFinalizationState {
    auto state = ConsumedDescriptorFinalizationState {
        .computed_descriptor_plan_count =
            emission.test_only_computed_dynamic_array_for_consumed_cleanup_descriptors.size(),
        .emitted_finalization_plan_count = emission.consumed_descriptor_finalization_plans.size(),
    };
    for (auto const& descriptor : emission.test_only_computed_dynamic_array_for_consumed_cleanup_descriptors) {
        record_consumed_descriptor_finalization_plan(state, descriptor.finalization_plan);
    }
    for (auto const& plan : emission.consumed_descriptor_finalization_plans) {
        record_consumed_descriptor_finalization_plan(state, plan);
    }
    state.all_ready = state.ready_plan_count > 0 && state.blocked_plan_count == 0;
    return state;
}

auto build_computed_consumed_cleanup_descriptor_model_state(
    lowering::LlvmIrEmissionResult const& emission
) -> ComputedConsumedCleanupDescriptorModelState {
    auto const& descriptors = emission.test_only_computed_dynamic_array_for_consumed_cleanup_descriptors;
    auto state = ComputedConsumedCleanupDescriptorModelState {
        .descriptor_model_count = descriptors.size(),
    };
    state.all_finalization_ready = state.descriptor_model_count > 0;
    state.enclosing_function_names.reserve(descriptors.size());
    state.cleanup_owner_names.reserve(descriptors.size());
    state.descriptor_storage_names.reserve(descriptors.size());
    state.cleanup_operation_names.reserve(descriptors.size());
    state.source_type_names.reserve(descriptors.size());
    state.element_source_type_names.reserve(descriptors.size());
    for (auto const& descriptor : descriptors) {
        auto const readiness = lowering::plan_consumed_descriptor_finalization_readiness(
            descriptor.finalization_plan
        );
        state.enclosing_function_names.push_back(descriptor.enclosing_function_name);
        state.cleanup_owner_names.push_back(descriptor.finalization_plan.cleanup_owner_name);
        state.descriptor_storage_names.push_back(descriptor.finalization_plan.descriptor_storage_name);
        state.cleanup_operation_names.push_back(descriptor.finalization_plan.cleanup_operation_name);
        state.source_type_names.push_back(descriptor.source_type_name);
        state.element_source_type_names.push_back(descriptor.element_source_type_name);
        if (readiness.ready) {
            ++state.ready_model_count;
        } else {
            ++state.blocked_model_count;
        }
        state.all_finalization_ready = state.all_finalization_ready && readiness.ready;
    }
    return state;
}

auto build_computed_consumed_cleanup_descriptor_state(
    ComputedCleanupProofModel const& proof_model
) -> ComputedConsumedCleanupDescriptorState {
    auto state = ComputedConsumedCleanupDescriptorState {
        .descriptor_count = proof_model.cleanup_call_report_events.consumed_descriptor_events.size(),
        .structured_proof_count = proof_model.summary.structured_consumed_cleanup_descriptor_count,
        .ir_fallback_proof_count = proof_model.summary.ir_consumed_cleanup_descriptor_fallback_count,
    };
    state.all_finalized = state.descriptor_count > 0;
    state.cleanup_owner_names.reserve(proof_model.cleanup_call_report_events.consumed_descriptor_events.size());
    state.descriptor_storage_names.reserve(proof_model.cleanup_call_report_events.consumed_descriptor_events.size());
    for (auto const& event : proof_model.cleanup_call_report_events.consumed_descriptor_events) {
        state.cleanup_owner_names.push_back(event.resumption.target_owner_name);
        state.descriptor_storage_names.push_back(event.descriptor_storage_name);
    }
    return state;
}

auto build_computed_inserted_cleanup_call_state(
    ComputedCleanupProofModel const& proof_model
) -> ComputedInsertedCleanupCallState {
    auto state = ComputedInsertedCleanupCallState {
        .call_count = proof_model.cleanup_call_report_events.inserted_call_events.size(),
        .structured_proof_count = proof_model.summary.structured_inserted_cleanup_call_count,
        .ir_fallback_proof_count = proof_model.summary.ir_inserted_cleanup_call_fallback_count,
    };
    state.all_inserted = state.call_count > 0;
    state.cleanup_owner_names.reserve(proof_model.cleanup_call_report_events.inserted_call_events.size());
    state.data_pointer_names.reserve(proof_model.cleanup_call_report_events.inserted_call_events.size());
    state.capacity_names.reserve(proof_model.cleanup_call_report_events.inserted_call_events.size());
    for (auto const& event : proof_model.cleanup_call_report_events.inserted_call_events) {
        state.cleanup_owner_names.push_back(event.resumption.target_owner_name);
        state.data_pointer_names.push_back(event.operands.data_pointer_name);
        state.capacity_names.push_back(event.operands.capacity_name);
    }
    return state;
}

auto build_computed_cleanup_call_insertion_gate_state(
    ComputedCleanupProofModel const& proof_model
) -> ComputedCleanupCallInsertionGateState {
    auto state = ComputedCleanupCallInsertionGateState {
        .gate_count = proof_model.cleanup_call_report_events.insertion_gate_events.size(),
    };
    state.all_state_verified = state.gate_count > 0;
    state.all_operands_proven = state.gate_count > 0;
    state.all_cleanup_calls_authorized = state.gate_count > 0;
    state.cleanup_owner_names.reserve(proof_model.cleanup_call_report_events.insertion_gate_events.size());
    state.cleanup_operation_names.reserve(proof_model.cleanup_call_report_events.insertion_gate_events.size());
    for (auto const& event : proof_model.cleanup_call_report_events.insertion_gate_events) {
        state.cleanup_owner_names.push_back(event.resumption.target_owner_name);
        state.cleanup_operation_names.push_back(event.resumption.operation_name + ".call");
        state.all_state_verified = state.all_state_verified && event.decision.state_verified;
        state.all_operands_proven = state.all_operands_proven && event.decision.operands_proven;
        state.all_cleanup_calls_authorized =
            state.all_cleanup_calls_authorized && event.decision.cleanup_calls_authorized;
        if (event.decision.insertion_ready) {
            ++state.ready_count;
        } else {
            ++state.blocked_count;
        }
    }
    state.all_ready = state.ready_count > 0 && state.blocked_count == 0;
    return state;
}

auto build_computed_cleanup_call_plan_render_state(
    ComputedCleanupProofModel const& proof_model
) -> ComputedCleanupCallPlanRenderState {
    auto state = ComputedCleanupCallPlanRenderState {
        .plan_count = proof_model.cleanup_call_report_events.plan_events.size(),
        .render_count = proof_model.cleanup_call_report_events.render_events.size(),
    };
    state.all_state_verified = state.plan_count > 0;
    state.all_operands_proven = state.plan_count > 0;
    state.all_cleanup_calls_enabled = state.plan_count > 0;
    state.cleanup_owner_names.reserve(proof_model.cleanup_call_report_events.plan_events.size());
    state.cleanup_operation_names.reserve(proof_model.cleanup_call_report_events.plan_events.size());
    state.data_pointer_names.reserve(proof_model.cleanup_call_report_events.plan_events.size());
    state.element_size_bytes.reserve(proof_model.cleanup_call_report_events.plan_events.size());
    state.capacity_names.reserve(proof_model.cleanup_call_report_events.plan_events.size());
    for (auto const& event : proof_model.cleanup_call_report_events.plan_events) {
        auto const state_verified =
            event.acquisition.target_owner_name == event.resumption.source_owner_name &&
            event.acquisition.source_owner_name == event.resumption.target_owner_name;
        auto const operands_proven = computed_cleanup_call_operands_complete(event.operands);
        auto const cleanup_calls_enabled =
            event.acquisition.cleanup_calls_enabled && event.resumption.cleanup_calls_enabled;
        state.cleanup_owner_names.push_back(event.resumption.target_owner_name);
        state.cleanup_operation_names.push_back(event.resumption.operation_name + ".call");
        state.data_pointer_names.push_back(event.operands.data_pointer_name);
        state.element_size_bytes.push_back(event.operands.element_size_bytes);
        state.capacity_names.push_back(event.operands.capacity_name);
        state.all_state_verified = state.all_state_verified && state_verified;
        state.all_operands_proven = state.all_operands_proven && operands_proven;
        state.all_cleanup_calls_enabled = state.all_cleanup_calls_enabled && cleanup_calls_enabled;
        if (state_verified) {
            ++state.planned_count;
        }
        if (state_verified && operands_proven) {
            ++state.renderable_count;
        }
    }
    state.all_renderable = state.renderable_count > 0 && state.renderable_count == state.render_count;
    return state;
}

auto build_computed_cleanup_call_emission_gate_state(
    ComputedCleanupProofModel const& proof_model
) -> ComputedCleanupCallEmissionGateState {
    auto state = ComputedCleanupCallEmissionGateState {
        .gate_count = proof_model.cleanup_call_report_events.emission_gate_events.size(),
    };
    state.all_state_verified = state.gate_count > 0;
    state.all_cleanup_calls_enabled = state.gate_count > 0;
    state.cleanup_owner_names.reserve(proof_model.cleanup_call_report_events.emission_gate_events.size());
    state.acquire_operation_names.reserve(proof_model.cleanup_call_report_events.emission_gate_events.size());
    state.resume_operation_names.reserve(proof_model.cleanup_call_report_events.emission_gate_events.size());
    for (auto const& event : proof_model.cleanup_call_report_events.emission_gate_events) {
        auto const state_verified =
            event.acquisition.target_owner_name == event.resumption.source_owner_name &&
            event.acquisition.source_owner_name == event.resumption.target_owner_name;
        auto const cleanup_calls_enabled =
            event.acquisition.cleanup_calls_enabled && event.resumption.cleanup_calls_enabled;
        state.cleanup_owner_names.push_back(event.resumption.target_owner_name);
        state.acquire_operation_names.push_back(event.acquisition.operation_name);
        state.resume_operation_names.push_back(event.resumption.operation_name);
        state.all_state_verified = state.all_state_verified && state_verified;
        state.all_cleanup_calls_enabled = state.all_cleanup_calls_enabled && cleanup_calls_enabled;
        if (state_verified && cleanup_calls_enabled) {
            ++state.ready_count;
        } else {
            ++state.blocked_count;
        }
    }
    state.all_ready = state.ready_count > 0 && state.blocked_count == 0;
    return state;
}

auto build_computed_cleanup_proof_summary_state(
    ComputedCleanupProofModel const& proof_model
) -> ComputedCleanupProofSummaryState {
    return ComputedCleanupProofSummaryState {
        .cleanup_proof_model_count = proof_model.summary.cleanup_proof_model_count,
        .verified_inserted_cleanup_pair_count = proof_model.summary.verified_inserted_cleanup_pair_count,
        .structured_inserted_cleanup_handoff_count =
            proof_model.summary.structured_inserted_cleanup_handoff_count,
        .structured_inserted_cleanup_handoff_use_count =
            proof_model.summary.structured_inserted_cleanup_handoff_use_count,
        .ir_inserted_cleanup_handoff_fallback_count =
            proof_model.summary.ir_inserted_cleanup_handoff_fallback_count,
        .structured_cleanup_operand_count = proof_model.summary.structured_cleanup_operand_count,
        .structured_cleanup_operand_use_count = proof_model.summary.structured_cleanup_operand_use_count,
        .ir_cleanup_operand_fallback_count = proof_model.summary.ir_cleanup_operand_fallback_count,
        .structured_inserted_cleanup_call_count =
            proof_model.summary.structured_inserted_cleanup_call_count,
        .ir_inserted_cleanup_call_fallback_count =
            proof_model.summary.ir_inserted_cleanup_call_fallback_count,
        .structured_consumed_cleanup_descriptor_count =
            proof_model.summary.structured_consumed_cleanup_descriptor_count,
        .ir_consumed_cleanup_descriptor_fallback_count =
            proof_model.summary.ir_consumed_cleanup_descriptor_fallback_count,
    };
}

auto build_computed_inserted_cleanup_handoff_state(
    ComputedCleanupProofModel const& proof_model
) -> ComputedInsertedCleanupHandoffState {
    auto state = ComputedInsertedCleanupHandoffState {
        .from_metadata = proof_model.inserted_cleanup_state.from_metadata,
        .transition_count = proof_model.inserted_cleanup_state.transition_events.size(),
        .verification_count = proof_model.inserted_cleanup_state.verification_events.size(),
    };
    state.all_cleanup_calls_enabled = state.transition_count > 0;
    state.cleanup_owner_names.reserve(proof_model.inserted_cleanup_state.transition_events.size());
    state.acquire_operation_names.reserve(proof_model.inserted_cleanup_state.transition_events.size());
    state.resume_operation_names.reserve(proof_model.inserted_cleanup_state.transition_events.size());
    for (auto const& event : proof_model.inserted_cleanup_state.transition_events) {
        state.cleanup_owner_names.push_back(event.resumption.target_owner_name);
        state.acquire_operation_names.push_back(event.acquisition.operation_name);
        state.resume_operation_names.push_back(event.resumption.operation_name);
        state.all_cleanup_calls_enabled =
            state.all_cleanup_calls_enabled &&
            event.acquisition.cleanup_calls_enabled &&
            event.resumption.cleanup_calls_enabled;
    }
    for (auto const& event : proof_model.inserted_cleanup_state.verification_events) {
        if (event.kind == InsertedCleanupStateVerificationKind::paired) {
            ++state.paired_count;
        } else {
            ++state.blocked_count;
        }
    }
    state.all_paired = state.paired_count > 0 && state.blocked_count == 0;
    return state;
}

}  // namespace

void populate_lowering_emission_reports(
    CompilePipelineResult& result,
    lowering::LlvmIrEmissionResult&& emission,
    CompilePipelineOptions const& options
) {
    result.ir_text = std::move(emission.ir_text);
    auto const cleanup_proof_model = build_computed_cleanup_proof_model(
        result.ir_text,
        emission.computed_dynamic_array_inserted_cleanup_handoffs,
        emission.computed_dynamic_array_cleanup_call_operands
    );
    result.dynamic_array_construction_plan_report =
        emission.dynamic_array_construction_plan_report();
    result.dynamic_array_runtime_request_report =
        emission.dynamic_array_runtime_request_report();
    result.dynamic_array_allocation_call_ir =
        std::move(emission.dynamic_array_allocation_call_ir);
    result.dynamic_array_descriptor_cleanup_plan_report =
        emission.dynamic_array_descriptor_cleanup_plan_report();
    result.dynamic_array_cleanup_obligation_report =
        emission.dynamic_array_cleanup_obligation_report();
    result.dynamic_array_cleanup_sequence_plan_report =
        emission.dynamic_array_cleanup_sequence_plan_report();
    result.dynamic_array_cleanup_sequence_verification_report =
        emission.dynamic_array_cleanup_sequence_verification_report();
    result.dynamic_array_cleanup_sequence_verification_passed =
        !emission.dynamic_array_cleanup_sequence_verifications.empty() &&
        lowering::dynamic_array_cleanup_sequence_verification_report_passed(
            emission.dynamic_array_cleanup_sequence_verifications
        );
    result.dynamic_array_cleanup_emission_gate_report =
        emission.dynamic_array_cleanup_emission_gate_report();
    if (emission.dynamic_array_cleanup_emission_capability.has_value()) {
        result.dynamic_array_cleanup_capability_proven =
            lowering::dynamic_array_cleanup_emission_capability_proven(
                *emission.dynamic_array_cleanup_emission_capability
            );
        result.dynamic_array_cleanup_missing_element_drop_pairs =
            emission.dynamic_array_cleanup_emission_capability->missing_element_drop_pairs;
    }
    result.dynamic_array_cleanup_emission_capability_report =
        emission.dynamic_array_cleanup_emission_capability_report();
    result.dynamic_array_cleanup_availability = DynamicArrayCleanupAvailability {
        .missing_element_drop_pairs = result.dynamic_array_cleanup_missing_element_drop_pairs,
        .descriptor_origins_available = !result.semantic_result.dynamic_array_descriptor_origins.empty(),
        .descriptor_cleanup_plans_available = !emission.dynamic_array_descriptor_cleanup_plans.empty(),
        .cleanup_obligations_available = !emission.dynamic_array_cleanup_obligations.empty(),
        .sequence_verification_available = !emission.dynamic_array_cleanup_sequence_verifications.empty(),
        .sequence_verification_passed = result.dynamic_array_cleanup_sequence_verification_passed,
        .cleanup_capability_proven = result.dynamic_array_cleanup_capability_proven,
    };
    result.emitted_dynamic_array_cleanup_obligation_report =
        std::move(emission.emitted_dynamic_array_cleanup_obligation_report);
    result.emitted_dynamic_array_cleanup_sequence_plan_report =
        std::move(emission.emitted_dynamic_array_cleanup_sequence_plan_report);
    result.emitted_dynamic_array_cleanup_sequence_verification_report =
        std::move(emission.emitted_dynamic_array_cleanup_sequence_verification_report);
    result.emitted_dynamic_array_cleanup_emission_gate_report =
        std::move(emission.emitted_dynamic_array_cleanup_emission_gate_report);
    result.emitted_dynamic_array_cleanup_emission_capability_report =
        std::move(emission.emitted_dynamic_array_cleanup_emission_capability_report);
    result.computed_dynamic_array_for_descriptor_render_report =
        emission.computed_dynamic_array_for_descriptor_render_report();
    result.computed_dynamic_array_for_descriptor_render_state =
        build_computed_dynamic_array_for_descriptor_render_state(emission);
    result.computed_dynamic_array_for_loop_control_render_report =
        emission.computed_dynamic_array_for_loop_control_render_report();
    result.computed_dynamic_array_for_loop_control_render_state =
        build_computed_dynamic_array_for_loop_control_render_state(emission);
    result.computed_dynamic_array_for_element_address_render_report =
        emission.computed_dynamic_array_for_element_address_render_report();
    result.computed_dynamic_array_for_element_address_render_state =
        build_computed_dynamic_array_for_element_address_render_state(emission);
    result.computed_dynamic_array_for_element_load_render_report =
        emission.computed_dynamic_array_for_element_load_render_report();
    result.computed_dynamic_array_for_element_load_render_state =
        build_computed_dynamic_array_for_element_load_render_state(emission);
    result.computed_dynamic_array_for_loop_continue_render_report =
        emission.computed_dynamic_array_for_loop_continue_render_report();
    result.computed_dynamic_array_for_loop_continue_render_state =
        build_computed_dynamic_array_for_loop_continue_render_state(emission);
    result.computed_dynamic_array_for_loop_render_sequence_report =
        emission.computed_dynamic_array_for_loop_render_sequence_report();
    result.computed_dynamic_array_for_loop_render_sequence_state =
        build_computed_dynamic_array_for_loop_render_sequence_state(emission);
    result.computed_dynamic_array_for_loop_exit_cleanup_report =
        emission.computed_dynamic_array_for_loop_exit_cleanup_report();
    result.computed_dynamic_array_for_loop_exit_cleanup_state =
        build_computed_dynamic_array_for_loop_exit_cleanup_state(emission);
    result.computed_dynamic_array_for_cleanup_transition_report =
        emission.computed_dynamic_array_for_cleanup_transition_report();
    result.computed_dynamic_array_for_cleanup_transition_state =
        build_computed_dynamic_array_for_cleanup_transition_state(emission);
    result.computed_dynamic_array_for_inserted_cleanup_transition_report =
        cleanup_proof_model.reports.inserted_cleanup_transition_report;
    result.computed_dynamic_array_for_inserted_cleanup_state_verification_report =
        cleanup_proof_model.reports.inserted_cleanup_state_verification_report;
    result.computed_dynamic_array_for_inserted_cleanup_handoff_state =
        build_computed_inserted_cleanup_handoff_state(cleanup_proof_model);
    result.computed_dynamic_array_for_cleanup_proof_summary_state =
        build_computed_cleanup_proof_summary_state(cleanup_proof_model);
    result.computed_dynamic_array_for_cleanup_call_emission_gate_report =
        cleanup_proof_model.reports.cleanup_call_emission_gate_report;
    result.computed_dynamic_array_for_cleanup_call_emission_gate_state =
        build_computed_cleanup_call_emission_gate_state(cleanup_proof_model);
    result.computed_dynamic_array_for_cleanup_call_plan_report =
        cleanup_proof_model.reports.cleanup_call_plan_report;
    result.computed_dynamic_array_for_cleanup_call_render_report =
        cleanup_proof_model.reports.cleanup_call_render_report;
    result.computed_dynamic_array_for_cleanup_call_plan_render_state =
        build_computed_cleanup_call_plan_render_state(cleanup_proof_model);
    result.computed_dynamic_array_for_cleanup_call_insertion_gate_report =
        cleanup_proof_model.reports.cleanup_call_insertion_gate_report;
    result.computed_dynamic_array_for_cleanup_call_insertion_gate_state =
        build_computed_cleanup_call_insertion_gate_state(cleanup_proof_model);
    result.computed_dynamic_array_for_inserted_cleanup_call_report =
        cleanup_proof_model.reports.inserted_cleanup_call_report;
    result.computed_dynamic_array_for_inserted_cleanup_call_state =
        build_computed_inserted_cleanup_call_state(cleanup_proof_model);
    result.consumed_descriptor_finalization_plan_report =
        emission.consumed_descriptor_finalization_plan_report();
    result.consumed_descriptor_finalization_state =
        build_consumed_descriptor_finalization_state(emission);
    result.computed_dynamic_array_for_consumed_cleanup_descriptor_model_report =
        emission.computed_dynamic_array_for_consumed_cleanup_descriptor_model_report();
    result.computed_dynamic_array_for_consumed_cleanup_descriptor_model_state =
        build_computed_consumed_cleanup_descriptor_model_state(emission);
    result.computed_dynamic_array_for_consumed_cleanup_descriptor_state =
        build_computed_consumed_cleanup_descriptor_state(cleanup_proof_model);
    result.computed_dynamic_array_for_consumed_cleanup_descriptor_report =
        cleanup_proof_model.reports.consumed_cleanup_descriptor_report;
    result.computed_dynamic_array_for_production_emission_gate_report =
        emission.computed_dynamic_array_for_production_emission_gate_report();
    result.computed_dynamic_array_for_production_emission_gate_state =
        build_computed_dynamic_array_for_production_emission_gate_state(emission);
    result.computed_dynamic_array_for_production_sequence_report =
        emission.computed_dynamic_array_for_production_sequence_report();
    result.computed_dynamic_array_for_production_sequence_state =
        build_computed_dynamic_array_for_production_sequence_state(emission);
    result.computed_dynamic_array_for_production_readiness =
        plan_computed_dynamic_array_for_production_readiness(
            result.computed_dynamic_array_for_production_emission_gate_state,
            result.computed_dynamic_array_for_production_sequence_state
        );
    result.test_only_computed_dynamic_array_for_production_sequence_module_ir =
        std::move(emission.test_only_computed_dynamic_array_for_production_sequence_module_ir);
    result.dynamic_array_cleanup_production_readiness =
        plan_dynamic_array_cleanup_production_readiness(result, options);
    result.dynamic_array_cleanup_production_readiness_report = {
        format_dynamic_array_cleanup_production_readiness(result.dynamic_array_cleanup_production_readiness),
    };
    result.planned_drop_report = emission.planned_drop_report();
    result.emitted_drop_declaration_report =
        emission.emitted_drop_declaration_report();
    result.planned_drop_action_report =
        emission.planned_drop_action_report();
    result.drop_cleanup_authorization_report =
        emission.drop_cleanup_authorization_report();
    result.drop_readiness_snapshot = emission.drop_readiness_snapshot();
    result.drop_readiness_snapshot_report =
        emission.drop_readiness_snapshot_report();
    result.drop_readiness_summary = emission.drop_readiness_summary();
    result.drop_readiness_summary_report =
        emission.drop_readiness_summary_report();
    result.drop_readiness_relation_report =
        emission.drop_readiness_relation_report();
    result.drop_readiness_blocker_summary =
        lowering::summarize_drop_readiness_blockers(result.drop_readiness_snapshot);
    result.drop_readiness_blocker_report =
        lowering::format_drop_readiness_blocker_report(result.drop_readiness_blocker_summary);
    result.drop_readiness_source_correlation_report =
        format_drop_readiness_source_correlation_report(result.drop_readiness_snapshot);
    result.semantic_drop_lowering_authorizations = std::move(emission.semantic_drop_lowering_authorizations);
}

}  // namespace orison::pipeline
