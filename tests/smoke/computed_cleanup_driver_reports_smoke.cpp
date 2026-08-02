#include "computed_dynamic_array_audit_expectations.hpp"

#include "computed_cleanup_reports.hpp"

#include <cassert>

namespace {

namespace driver = orison::driver;
namespace pipeline = orison::pipeline;
namespace smoke = orison::tests::smoke;

void assert_computed_cleanup_capability_reports() {
    auto disabled = driver::computed_cleanup_call_insertion_capability_report(
        pipeline::ComputedCleanupCallInsertionCapabilityState {}
    );
    assert(disabled.size() == 1);
    assert(disabled.front() == smoke::computed_dynamic_array_cleanup_call_insertion_capability_disabled_report);

    auto enabled = driver::computed_cleanup_call_insertion_capability_report(
        pipeline::ComputedCleanupCallInsertionCapabilityState {
            .cleanup_call_authorization_enabled = true,
            .cleanup_call_insertion_enabled = true,
            .enabled = true,
        }
    );
    assert(enabled.size() == 1);
    assert(enabled.front() == smoke::computed_dynamic_array_cleanup_call_insertion_capability_enabled_report);
}

void assert_dynamic_array_cleanup_emission_capability_reports() {
    auto absent = driver::dynamic_array_cleanup_emission_capability_state_report(
        pipeline::DynamicArrayCleanupEmissionCapabilityState {}
    );
    assert(absent.empty());

    auto proven = driver::dynamic_array_cleanup_emission_capability_state_report(
        pipeline::DynamicArrayCleanupEmissionCapabilityState {
            .function_symbol_names = {"use_items"},
            .cleanup_pairs = {"items:__orison_dynamic_array_cleanup.0"},
            .cleanup_operation_names = {"__orison_dynamic_array_cleanup.0"},
            .cleanup_owner_names = {"items"},
            .element_drop_pairs = {"items:items.element:__orison_drop.Payload"},
            .capability_metadata_available = true,
            .proven = true,
            .emission_enabled = true,
            .descriptor_storage_bound = true,
            .sequence_verified = true,
            .element_cleanup_authorized_or_not_required = true,
            .descriptor_deallocation_authorized = true,
        }
    );
    assert(proven.size() == 1);
    assert(proven.front().find("function use_items dynamic array cleanup emission capability proven") !=
        std::string::npos);
    assert(proven.front().find("dynamic array cleanup emission capability proven") != std::string::npos);
    assert(
        proven.front().find("cleanup-pairs [items:__orison_dynamic_array_cleanup.0]") !=
        std::string::npos
    );
    assert(
        proven.front().find("element-drop-pairs [items:items.element:__orison_drop.Payload]") !=
        std::string::npos
    );
    assert(proven.front().find("[element cleanup ok]") != std::string::npos);

    auto blocked = driver::dynamic_array_cleanup_emission_capability_state_report(
        pipeline::DynamicArrayCleanupEmissionCapabilityState {
            .function_symbol_names = {"use_items"},
            .cleanup_pairs = {"items:__orison_dynamic_array_cleanup.0"},
            .cleanup_operation_names = {"__orison_dynamic_array_cleanup.0"},
            .cleanup_owner_names = {"items"},
            .missing_element_drop_pairs = {"items:items.element:__orison_drop.Payload"},
            .capability_metadata_available = true,
            .proven = false,
            .emission_enabled = true,
            .descriptor_storage_bound = true,
            .sequence_verified = true,
            .element_cleanup_authorized_or_not_required = false,
            .descriptor_deallocation_authorized = true,
        }
    );
    assert(blocked.size() == 1);
    assert(blocked.front().find("dynamic array cleanup emission capability blocked") != std::string::npos);
    assert(
        blocked.front().find("missing-element-drop-pairs [items:items.element:__orison_drop.Payload]") !=
        std::string::npos
    );
    assert(blocked.front().find("[element cleanup missing]") != std::string::npos);
}

void assert_computed_cleanup_readiness_reports() {
    auto blocked = driver::computed_cleanup_call_insertion_readiness_report(
        pipeline::ComputedCleanupCallInsertionGateState {
            .cleanup_owner_names = {"items"},
            .cleanup_operation_names = {"items.computed_for.0.cleanup.resume.call"},
            .cleanup_calls_blocked_reasons = {"later owner use"},
            .all_state_verified = true,
            .all_operands_proven = true,
            .all_cleanup_calls_authorized = false,
            .all_ready = false,
            .gate_count = 1,
            .ready_count = 0,
            .blocked_count = 1,
            .cleanup_call_blocker_count = 1,
        }
    );
    assert(blocked.size() == 2);
    assert(blocked[0] == smoke::computed_dynamic_array_cleanup_call_insertion_readiness_blocked_report);
    assert(
        blocked[1].find(
            "cleanup-operation items.computed_for.0.cleanup.resume.call cleanup-blocked-reason later owner use"
        ) != std::string::npos
    );

    auto ready = driver::computed_cleanup_call_insertion_readiness_report(
        pipeline::ComputedCleanupCallInsertionGateState {
            .cleanup_owner_names = {"items"},
            .cleanup_operation_names = {"items.computed_for.0.cleanup.resume.call"},
            .all_state_verified = true,
            .all_operands_proven = true,
            .all_cleanup_calls_authorized = true,
            .all_ready = true,
            .gate_count = 1,
            .ready_count = 1,
            .blocked_count = 0,
        }
    );
    assert(ready.size() == 2);
    assert(ready[0] == smoke::computed_dynamic_array_cleanup_call_insertion_readiness_ready_report);
    assert(ready[1] == smoke::computed_dynamic_array_cleanup_call_insertion_readiness_detail_report);
}

void assert_computed_dynamic_array_render_reports() {
    auto descriptor = driver::computed_dynamic_array_for_descriptor_render_state_report(
        pipeline::ComputedDynamicArrayForDescriptorRenderState {
            .enclosing_function_names = {"sum_words"},
            .cleanup_owner_names = {"items"},
            .source_type_names = {"DynamicArray<UInt32>"},
            .element_source_type_names = {"UInt32"},
            .descriptor_storage_names = {"%items.addr"},
            .descriptor_value_names = {"%items.computed_for.descriptor"},
            .data_pointer_names = {"%items.computed_for.data"},
            .length_names = {"%items.computed_for.length"},
            .capacity_names = {"%items.computed_for.capacity"},
            .render_metadata_available = true,
            .all_descriptor_projections_ready = true,
            .render_count = 1,
            .rendered_ir_snippet_count = 4,
        }
    );
    assert(descriptor.size() == 2);
    assert(
        descriptor[0] ==
        "computed DynamicArray descriptor render planned renders 1 snippets 4 [metadata available] "
        "[descriptor projections ready] (metadata only)"
    );
    assert(
        descriptor[1] ==
        "computed DynamicArray descriptor render detail owner items function sum_words "
        "source DynamicArray<UInt32> element UInt32 descriptor %items.addr value %items.computed_for.descriptor "
        "data %items.computed_for.data length %items.computed_for.length capacity %items.computed_for.capacity "
        "(metadata only)"
    );

    auto control = driver::computed_dynamic_array_for_loop_control_render_state_report(
        pipeline::ComputedDynamicArrayForLoopControlRenderState {
            .enclosing_function_names = {"sum_words"},
            .cleanup_owner_names = {"items"},
            .source_type_names = {"DynamicArray<UInt32>"},
            .element_source_type_names = {"UInt32"},
            .condition_block_names = {"items.computed_for.condition"},
            .body_block_names = {"items.computed_for.body"},
            .continue_block_names = {"items.computed_for.continue"},
            .exit_block_names = {"items.computed_for.exit"},
            .index_names = {"%items.computed_for.index"},
            .next_index_names = {"%items.computed_for.next.index"},
            .bounds_check_names = {"%items.computed_for.more"},
            .render_metadata_available = true,
            .all_control_flow_names_ready = true,
            .render_count = 1,
            .rendered_ir_snippet_count = 5,
        }
    );
    assert(control.size() == 2);
    assert(
        control[0] ==
        "computed DynamicArray loop control render planned renders 1 snippets 5 [metadata available] "
        "[control flow ready] (metadata only)"
    );
    assert(
        control[1] ==
        "computed DynamicArray loop control render detail owner items function sum_words "
        "source DynamicArray<UInt32> element UInt32 condition items.computed_for.condition "
        "body items.computed_for.body continue items.computed_for.continue exit items.computed_for.exit "
        "index %items.computed_for.index next %items.computed_for.next.index bounds %items.computed_for.more "
        "(metadata only)"
    );

    auto address = driver::computed_dynamic_array_for_element_address_render_state_report(
        pipeline::ComputedDynamicArrayForElementAddressRenderState {
            .enclosing_function_names = {"sum_words"},
            .cleanup_owner_names = {"items"},
            .source_type_names = {"DynamicArray<UInt32>"},
            .element_source_type_names = {"UInt32"},
            .element_llvm_type_names = {"i32"},
            .data_pointer_names = {"%items.computed_for.data"},
            .index_names = {"%items.computed_for.index"},
            .element_address_names = {"%items.computed_for.element.addr"},
            .render_metadata_available = true,
            .all_element_address_inputs_ready = true,
            .render_count = 1,
            .rendered_ir_snippet_count = 1,
        }
    );
    assert(address.size() == 2);
    assert(
        address[0] ==
        "computed DynamicArray element address render planned renders 1 snippets 1 [metadata available] "
        "[element address ready] (metadata only)"
    );
    assert(
        address[1] ==
        "computed DynamicArray element address render detail owner items function sum_words "
        "source DynamicArray<UInt32> element UInt32 lowers-to i32 data %items.computed_for.data "
        "index %items.computed_for.index address %items.computed_for.element.addr (metadata only)"
    );

    auto load = driver::computed_dynamic_array_for_element_load_render_state_report(
        pipeline::ComputedDynamicArrayForElementLoadRenderState {
            .enclosing_function_names = {"sum_words"},
            .cleanup_owner_names = {"items"},
            .source_type_names = {"DynamicArray<UInt32>"},
            .element_source_type_names = {"UInt32"},
            .element_llvm_type_names = {"i32"},
            .element_address_names = {"%items.computed_for.element.addr"},
            .item_value_names = {"%items.computed_for.item"},
            .render_metadata_available = true,
            .all_element_load_inputs_ready = true,
            .render_count = 1,
            .rendered_ir_snippet_count = 1,
        }
    );
    assert(load.size() == 2);
    assert(
        load[0] ==
        "computed DynamicArray element load render planned renders 1 snippets 1 [metadata available] "
        "[element load ready] (metadata only)"
    );
    assert(
        load[1] ==
        "computed DynamicArray element load render detail owner items function sum_words "
        "source DynamicArray<UInt32> element UInt32 lowers-to i32 "
        "address %items.computed_for.element.addr item %items.computed_for.item (metadata only)"
    );

    auto loop_continue = driver::computed_dynamic_array_for_loop_continue_render_state_report(
        pipeline::ComputedDynamicArrayForLoopContinueRenderState {
            .enclosing_function_names = {"sum_words"},
            .cleanup_owner_names = {"items"},
            .source_type_names = {"DynamicArray<UInt32>"},
            .element_source_type_names = {"UInt32"},
            .continue_block_names = {"items.computed_for.continue"},
            .condition_block_names = {"items.computed_for.condition"},
            .index_names = {"%items.computed_for.index"},
            .next_index_names = {"%items.computed_for.next.index"},
            .render_metadata_available = true,
            .all_loop_continue_inputs_ready = true,
            .render_count = 1,
            .rendered_ir_snippet_count = 3,
        }
    );
    assert(loop_continue.size() == 2);
    assert(
        loop_continue[0] ==
        "computed DynamicArray loop continue render planned renders 1 snippets 3 [metadata available] "
        "[loop continue ready] (metadata only)"
    );
    assert(
        loop_continue[1] ==
        "computed DynamicArray loop continue render detail owner items function sum_words "
        "source DynamicArray<UInt32> element UInt32 continue items.computed_for.continue "
        "condition items.computed_for.condition index %items.computed_for.index "
        "next %items.computed_for.next.index (metadata only)"
    );

    auto sequence = driver::computed_dynamic_array_for_loop_render_sequence_state_report(
        pipeline::ComputedDynamicArrayForLoopRenderSequenceState {
            .enclosing_function_names = {"sum_words"},
            .cleanup_owner_names = {"items"},
            .source_type_names = {"DynamicArray<UInt32>"},
            .element_source_type_names = {"UInt32"},
            .body_block_names = {"items.computed_for.body"},
            .sequence_metadata_available = true,
            .all_body_blocks_ready = true,
            .sequence_count = 1,
            .rendered_ir_snippet_count = 15,
        }
    );
    assert(sequence.size() == 2);
    assert(
        sequence[0] ==
        "computed DynamicArray loop render sequence planned sequences 1 snippets 15 [metadata available] "
        "[body blocks ready] (metadata only)"
    );
    assert(
        sequence[1] ==
        "computed DynamicArray loop render sequence detail owner items function sum_words "
        "source DynamicArray<UInt32> element UInt32 body items.computed_for.body (metadata only)"
    );

    auto exit_cleanup = driver::computed_dynamic_array_for_loop_exit_cleanup_state_report(
        pipeline::ComputedDynamicArrayForLoopExitCleanupState {
            .enclosing_function_names = {"sum_words"},
            .cleanup_owner_names = {"items"},
            .source_type_names = {"DynamicArray<UInt32>"},
            .element_source_type_names = {"UInt32"},
            .exit_block_names = {"items.computed_for.exit"},
            .loop_entry_cleanup_owner_names = {"items.loop.entry"},
            .loop_exit_cleanup_owner_names = {"items"},
            .cleanup_resumption_operation_names = {"items.computed_for.cleanup.resume"},
            .cleanup_metadata_available = true,
            .all_cleanup_resumptions_ready = true,
            .cleanup_count = 1,
            .rendered_ir_snippet_count = 2,
        }
    );
    assert(exit_cleanup.size() == 2);
    assert(
        exit_cleanup[0] ==
        "computed DynamicArray loop exit cleanup planned cleanups 1 snippets 2 [metadata available] "
        "[cleanup resumptions ready] (metadata only)"
    );
    assert(
        exit_cleanup[1] ==
        "computed DynamicArray loop exit cleanup detail owner items function sum_words "
        "source DynamicArray<UInt32> element UInt32 exit items.computed_for.exit from items.loop.entry "
        "to items operation items.computed_for.cleanup.resume (metadata only)"
    );

    auto transition = driver::computed_dynamic_array_for_cleanup_transition_state_report(
        pipeline::ComputedDynamicArrayForCleanupTransitionState {
            .enclosing_function_names = {"sum_words"},
            .cleanup_owner_names = {"items"},
            .source_type_names = {"DynamicArray<UInt32>"},
            .element_source_type_names = {"UInt32"},
            .acquisition_source_owner_names = {"items"},
            .acquisition_target_owner_names = {"items.loop.entry"},
            .acquisition_operation_names = {"items.computed_for.cleanup.acquire"},
            .resumption_source_owner_names = {"items.loop.entry"},
            .resumption_target_owner_names = {"items"},
            .resumption_operation_names = {"items.computed_for.cleanup.resume"},
            .transition_metadata_available = true,
            .all_transitions_paired = true,
            .transition_count = 1,
        }
    );
    assert(transition.size() == 2);
    assert(
        transition[0] ==
        "computed DynamicArray cleanup transition planned transitions 1 [metadata available] "
        "[transitions paired] (metadata only)"
    );
    assert(
        transition[1] ==
        "computed DynamicArray cleanup transition detail owner items function sum_words "
        "source DynamicArray<UInt32> element UInt32 acquire-from items acquire-to items.loop.entry "
        "acquire-operation items.computed_for.cleanup.acquire resume-from items.loop.entry "
        "resume-to items resume-operation items.computed_for.cleanup.resume (metadata only)"
    );
}

void assert_computed_inserted_cleanup_handoff_reports() {
    auto empty = driver::computed_inserted_cleanup_handoff_state_report(
        pipeline::ComputedInsertedCleanupHandoffState {}
    );
    assert(empty.size() == 1);
    assert(empty.front() == smoke::computed_dynamic_array_inserted_cleanup_handoff_state_empty_report);
    assert(
        driver::computed_cleanup_call_blocker_summary_report(
            pipeline::ComputedInsertedCleanupHandoffState {}
        ).empty()
    );

    auto paired_disabled_state = pipeline::ComputedInsertedCleanupHandoffState {
        .cleanup_owner_names = {"items"},
        .acquire_operation_names = {"items.computed_for.0.cleanup.acquire"},
        .resume_operation_names = {"items.computed_for.0.cleanup.resume"},
        .cleanup_calls_blocked_reasons = {"later owner use"},
        .from_metadata = true,
        .all_paired = true,
        .all_cleanup_calls_enabled = false,
        .transition_count = 1,
        .verification_count = 1,
        .paired_count = 1,
        .blocked_count = 0,
        .cleanup_call_blocker_count = 1,
    };
    auto paired_disabled = driver::computed_inserted_cleanup_handoff_state_report(paired_disabled_state);
    assert(paired_disabled.size() == 2);
    assert(paired_disabled[0] == smoke::computed_dynamic_array_inserted_cleanup_handoff_state_paired_disabled_report);
    assert(
        paired_disabled[1].find(
            "acquire items.computed_for.0.cleanup.acquire resume items.computed_for.0.cleanup.resume "
            "cleanup-blocked-reason later owner use"
        ) != std::string::npos
    );
    auto paired_disabled_blockers =
        driver::computed_cleanup_call_blocker_summary_report(paired_disabled_state);
    assert(paired_disabled_blockers.size() == 1);
    assert(
        paired_disabled_blockers.front() ==
        "computed DynamicArray cleanup call blockers blocked cleanup-blockers 1 "
        "blocker-reasons [later owner use] (metadata only)"
    );

    auto paired_enabled = driver::computed_inserted_cleanup_handoff_state_report(
        pipeline::ComputedInsertedCleanupHandoffState {
            .cleanup_owner_names = {"items"},
            .acquire_operation_names = {"items.computed_for.0.cleanup.acquire"},
            .resume_operation_names = {"items.computed_for.0.cleanup.resume"},
            .from_metadata = true,
            .all_paired = true,
            .all_cleanup_calls_enabled = true,
            .transition_count = 1,
            .verification_count = 1,
            .paired_count = 1,
            .blocked_count = 0,
        }
    );
    assert(paired_enabled.size() == 2);
    assert(paired_enabled[0] == smoke::computed_dynamic_array_inserted_cleanup_handoff_state_paired_enabled_report);
    assert(paired_enabled[1] == smoke::computed_dynamic_array_inserted_cleanup_handoff_state_detail_report);
}

void assert_computed_cleanup_verification_and_emission_gate_reports() {
    auto blocked_verification = driver::computed_inserted_cleanup_state_verification_report(
        pipeline::ComputedInsertedCleanupStateVerificationState {
            .blocked_reasons = {"missing cleanup acquisition"},
            .from_metadata = true,
            .all_paired = false,
            .all_cleanup_calls_enabled = false,
            .verification_count = 1,
            .paired_count = 0,
            .blocked_count = 1,
        }
    );
    assert(blocked_verification.size() == 2);
    assert(
        blocked_verification[0] ==
        "computed DynamicArray inserted cleanup state verification blocked verifications 1 paired 0 blocked 1 "
        "[metadata-backed] [handoff blocked] [cleanup calls disabled] (inserted IR)"
    );
    assert(
        blocked_verification[1] ==
        "computed DynamicArray inserted cleanup state verification detail owner <unknown> blocked-reason "
        "missing cleanup acquisition (inserted IR)"
    );

    auto paired_verification = driver::computed_inserted_cleanup_state_verification_report(
        pipeline::ComputedInsertedCleanupStateVerificationState {
            .acquire_operation_names = {"items.computed_for.0.cleanup.acquire"},
            .resume_operation_names = {"items.computed_for.0.cleanup.resume"},
            .acquire_source_owner_names = {"items"},
            .acquire_target_owner_names = {"items.loop.entry"},
            .resume_source_owner_names = {"items.loop.entry"},
            .resume_target_owner_names = {"items"},
            .from_metadata = true,
            .all_paired = true,
            .all_cleanup_calls_enabled = true,
            .verification_count = 1,
            .paired_count = 1,
            .blocked_count = 0,
        }
    );
    assert(paired_verification.size() == 2);
    assert(
        paired_verification[0] ==
        "computed DynamicArray inserted cleanup state verification paired verifications 1 paired 1 blocked 0 "
        "[metadata-backed] [handoff paired] [cleanup calls enabled] (inserted IR)"
    );
    assert(
        paired_verification[1] ==
        "computed DynamicArray inserted cleanup state verification detail owner items acquire "
        "items.computed_for.0.cleanup.acquire resume items.computed_for.0.cleanup.resume acquire-from items "
        "acquire-to items.loop.entry resume-from items.loop.entry resume-to items (inserted IR)"
    );

    auto blocked_gate = driver::computed_cleanup_call_emission_gate_state_report(
        pipeline::ComputedCleanupCallEmissionGateState {
            .cleanup_owner_names = {"items"},
            .acquire_operation_names = {"items.computed_for.0.cleanup.acquire"},
            .resume_operation_names = {"items.computed_for.0.cleanup.resume"},
            .all_state_verified = true,
            .all_cleanup_calls_enabled = false,
            .all_ready = false,
            .gate_count = 1,
            .ready_count = 0,
            .blocked_count = 1,
        }
    );
    assert(blocked_gate.size() == 2);
    assert(
        blocked_gate[0] ==
        "computed DynamicArray cleanup call emission gate blocked gates 1 ready 0 blocked 1 "
        "[inserted state verified] [cleanup calls disabled] (inserted IR)"
    );
    assert(
        blocked_gate[1] ==
        "computed DynamicArray cleanup call emission gate detail owner items acquire "
        "items.computed_for.0.cleanup.acquire resume items.computed_for.0.cleanup.resume (inserted IR)"
    );

    auto ready_gate = driver::computed_cleanup_call_emission_gate_state_report(
        pipeline::ComputedCleanupCallEmissionGateState {
            .cleanup_owner_names = {"items"},
            .acquire_operation_names = {"items.computed_for.0.cleanup.acquire"},
            .resume_operation_names = {"items.computed_for.0.cleanup.resume"},
            .all_state_verified = true,
            .all_cleanup_calls_enabled = true,
            .all_ready = true,
            .gate_count = 1,
            .ready_count = 1,
            .blocked_count = 0,
        }
    );
    assert(ready_gate.size() == 2);
    assert(
        ready_gate[0] ==
        "computed DynamicArray cleanup call emission gate ready gates 1 ready 1 blocked 0 "
        "[inserted state verified] [cleanup calls enabled] (inserted IR)"
    );
    assert(
        ready_gate[1] ==
        "computed DynamicArray cleanup call emission gate detail owner items acquire "
        "items.computed_for.0.cleanup.acquire resume items.computed_for.0.cleanup.resume (inserted IR)"
    );
}

void assert_computed_cleanup_call_plan_and_render_reports() {
    auto blocked = pipeline::ComputedCleanupCallPlanRenderState {
        .cleanup_owner_names = {"items"},
        .cleanup_operation_names = {"items.computed_for.0.cleanup.resume.call"},
        .data_pointer_names = {"%items.computed_for.0.data"},
        .element_size_bytes = {"4"},
        .capacity_names = {"%items.computed_for.0.capacity"},
        .all_state_verified = true,
        .all_operands_proven = true,
        .all_cleanup_calls_enabled = false,
        .all_renderable = false,
        .plan_count = 1,
        .render_count = 1,
        .planned_count = 1,
        .renderable_count = 1,
    };
    auto blocked_plan = driver::computed_cleanup_call_plan_state_report(blocked);
    assert(blocked_plan.size() == 2);
    assert(
        blocked_plan[0] ==
        "computed DynamicArray cleanup call plan planned plans 1 planned 1 renderable 1 renders 1 "
        "[inserted state verified] [cleanup operands proven] [cleanup calls disabled] (inserted IR)"
    );
    assert(
        blocked_plan[1] ==
        "computed DynamicArray cleanup call plan detail owner items cleanup-operation "
        "items.computed_for.0.cleanup.resume.call data %items.computed_for.0.data element-size 4 capacity "
        "%items.computed_for.0.capacity (inserted IR)"
    );
    auto blocked_render = driver::computed_cleanup_call_render_state_report(blocked);
    assert(blocked_render.size() == 2);
    assert(
        blocked_render[0] ==
        "computed DynamicArray cleanup call render blocked renders 1 renderable 1 plans 1 "
        "[inserted state verified] [cleanup operands proven] [render blocked] (inserted IR)"
    );
    assert(
        blocked_render[1] ==
        "computed DynamicArray cleanup call render detail owner items cleanup-operation "
        "items.computed_for.0.cleanup.resume.call data %items.computed_for.0.data element-size 4 capacity "
        "%items.computed_for.0.capacity (inserted IR)"
    );

    auto rendered = blocked;
    rendered.all_cleanup_calls_enabled = true;
    rendered.all_renderable = true;
    auto rendered_plan = driver::computed_cleanup_call_plan_state_report(rendered);
    assert(rendered_plan.size() == 2);
    assert(
        rendered_plan[0] ==
        "computed DynamicArray cleanup call plan planned plans 1 planned 1 renderable 1 renders 1 "
        "[inserted state verified] [cleanup operands proven] [cleanup calls enabled] (inserted IR)"
    );
    auto rendered_call = driver::computed_cleanup_call_render_state_report(rendered);
    assert(rendered_call.size() == 2);
    assert(
        rendered_call[0] ==
        "computed DynamicArray cleanup call render rendered renders 1 renderable 1 plans 1 "
        "[inserted state verified] [cleanup operands proven] [renderable] (inserted IR)"
    );
}

void assert_computed_inserted_cleanup_call_reports() {
    auto absent = driver::computed_inserted_cleanup_call_state_report(
        pipeline::ComputedInsertedCleanupCallState {}
    );
    assert(absent.size() == 1);
    assert(absent.front() == smoke::computed_dynamic_array_inserted_cleanup_call_state_absent_report);

    auto inserted = driver::computed_inserted_cleanup_call_state_report(
        pipeline::ComputedInsertedCleanupCallState {
            .cleanup_owner_names = {"items"},
            .data_pointer_names = {"%items.computed_for.0.data"},
            .capacity_names = {"%items.computed_for.0.capacity"},
            .all_inserted = true,
            .call_count = 1,
            .structured_proof_count = 1,
            .ir_fallback_proof_count = 0,
        }
    );
    assert(inserted.size() == 2);
    assert(inserted[0] == smoke::computed_dynamic_array_inserted_cleanup_call_state_inserted_report);
    assert(inserted[1] == smoke::computed_dynamic_array_inserted_cleanup_call_state_detail_report);
}

void assert_consumed_descriptor_finalization_and_model_reports() {
    auto finalization = driver::consumed_descriptor_finalization_state_report(
        pipeline::ConsumedDescriptorFinalizationState {
            .cleanup_owner_names = {"items"},
            .descriptor_storage_names = {"%items.addr"},
            .all_ready = true,
            .computed_descriptor_plan_count = 1,
            .emitted_finalization_plan_count = 1,
            .ready_plan_count = 2,
            .blocked_plan_count = 0,
        }
    );
    assert(finalization.size() == 2);
    assert(
        finalization[0] ==
        "computed DynamicArray consumed descriptor finalization plans ready computed-descriptor-plans 1 "
        "emitted-finalization-plans 1 ready 2 blocked 0 (metadata only)"
    );
    assert(
        finalization[1] ==
        "computed DynamicArray consumed descriptor finalization plan detail owner items descriptor %items.addr "
        "(metadata only)"
    );

    auto blocked_finalization = driver::consumed_descriptor_finalization_state_report(
        pipeline::ConsumedDescriptorFinalizationState {
            .computed_descriptor_plan_count = 0,
            .emitted_finalization_plan_count = 0,
            .ready_plan_count = 0,
            .blocked_plan_count = 0,
        }
    );
    assert(blocked_finalization.size() == 1);
    assert(
        blocked_finalization[0] ==
        "computed DynamicArray consumed descriptor finalization plans blocked computed-descriptor-plans 0 "
        "emitted-finalization-plans 0 ready 0 blocked 0 (metadata only)"
    );

    auto model = driver::computed_consumed_cleanup_descriptor_model_state_report(
        pipeline::ComputedConsumedCleanupDescriptorModelState {
            .enclosing_function_names = {"sum_words"},
            .cleanup_owner_names = {"items"},
            .descriptor_storage_names = {"%items.addr"},
            .cleanup_operation_names = {"items.computed_for.cleanup.resume"},
            .source_type_names = {"DynamicArray<UInt32>"},
            .element_source_type_names = {"UInt32"},
            .all_finalization_ready = true,
            .descriptor_model_count = 1,
            .ready_model_count = 1,
            .blocked_model_count = 0,
        }
    );
    assert(model.size() == 2);
    assert(
        model[0] ==
        "computed DynamicArray consumed cleanup descriptor models ready models 1 ready 1 blocked 0 "
        "[finalization ready] (metadata only)"
    );
    assert(
        model[1] ==
        "computed DynamicArray consumed cleanup descriptor model detail owner items function sum_words "
        "source DynamicArray<UInt32> element UInt32 descriptor %items.addr cleanup-operation "
        "items.computed_for.cleanup.resume (metadata only)"
    );
}

void assert_computed_consumed_cleanup_descriptor_reports() {
    auto absent = driver::computed_consumed_cleanup_descriptor_state_report(
        pipeline::ComputedConsumedCleanupDescriptorState {}
    );
    assert(absent.size() == 1);
    assert(absent.front() == smoke::computed_dynamic_array_consumed_cleanup_descriptor_state_absent_report);

    auto finalized = driver::computed_consumed_cleanup_descriptor_state_report(
        pipeline::ComputedConsumedCleanupDescriptorState {
            .cleanup_owner_names = {"items"},
            .descriptor_storage_names = {"%items.addr"},
            .all_finalized = true,
            .descriptor_count = 1,
            .structured_proof_count = 1,
            .ir_fallback_proof_count = 0,
        }
    );
    assert(finalized.size() == 2);
    assert(finalized[0] == smoke::computed_dynamic_array_consumed_cleanup_descriptor_state_finalized_report);
    assert(finalized[1] == smoke::computed_dynamic_array_consumed_cleanup_descriptor_state_detail_report);
}

void assert_computed_cleanup_proof_summary_reports() {
    auto empty = driver::computed_cleanup_proof_summary_state_report(
        pipeline::ComputedCleanupProofSummaryState {}
    );
    assert(empty.size() == 1);
    assert(empty.front() == smoke::computed_dynamic_array_cleanup_proof_summary_empty_report);

    auto inserted = driver::computed_cleanup_proof_summary_state_report(
        pipeline::ComputedCleanupProofSummaryState {
            .cleanup_proof_model_count = 1,
            .verified_inserted_cleanup_pair_count = 1,
            .structured_inserted_cleanup_handoff_count = 2,
            .structured_inserted_cleanup_handoff_use_count = 2,
            .ir_inserted_cleanup_handoff_fallback_count = 0,
            .structured_cleanup_operand_count = 1,
            .structured_cleanup_operand_use_count = 1,
            .ir_cleanup_operand_fallback_count = 0,
            .structured_inserted_cleanup_call_count = 1,
            .ir_inserted_cleanup_call_fallback_count = 0,
            .structured_consumed_cleanup_descriptor_count = 1,
            .ir_consumed_cleanup_descriptor_fallback_count = 0,
        }
    );
    assert(inserted.size() == 1);
    assert(inserted.front() == smoke::computed_dynamic_array_cleanup_proof_summary_inserted_report);
}

void assert_computed_cleanup_unknown_detail_fallbacks() {
    auto readiness = driver::computed_cleanup_call_insertion_readiness_report(
        pipeline::ComputedCleanupCallInsertionGateState {
            .cleanup_owner_names = {"items"},
            .all_state_verified = true,
            .all_operands_proven = true,
            .all_cleanup_calls_authorized = false,
            .all_ready = false,
            .gate_count = 1,
            .ready_count = 0,
            .blocked_count = 1,
        }
    );
    assert(readiness.size() == 2);
    assert(
        readiness[1] ==
        "computed DynamicArray cleanup call insertion readiness detail owner items cleanup-operation <unknown> "
        "(metadata only)"
    );

    auto inserted = driver::computed_inserted_cleanup_call_state_report(
        pipeline::ComputedInsertedCleanupCallState {
            .cleanup_owner_names = {"items"},
            .all_inserted = true,
            .call_count = 1,
            .structured_proof_count = 1,
            .ir_fallback_proof_count = 0,
        }
    );
    assert(inserted.size() == 2);
    assert(
        inserted[1] ==
        "computed DynamicArray inserted cleanup call detail owner items data <unknown> capacity <unknown> "
        "(inserted IR)"
    );

    auto consumed = driver::computed_consumed_cleanup_descriptor_state_report(
        pipeline::ComputedConsumedCleanupDescriptorState {
            .cleanup_owner_names = {"items"},
            .all_finalized = true,
            .descriptor_count = 1,
            .structured_proof_count = 1,
            .ir_fallback_proof_count = 0,
        }
    );
    assert(consumed.size() == 2);
    assert(
        consumed[1] ==
        "computed DynamicArray consumed cleanup descriptor detail owner items descriptor <unknown> (inserted IR)"
    );

    auto handoff = driver::computed_inserted_cleanup_handoff_state_report(
        pipeline::ComputedInsertedCleanupHandoffState {
            .cleanup_owner_names = {"items"},
            .from_metadata = true,
            .all_paired = true,
            .all_cleanup_calls_enabled = false,
            .transition_count = 1,
            .verification_count = 1,
            .paired_count = 1,
            .blocked_count = 0,
        }
    );
    assert(handoff.size() == 2);
    assert(handoff[1] == smoke::computed_dynamic_array_inserted_cleanup_handoff_state_unknown_detail_report);
}

void assert_computed_dynamic_array_production_reports() {
    auto disabled_gate = driver::computed_dynamic_array_for_production_emission_gate_state_report(
        pipeline::ComputedDynamicArrayForProductionEmissionGateState {
            .cleanup_owner_names = {"items"},
            .gate_metadata_available = true,
            .all_ownership_ready = true,
            .all_loop_render_ready = true,
            .all_loop_cleanup_ownership_ready = true,
            .all_function_cleanup_resumption_ready = true,
            .all_exit_cleanup_ready = true,
            .all_production_sequences_planned = true,
            .any_production_emission_enabled = false,
            .gate_count = 1,
            .rendered_ir_snippet_count = 17,
        }
    );
    assert(disabled_gate.size() == 2);
    assert(
        disabled_gate[0] ==
        "computed DynamicArray production emission gate planned gates 1 snippets 17 [metadata available] "
        "[ownership ready] [loop render ready] [loop cleanup ownership ready] "
        "[function cleanup resumption ready] [exit cleanup ready] [production sequence planned] "
        "[production emission disabled] (metadata only)"
    );
    assert(disabled_gate[1] == "computed DynamicArray production emission gate detail owner items (metadata only)");

    auto enabled_gate = driver::computed_dynamic_array_for_production_emission_gate_state_report(
        pipeline::ComputedDynamicArrayForProductionEmissionGateState {
            .cleanup_owner_names = {"items"},
            .gate_metadata_available = true,
            .all_ownership_ready = true,
            .all_loop_render_ready = true,
            .all_loop_cleanup_ownership_ready = true,
            .all_function_cleanup_resumption_ready = true,
            .all_exit_cleanup_ready = true,
            .all_production_sequences_planned = true,
            .any_production_emission_enabled = true,
            .gate_count = 1,
            .rendered_ir_snippet_count = 17,
        }
    );
    assert(enabled_gate.size() == 2);
    assert(
        enabled_gate[0] ==
        "computed DynamicArray production emission gate planned gates 1 snippets 17 [metadata available] "
        "[ownership ready] [loop render ready] [loop cleanup ownership ready] "
        "[function cleanup resumption ready] [exit cleanup ready] [production sequence planned] "
        "[production emission enabled] (metadata only)"
    );

    auto sequence = driver::computed_dynamic_array_for_production_sequence_state_report(
        pipeline::ComputedDynamicArrayForProductionSequenceState {
            .cleanup_owner_names = {"items"},
            .sequence_metadata_available = true,
            .module_comments_emitted = false,
            .sequence_count = 1,
            .rendered_ir_snippet_count = 17,
            .module_comment_line_count = 0,
        }
    );
    assert(sequence.size() == 2);
    assert(
        sequence[0] ==
        "computed DynamicArray production sequence planned sequences 1 snippets 17 module-comments 0 "
        "[metadata available] [module comments absent] (metadata only)"
    );
    assert(sequence[1] == "computed DynamicArray production sequence detail owner items (metadata only)");

    auto ready = driver::dynamic_array_cleanup_production_readiness_state_report(
        pipeline::DynamicArrayCleanupProductionReadiness {
            .descriptor_origins_available = true,
            .descriptor_cleanup_plans_available = true,
            .cleanup_obligations_available = true,
            .sequence_verification_available = true,
            .sequence_verification_passed = true,
            .cleanup_capability_proven = true,
            .production_signature_lowering_enabled = true,
            .production_construction_lowering_enabled = true,
            .production_cleanup_emission_enabled = true,
        }
    );
    assert(ready.size() == 1);
    assert(
        ready.front() ==
        "dynamic array cleanup production readiness ready [descriptor origins ok] [cleanup plans ok] "
        "[cleanup obligations ok] [sequence verification ok] [sequence passed ok] [cleanup capability ok] "
        "[production signatures ok] [production construction ok] [production cleanup emission ok] (metadata only)"
    );

    auto blocked = driver::dynamic_array_cleanup_production_readiness_state_report(
        pipeline::DynamicArrayCleanupProductionReadiness {
            .missing_element_drop_pairs = {"items:items.element:__orison_drop.Payload"},
            .descriptor_origins_available = true,
            .descriptor_cleanup_plans_available = true,
            .cleanup_obligations_available = true,
            .sequence_verification_available = true,
            .sequence_verification_passed = true,
            .cleanup_capability_proven = false,
            .production_signature_lowering_enabled = true,
            .production_construction_lowering_enabled = true,
            .production_cleanup_emission_enabled = true,
        }
    );
    assert(blocked.size() == 1);
    assert(
        blocked.front() ==
        "dynamic array cleanup production readiness blocked [descriptor origins ok] [cleanup plans ok] "
        "[cleanup obligations ok] [sequence verification ok] [sequence passed ok] [cleanup capability missing] "
        "missing-element-drop-pairs [items:items.element:__orison_drop.Payload] [production signatures ok] "
        "[production construction ok] [production cleanup emission ok] (metadata only)"
    );
}

void assert_aggregate_projection_access_plan_reports() {
    auto empty = driver::aggregate_projection_access_plan_state_report(
        pipeline::AggregateProjectionAccessPlanState {}
    );
    assert(empty.empty());

    auto report = driver::aggregate_projection_access_plan_state_report(
        pipeline::AggregateProjectionAccessPlanState {
            .function_symbol_names = {"main", "main", "method.Box.payload"},
            .intents = {
                orison::lowering::AggregateProjectionAccessIntent::value_read,
                orison::lowering::AggregateProjectionAccessIntent::explicit_transfer,
                orison::lowering::AggregateProjectionAccessIntent::value_read,
            },
            .statuses = {
                orison::lowering::AggregateProjectionAccessStatus::requires_explicit_boundary,
                orison::lowering::AggregateProjectionAccessStatus::allowed,
                orison::lowering::AggregateProjectionAccessStatus::allowed,
            },
            .binding_names = {"box.payload", "box.payload", "this.payload"},
            .source_type_names = {"Payload", "Payload", "Payload"},
            .diagnostics = {
                "aggregate path read of owned projection requires an explicit ownership transfer",
                "",
                "",
            },
            .receiver_projections = {false, false, true},
            .access_plans_available = true,
            .plan_count = 3,
            .allowed_count = 2,
            .blocked_count = 1,
            .receiver_projection_count = 1,
        }
    );
    assert(report.size() == 3);
    assert(
        report[0] ==
        "function main aggregate projection access intent value_read status requires_explicit_boundary "
        "binding box.payload source Payload receiver false diagnostic aggregate path read of owned projection "
        "requires an explicit ownership transfer"
    );
    assert(
        report[1] ==
        "function main aggregate projection access intent explicit_transfer status allowed binding box.payload "
        "source Payload receiver false"
    );
    assert(
        report[2] ==
        "function method.Box.payload aggregate projection access intent value_read status allowed binding "
        "this.payload source Payload receiver true"
    );

    auto unknown_detail = driver::aggregate_projection_access_plan_state_report(
        pipeline::AggregateProjectionAccessPlanState {
            .function_symbol_names = {"main"},
            .access_plans_available = true,
            .plan_count = 1,
        }
    );
    assert(unknown_detail.size() == 1);
    assert(
        unknown_detail[0] ==
        "function main aggregate projection access intent <unknown> status <unknown> binding <unknown> "
        "source <unknown> receiver <unknown>"
    );
}

}  // namespace

auto main() -> int {
    assert_computed_cleanup_capability_reports();
    assert_dynamic_array_cleanup_emission_capability_reports();
    assert_computed_cleanup_readiness_reports();
    assert_computed_dynamic_array_render_reports();
    assert_computed_inserted_cleanup_handoff_reports();
    assert_computed_cleanup_verification_and_emission_gate_reports();
    assert_computed_cleanup_call_plan_and_render_reports();
    assert_computed_inserted_cleanup_call_reports();
    assert_consumed_descriptor_finalization_and_model_reports();
    assert_computed_consumed_cleanup_descriptor_reports();
    assert_computed_cleanup_proof_summary_reports();
    assert_computed_cleanup_unknown_detail_fallbacks();
    assert_computed_dynamic_array_production_reports();
    assert_aggregate_projection_access_plan_reports();
    return 0;
}
