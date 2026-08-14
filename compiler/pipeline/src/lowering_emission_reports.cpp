#include "lowering_emission_reports.hpp"

#include "orison/lowering/dynamic_array_cleanup_plan.hpp"
#include "orison/lowering/aggregate_path.hpp"
#include "orison/lowering/llvm_ir_verifier.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/semantics/drop_model.hpp"

#include "dynamic_array_cleanup_readiness.hpp"
#include "computed_cleanup_proof_model.hpp"
#include "runtime_indexed_cleanup_ir_composition.hpp"

#include <algorithm>
#include <optional>
#include <sstream>
#include <unordered_set>

namespace orison::pipeline {

namespace {

auto runtime_indexed_cleanup_function_ir_rewrite_requested(
    CompilePipelineOptions const& options
) -> bool {
    return options.runtime_indexed_cleanup_function_ir_module_rewrite_enabled ||
        options.runtime_indexed_cleanup_verified_function_ir_rewrite_enabled;
}

auto logical_line_count(std::string const& text) -> std::size_t {
    if (text.empty()) {
        return 0;
    }

    auto line_count = static_cast<std::size_t>(
        std::count(text.begin(), text.end(), '\n')
    );
    if (text.back() != '\n') {
        ++line_count;
    }
    return line_count;
}

auto function_ir_slice(
    std::string const& ir_text,
    std::string const& function_symbol_name
) -> std::string {
    if (ir_text.empty() || function_symbol_name.empty()) {
        return {};
    }

    auto const define_marker = "define ";
    auto const symbol_marker = "@" + function_symbol_name + "(";
    auto search_position = std::string::size_type {0};
    while (true) {
        auto const define_position = ir_text.find(define_marker, search_position);
        if (define_position == std::string::npos) {
            return {};
        }
        auto const line_end = ir_text.find('\n', define_position);
        if (line_end == std::string::npos) {
            return {};
        }
        auto const header = ir_text.substr(define_position, line_end - define_position);
        if (header.find(symbol_marker) != std::string::npos) {
            auto const function_end = ir_text.find("\n}\n", line_end);
            if (function_end == std::string::npos) {
                return ir_text.substr(define_position);
            }
            return ir_text.substr(define_position, function_end + 3 - define_position);
        }
        search_position = line_end + 1;
    }
}

auto joined_lines(std::vector<std::string> const& lines) -> std::string {
    auto text = std::string {};
    for (auto const& line : lines) {
        text += line;
    }
    return text;
}

auto runtime_indexed_cleanup_text_splice_range(
    std::size_t start_offset,
    std::size_t end_offset
) -> RuntimeIndexedCleanupTextSpliceRange {
    return RuntimeIndexedCleanupTextSpliceRange {
        .start_offset = start_offset,
        .end_offset = end_offset,
    };
}

auto runtime_indexed_cleanup_text_splice_range(
    RuntimeIndexedCleanupTextSpliceRange range
) -> RuntimeIndexedCleanupTextSpliceRange {
    return runtime_indexed_cleanup_text_splice_range(range.start_offset, range.end_offset);
}

auto build_runtime_indexed_cleanup_function_ir_insertion(
    RuntimeIndexedCleanupFunctionCfgRewritePlan const& rewrite_plan
) -> RuntimeIndexedCleanupFunctionIrInsertion {
    return RuntimeIndexedCleanupFunctionIrInsertion {
        .predecessor_block_name = rewrite_plan.predecessor_block_name,
        .inserted_branch_text = rewrite_plan.inserted_branch_text,
        .cfg_lines = rewrite_plan.candidate_cfg_lines,
    };
}

auto replace_once(
    std::string const& text,
    std::string const& old_text,
    std::string const& new_text
) -> std::string {
    if (text.empty() || old_text.empty() || new_text.empty()) {
        return {};
    }
    if (occurrence_count(text, old_text) != 1) {
        return {};
    }

    auto const replacement_position = text.find(old_text);
    auto replaced = text.substr(0, replacement_position);
    replaced += new_text;
    replaced += text.substr(replacement_position + old_text.size());
    return replaced;
}

auto build_dynamic_array_descriptor_cleanup_plan_state(
    lowering::LlvmIrEmissionResult const& emission
) -> DynamicArrayDescriptorCleanupPlanState {
    return DynamicArrayDescriptorCleanupPlanState {
        .plans = emission.dynamic_array_descriptor_cleanup_plans,
    };
}

auto build_dynamic_array_construction_plan_state(
    lowering::LlvmIrEmissionResult const& emission
) -> DynamicArrayConstructionPlanState {
    return DynamicArrayConstructionPlanState {
        .plans = emission.dynamic_array_construction_plans,
    };
}

auto build_dynamic_array_runtime_request_state(
    lowering::LlvmIrEmissionResult const& emission
) -> DynamicArrayRuntimeRequestState {
    return DynamicArrayRuntimeRequestState {
        .operations = emission.dynamic_array_runtime_operations,
    };
}

auto build_dynamic_array_allocation_call_emission_state(
    std::vector<std::string>&& rendered_ir_snippets
) -> DynamicArrayAllocationCallEmissionState {
    auto state = DynamicArrayAllocationCallEmissionState {
        .ir_artifact_state = DynamicArrayAllocationCallIrArtifactState {
            .rendered_ir_snippets = std::move(rendered_ir_snippets),
        },
    };
    state.rendered_call_count = state.ir_artifact_state.rendered_ir_snippets.size();
    state.allocation_calls_rendered = state.rendered_call_count > 0;
    return state;
}

auto build_planned_drop_declaration_state(
    lowering::LlvmIrEmissionResult const& emission
) -> PlannedDropDeclarationState {
    return PlannedDropDeclarationState {
        .declarations = emission.planned_drop_declarations,
    };
}

auto build_planned_drop_action_state(
    lowering::LlvmIrEmissionResult const& emission
) -> PlannedDropActionState {
    return PlannedDropActionState {
        .actions = emission.planned_drop_actions,
    };
}

auto build_drop_cleanup_authorization_state(
    lowering::LlvmIrEmissionResult const& emission
) -> DropCleanupAuthorizationState {
    auto state = DropCleanupAuthorizationState {};
    state.cleanups = emission.drop_cleanups;
    state.authorizations.reserve(emission.drop_cleanups.size());
    for (auto const& cleanup : emission.drop_cleanups) {
        state.authorizations.push_back(
            lowering::plan_drop_cleanup_authorization(
                cleanup,
                emission.planned_drop_declarations,
                emission.semantic_drop_lowering_authorizations
            )
        );
    }
    return state;
}

auto build_dynamic_array_cleanup_obligation_state(
    lowering::LlvmIrEmissionResult const& emission
) -> DynamicArrayCleanupObligationState {
    return DynamicArrayCleanupObligationState {
        .obligations = emission.dynamic_array_cleanup_obligations,
    };
}

auto build_emitted_dynamic_array_cleanup_obligation_state(
    std::vector<lowering::DynamicArrayCleanupObligationRecord> const& records
) -> DynamicArrayCleanupObligationState {
    auto state = DynamicArrayCleanupObligationState {};
    state.function_symbol_names.reserve(records.size());
    state.obligations.reserve(records.size());
    for (auto const& record : records) {
        state.function_symbol_names.push_back(record.function_symbol_name);
        state.obligations.push_back(record.obligation);
    }
    return state;
}

auto build_dynamic_array_cleanup_sequence_plan_state(
    lowering::LlvmIrEmissionResult const& emission
) -> DynamicArrayCleanupSequencePlanState {
    return DynamicArrayCleanupSequencePlanState {
        .plans = emission.dynamic_array_cleanup_sequence_plans,
    };
}

auto build_dynamic_array_cleanup_sequence_verification_state(
    lowering::LlvmIrEmissionResult const& emission
) -> DynamicArrayCleanupSequenceVerificationState {
    return DynamicArrayCleanupSequenceVerificationState {
        .verifications = emission.dynamic_array_cleanup_sequence_verifications,
    };
}

auto build_emitted_dynamic_array_cleanup_sequence_plan_state(
    std::vector<lowering::DynamicArrayCleanupSequencePlanRecord> const& records
) -> DynamicArrayCleanupSequencePlanState {
    auto state = DynamicArrayCleanupSequencePlanState {};
    state.function_symbol_names.reserve(records.size());
    state.plans.reserve(records.size());
    for (auto const& record : records) {
        state.function_symbol_names.push_back(record.function_symbol_name);
        state.plans.push_back(record.plan);
    }
    return state;
}

auto build_emitted_dynamic_array_cleanup_sequence_verification_state(
    std::vector<lowering::DynamicArrayCleanupSequenceVerificationRecord> const& records
) -> DynamicArrayCleanupSequenceVerificationState {
    auto state = DynamicArrayCleanupSequenceVerificationState {};
    state.function_symbol_names.reserve(records.size());
    state.verifications.reserve(records.size());
    for (auto const& record : records) {
        state.function_symbol_names.push_back(record.function_symbol_name);
        state.verifications.push_back(record.verification);
    }
    return state;
}

auto build_computed_dynamic_array_for_production_sequence_state(
    lowering::LlvmIrEmissionResult const& emission
) -> ComputedDynamicArrayForProductionSequenceState {
    auto state = ComputedDynamicArrayForProductionSequenceState {
        .module_comments_emitted = !emission.computed_dynamic_array_for_production_sequence_module_ir.empty(),
        .sequence_count = emission.computed_dynamic_array_for_production_sequences.size(),
        .module_comment_line_count =
            emission.computed_dynamic_array_for_production_sequence_module_ir.size(),
    };
    state.sequence_metadata_available = state.sequence_count > 0;
    state.cleanup_owner_names.reserve(emission.computed_dynamic_array_for_production_sequences.size());
    for (auto const& sequence : emission.computed_dynamic_array_for_production_sequences) {
        state.cleanup_owner_names.push_back(sequence.cleanup_owner_name);
        state.rendered_ir_snippet_count += sequence.rendered_ir.size();
    }
    return state;
}

auto build_dynamic_array_cleanup_emission_capability_state(
    lowering::LlvmIrEmissionResult const& emission
) -> DynamicArrayCleanupEmissionCapabilityState {
    auto const& capability = emission.dynamic_array_cleanup_emission_capability;
    if (!capability.has_value()) {
        return {};
    }
    auto function_symbol_names = std::vector<std::string> {};
    function_symbol_names.reserve(emission.emitted_dynamic_array_cleanup_emission_capabilities.size());
    for (auto const& emitted_capability : emission.emitted_dynamic_array_cleanup_emission_capabilities) {
        function_symbol_names.push_back(emitted_capability.function_symbol_name);
    }
    return DynamicArrayCleanupEmissionCapabilityState {
        .function_symbol_names = std::move(function_symbol_names),
        .cleanup_pairs = capability->cleanup_pairs,
        .cleanup_operation_names = capability->cleanup_operation_names,
        .cleanup_owner_names = capability->cleanup_owner_names,
        .element_drop_pairs = capability->element_drop_pairs,
        .missing_element_drop_pairs = capability->missing_element_drop_pairs,
        .capability_metadata_available = true,
        .proven = lowering::dynamic_array_cleanup_emission_capability_proven(*capability),
        .emission_enabled = capability->emission_enabled,
        .descriptor_storage_bound = capability->descriptor_storage_bound,
        .sequence_verified = capability->sequence_verified,
        .element_cleanup_authorized_or_not_required =
            capability->element_cleanup_authorized_or_not_required,
        .descriptor_deallocation_authorized = capability->descriptor_deallocation_authorized,
    };
}

auto build_runtime_indexed_cleanup_capability_state(
    lowering::LlvmIrEmissionResult const& emission
) -> RuntimeIndexedCleanupCapabilityState {
    auto state = RuntimeIndexedCleanupCapabilityState {
        .capabilities = emission.runtime_indexed_cleanup_capabilities,
        .capability_metadata_available = !emission.runtime_indexed_cleanup_capabilities.empty(),
        .all_prerequisites_ready = !emission.runtime_indexed_cleanup_capabilities.empty(),
        .any_production_enabled = false,
        .capability_count = emission.runtime_indexed_cleanup_capabilities.size(),
    };
    for (auto const& capability : emission.runtime_indexed_cleanup_capabilities) {
        state.all_prerequisites_ready = state.all_prerequisites_ready && capability.prerequisites_ready;
        state.any_production_enabled = state.any_production_enabled || capability.production_enabled;
    }
    return state;
}

auto build_runtime_indexed_cleanup_emission_plan_state(
    lowering::LlvmIrEmissionResult const& emission
) -> RuntimeIndexedCleanupEmissionPlanState {
    auto state = RuntimeIndexedCleanupEmissionPlanState {
        .plans = emission.runtime_indexed_cleanup_emission_plans,
        .plan_metadata_available = !emission.runtime_indexed_cleanup_emission_plans.empty(),
        .all_prerequisites_ready = !emission.runtime_indexed_cleanup_emission_plans.empty(),
        .any_production_gate_requested = false,
        .any_production_enabled = false,
        .any_length_load_slice_lowerable = false,
        .any_loop_block_slice_lowerable = false,
        .any_skip_branch_slice_lowerable = false,
        .any_live_element_drop_slice_lowerable = false,
        .any_cleanup_tail_slice_lowerable = false,
        .any_structured_ir_plan_complete = false,
        .all_function_insertion_targets_known = !emission.runtime_indexed_cleanup_emission_plans.empty(),
        .any_function_insertion_planned = false,
        .plan_count = emission.runtime_indexed_cleanup_emission_plans.size(),
    };
    for (auto const& plan : emission.runtime_indexed_cleanup_emission_plans) {
        state.all_prerequisites_ready = state.all_prerequisites_ready && plan.prerequisites_ready;
        state.any_production_gate_requested =
            state.any_production_gate_requested || plan.production_gate_requested;
        state.any_production_enabled = state.any_production_enabled || plan.production_enabled;
        state.any_length_load_slice_lowerable =
            state.any_length_load_slice_lowerable || plan.length_load_slice_lowerable;
        state.any_loop_block_slice_lowerable =
            state.any_loop_block_slice_lowerable || plan.loop_block_slice_lowerable;
        state.any_skip_branch_slice_lowerable =
            state.any_skip_branch_slice_lowerable || plan.skip_branch_slice_lowerable;
        state.any_live_element_drop_slice_lowerable =
            state.any_live_element_drop_slice_lowerable || plan.live_element_drop_slice_lowerable;
        state.any_cleanup_tail_slice_lowerable =
            state.any_cleanup_tail_slice_lowerable || plan.cleanup_tail_slice_lowerable;
        state.any_structured_ir_plan_complete =
            state.any_structured_ir_plan_complete || plan.ir_plan.complete;
        state.all_function_insertion_targets_known =
            state.all_function_insertion_targets_known && plan.function_insertion_target_known;
        state.any_function_insertion_planned =
            state.any_function_insertion_planned || plan.function_insertion_planned;
        if (plan.ir_plan.complete) {
            ++state.structured_ir_plan_count;
        }
        if (plan.function_insertion_planned) {
            ++state.function_insertion_plan_count;
        }
        state.operation_count += plan.operation_count;
        state.comment_ir_preview_line_count += plan.comment_ir_preview_line_count;
        state.gated_ir_slice_line_count += plan.gated_ir_slice_line_count;
    }
    return state;
}

auto build_aggregate_projection_access_plan_state(
    lowering::LlvmIrEmissionResult const& emission
) -> AggregateProjectionAccessPlanState {
    auto const& access_plans = emission.aggregate_projection_access_plans;
    auto state = AggregateProjectionAccessPlanState {
        .plan_count = access_plans.size(),
    };
    state.access_plans_available = state.plan_count > 0;
    state.function_symbol_names.reserve(access_plans.size());
    state.intents.reserve(access_plans.size());
    state.statuses.reserve(access_plans.size());
    state.binding_names.reserve(access_plans.size());
    state.source_type_names.reserve(access_plans.size());
    state.diagnostics.reserve(access_plans.size());
    state.receiver_projections.reserve(access_plans.size());
    for (auto const& access_plan : access_plans) {
        state.function_symbol_names.push_back(access_plan.function_symbol_name);
        state.intents.push_back(access_plan.plan.intent);
        state.statuses.push_back(access_plan.plan.status);
        state.binding_names.push_back(access_plan.plan.binding_name);
        state.source_type_names.push_back(access_plan.plan.source_type_name);
        state.diagnostics.push_back(lowering::aggregate_projection_access_diagnostic(access_plan.plan));
        state.receiver_projections.push_back(access_plan.plan.receiver_projection);
        if (access_plan.plan.status == lowering::AggregateProjectionAccessStatus::allowed ||
            access_plan.plan.status == lowering::AggregateProjectionAccessStatus::non_owned_projection) {
            ++state.allowed_count;
        }
        if (access_plan.plan.status == lowering::AggregateProjectionAccessStatus::requires_explicit_boundary ||
            access_plan.plan.status == lowering::AggregateProjectionAccessStatus::boundary_not_enabled) {
            ++state.blocked_count;
        }
        if (access_plan.plan.receiver_projection) {
            ++state.receiver_projection_count;
        }
    }
    return state;
}

auto build_runtime_indexed_cleanup_ir_render_state(
    lowering::LlvmIrEmissionResult const& emission
) -> RuntimeIndexedCleanupIrRenderState {
    auto state = RuntimeIndexedCleanupIrRenderState {
        .render_metadata_available = !emission.runtime_indexed_cleanup_emission_plans.empty(),
        .all_structured_plans_complete = !emission.runtime_indexed_cleanup_emission_plans.empty(),
        .all_rendered_lines_match_artifact = !emission.runtime_indexed_cleanup_emission_plans.empty(),
        .plan_count = emission.runtime_indexed_cleanup_emission_plans.size(),
    };
    for (auto const& plan : emission.runtime_indexed_cleanup_emission_plans) {
        auto rendered_lines = lowering::render_runtime_indexed_cleanup_ir_plan(plan.ir_plan);
        state.all_structured_plans_complete =
            state.all_structured_plans_complete && plan.ir_plan.complete;
        state.all_rendered_lines_match_artifact =
            state.all_rendered_lines_match_artifact &&
            rendered_lines == plan.gated_ir_slice_lines;
        if (!rendered_lines.empty()) {
            ++state.rendered_plan_count;
        }
        state.rendered_ir_line_count += rendered_lines.size();
        state.rendered_ir_lines.insert(
            state.rendered_ir_lines.end(),
            rendered_lines.begin(),
            rendered_lines.end()
        );
    }
    return state;
}

auto build_runtime_indexed_cleanup_module_ir_artifact_state(
    RuntimeIndexedCleanupIrRenderState const& render_state
) -> RuntimeIndexedCleanupModuleIrArtifactState {
    auto artifact_state = RuntimeIndexedCleanupModuleIrArtifactState {
        .artifact_available = render_state.render_metadata_available &&
            render_state.all_structured_plans_complete &&
            render_state.all_rendered_lines_match_artifact &&
            !render_state.rendered_ir_lines.empty(),
        .separate_from_module_ir = true,
    };
    if (!artifact_state.artifact_available) {
        return artifact_state;
    }

    artifact_state.rendered_ir_lines = render_state.rendered_ir_lines;
    artifact_state.rendered_ir_line_count = artifact_state.rendered_ir_lines.size();
    return artifact_state;
}

auto build_runtime_indexed_cleanup_module_ir_insertion_gate_state(
    RuntimeIndexedCleanupIrRenderState const& render_state,
    RuntimeIndexedCleanupModuleIrArtifactState const& artifact_state,
    CompilePipelineOptions const& options
) -> RuntimeIndexedCleanupModuleIrInsertionGateState {
    return RuntimeIndexedCleanupModuleIrInsertionGateState {
        .insertion_requested = options.runtime_indexed_cleanup_module_ir_insertion_enabled,
        .artifact_available = artifact_state.artifact_available,
        .render_parity_verified = render_state.all_rendered_lines_match_artifact,
        .insertion_enabled = options.runtime_indexed_cleanup_module_ir_insertion_enabled &&
            artifact_state.artifact_available &&
            render_state.all_rendered_lines_match_artifact,
        .remains_separate_from_module_ir = true,
    };
}

auto build_runtime_indexed_cleanup_module_ir_insertion_preview_state(
    std::string const& ir_text,
    RuntimeIndexedCleanupModuleIrArtifactState const& artifact_state,
    RuntimeIndexedCleanupModuleIrInsertionGateState const& insertion_gate_state
) -> RuntimeIndexedCleanupModuleIrInsertionPreviewState {
    auto original_module_line_count = logical_line_count(ir_text);
    auto preview_state = RuntimeIndexedCleanupModuleIrInsertionPreviewState {
        .preview_available = insertion_gate_state.insertion_enabled,
        .insertion_point_found = insertion_gate_state.insertion_enabled,
        .would_modify_module_ir = false,
        .insertion_line_index = original_module_line_count,
        .original_module_line_count = original_module_line_count,
    };
    if (!preview_state.preview_available) {
        preview_state.projected_module_line_count = original_module_line_count;
        return preview_state;
    }

    preview_state.inserted_ir_line_count = artifact_state.rendered_ir_line_count;
    preview_state.projected_module_line_count =
        original_module_line_count + preview_state.inserted_ir_line_count;
    return preview_state;
}

auto build_runtime_indexed_cleanup_module_ir_candidate_state(
    std::string const& ir_text,
    RuntimeIndexedCleanupModuleIrArtifactState const& artifact_state,
    RuntimeIndexedCleanupModuleIrInsertionPreviewState const& preview_state
) -> RuntimeIndexedCleanupModuleIrCandidateState {
    auto candidate_state = RuntimeIndexedCleanupModuleIrCandidateState {
        .candidate_available = preview_state.preview_available &&
            preview_state.insertion_point_found,
        .separate_from_module_ir = true,
        .original_module_line_count = preview_state.original_module_line_count,
    };
    if (!candidate_state.candidate_available) {
        candidate_state.candidate_module_line_count = preview_state.original_module_line_count;
        return candidate_state;
    }

    candidate_state.candidate_ir_text = ir_text;
    if (!candidate_state.candidate_ir_text.empty() &&
        candidate_state.candidate_ir_text.back() != '\n') {
        candidate_state.candidate_ir_text += "\n";
    }
    for (auto const& rendered_line : artifact_state.rendered_ir_lines) {
        candidate_state.candidate_ir_text += rendered_line;
    }
    candidate_state.inserted_ir_line_count = artifact_state.rendered_ir_line_count;
    candidate_state.candidate_module_line_count =
        logical_line_count(candidate_state.candidate_ir_text);
    return candidate_state;
}

auto build_runtime_indexed_cleanup_module_ir_candidate_verification_state(
    std::string const& ir_text,
    RuntimeIndexedCleanupModuleIrArtifactState const& artifact_state,
    RuntimeIndexedCleanupModuleIrCandidateState const& candidate_state
) -> RuntimeIndexedCleanupModuleIrCandidateVerificationState {
    auto verification_state = RuntimeIndexedCleanupModuleIrCandidateVerificationState {
        .verification_available = candidate_state.candidate_available &&
            !artifact_state.rendered_ir_lines.empty(),
    };
    if (!verification_state.verification_available) {
        return verification_state;
    }

    auto const& cleanup_block_anchor = artifact_state.rendered_ir_lines.front();
    verification_state.candidate_cleanup_block_count =
        occurrence_count(candidate_state.candidate_ir_text, cleanup_block_anchor);
    verification_state.emitted_module_cleanup_block_count =
        occurrence_count(ir_text, cleanup_block_anchor);
    verification_state.candidate_contains_cleanup_block_once =
        verification_state.candidate_cleanup_block_count == 1;
    verification_state.emitted_module_excludes_cleanup_block =
        verification_state.emitted_module_cleanup_block_count == 0;
    verification_state.verified =
        verification_state.candidate_contains_cleanup_block_once &&
        verification_state.emitted_module_excludes_cleanup_block;
    return verification_state;
}

auto apply_runtime_indexed_cleanup_module_ir_mutation(
    CompilePipelineOptions const& options,
    RuntimeIndexedCleanupModuleIrArtifactState const& artifact_state,
    RuntimeIndexedCleanupModuleIrCandidateState const& candidate_state,
    RuntimeIndexedCleanupModuleIrCandidateVerificationState const& verification_state,
    std::string& ir_text
) -> RuntimeIndexedCleanupModuleIrMutationState {
    auto mutation_state = RuntimeIndexedCleanupModuleIrMutationState {
        .mutation_requested = options.runtime_indexed_cleanup_module_ir_mutation_enabled,
        .candidate_verified = verification_state.verified,
    };
    if (mutation_state.mutation_requested && mutation_state.candidate_verified) {
        ir_text = candidate_state.candidate_ir_text;
        mutation_state.mutation_applied = true;
    }

    mutation_state.module_matches_candidate =
        mutation_state.mutation_applied &&
        candidate_state.candidate_available &&
        ir_text == candidate_state.candidate_ir_text;
    mutation_state.final_module_line_count = logical_line_count(ir_text);
    if (!artifact_state.rendered_ir_lines.empty()) {
        mutation_state.final_module_cleanup_block_count =
            occurrence_count(ir_text, artifact_state.rendered_ir_lines.front());
    }
    return mutation_state;
}

auto build_runtime_indexed_cleanup_function_cfg_rewrite_plan_state(
    RuntimeIndexedCleanupEmissionPlanState const& emission_plan_state
) -> RuntimeIndexedCleanupFunctionCfgRewritePlanState {
    auto state = RuntimeIndexedCleanupFunctionCfgRewritePlanState {
        .metadata_available = emission_plan_state.plan_metadata_available,
        .all_targets_known = emission_plan_state.plan_metadata_available,
        .function_ir_unchanged = true,
        .plan_count = emission_plan_state.plan_count,
    };
    state.plans.reserve(emission_plan_state.plans.size());
    for (auto const& emission_plan : emission_plan_state.plans) {
        auto rewrite_plan = RuntimeIndexedCleanupFunctionCfgRewritePlan {
            .function_symbol_name = emission_plan.function_symbol_name,
            .owner_name = emission_plan.owner_name,
            .predecessor_block_name = emission_plan.function_predecessor_block_name,
            .insertion_block_name = emission_plan.function_insertion_block_name,
            .continuation_block_name = emission_plan.function_continuation_block_name,
            .target_known = emission_plan.function_insertion_target_known,
            .rewrite_candidate_available = emission_plan.function_insertion_planned &&
                !emission_plan.gated_ir_slice_lines.empty(),
            .function_ir_unchanged = true,
            .source_line = emission_plan.source_line,
            .cleanup_slice_line_count = emission_plan.gated_ir_slice_line_count,
        };
        if (rewrite_plan.rewrite_candidate_available) {
            rewrite_plan.replaced_terminator_text =
                "br label %" + rewrite_plan.continuation_block_name;
            rewrite_plan.inserted_branch_text =
                "br label %" + rewrite_plan.insertion_block_name;
            rewrite_plan.continuation_block_text =
                rewrite_plan.continuation_block_name + ":\n";
            rewrite_plan.candidate_cfg_lines.push_back(
                "  " + rewrite_plan.inserted_branch_text + "\n"
            );
            rewrite_plan.candidate_cfg_lines.push_back(
                rewrite_plan.insertion_block_name + ":\n"
            );
            rewrite_plan.candidate_cfg_lines.insert(
                rewrite_plan.candidate_cfg_lines.end(),
                emission_plan.gated_ir_slice_lines.begin(),
                emission_plan.gated_ir_slice_lines.end()
            );
            rewrite_plan.continuation_block_generated =
                joined_lines(rewrite_plan.candidate_cfg_lines).find(
                    rewrite_plan.continuation_block_text
                ) != std::string::npos;
            rewrite_plan.candidate_cfg_line_count =
                rewrite_plan.candidate_cfg_lines.size();
        }
        state.all_targets_known = state.all_targets_known && rewrite_plan.target_known;
        state.any_rewrite_candidate_available =
            state.any_rewrite_candidate_available || rewrite_plan.rewrite_candidate_available;
        state.any_continuation_block_generated =
            state.any_continuation_block_generated || rewrite_plan.continuation_block_generated;
        if (rewrite_plan.rewrite_candidate_available) {
            ++state.rewrite_candidate_count;
        }
        state.cleanup_slice_line_count += rewrite_plan.cleanup_slice_line_count;
        state.candidate_cfg_line_count += rewrite_plan.candidate_cfg_line_count;
        state.plans.push_back(std::move(rewrite_plan));
    }
    return state;
}

auto build_runtime_indexed_cleanup_function_cfg_rewrite_verification_state(
    std::string const& ir_text,
    RuntimeIndexedCleanupFunctionCfgRewritePlanState const& rewrite_plan_state
) -> RuntimeIndexedCleanupFunctionCfgRewriteVerificationState {
    auto state = RuntimeIndexedCleanupFunctionCfgRewriteVerificationState {
        .verification_metadata_available = rewrite_plan_state.metadata_available,
        .all_functions_found = rewrite_plan_state.metadata_available,
        .all_predecessor_blocks_found = rewrite_plan_state.metadata_available,
        .all_insertion_blocks_absent = rewrite_plan_state.metadata_available,
        .all_continuation_blocks_found = rewrite_plan_state.metadata_available,
        .all_candidate_insertion_blocks_found = rewrite_plan_state.metadata_available,
        .all_candidate_continuation_blocks_found = rewrite_plan_state.metadata_available,
        .all_candidates_verified = rewrite_plan_state.metadata_available,
        .all_verified = rewrite_plan_state.metadata_available,
        .verification_count = rewrite_plan_state.plan_count,
    };
    state.verifications.reserve(rewrite_plan_state.plans.size());
    for (auto const& rewrite_plan : rewrite_plan_state.plans) {
        auto const function_ir = function_ir_slice(ir_text, rewrite_plan.function_symbol_name);
        auto const candidate_cfg_text = joined_lines(rewrite_plan.candidate_cfg_lines);
        auto verification = RuntimeIndexedCleanupFunctionCfgRewriteVerification {
            .function_symbol_name = rewrite_plan.function_symbol_name,
            .predecessor_block_name = rewrite_plan.predecessor_block_name,
            .insertion_block_name = rewrite_plan.insertion_block_name,
            .continuation_block_name = rewrite_plan.continuation_block_name,
            .verification_available = rewrite_plan.rewrite_candidate_available,
            .function_found = !function_ir.empty(),
            .predecessor_block_found = block_label_found(function_ir, rewrite_plan.predecessor_block_name),
            .insertion_block_absent = !block_label_found(function_ir, rewrite_plan.insertion_block_name),
            .continuation_block_found = block_label_found(function_ir, rewrite_plan.continuation_block_name),
            .candidate_insertion_block_found =
                block_label_found(candidate_cfg_text, rewrite_plan.insertion_block_name),
            .candidate_continuation_block_found =
                block_label_found(candidate_cfg_text, rewrite_plan.continuation_block_name),
        };
        verification.candidate_verified =
            verification.verification_available &&
            verification.candidate_insertion_block_found &&
            verification.candidate_continuation_block_found;
        verification.verified =
            verification.verification_available &&
            verification.function_found &&
            verification.predecessor_block_found &&
            verification.insertion_block_absent &&
            verification.continuation_block_found;
        state.all_functions_found = state.all_functions_found && verification.function_found;
        state.all_predecessor_blocks_found =
            state.all_predecessor_blocks_found && verification.predecessor_block_found;
        state.all_insertion_blocks_absent =
            state.all_insertion_blocks_absent && verification.insertion_block_absent;
        state.all_continuation_blocks_found =
            state.all_continuation_blocks_found && verification.continuation_block_found;
        state.all_candidate_insertion_blocks_found =
            state.all_candidate_insertion_blocks_found && verification.candidate_insertion_block_found;
        state.all_candidate_continuation_blocks_found =
            state.all_candidate_continuation_blocks_found && verification.candidate_continuation_block_found;
        state.all_candidates_verified =
            state.all_candidates_verified && verification.candidate_verified;
        state.all_verified = state.all_verified && verification.verified;
        if (verification.candidate_verified) {
            ++state.candidate_verified_count;
        }
        if (verification.verified) {
            ++state.verified_count;
        }
        state.verifications.push_back(std::move(verification));
    }
    return state;
}

auto build_runtime_indexed_cleanup_function_ir_rewrite_candidate_state(
    std::string const& ir_text,
    RuntimeIndexedCleanupFunctionCfgRewritePlanState const& rewrite_plan_state
) -> RuntimeIndexedCleanupFunctionIrRewriteCandidateState {
    auto state = RuntimeIndexedCleanupFunctionIrRewriteCandidateState {
        .metadata_available = rewrite_plan_state.metadata_available,
        .all_candidates_separate_from_module_ir = rewrite_plan_state.metadata_available,
        .candidate_count = rewrite_plan_state.plan_count,
    };
    state.candidates.reserve(rewrite_plan_state.plans.size());
    for (auto const& rewrite_plan : rewrite_plan_state.plans) {
        auto candidate = RuntimeIndexedCleanupFunctionIrRewriteCandidate {
            .function_symbol_name = rewrite_plan.function_symbol_name,
            .predecessor_block_name = rewrite_plan.predecessor_block_name,
            .insertion_block_name = rewrite_plan.insertion_block_name,
            .continuation_block_name = rewrite_plan.continuation_block_name,
            .replaced_terminator_text = rewrite_plan.replaced_terminator_text,
            .inserted_branch_text = rewrite_plan.inserted_branch_text,
            .original_function_ir_text = function_ir_slice(ir_text, rewrite_plan.function_symbol_name),
            .separate_from_module_ir = true,
            .source_line = rewrite_plan.source_line,
        };
        candidate.original_function_line_count =
            logical_line_count(candidate.original_function_ir_text);
        auto const original_predecessor_terminator =
            predecessor_terminator_pattern(
                candidate.original_function_ir_text,
                candidate.predecessor_block_name
            );
        auto const terminator_position =
            predecessor_terminator_position(
                candidate.original_function_ir_text,
                candidate.predecessor_block_name,
                original_predecessor_terminator
            );
        if (!original_predecessor_terminator.empty()) {
            candidate.replaced_terminator_text =
                original_predecessor_terminator.substr(2, original_predecessor_terminator.size() - 3);
        }
        if (terminator_position != std::string::npos) {
            candidate.splice_range_available = true;
            candidate.splice_range = RuntimeIndexedCleanupTextSpliceRange {
                .start_offset = terminator_position,
                .end_offset = terminator_position + original_predecessor_terminator.size(),
            };
        }
        if (rewrite_plan.rewrite_candidate_available) {
            auto const rewrite_result =
                rewrite_predecessor_terminator_and_insert_cfg_result(
                    candidate.original_function_ir_text,
                    build_runtime_indexed_cleanup_function_ir_insertion(rewrite_plan)
                );
            candidate.candidate_function_ir_text = rewrite_result.rewritten_function_ir;
            candidate.composition_failure = rewrite_result.failure;
        }
        candidate.candidate_available =
            rewrite_plan.rewrite_candidate_available &&
            !candidate.original_function_ir_text.empty() &&
            !candidate.candidate_function_ir_text.empty();
        candidate.function_ir_changed =
            candidate.candidate_available &&
            candidate.candidate_function_ir_text != candidate.original_function_ir_text;
        candidate.predecessor_terminator_replaced =
            candidate.candidate_available &&
            !original_predecessor_terminator.empty() &&
            predecessor_branch_pattern(
                candidate.candidate_function_ir_text,
                candidate.predecessor_block_name,
                candidate.inserted_branch_text
            ) == "  " + candidate.inserted_branch_text + "\n";
        if (!candidate.candidate_available) {
            candidate.candidate_function_ir_text.clear();
        }
        candidate.candidate_function_line_count =
            logical_line_count(candidate.candidate_function_ir_text);
        if (candidate.candidate_available) {
            candidate.inserted_cfg_line_count =
                candidate.candidate_function_line_count - candidate.original_function_line_count;
        } else if (rewrite_plan.candidate_cfg_line_count > 0) {
            candidate.inserted_cfg_line_count = rewrite_plan.candidate_cfg_line_count - 1;
        }

        state.any_candidate_available =
            state.any_candidate_available || candidate.candidate_available;
        state.any_function_ir_changed =
            state.any_function_ir_changed || candidate.function_ir_changed;
        state.all_candidates_separate_from_module_ir =
            state.all_candidates_separate_from_module_ir && candidate.separate_from_module_ir;
        state.all_splice_ranges_available =
            state.all_splice_ranges_available && candidate.splice_range_available;
        if (candidate.candidate_available) {
            ++state.available_candidate_count;
        } else if (candidate.composition_failure != RuntimeIndexedCleanupIrCompositionFailure::none) {
            ++state.composition_failure_count;
            if (state.first_composition_failure == RuntimeIndexedCleanupIrCompositionFailure::none) {
                state.first_composition_failure = candidate.composition_failure;
            }
        }
        state.original_function_line_count += candidate.original_function_line_count;
        state.candidate_function_line_count += candidate.candidate_function_line_count;
        state.inserted_cfg_line_count += candidate.inserted_cfg_line_count;
        state.candidates.push_back(std::move(candidate));
    }
    for (auto left_index = std::size_t {0}; left_index < state.candidates.size(); ++left_index) {
        for (auto right_index = left_index + 1; right_index < state.candidates.size(); ++right_index) {
            auto const& left = state.candidates[left_index];
            auto const& right = state.candidates[right_index];
            if (left.function_symbol_name != right.function_symbol_name) {
                continue;
            }
            auto const ordered =
                left.splice_range_available &&
                right.splice_range_available &&
                left.splice_range.start_offset <= right.splice_range.start_offset;
            auto const non_overlapping =
                ordered &&
                left.splice_range.end_offset <= right.splice_range.start_offset;
            state.same_function_splice_ranges_ordered =
                state.same_function_splice_ranges_ordered && ordered;
            state.same_function_splice_ranges_non_overlapping =
                state.same_function_splice_ranges_non_overlapping && non_overlapping;
        }
    }
    return state;
}

auto build_runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state(
    RuntimeIndexedCleanupFunctionIrRewriteCandidateState const& candidate_state
) -> RuntimeIndexedCleanupFunctionIrRewriteCandidateVerificationState {
    auto state = RuntimeIndexedCleanupFunctionIrRewriteCandidateVerificationState {
        .verification_metadata_available = candidate_state.metadata_available,
        .all_original_functions_exclude_cleanup_cfg = candidate_state.metadata_available,
        .all_candidates_contain_cleanup_cfg_once = candidate_state.metadata_available,
        .all_candidates_contain_continuation_once = candidate_state.metadata_available,
        .all_candidates_route_predecessors_to_cleanup = candidate_state.metadata_available,
        .all_original_predecessor_terminators_found = candidate_state.metadata_available,
        .all_predecessor_terminators_replaced = candidate_state.metadata_available,
        .all_candidate_functions_changed = candidate_state.metadata_available,
        .all_candidates_separate_from_module_ir = candidate_state.metadata_available,
        .all_splice_ranges_available = candidate_state.metadata_available,
        .same_function_splice_ranges_ordered = candidate_state.same_function_splice_ranges_ordered,
        .same_function_splice_ranges_non_overlapping =
            candidate_state.same_function_splice_ranges_non_overlapping,
        .all_verified = candidate_state.metadata_available,
        .verification_count = candidate_state.candidate_count,
    };
    state.verifications.reserve(candidate_state.candidates.size());
    for (auto const& candidate : candidate_state.candidates) {
        auto const insertion_label = candidate.insertion_block_name + ":\n";
        auto const continuation_label = candidate.continuation_block_name + ":\n";
        auto const original_predecessor_branch =
            predecessor_terminator_pattern(
                candidate.original_function_ir_text,
                candidate.predecessor_block_name
            );
        auto const candidate_predecessor_branch =
            predecessor_branch_pattern(
                candidate.candidate_function_ir_text,
                candidate.predecessor_block_name,
                candidate.inserted_branch_text
            );
        auto verification = RuntimeIndexedCleanupFunctionIrRewriteCandidateVerification {
            .function_symbol_name = candidate.function_symbol_name,
            .predecessor_block_name = candidate.predecessor_block_name,
            .insertion_block_name = candidate.insertion_block_name,
            .continuation_block_name = candidate.continuation_block_name,
            .verification_available = candidate.candidate_available,
            .candidate_function_changed = candidate.function_ir_changed,
            .predecessor_terminator_replaced = candidate.predecessor_terminator_replaced,
            .splice_range_available = candidate.splice_range_available,
            .separate_from_module_ir = candidate.separate_from_module_ir,
            .composition_failure = candidate.composition_failure,
            .source_line = candidate.source_line,
            .original_cleanup_block_count =
                occurrence_count(candidate.original_function_ir_text, insertion_label),
            .candidate_cleanup_block_count =
                occurrence_count(candidate.candidate_function_ir_text, insertion_label),
            .candidate_continuation_block_count =
                occurrence_count(candidate.candidate_function_ir_text, continuation_label),
            .original_predecessor_terminator_count =
                original_predecessor_branch.empty() ? std::size_t {0} : std::size_t {1},
            .candidate_predecessor_cleanup_branch_count =
                candidate_predecessor_branch.empty() ? std::size_t {0} : std::size_t {1},
            .splice_range = candidate.splice_range,
        };
        verification.original_function_excludes_cleanup_cfg =
            verification.original_cleanup_block_count == 0;
        verification.candidate_contains_cleanup_cfg_once =
            verification.candidate_cleanup_block_count == 1;
        verification.candidate_contains_continuation_once =
            verification.candidate_continuation_block_count == 1;
        verification.original_predecessor_terminator_found =
            verification.original_predecessor_terminator_count == 1;
        verification.candidate_routes_predecessor_to_cleanup =
            verification.candidate_predecessor_cleanup_branch_count == 1;
        verification.verified =
            verification.verification_available &&
            verification.original_function_excludes_cleanup_cfg &&
            verification.candidate_contains_cleanup_cfg_once &&
            verification.candidate_contains_continuation_once &&
            verification.original_predecessor_terminator_found &&
            verification.candidate_routes_predecessor_to_cleanup &&
            verification.predecessor_terminator_replaced &&
            verification.splice_range_available &&
            verification.candidate_function_changed &&
            verification.separate_from_module_ir;

        state.all_original_functions_exclude_cleanup_cfg =
            state.all_original_functions_exclude_cleanup_cfg &&
            verification.original_function_excludes_cleanup_cfg;
        state.all_candidates_contain_cleanup_cfg_once =
            state.all_candidates_contain_cleanup_cfg_once &&
            verification.candidate_contains_cleanup_cfg_once;
        state.all_candidates_contain_continuation_once =
            state.all_candidates_contain_continuation_once &&
            verification.candidate_contains_continuation_once;
        state.all_original_predecessor_terminators_found =
            state.all_original_predecessor_terminators_found &&
            verification.original_predecessor_terminator_found;
        state.all_candidates_route_predecessors_to_cleanup =
            state.all_candidates_route_predecessors_to_cleanup &&
            verification.candidate_routes_predecessor_to_cleanup;
        state.all_predecessor_terminators_replaced =
            state.all_predecessor_terminators_replaced &&
            verification.predecessor_terminator_replaced;
        state.all_candidate_functions_changed =
            state.all_candidate_functions_changed && verification.candidate_function_changed;
        state.all_candidates_separate_from_module_ir =
            state.all_candidates_separate_from_module_ir && verification.separate_from_module_ir;
        state.all_splice_ranges_available =
            state.all_splice_ranges_available && verification.splice_range_available;
        state.all_verified = state.all_verified && verification.verified;
        if (verification.verified) {
            ++state.verified_count;
        } else if (verification.composition_failure != RuntimeIndexedCleanupIrCompositionFailure::none) {
            ++state.composition_failure_count;
            if (state.first_composition_failure == RuntimeIndexedCleanupIrCompositionFailure::none) {
                state.first_composition_failure = verification.composition_failure;
            }
        }
        state.verifications.push_back(std::move(verification));
    }
    return state;
}

auto build_runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state(
    CompilePipelineOptions const& options,
    std::string const& ir_text,
    RuntimeIndexedCleanupFunctionIrRewriteCandidateState const& function_candidate_state,
    RuntimeIndexedCleanupFunctionIrRewriteCandidateVerificationState const& function_verification_state
) -> RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateState {
    auto state = RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateState {
        .rewrite_requested = runtime_indexed_cleanup_function_ir_rewrite_requested(options),
        .metadata_available = function_candidate_state.metadata_available,
        .all_candidates_separate_from_module_ir = function_candidate_state.metadata_available,
        .first_composition_failure = function_candidate_state.first_composition_failure,
        .candidate_count = function_candidate_state.candidate_count,
        .composition_failure_count = function_candidate_state.composition_failure_count,
        .original_module_line_count = logical_line_count(ir_text),
    };
    state.candidates.reserve(function_candidate_state.candidates.size());
    for (auto index = std::size_t {0}; index < function_candidate_state.candidates.size(); ++index) {
        auto const& function_candidate = function_candidate_state.candidates[index];
        auto const function_verified =
            index < function_verification_state.verifications.size() &&
            function_verification_state.verifications[index].verified;
        auto candidate = RuntimeIndexedCleanupFunctionIrModuleRewriteCandidate {
            .function_symbol_name = function_candidate.function_symbol_name,
            .rewrite_requested = state.rewrite_requested,
            .function_candidate_verified = function_verified,
            .separate_from_module_ir = true,
            .source_line = function_candidate.source_line,
            .original_module_line_count = state.original_module_line_count,
            .function_replacement_count =
                occurrence_count(ir_text, function_candidate.original_function_ir_text),
        };
        if (candidate.rewrite_requested && candidate.function_candidate_verified) {
            candidate.candidate_module_ir_text =
                replace_once(
                    ir_text,
                    function_candidate.original_function_ir_text,
                    function_candidate.candidate_function_ir_text
                );
        }
        candidate.candidate_available = !candidate.candidate_module_ir_text.empty();
        candidate.module_ir_changed =
            candidate.candidate_available &&
            candidate.candidate_module_ir_text != ir_text;
        candidate.candidate_module_line_count =
            logical_line_count(candidate.candidate_module_ir_text);

        state.any_candidate_available =
            state.any_candidate_available || candidate.candidate_available;
        state.any_module_ir_changed =
            state.any_module_ir_changed || candidate.module_ir_changed;
        state.all_candidates_separate_from_module_ir =
            state.all_candidates_separate_from_module_ir && candidate.separate_from_module_ir;
        if (candidate.candidate_available) {
            ++state.available_candidate_count;
        }
        state.candidate_module_line_count += candidate.candidate_module_line_count;
        state.candidates.push_back(std::move(candidate));
    }
    return state;
}

auto build_runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state(
    RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateState const& module_candidate_state,
    RuntimeIndexedCleanupFunctionIrRewriteCandidateState const& function_candidate_state,
    RuntimeIndexedCleanupFunctionIrRewriteCandidateVerificationState const& function_verification_state
) -> RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateVerificationState {
    auto state = RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateVerificationState {
        .verification_metadata_available = module_candidate_state.metadata_available,
        .all_candidate_functions_found = module_candidate_state.metadata_available,
        .all_candidate_functions_match_verified_candidates = module_candidate_state.metadata_available,
        .all_replacement_targets_unique = module_candidate_state.metadata_available,
        .all_module_ir_changed = module_candidate_state.metadata_available,
        .all_candidates_separate_from_module_ir = module_candidate_state.metadata_available,
        .same_function_splice_ranges_non_overlapping =
            function_verification_state.same_function_splice_ranges_non_overlapping,
        .all_llvm_verifier_passed = module_candidate_state.metadata_available,
        .all_verified = module_candidate_state.metadata_available,
        .first_composition_failure = module_candidate_state.first_composition_failure,
        .verification_count = module_candidate_state.candidate_count,
        .composition_failure_count = module_candidate_state.composition_failure_count,
    };
    for (auto left_index = std::size_t {0}; left_index < function_candidate_state.candidates.size(); ++left_index) {
        for (auto right_index = left_index + 1; right_index < function_candidate_state.candidates.size(); ++right_index) {
            auto const& left = function_candidate_state.candidates[left_index];
            auto const& right = function_candidate_state.candidates[right_index];
            if (left.function_symbol_name != right.function_symbol_name ||
                !left.splice_range_available ||
                !right.splice_range_available) {
                continue;
            }
            auto const ranges_overlap =
                left.splice_range.start_offset < right.splice_range.end_offset &&
                right.splice_range.start_offset < left.splice_range.end_offset;
            if (!ranges_overlap) {
                continue;
            }
            state.splice_conflicts.push_back(
                RuntimeIndexedCleanupFunctionIrModuleRewriteSpliceConflict {
                    .function_symbol_name = left.function_symbol_name,
                    .left_candidate_index = left_index,
                    .right_candidate_index = right_index,
                    .left_source_line = left.source_line,
                    .right_source_line = right.source_line,
                    .left_splice_range = left.splice_range,
                    .right_splice_range = right.splice_range,
                }
            );
        }
    }
    state.splice_conflict_count = state.splice_conflicts.size();
    state.verifications.reserve(module_candidate_state.candidates.size());
    for (auto index = std::size_t {0}; index < module_candidate_state.candidates.size(); ++index) {
        auto const& module_candidate = module_candidate_state.candidates[index];
        auto const& function_candidate = function_candidate_state.candidates[index];
        auto const function_target_count = static_cast<std::size_t>(
            std::count_if(
                module_candidate_state.candidates.begin(),
                module_candidate_state.candidates.end(),
                [&](auto const& candidate) {
                    return candidate.function_symbol_name == module_candidate.function_symbol_name;
                }
            )
        );
        auto const replacement_target_acceptable =
            module_candidate.function_replacement_count == 1 &&
            (function_target_count == 1 ||
             (function_verification_state.all_verified &&
              function_verification_state.all_splice_ranges_available &&
              function_verification_state.same_function_splice_ranges_ordered &&
              function_verification_state.same_function_splice_ranges_non_overlapping));
        auto const candidate_function_ir =
            function_ir_slice(
                module_candidate.candidate_module_ir_text,
                module_candidate.function_symbol_name
            );
        auto verifier_diagnostics = diagnostics::DiagnosticBag {};
        if (module_candidate.candidate_available) {
            verifier_diagnostics =
                lowering::LlvmIrVerifier {}.verify(module_candidate.candidate_module_ir_text);
        }
        auto verification = RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateVerification {
            .function_symbol_name = module_candidate.function_symbol_name,
            .llvm_verifier_diagnostic_text =
                verifier_diagnostics.render("<runtime-indexed-cleanup-function-module-candidate>"),
            .verification_available = module_candidate.candidate_available,
            .candidate_function_found = !candidate_function_ir.empty(),
            .candidate_function_matches_verified_candidate =
                candidate_function_ir == function_candidate.candidate_function_ir_text,
            .replacement_target_unique = replacement_target_acceptable,
            .module_ir_changed = module_candidate.module_ir_changed,
            .separate_from_module_ir = module_candidate.separate_from_module_ir,
            .llvm_verifier_ran = module_candidate.candidate_available,
            .llvm_verifier_passed =
                module_candidate.candidate_available && !verifier_diagnostics.has_errors(),
            .source_line = module_candidate.source_line,
            .function_replacement_count = module_candidate.function_replacement_count,
            .llvm_verifier_diagnostic_count = verifier_diagnostics.entries().size(),
        };
        verification.verified =
            verification.verification_available &&
            verification.candidate_function_found &&
            verification.candidate_function_matches_verified_candidate &&
            verification.replacement_target_unique &&
            verification.module_ir_changed &&
            verification.separate_from_module_ir &&
            verification.llvm_verifier_passed;

        state.all_candidate_functions_found =
            state.all_candidate_functions_found && verification.candidate_function_found;
        state.all_candidate_functions_match_verified_candidates =
            state.all_candidate_functions_match_verified_candidates &&
            verification.candidate_function_matches_verified_candidate;
        state.all_replacement_targets_unique =
            state.all_replacement_targets_unique && verification.replacement_target_unique;
        state.all_module_ir_changed =
            state.all_module_ir_changed && verification.module_ir_changed;
        state.all_candidates_separate_from_module_ir =
            state.all_candidates_separate_from_module_ir && verification.separate_from_module_ir;
        state.any_llvm_verifier_ran =
            state.any_llvm_verifier_ran || verification.llvm_verifier_ran;
        state.all_llvm_verifier_passed =
            state.all_llvm_verifier_passed && verification.llvm_verifier_passed;
        state.all_verified = state.all_verified && verification.verified;
        if (verification.verified) {
            ++state.verified_count;
        }
        if (verification.llvm_verifier_passed) {
            ++state.llvm_verified_count;
        }
        state.llvm_verifier_diagnostic_count += verification.llvm_verifier_diagnostic_count;
        state.verifications.push_back(std::move(verification));
    }
    return state;
}

auto apply_runtime_indexed_cleanup_function_ir_module_rewrite_mutation(
    CompilePipelineOptions const& options,
    std::string const& base_ir_text,
    RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateState const& candidate_state,
    RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateVerificationState const& verification_state,
    RuntimeIndexedCleanupFunctionIrRewriteCandidateState const& function_candidate_state,
    std::string& ir_text
) -> RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState {
    auto state = RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState {
        .mutation_requested =
            (options.runtime_indexed_cleanup_module_ir_mutation_enabled &&
             options.runtime_indexed_cleanup_function_ir_module_rewrite_enabled) ||
            options.runtime_indexed_cleanup_verified_function_ir_rewrite_enabled,
        .candidate_verified = verification_state.all_verified,
        .replacement_targets_unique = verification_state.all_replacement_targets_unique,
        .candidate_count = candidate_state.candidate_count,
    };

    if (state.mutation_requested && state.candidate_verified && state.replacement_targets_unique &&
        !candidate_state.candidates.empty()) {
        auto ordered_candidates =
            std::vector<RuntimeIndexedCleanupFunctionIrModuleRewriteCandidate const*> {};
        ordered_candidates.reserve(candidate_state.candidates.size());
        for (auto const& candidate : candidate_state.candidates) {
            if (!candidate.candidate_available) {
                return state;
            }
            ordered_candidates.push_back(&candidate);
        }
        std::sort(
            ordered_candidates.begin(),
            ordered_candidates.end(),
            [](auto const* left, auto const* right) {
                return left->function_symbol_name < right->function_symbol_name;
            }
        );

        auto composed_ir = base_ir_text;
        for (auto index = std::size_t {0}; index < ordered_candidates.size();) {
            auto const function_symbol_name = ordered_candidates[index]->function_symbol_name;
            auto group_end = index + 1;
            while (group_end < ordered_candidates.size() &&
                   ordered_candidates[group_end]->function_symbol_name == function_symbol_name) {
                ++group_end;
            }
            if (group_end - index == 1) {
                auto const* candidate = ordered_candidates[index];
                auto const original_function = function_ir_slice(composed_ir, candidate->function_symbol_name);
                if (original_function.empty()) {
                    return state;
                }
                auto const candidate_function =
                    function_ir_slice(candidate->candidate_module_ir_text, candidate->function_symbol_name);
                if (candidate_function.empty()) {
                    return state;
                }
                composed_ir = replace_once(composed_ir, original_function, candidate_function);
                if (composed_ir.empty()) {
                    return state;
                }
                index = group_end;
                continue;
            }

            auto function_candidates =
                std::vector<RuntimeIndexedCleanupFunctionIrRewriteCandidate const*> {};
            function_candidates.reserve(group_end - index);
            for (auto candidate_index = index; candidate_index < group_end; ++candidate_index) {
                auto const module_candidate_position = static_cast<std::size_t>(
                    ordered_candidates[candidate_index] - candidate_state.candidates.data()
                );
                if (module_candidate_position >= function_candidate_state.candidates.size()) {
                    return state;
                }
                function_candidates.push_back(&function_candidate_state.candidates[module_candidate_position]);
            }
            auto const original_function = function_ir_slice(composed_ir, function_symbol_name);
            auto const operation_result =
                build_runtime_indexed_cleanup_function_ir_rewrite_operation_result(
                    original_function,
                    std::move(function_candidates)
                );
            if (!operation_result.succeeded()) {
                state.composition_failure = operation_result.failure;
                return state;
            }
            auto const stage_result =
                apply_runtime_indexed_cleanup_function_ir_rewrite_operation_stages(
                    original_function,
                    operation_result.operation
                );
            if (!state.rewrite_apply_stage_available) {
                state.rewrite_apply_stage_available = true;
                state.branch_replacements_applied = stage_result.branch_replacements_applied;
                state.cleanup_cfg_appended = stage_result.cleanup_cfg_appended;
                state.phi_predecessors_retargeted = stage_result.phi_predecessors_retargeted;
            } else {
                state.branch_replacements_applied =
                    state.branch_replacements_applied && stage_result.branch_replacements_applied;
                state.cleanup_cfg_appended =
                    state.cleanup_cfg_appended && stage_result.cleanup_cfg_appended;
                state.phi_predecessors_retargeted =
                    state.phi_predecessors_retargeted && stage_result.phi_predecessors_retargeted;
            }
            if (!stage_result.succeeded()) {
                state.composition_failure = stage_result.failure;
                state.composition_failure_part_available = stage_result.validation_part_available;
                state.composition_failure_part_index = stage_result.validation_part_index;
                state.composition_failure_splice_range =
                    runtime_indexed_cleanup_text_splice_range(stage_result.validation_splice_range);
                return state;
            }
            composed_ir = replace_once(composed_ir, original_function, stage_result.staged_function_ir);
            if (composed_ir.empty()) {
                return state;
            }
            index = group_end;
        }

        ir_text = std::move(composed_ir);
        state.mutation_applied = true;
        state.module_matches_candidate = true;
        auto verifier_diagnostics = lowering::LlvmIrVerifier {}.verify(ir_text);
        state.llvm_verifier_passed = !verifier_diagnostics.has_errors();
        state.llvm_verifier_diagnostic_count = verifier_diagnostics.entries().size();
    }

    state.final_module_line_count = logical_line_count(ir_text);
    return state;
}

auto label_from_member_cleanup_preview(
    std::vector<std::string> const& lines,
    std::string_view token
) -> std::string {
    for (auto const& line : lines) {
        if (line.find(token) == std::string::npos || line.empty() || line.back() != '\n') {
            continue;
        }
        auto label = line.substr(0, line.size() - 1);
        if (!label.empty() && label.back() == ':') {
            label.pop_back();
            return label;
        }
    }
    return {};
}

auto selected_runtime_indexed_member_cleanup_element_address(
    lowering::RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan const& plan,
    std::string const& function_ir
) -> std::string {
    auto const needle = " = getelementptr %record." + plan.element_source_type_name + ", ptr ";
    auto position = std::string::size_type {0};
    auto selected = std::string {};
    while ((position = function_ir.find(needle, position)) != std::string::npos) {
        auto const line_start = function_ir.rfind('\n', position);
        auto const value_start = line_start == std::string::npos ? std::string::size_type {0} : line_start + 1;
        auto const value_end = position;
        auto trimmed_value_start = value_start;
        while (trimmed_value_start < value_end && function_ir[trimmed_value_start] == ' ') {
            ++trimmed_value_start;
        }
        if (trimmed_value_start < value_end &&
            function_ir[trimmed_value_start] == '%' &&
            function_ir.substr(trimmed_value_start, value_end - trimmed_value_start).find(".element.addr") !=
                std::string::npos) {
            selected = function_ir.substr(trimmed_value_start, value_end - trimmed_value_start);
        }
        position += needle.size();
    }
    return selected;
}

auto selected_runtime_indexed_member_cleanup_index_operand(
    std::string const& selected_element_address,
    std::string const& function_ir
) -> std::string {
    if (selected_element_address.empty() || function_ir.empty()) {
        return {};
    }
    auto const assignment_marker = selected_element_address + " = getelementptr ";
    auto const marker_position = function_ir.find(assignment_marker);
    if (marker_position == std::string::npos) {
        return {};
    }
    auto const line_end = function_ir.find('\n', marker_position);
    auto const line = function_ir.substr(
        marker_position,
        line_end == std::string::npos ? std::string::npos : line_end - marker_position
    );
    auto constexpr index_marker = std::string_view {", i64 "};
    auto const index_position = line.rfind(index_marker);
    if (index_position == std::string::npos) {
        return {};
    }
    return line.substr(index_position + index_marker.size());
}

auto member_cleanup_descriptor_cleanup_plan(
    lowering::LlvmIrEmissionResult const& emission,
    lowering::RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan const& plan
) -> std::optional<lowering::DynamicArrayDescriptorCleanupPlan> {
    for (auto const& cleanup_plan : emission.dynamic_array_descriptor_cleanup_plans) {
        if (cleanup_plan.owner_name == plan.owner_name &&
            cleanup_plan.element_source_type_name == plan.element_source_type_name &&
            !cleanup_plan.descriptor_storage_name.empty()) {
            return cleanup_plan;
        }
    }
    for (auto const& construction_plan : emission.dynamic_array_construction_plans) {
        if (construction_plan.owner_name == plan.owner_name &&
            construction_plan.element_source_type_name == plan.element_source_type_name) {
            return lowering::DynamicArrayDescriptorCleanupPlan {
                .owner_name = construction_plan.owner_name,
                .source_type_name = construction_plan.source_type_name,
                .element_source_type_name = construction_plan.element_source_type_name,
                .element_llvm_type = construction_plan.element_llvm_type,
                .descriptor_storage_name =
                    lowering::llvm_local_value_name(construction_plan.owner_name + ".addr"),
                .descriptor_storage_status =
                    lowering::DynamicArrayDescriptorStorageStatus::lowered_local_descriptor,
                .element_size_bytes = construction_plan.element_size_bytes,
            };
        }
    }
    return std::nullopt;
}

auto member_cleanup_executable_cfg_append(
    lowering::RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan const& plan,
    std::string const& continuation_terminator_text,
    std::string const& selected_element_address,
    std::string const& selected_index_operand,
    std::optional<lowering::DynamicArrayDescriptorCleanupPlan> const& descriptor_cleanup_plan
) -> std::string {
    auto const skip_block_name =
        label_from_member_cleanup_preview(plan.appended_cfg_preview_lines, ".skip_moved");
    auto const sibling_drop_block_name = plan.sibling_drop_block_name;
    auto const preserve_block_name = plan.preserve_block_name;
    if (plan.entry_block_name.empty() ||
        skip_block_name.empty() ||
        sibling_drop_block_name.empty() ||
        preserve_block_name.empty() ||
        plan.exit_block_name.empty() ||
        plan.member_cleanup_target_symbol_name.empty() ||
        selected_element_address.empty() ||
        continuation_terminator_text.empty()) {
        return {};
    }
    if (descriptor_cleanup_plan.has_value() && selected_index_operand.empty()) {
        return {};
    }

    auto output = std::ostringstream {};
    output << plan.entry_block_name << ":\n"
           << "  ; runtime-index member cleanup rewrite entry\n"
           << "  br label %" << sibling_drop_block_name << "\n"
           << skip_block_name << ":\n"
           << "  ; preserve moved member\n"
           << "  br label %" << preserve_block_name << "\n";
    if (!descriptor_cleanup_plan.has_value()) {
        output << sibling_drop_block_name << ":\n"
               << "  call void @" << plan.member_cleanup_target_symbol_name
               << "(ptr " << selected_element_address << ")\n"
               << "  br label %" << preserve_block_name << "\n";
    } else {
        auto const& cleanup_plan = *descriptor_cleanup_plan;
        auto const prefix = "%" + plan.owner_name + ".member_cleanup";
        auto const label_prefix = plan.owner_name + ".member_cleanup";
        auto const descriptor_name = prefix + ".descriptor";
        output << sibling_drop_block_name << ":\n"
               << lowering::emit_dynamic_array_descriptor_load(
                   descriptor_name,
                   cleanup_plan.descriptor_storage_name
               )
               << lowering::emit_dynamic_array_descriptor_field_projection(
                   prefix + ".cleanup.data",
                   descriptor_name,
                   lowering::DynamicArrayDescriptorField::data
               )
               << lowering::emit_dynamic_array_descriptor_field_projection(
                   prefix + ".cleanup.length",
                   descriptor_name,
                   lowering::DynamicArrayDescriptorField::length
               )
               << lowering::emit_dynamic_array_descriptor_field_projection(
                   prefix + ".cleanup.capacity",
                   descriptor_name,
                   lowering::DynamicArrayDescriptorField::capacity
               )
               << "  br label %" << label_prefix << ".walk\n"
               << label_prefix << ".walk:\n"
               << "  " << prefix << ".index = phi i64 [ 0, %" << sibling_drop_block_name
               << " ], [ " << prefix << ".next, %" << label_prefix << ".continue ]\n"
               << "  " << prefix << ".more = icmp ult i64 " << prefix
               << ".index, " << prefix << ".cleanup.length\n"
               << "  br i1 " << prefix << ".more, label %" << label_prefix
               << ".live_check, label %" << label_prefix << ".deallocate\n"
               << label_prefix << ".live_check:\n"
               << "  " << prefix << ".is_moved = icmp eq i64 " << prefix
               << ".index, " << selected_index_operand << "\n"
               << "  br i1 " << prefix << ".is_moved, label %" << label_prefix
               << ".drop_siblings_for_moved, label %" << label_prefix << ".drop_element\n"
               << label_prefix << ".drop_siblings_for_moved:\n"
               << "  " << prefix << ".moved.addr = getelementptr "
               << cleanup_plan.element_llvm_type << ", ptr "
               << prefix << ".cleanup.data, i64 " << prefix << ".index\n"
               << "  call void @" << plan.member_cleanup_target_symbol_name
               << "(ptr " << prefix << ".moved.addr)\n"
               << "  br label %" << label_prefix << ".continue\n"
               << label_prefix << ".drop_element:\n"
               << "  " << prefix << ".element.addr = getelementptr "
               << cleanup_plan.element_llvm_type << ", ptr "
               << prefix << ".cleanup.data, i64 " << prefix << ".index\n"
               << "  call void @" << semantics::drop_abi_symbol_name(plan.element_source_type_name)
               << "(ptr " << prefix << ".element.addr)\n"
               << "  store " << cleanup_plan.element_llvm_type
               << " zeroinitializer, ptr " << prefix << ".element.addr\n"
               << "  br label %" << label_prefix << ".continue\n"
               << label_prefix << ".continue:\n"
               << "  " << prefix << ".next = add i64 " << prefix << ".index, 1\n"
               << "  br label %" << label_prefix << ".walk\n"
               << label_prefix << ".deallocate:\n"
               << "  call void @__orison_dynamic_array_deallocate(ptr "
               << prefix << ".cleanup.data, i64 " << cleanup_plan.element_size_bytes
               << ", i64 " << prefix << ".cleanup.capacity)\n"
               << lowering::emit_dynamic_array_descriptor_finalization(
                   cleanup_plan.descriptor_storage_name
               )
               << "  br label %" << preserve_block_name << "\n";
    }
    output << preserve_block_name << ":\n"
           << "  br label %" << plan.exit_block_name << "\n"
           << plan.exit_block_name << ":\n"
           << continuation_terminator_text;
    return output.str();
}

auto member_cleanup_sibling_fields(
    lowering::LlvmIrEmissionResult const& emission,
    lowering::RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan const& plan
) -> std::vector<lowering::RuntimeIndexedMemberCleanupSiblingField> {
    auto fields = std::vector<lowering::RuntimeIndexedMemberCleanupSiblingField> {};
    for (auto const& field : emission.runtime_indexed_member_cleanup_sibling_fields) {
        if (field.owner_name == plan.owner_name &&
            field.index_expression_text == plan.index_expression_text &&
            field.element_source_type_name == plan.element_source_type_name &&
            field.moved_source_type_name == plan.moved_source_type_name &&
            field.moved_member_path == plan.moved_member_path &&
            field.drop_definition_available) {
            fields.push_back(field);
        }
    }
    return fields;
}

auto member_cleanup_helper_definition(
    std::vector<lowering::RuntimeIndexedMemberCleanupSiblingField> const& sibling_fields,
    lowering::RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan const& plan
) -> std::string {
    if (plan.member_cleanup_target_symbol_name.empty() ||
        plan.element_source_type_name.empty() ||
        plan.moved_member_path.empty()) {
        return {};
    }
    auto output = std::ostringstream {};
    output << "define void @" << plan.member_cleanup_target_symbol_name << "(ptr %value) {\n"
           << "entry:\n";
    if (!sibling_fields.empty()) {
        auto emitted_address_names = std::unordered_set<std::string> {};
        for (auto const& field : sibling_fields) {
            if (field.field_path.empty() ||
                field.field_indices.size() != field.field_path.size() ||
                field.container_llvm_type_names.size() != field.field_path.size()) {
                return {};
            }
            auto source_pointer_name = std::string {"%value"};
            auto field_address_name = std::string {};
            auto path_prefix = std::string {};
            for (auto index = std::size_t {0}; index < field.field_path.size(); ++index) {
                if (!path_prefix.empty()) {
                    path_prefix += ".";
                }
                path_prefix += field.field_path[index];
                field_address_name = "%" + plan.element_source_type_name +
                    ".member_cleanup." + path_prefix + ".addr";
                if (emitted_address_names.insert(field_address_name).second) {
                    output << "  " << field_address_name << " = getelementptr "
                           << field.container_llvm_type_names[index] << ", ptr "
                           << source_pointer_name << ", i32 0, i32 "
                           << field.field_indices[index] << "\n";
                }
                source_pointer_name = field_address_name;
            }
            output << "  call void @" << field.drop_symbol_name
                   << "(ptr " << field_address_name << ")\n"
                   << "  store " << field.field_llvm_type_name << " zeroinitializer, ptr "
                   << field_address_name << "\n";
        }
    } else {
        output << "  ; no sibling cleanup targets for %record." << plan.element_source_type_name
               << " except " << plan.moved_member_path.front() << "\n";
    }
    output << "  ret void\n"
           << "}\n";
    return output.str();
}

auto ensure_member_cleanup_helper_definition(
    std::string& ir_text,
    lowering::LlvmIrEmissionResult const& emission,
    lowering::RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan const& plan
) -> bool {
    if (plan.member_cleanup_target_symbol_name.empty()) {
        return false;
    }
    if (ir_text.find("define void @" + plan.member_cleanup_target_symbol_name + "(") != std::string::npos) {
        return true;
    }
    auto const sibling_fields = member_cleanup_sibling_fields(emission, plan);
    auto const helper_definition = member_cleanup_helper_definition(sibling_fields, plan);
    if (helper_definition.empty()) {
        return false;
    }
    auto const first_function = ir_text.find("define ");
    if (first_function == std::string::npos) {
        return false;
    }
    ir_text.insert(first_function, helper_definition + "\n");
    return true;
}

auto final_return_terminator_splice(
    std::string const& function_ir
) -> RuntimeIndexedCleanupTextSpliceRange {
    auto const terminator_start = function_ir.rfind("\n  ret ");
    if (terminator_start == std::string::npos) {
        return {};
    }
    auto const line_start = terminator_start + 1;
    auto const line_end = function_ir.find('\n', line_start);
    if (line_end == std::string::npos) {
        return {};
    }
    return RuntimeIndexedCleanupTextSpliceRange {
        .start_offset = line_start,
        .end_offset = line_end + 1,
    };
}

auto apply_runtime_indexed_member_cleanup_function_ir_module_rewrite_mutation(
    lowering::LlvmIrEmissionResult const& emission,
    std::string& ir_text
) -> RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState {
    auto state = RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState {
        .mutation_requested =
            std::ranges::any_of(
                emission.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans,
                [](auto const& plan) {
                    return plan.execution_requested;
                }
            ),
        .candidate_count =
            emission.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans.size(),
    };
    if (!state.mutation_requested ||
        emission.runtime_indexed_member_cleanup_function_rewrite_edit_script_plans.empty() ||
        emission.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans.empty() ||
        emission.runtime_indexed_member_cleanup_function_rewrite_edit_script_plans.size() !=
            emission.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans.size()) {
        state.final_module_line_count = logical_line_count(ir_text);
        return state;
    }

    auto composed_ir = ir_text;
    state.candidate_verified = true;
    state.replacement_targets_unique = true;
    for (auto index = std::size_t {0};
         index < emission.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans.size();
         ++index) {
        auto const& execution_plan =
            emission.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans[index];
        auto const& edit_plan =
            emission.runtime_indexed_member_cleanup_function_rewrite_edit_script_plans[index];
        auto const candidate_ready =
            execution_plan.execution_enabled &&
            edit_plan.edit_script_ready &&
            !execution_plan.function_symbol_name.empty() &&
            execution_plan.function_symbol_name == edit_plan.function_symbol_name;
        state.candidate_verified = state.candidate_verified && candidate_ready;
        if (!candidate_ready) {
            state.final_module_line_count = logical_line_count(ir_text);
            return state;
        }

        auto const original_function_ir = function_ir_slice(composed_ir, execution_plan.function_symbol_name);
        auto expected_branch_text = "  " + edit_plan.expected_branch_text + "\n";
        auto const replacement_branch_text = "  " + edit_plan.replacement_branch_text + "\n";
        auto terminator_position = original_function_ir.find(expected_branch_text);
        if (occurrence_count(original_function_ir, expected_branch_text) != 1) {
            auto const final_return_range = final_return_terminator_splice(original_function_ir);
            if (final_return_range.start_offset < final_return_range.end_offset &&
                final_return_range.end_offset <= original_function_ir.size()) {
                terminator_position = final_return_range.start_offset;
                expected_branch_text = original_function_ir.substr(
                    final_return_range.start_offset,
                    final_return_range.end_offset - final_return_range.start_offset
                );
            }
        }
        auto const target_unique =
            !original_function_ir.empty() &&
            occurrence_count(composed_ir, original_function_ir) == 1 &&
            occurrence_count(original_function_ir, expected_branch_text) == 1 &&
            terminator_position != std::string::npos;
        state.replacement_targets_unique = state.replacement_targets_unique && target_unique;
        if (!target_unique) {
            state.final_module_line_count = logical_line_count(ir_text);
            return state;
        }

        auto const selected_element_address =
            selected_runtime_indexed_member_cleanup_element_address(edit_plan, original_function_ir);
        auto const selected_index_operand =
            selected_runtime_indexed_member_cleanup_index_operand(selected_element_address, original_function_ir);
        auto const descriptor_cleanup_plan =
            member_cleanup_descriptor_cleanup_plan(emission, edit_plan);
        auto const append_text = member_cleanup_executable_cfg_append(
            edit_plan,
            expected_branch_text,
            selected_element_address,
            selected_index_operand,
            descriptor_cleanup_plan
        );
        if (append_text.empty()) {
            state.candidate_verified = false;
            state.final_module_line_count = logical_line_count(ir_text);
            return state;
        }
        auto const stage_result = apply_runtime_indexed_cleanup_function_ir_edit_script_stages(
            original_function_ir,
            RuntimeIndexedCleanupFunctionIrEditScript {
                .branch_replacements = {
                    RuntimeIndexedCleanupFunctionIrBranchReplacement {
                        .predecessor_block_name = edit_plan.insertion_anchor,
                        .continuation_block_name = edit_plan.exit_block_name,
                        .expected_branch_text = expected_branch_text,
                        .replacement_branch_text = replacement_branch_text,
                        .splice_range = RuntimeIndexedCleanupTextSpliceRange {
                            .start_offset = terminator_position,
                            .end_offset = terminator_position + expected_branch_text.size(),
                        },
                    },
                },
                .cleanup_cfg_append = RuntimeIndexedCleanupFunctionIrCleanupCfgAppend {
                    .placement =
                        RuntimeIndexedCleanupFunctionIrCleanupCfgAppendPlacement::before_function_closing_brace,
                    .expected_closing_text = "\n}\n",
                    .append_text = append_text,
                },
                .phi_predecessor_retargets = {
                    RuntimeIndexedCleanupFunctionIrPhiPredecessorRetarget {
                        .old_predecessor_block_name = edit_plan.phi_old_predecessor_block_name,
                        .new_predecessor_block_name = edit_plan.phi_new_predecessor_block_name,
                    },
                },
            }
        );
        if (!state.rewrite_apply_stage_available) {
            state.rewrite_apply_stage_available = true;
            state.branch_replacements_applied = stage_result.branch_replacements_applied;
            state.cleanup_cfg_appended = stage_result.cleanup_cfg_appended;
            state.phi_predecessors_retargeted = stage_result.phi_predecessors_retargeted;
        } else {
            state.branch_replacements_applied =
                state.branch_replacements_applied && stage_result.branch_replacements_applied;
            state.cleanup_cfg_appended =
                state.cleanup_cfg_appended && stage_result.cleanup_cfg_appended;
            state.phi_predecessors_retargeted =
                state.phi_predecessors_retargeted && stage_result.phi_predecessors_retargeted;
        }
        if (!stage_result.succeeded()) {
            state.composition_failure = stage_result.failure;
            state.composition_failure_part_available = stage_result.validation_part_available;
            state.composition_failure_part_index = stage_result.validation_part_index;
            state.composition_failure_splice_range =
                runtime_indexed_cleanup_text_splice_range(stage_result.validation_splice_range);
            state.final_module_line_count = logical_line_count(ir_text);
            return state;
        }
        composed_ir = replace_once(composed_ir, original_function_ir, stage_result.staged_function_ir);
        if (composed_ir.empty()) {
            state.final_module_line_count = logical_line_count(ir_text);
            return state;
        }
        if (!ensure_member_cleanup_helper_definition(composed_ir, emission, edit_plan)) {
            state.candidate_verified = false;
            state.final_module_line_count = logical_line_count(ir_text);
            return state;
        }
    }

    ir_text = std::move(composed_ir);
    state.mutation_applied = true;
    state.module_matches_candidate = true;
    auto verifier_diagnostics = lowering::LlvmIrVerifier {}.verify(ir_text);
    state.llvm_verifier_passed = !verifier_diagnostics.has_errors();
    state.llvm_verifier_diagnostic_count = verifier_diagnostics.entries().size();
    state.final_module_line_count = logical_line_count(ir_text);
    return state;
}

}  // namespace

auto format_runtime_indexed_cleanup_production_readiness_diagnostic(
    RuntimeIndexedCleanupModuleIrProductionReadinessState const& state
) -> std::string {
    if (state.diagnostic_blocker_kind ==
        RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::None) {
        return {};
    }
    auto diagnostic = std::ostringstream {};
    diagnostic << "runtime-index cleanup blocked: ";
    switch (state.diagnostic_blocker_kind) {
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::None:
        return {};
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::InsertionGate:
        diagnostic << "module insertion gate disabled";
        break;
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::InsertionPreview:
        diagnostic << "module insertion preview unavailable";
        break;
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::Candidate:
        diagnostic << "module ir candidate unavailable";
        break;
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::CandidateVerification:
        diagnostic << "module ir candidate verification blocked";
        break;
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::ModuleMutation:
        diagnostic << "module mutation disabled";
        break;
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::FunctionIntegration:
        diagnostic << "function integration blocked";
        break;
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::FunctionSpliceConflict:
        diagnostic << "overlapping same-function splice ranges";
        if (state.diagnostic_left_source_line != 0 || state.diagnostic_right_source_line != 0) {
            diagnostic << " left-line " << state.diagnostic_left_source_line
                       << " right-line " << state.diagnostic_right_source_line;
        }
        break;
    }
    if (state.diagnostic_composition_failure != RuntimeIndexedCleanupIrCompositionFailure::none) {
        diagnostic << " composition-failure "
                   << runtime_indexed_cleanup_ir_composition_failure_token(
                          state.diagnostic_composition_failure
                      );
        if (state.diagnostic_composition_failure_part_available) {
            diagnostic << " composition-part " << state.diagnostic_composition_failure_part_index
                       << " splice-range " << state.diagnostic_composition_failure_splice_range.start_offset
                       << ".." << state.diagnostic_composition_failure_splice_range.end_offset;
        }
    }
    if (state.diagnostic_rewrite_apply_stage_available) {
        diagnostic << " apply-stages available"
                   << " branch-replacements "
                   << (state.diagnostic_branch_replacements_applied ? "true" : "false")
                   << " cleanup-cfg-appended "
                   << (state.diagnostic_cleanup_cfg_appended ? "true" : "false")
                   << " phi-retargeted "
                   << (state.diagnostic_phi_predecessors_retargeted ? "true" : "false");
    }
    return diagnostic.str();
}

auto runtime_indexed_cleanup_production_readiness_blocker_kind_name(
    RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind kind
) -> std::string_view {
    switch (kind) {
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::None:
        return "none";
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::InsertionGate:
        return "insertion-gate";
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::InsertionPreview:
        return "insertion-preview";
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::Candidate:
        return "candidate";
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::CandidateVerification:
        return "candidate-verification";
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::ModuleMutation:
        return "module-mutation";
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::FunctionIntegration:
        return "function-integration";
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::FunctionSpliceConflict:
        return "function-splice-conflict";
    }
    return "unknown";
}

auto runtime_indexed_cleanup_production_readiness_blocker_stage_name(
    RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind kind
) -> std::string_view {
    switch (kind) {
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::None:
        return {};
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::InsertionGate:
        return "module insertion gate";
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::InsertionPreview:
        return "module insertion preview";
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::Candidate:
        return "module ir candidate";
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::CandidateVerification:
        return "module ir candidate verification";
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::ModuleMutation:
        return "module mutation";
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::FunctionIntegration:
        return "function integration";
    case RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::FunctionSpliceConflict:
        return "function splice conflict";
    }
    return {};
}

auto format_runtime_indexed_cleanup_production_readiness_report(
    RuntimeIndexedCleanupModuleIrProductionReadinessState const& state
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index cleanup module-ir production-readiness "
           << "insertion-gate " << (state.insertion_gate_ready ? "ready" : "blocked")
           << " insertion-preview " << (state.insertion_preview_ready ? "ready" : "blocked")
           << " candidate " << (state.candidate_ready ? "ready" : "blocked")
           << " candidate-verification " << (state.candidate_verified ? "verified" : "blocked")
           << " module-mutation " << (state.module_mutation_enabled ? "enabled" : "disabled")
           << " function-integration " << (state.function_integration_ready ? "ready" : "blocked")
           << " splice-conflicts " << state.function_splice_conflict_count
           << " splice-conflict-check " << (state.function_splice_conflict_free ? "clear" : "blocked")
           << " production " << (state.production_ready ? "ready" : "blocked")
           << " blocker-count " << state.blockers.size()
           << " blocker-kind "
           << runtime_indexed_cleanup_production_readiness_blocker_kind_name(state.diagnostic_blocker_kind);
    if (!state.diagnostic_function_symbol_name.empty()) {
        report << " function " << state.diagnostic_function_symbol_name;
    }
    if (state.diagnostic_source_available) {
        report << " source-line " << state.diagnostic_source_line;
    }
    auto diagnostic = format_runtime_indexed_cleanup_production_readiness_diagnostic(state);
    if (!diagnostic.empty()) {
        report << " diagnostic " << diagnostic;
    }
    return report.str();
}

auto format_runtime_indexed_cleanup_production_readiness_blocker_report(
    RuntimeIndexedCleanupModuleIrProductionReadinessState const& state
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    lines.reserve(state.blockers.size());
    for (auto index = std::size_t {0}; index < state.blockers.size(); ++index) {
        auto const& blocker = state.blockers[index];
        auto line = std::ostringstream {};
        line << "runtime-index cleanup module-ir production-readiness blocker "
             << "index " << index
             << " kind " << runtime_indexed_cleanup_production_readiness_blocker_kind_name(blocker.kind)
             << " stage " << blocker.stage_name;
        if (!blocker.function_symbol_name.empty()) {
            line << " function " << blocker.function_symbol_name;
        }
        if (blocker.source_available) {
            line << " source-line " << blocker.source_line;
        }
        if (blocker.composition_failure != RuntimeIndexedCleanupIrCompositionFailure::none) {
            line << " composition-failure "
                 << runtime_indexed_cleanup_ir_composition_failure_token(blocker.composition_failure);
            if (blocker.composition_failure_part_available) {
                line << " composition-part " << blocker.composition_failure_part_index
                     << " splice-range " << blocker.composition_failure_splice_range.start_offset
                     << ".." << blocker.composition_failure_splice_range.end_offset;
            }
        }
        if (blocker.rewrite_apply_stage_available) {
            line << " apply-stages available"
                 << " branch-replacements " << (blocker.branch_replacements_applied ? "true" : "false")
                 << " cleanup-cfg-appended " << (blocker.cleanup_cfg_appended ? "true" : "false")
                 << " phi-retargeted " << (blocker.phi_predecessors_retargeted ? "true" : "false");
        }
        lines.push_back(line.str());
    }
    return lines;
}

namespace {

auto runtime_indexed_cleanup_readiness_blocker(
    RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind blocker_kind,
    RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateVerificationState const& function_verification_state,
    RuntimeIndexedCleanupIrCompositionFailure composition_failure =
        RuntimeIndexedCleanupIrCompositionFailure::none,
    bool composition_failure_part_available = false,
    std::size_t composition_failure_part_index = 0,
    RuntimeIndexedCleanupTextSpliceRange composition_failure_splice_range =
        RuntimeIndexedCleanupTextSpliceRange {},
    bool rewrite_apply_stage_available = false,
    bool branch_replacements_applied = false,
    bool cleanup_cfg_appended = false,
    bool phi_predecessors_retargeted = false
) -> RuntimeIndexedCleanupModuleIrProductionReadinessBlocker {
    auto blocker = RuntimeIndexedCleanupModuleIrProductionReadinessBlocker {
        .kind = blocker_kind,
        .stage_name =
            std::string(runtime_indexed_cleanup_production_readiness_blocker_stage_name(blocker_kind)),
        .composition_failure = composition_failure,
        .composition_failure_part_available = composition_failure_part_available,
        .composition_failure_part_index = composition_failure_part_index,
        .composition_failure_splice_range = composition_failure_splice_range,
        .rewrite_apply_stage_available = rewrite_apply_stage_available,
        .branch_replacements_applied = branch_replacements_applied,
        .cleanup_cfg_appended = cleanup_cfg_appended,
        .phi_predecessors_retargeted = phi_predecessors_retargeted,
    };
    if (!function_verification_state.verifications.empty()) {
        auto const& verification = function_verification_state.verifications.front();
        blocker.function_symbol_name = verification.function_symbol_name;
        blocker.source_available = verification.source_line != 0;
        blocker.source_line = verification.source_line;
    }
    return blocker;
}

auto append_runtime_indexed_cleanup_readiness_blocker(
    RuntimeIndexedCleanupModuleIrProductionReadinessState& readiness_state,
    RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind blocker_kind,
    RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateVerificationState const& function_verification_state,
    RuntimeIndexedCleanupIrCompositionFailure composition_failure =
        RuntimeIndexedCleanupIrCompositionFailure::none,
    bool composition_failure_part_available = false,
    std::size_t composition_failure_part_index = 0,
    RuntimeIndexedCleanupTextSpliceRange composition_failure_splice_range =
        RuntimeIndexedCleanupTextSpliceRange {},
    bool rewrite_apply_stage_available = false,
    bool branch_replacements_applied = false,
    bool cleanup_cfg_appended = false,
    bool phi_predecessors_retargeted = false
) -> void {
    readiness_state.blockers.push_back(
        runtime_indexed_cleanup_readiness_blocker(
            blocker_kind,
            function_verification_state,
            composition_failure,
            composition_failure_part_available,
            composition_failure_part_index,
            composition_failure_splice_range,
            rewrite_apply_stage_available,
            branch_replacements_applied,
            cleanup_cfg_appended,
            phi_predecessors_retargeted
        )
    );
}

auto apply_runtime_indexed_cleanup_first_readiness_blocker(
    RuntimeIndexedCleanupModuleIrProductionReadinessState& readiness_state
) -> void {
    if (readiness_state.blockers.empty()) {
        return;
    }
    auto const& blocker = readiness_state.blockers.front();
    readiness_state.diagnostic_blocker_kind = blocker.kind;
    readiness_state.diagnostic_blocker_stage_name = blocker.stage_name;
    if (readiness_state.diagnostic_function_symbol_name.empty()) {
        readiness_state.diagnostic_function_symbol_name = blocker.function_symbol_name;
    }
    readiness_state.diagnostic_composition_failure = blocker.composition_failure;
    readiness_state.diagnostic_composition_failure_part_available =
        blocker.composition_failure_part_available;
    readiness_state.diagnostic_composition_failure_part_index = blocker.composition_failure_part_index;
    readiness_state.diagnostic_composition_failure_splice_range =
        blocker.composition_failure_splice_range;
    readiness_state.diagnostic_rewrite_apply_stage_available = blocker.rewrite_apply_stage_available;
    readiness_state.diagnostic_branch_replacements_applied = blocker.branch_replacements_applied;
    readiness_state.diagnostic_cleanup_cfg_appended = blocker.cleanup_cfg_appended;
    readiness_state.diagnostic_phi_predecessors_retargeted = blocker.phi_predecessors_retargeted;
    readiness_state.diagnostic_source_available = blocker.source_available;
    readiness_state.diagnostic_source_line = blocker.source_line;
}

auto build_runtime_indexed_cleanup_module_ir_production_readiness_state(
    RuntimeIndexedCleanupModuleIrInsertionGateState const& insertion_gate_state,
    RuntimeIndexedCleanupModuleIrInsertionPreviewState const& preview_state,
    RuntimeIndexedCleanupModuleIrCandidateState const& candidate_state,
    RuntimeIndexedCleanupModuleIrCandidateVerificationState const& verification_state,
    RuntimeIndexedCleanupModuleIrMutationState const& mutation_state,
    RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateVerificationState const& function_verification_state,
    RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState const& function_mutation_state
) -> RuntimeIndexedCleanupModuleIrProductionReadinessState {
    auto const function_splice_conflict_free =
        function_verification_state.splice_conflict_count == 0;
    auto const function_composition_failure =
        function_mutation_state.composition_failure != RuntimeIndexedCleanupIrCompositionFailure::none
            ? function_mutation_state.composition_failure
            : function_verification_state.first_composition_failure;
    auto const function_mutation_ready =
        function_mutation_state.mutation_requested &&
        function_mutation_state.candidate_verified &&
        function_mutation_state.mutation_applied &&
        function_mutation_state.module_matches_candidate &&
        function_mutation_state.llvm_verifier_passed &&
        function_splice_conflict_free;
    auto const function_only_integration_ready =
        function_mutation_ready &&
        !insertion_gate_state.insertion_enabled &&
        !mutation_state.mutation_applied;
    auto readiness_state = RuntimeIndexedCleanupModuleIrProductionReadinessState {
        .insertion_gate_ready =
            insertion_gate_state.insertion_enabled ||
            function_only_integration_ready,
        .insertion_preview_ready =
            (preview_state.preview_available && preview_state.insertion_point_found) ||
            function_only_integration_ready,
        .candidate_ready = candidate_state.candidate_available || function_mutation_state.mutation_requested,
        .candidate_verified =
            verification_state.verified ||
            (function_mutation_state.candidate_verified && function_splice_conflict_free),
        .module_mutation_enabled =
            (mutation_state.mutation_applied && mutation_state.module_matches_candidate) ||
            function_mutation_ready,
        .function_integration_ready = function_mutation_ready,
        .function_splice_conflict_free = function_splice_conflict_free,
        .function_splice_conflict_count = function_verification_state.splice_conflict_count,
    };
    readiness_state.production_ready =
        readiness_state.insertion_gate_ready &&
        readiness_state.insertion_preview_ready &&
        readiness_state.candidate_ready &&
        readiness_state.candidate_verified &&
        readiness_state.module_mutation_enabled &&
        readiness_state.function_integration_ready &&
        readiness_state.function_splice_conflict_free;
    if (!readiness_state.function_splice_conflict_free) {
        if (!function_verification_state.splice_conflicts.empty()) {
            auto const& conflict = function_verification_state.splice_conflicts.front();
            readiness_state.diagnostic_function_symbol_name = conflict.function_symbol_name;
            readiness_state.diagnostic_left_candidate_index = conflict.left_candidate_index;
            readiness_state.diagnostic_right_candidate_index = conflict.right_candidate_index;
            readiness_state.diagnostic_left_source_line = conflict.left_source_line;
            readiness_state.diagnostic_right_source_line = conflict.right_source_line;
        }
        append_runtime_indexed_cleanup_readiness_blocker(
            readiness_state,
            RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::FunctionSpliceConflict,
            function_verification_state
        );
    }
    if (!readiness_state.insertion_gate_ready) {
        append_runtime_indexed_cleanup_readiness_blocker(
            readiness_state,
            RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::InsertionGate,
            function_verification_state
        );
    }
    if (!readiness_state.insertion_preview_ready) {
        append_runtime_indexed_cleanup_readiness_blocker(
            readiness_state,
            RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::InsertionPreview,
            function_verification_state
        );
    }
    if (!readiness_state.candidate_ready) {
        append_runtime_indexed_cleanup_readiness_blocker(
            readiness_state,
            RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::Candidate,
            function_verification_state
        );
    }
    if (!readiness_state.candidate_verified) {
        append_runtime_indexed_cleanup_readiness_blocker(
            readiness_state,
            RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::CandidateVerification,
            function_verification_state,
            function_verification_state.first_composition_failure
        );
    }
    if (!readiness_state.module_mutation_enabled) {
        append_runtime_indexed_cleanup_readiness_blocker(
            readiness_state,
            RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::ModuleMutation,
            function_verification_state,
            function_composition_failure,
            function_mutation_state.composition_failure_part_available,
            function_mutation_state.composition_failure_part_index,
            function_mutation_state.composition_failure_splice_range,
            function_mutation_state.rewrite_apply_stage_available,
            function_mutation_state.branch_replacements_applied,
            function_mutation_state.cleanup_cfg_appended,
            function_mutation_state.phi_predecessors_retargeted
        );
    }
    if (!readiness_state.function_integration_ready) {
        append_runtime_indexed_cleanup_readiness_blocker(
            readiness_state,
            RuntimeIndexedCleanupModuleIrProductionReadinessBlockerKind::FunctionIntegration,
            function_verification_state,
            function_composition_failure,
            function_mutation_state.composition_failure_part_available,
            function_mutation_state.composition_failure_part_index,
            function_mutation_state.composition_failure_splice_range,
            function_mutation_state.rewrite_apply_stage_available,
            function_mutation_state.branch_replacements_applied,
            function_mutation_state.cleanup_cfg_appended,
            function_mutation_state.phi_predecessors_retargeted
        );
    }
    apply_runtime_indexed_cleanup_first_readiness_blocker(readiness_state);
    readiness_state.diagnostic_text =
        format_runtime_indexed_cleanup_production_readiness_diagnostic(readiness_state);
    return readiness_state;
}

auto build_computed_dynamic_array_for_descriptor_render_state(
    lowering::LlvmIrEmissionResult const& emission
) -> ComputedDynamicArrayForDescriptorRenderState {
    auto const& renders = emission.computed_dynamic_array_for_descriptor_renders;
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
    auto const& renders = emission.computed_dynamic_array_for_loop_control_renders;
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
    auto const& renders = emission.computed_dynamic_array_for_element_address_renders;
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
    auto const& renders = emission.computed_dynamic_array_for_element_load_renders;
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
    auto const& renders = emission.computed_dynamic_array_for_loop_continue_renders;
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
    auto const& sequences = emission.computed_dynamic_array_for_loop_render_sequences;
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
    auto const& cleanups = emission.computed_dynamic_array_for_loop_exit_cleanups;
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
    auto const& transitions = emission.computed_dynamic_array_for_cleanup_transitions;
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
        .gate_count = emission.computed_dynamic_array_for_production_emission_gates.size(),
    };
    state.gate_metadata_available = state.gate_count > 0;
    state.all_ownership_ready = state.gate_metadata_available;
    state.all_loop_render_ready = state.gate_metadata_available;
    state.all_loop_cleanup_ownership_ready = state.gate_metadata_available;
    state.all_function_cleanup_resumption_ready = state.gate_metadata_available;
    state.all_exit_cleanup_ready = state.gate_metadata_available;
    state.all_production_sequences_planned = state.gate_metadata_available;
    state.cleanup_owner_names.reserve(emission.computed_dynamic_array_for_production_emission_gates.size());
    for (auto const& gate : emission.computed_dynamic_array_for_production_emission_gates) {
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
            emission.computed_dynamic_array_for_consumed_cleanup_descriptors.size(),
        .emitted_finalization_plan_count = emission.consumed_descriptor_finalization_plans.size(),
    };
    for (auto const& descriptor : emission.computed_dynamic_array_for_consumed_cleanup_descriptors) {
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
    auto const& descriptors = emission.computed_dynamic_array_for_consumed_cleanup_descriptors;
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
    state.cleanup_calls_blocked_reasons.reserve(proof_model.cleanup_call_report_events.insertion_gate_events.size());
    for (auto const& event : proof_model.cleanup_call_report_events.insertion_gate_events) {
        state.cleanup_owner_names.push_back(event.resumption.target_owner_name);
        state.cleanup_operation_names.push_back(event.resumption.operation_name + ".call");
        state.all_state_verified = state.all_state_verified && event.decision.state_verified;
        state.all_operands_proven = state.all_operands_proven && event.decision.operands_proven;
        state.all_cleanup_calls_authorized =
            state.all_cleanup_calls_authorized && event.decision.cleanup_calls_authorized;
        if (event.decision.insertion_ready) {
            ++state.ready_count;
            state.cleanup_calls_blocked_reasons.push_back({});
        } else {
            ++state.blocked_count;
            if (!event.resumption.cleanup_calls_blocked_reason.empty()) {
                ++state.cleanup_call_blocker_count;
            }
            state.cleanup_calls_blocked_reasons.push_back(event.resumption.cleanup_calls_blocked_reason);
        }
    }
    state.all_ready = state.ready_count > 0 && state.blocked_count == 0;
    return state;
}

auto build_computed_cleanup_call_insertion_capability_state(
    CompilePipelineOptions const& options
) -> ComputedCleanupCallInsertionCapabilityState {
    auto lowering_options = lowering::LlvmIrEmissionOptions {};
    lowering_options.fixture_authorize_computed_dynamic_array_cleanup_calls =
        options.fixture_authorize_computed_dynamic_array_cleanup_calls;
    lowering_options.fixture_insert_computed_dynamic_array_cleanup_calls =
        options.fixture_insert_computed_dynamic_array_cleanup_calls;
    lowering_options.enable_computed_dynamic_array_local_cleanup_call_insertion =
        options.computed_dynamic_array_local_cleanup_call_insertion_enabled;
    auto const capability =
        lowering::computed_dynamic_array_cleanup_call_insertion_capability(lowering_options);
    return ComputedCleanupCallInsertionCapabilityState {
        .cleanup_call_authorization_enabled = capability.cleanup_call_authorization_enabled,
        .cleanup_call_insertion_enabled = capability.cleanup_call_insertion_enabled,
        .enabled = capability.enabled,
    };
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
    state.cleanup_calls_blocked_reasons.reserve(proof_model.inserted_cleanup_state.transition_events.size());
    for (auto const& event : proof_model.inserted_cleanup_state.transition_events) {
        state.cleanup_owner_names.push_back(event.resumption.target_owner_name);
        state.acquire_operation_names.push_back(event.acquisition.operation_name);
        state.resume_operation_names.push_back(event.resumption.operation_name);
        if (event.acquisition.cleanup_call_authorization_origin ==
                lowering::ComputedDynamicArrayCleanupCallAuthorizationOrigin::production_local_cleanup_plan &&
            event.resumption.cleanup_call_authorization_origin ==
                lowering::ComputedDynamicArrayCleanupCallAuthorizationOrigin::production_local_cleanup_plan) {
            ++state.production_cleanup_call_authorization_count;
        }
        if (event.acquisition.cleanup_call_authorization_origin ==
                lowering::ComputedDynamicArrayCleanupCallAuthorizationOrigin::explicit_test_seam &&
            event.resumption.cleanup_call_authorization_origin ==
                lowering::ComputedDynamicArrayCleanupCallAuthorizationOrigin::explicit_test_seam) {
            ++state.explicit_test_seam_cleanup_call_authorization_count;
        }
        auto const cleanup_calls_enabled =
            event.acquisition.cleanup_calls_enabled && event.resumption.cleanup_calls_enabled;
        state.all_cleanup_calls_enabled = state.all_cleanup_calls_enabled && cleanup_calls_enabled;
        if (!cleanup_calls_enabled) {
            ++state.cleanup_call_blocker_count;
            state.cleanup_calls_blocked_reasons.push_back(
                event.acquisition.cleanup_calls_blocked_reason.empty() ?
                    event.resumption.cleanup_calls_blocked_reason :
                    event.acquisition.cleanup_calls_blocked_reason
            );
        } else {
            state.cleanup_calls_blocked_reasons.push_back({});
        }
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

auto build_computed_inserted_cleanup_transition_state(
    ComputedCleanupProofModel const& proof_model
) -> ComputedInsertedCleanupTransitionState {
    auto const& transitions = proof_model.inserted_cleanup_state.transition_events;
    auto state = ComputedInsertedCleanupTransitionState {
        .from_metadata = proof_model.inserted_cleanup_state.from_metadata,
        .transition_count = transitions.size(),
    };
    state.transitions_available = state.transition_count > 0;
    state.all_cleanup_calls_enabled = state.transitions_available;
    state.acquire_source_owner_names.reserve(transitions.size());
    state.acquire_target_owner_names.reserve(transitions.size());
    state.resume_source_owner_names.reserve(transitions.size());
    state.resume_target_owner_names.reserve(transitions.size());
    state.acquire_operation_names.reserve(transitions.size());
    state.resume_operation_names.reserve(transitions.size());
    for (auto const& event : transitions) {
        state.acquire_source_owner_names.push_back(event.acquisition.source_owner_name);
        state.acquire_target_owner_names.push_back(event.acquisition.target_owner_name);
        state.resume_source_owner_names.push_back(event.resumption.source_owner_name);
        state.resume_target_owner_names.push_back(event.resumption.target_owner_name);
        state.acquire_operation_names.push_back(event.acquisition.operation_name);
        state.resume_operation_names.push_back(event.resumption.operation_name);
        state.all_cleanup_calls_enabled =
            state.all_cleanup_calls_enabled &&
            event.acquisition.cleanup_calls_enabled &&
            event.resumption.cleanup_calls_enabled;
    }
    return state;
}

auto build_computed_inserted_cleanup_state_verification_state(
    ComputedCleanupProofModel const& proof_model
) -> ComputedInsertedCleanupStateVerificationState {
    auto const& verifications = proof_model.inserted_cleanup_state.verification_events;
    auto state = ComputedInsertedCleanupStateVerificationState {
        .from_metadata = proof_model.inserted_cleanup_state.from_metadata,
        .verification_count = verifications.size(),
    };
    state.all_cleanup_calls_enabled = state.verification_count > 0;
    state.acquire_operation_names.reserve(verifications.size());
    state.resume_operation_names.reserve(verifications.size());
    state.acquire_source_owner_names.reserve(verifications.size());
    state.acquire_target_owner_names.reserve(verifications.size());
    state.resume_source_owner_names.reserve(verifications.size());
    state.resume_target_owner_names.reserve(verifications.size());
    state.blocked_reasons.reserve(verifications.size());
    for (auto const& event : verifications) {
        if (event.kind == InsertedCleanupStateVerificationKind::paired && event.acquisition.has_value()) {
            auto const& acquisition = *event.acquisition;
            state.acquire_operation_names.push_back(acquisition.operation_name);
            state.resume_operation_names.push_back(event.operation.operation_name);
            state.acquire_source_owner_names.push_back(acquisition.source_owner_name);
            state.acquire_target_owner_names.push_back(acquisition.target_owner_name);
            state.resume_source_owner_names.push_back(event.operation.source_owner_name);
            state.resume_target_owner_names.push_back(event.operation.target_owner_name);
            state.all_cleanup_calls_enabled =
                state.all_cleanup_calls_enabled &&
                acquisition.cleanup_calls_enabled &&
                event.operation.cleanup_calls_enabled;
            ++state.paired_count;
        } else {
            state.blocked_reasons.push_back(event.reason);
            state.all_cleanup_calls_enabled = false;
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
    result.dynamic_array_construction_plan_state =
        build_dynamic_array_construction_plan_state(emission);
    result.dynamic_array_runtime_request_state =
        build_dynamic_array_runtime_request_state(emission);
    result.dynamic_array_allocation_call_emission_state =
        build_dynamic_array_allocation_call_emission_state(
            std::move(emission.dynamic_array_allocation_call_ir)
        );
    result.dynamic_array_descriptor_cleanup_plan_state =
        build_dynamic_array_descriptor_cleanup_plan_state(emission);
    result.dynamic_array_cleanup_obligation_state =
        build_dynamic_array_cleanup_obligation_state(emission);
    result.dynamic_array_cleanup_sequence_plan_state =
        build_dynamic_array_cleanup_sequence_plan_state(emission);
    result.dynamic_array_cleanup_sequence_verification_state =
        build_dynamic_array_cleanup_sequence_verification_state(emission);
    result.dynamic_array_cleanup_sequence_verification_passed =
        !emission.dynamic_array_cleanup_sequence_verifications.empty() &&
        lowering::dynamic_array_cleanup_sequence_verification_report_passed(
            emission.dynamic_array_cleanup_sequence_verifications
        );
    if (emission.dynamic_array_cleanup_emission_capability.has_value()) {
        result.dynamic_array_cleanup_capability_proven =
            lowering::dynamic_array_cleanup_emission_capability_proven(
                *emission.dynamic_array_cleanup_emission_capability
            );
    }
    result.dynamic_array_cleanup_emission_capability_state =
        build_dynamic_array_cleanup_emission_capability_state(emission);
    result.dynamic_array_cleanup_availability = DynamicArrayCleanupAvailability {
        .missing_element_drop_pairs =
            result.dynamic_array_cleanup_emission_capability_state.missing_element_drop_pairs,
        .descriptor_origins_available = !result.semantic_result.dynamic_array_descriptor_origins.empty(),
        .descriptor_cleanup_plans_available = !emission.dynamic_array_descriptor_cleanup_plans.empty(),
        .cleanup_obligations_available = !emission.dynamic_array_cleanup_obligations.empty(),
        .sequence_verification_available = !emission.dynamic_array_cleanup_sequence_verifications.empty(),
        .sequence_verification_passed = result.dynamic_array_cleanup_sequence_verification_passed,
        .cleanup_capability_proven = result.dynamic_array_cleanup_capability_proven,
    };
    result.emitted_dynamic_array_cleanup_obligation_state =
        build_emitted_dynamic_array_cleanup_obligation_state(emission.emitted_dynamic_array_cleanup_obligations);
    result.emitted_dynamic_array_cleanup_sequence_plan_state =
        build_emitted_dynamic_array_cleanup_sequence_plan_state(emission.emitted_dynamic_array_cleanup_sequence_plans);
    result.emitted_dynamic_array_cleanup_sequence_verification_state =
        build_emitted_dynamic_array_cleanup_sequence_verification_state(
            emission.emitted_dynamic_array_cleanup_sequence_verifications
        );
    result.computed_dynamic_array_for_descriptor_render_state =
        build_computed_dynamic_array_for_descriptor_render_state(emission);
    result.computed_dynamic_array_for_loop_control_render_state =
        build_computed_dynamic_array_for_loop_control_render_state(emission);
    result.computed_dynamic_array_for_element_address_render_state =
        build_computed_dynamic_array_for_element_address_render_state(emission);
    result.computed_dynamic_array_for_element_load_render_state =
        build_computed_dynamic_array_for_element_load_render_state(emission);
    result.computed_dynamic_array_for_loop_continue_render_state =
        build_computed_dynamic_array_for_loop_continue_render_state(emission);
    result.computed_dynamic_array_for_loop_render_sequence_state =
        build_computed_dynamic_array_for_loop_render_sequence_state(emission);
    result.computed_dynamic_array_for_loop_exit_cleanup_state =
        build_computed_dynamic_array_for_loop_exit_cleanup_state(emission);
    result.computed_dynamic_array_for_cleanup_transition_state =
        build_computed_dynamic_array_for_cleanup_transition_state(emission);
    result.computed_dynamic_array_for_inserted_cleanup_transition_state =
        build_computed_inserted_cleanup_transition_state(cleanup_proof_model);
    result.computed_dynamic_array_for_inserted_cleanup_state_verification_state =
        build_computed_inserted_cleanup_state_verification_state(cleanup_proof_model);
    result.computed_dynamic_array_for_inserted_cleanup_handoff_state =
        build_computed_inserted_cleanup_handoff_state(cleanup_proof_model);
    result.computed_dynamic_array_for_cleanup_proof_summary_state =
        build_computed_cleanup_proof_summary_state(cleanup_proof_model);
    result.computed_dynamic_array_for_cleanup_call_emission_gate_state =
        build_computed_cleanup_call_emission_gate_state(cleanup_proof_model);
    result.computed_dynamic_array_for_cleanup_call_plan_render_state =
        build_computed_cleanup_call_plan_render_state(cleanup_proof_model);
    result.computed_dynamic_array_for_cleanup_call_insertion_gate_state =
        build_computed_cleanup_call_insertion_gate_state(cleanup_proof_model);
    result.computed_dynamic_array_for_cleanup_call_insertion_capability_state =
        build_computed_cleanup_call_insertion_capability_state(options);
    result.computed_dynamic_array_for_inserted_cleanup_call_state =
        build_computed_inserted_cleanup_call_state(cleanup_proof_model);
    result.consumed_descriptor_finalization_state =
        build_consumed_descriptor_finalization_state(emission);
    result.computed_dynamic_array_for_consumed_cleanup_descriptor_model_state =
        build_computed_consumed_cleanup_descriptor_model_state(emission);
    result.computed_dynamic_array_for_consumed_cleanup_descriptor_state =
        build_computed_consumed_cleanup_descriptor_state(cleanup_proof_model);
    result.aggregate_projection_access_plan_state =
        build_aggregate_projection_access_plan_state(emission);
    result.computed_dynamic_array_for_production_emission_gate_state =
        build_computed_dynamic_array_for_production_emission_gate_state(emission);
    result.computed_dynamic_array_for_production_sequence_state =
        build_computed_dynamic_array_for_production_sequence_state(emission);
    result.computed_dynamic_array_for_production_readiness =
        plan_computed_dynamic_array_for_production_readiness(
            result.computed_dynamic_array_for_production_emission_gate_state,
            result.computed_dynamic_array_for_production_sequence_state,
            result.computed_dynamic_array_for_inserted_cleanup_transition_state,
            result.computed_dynamic_array_for_inserted_cleanup_state_verification_state,
            result.computed_dynamic_array_for_cleanup_call_insertion_capability_state
        );
    result.computed_dynamic_array_for_production_sequence_module_ir_artifact_state.comment_ir_lines =
        std::move(emission.computed_dynamic_array_for_production_sequence_module_ir);
    result.dynamic_array_cleanup_production_readiness =
        plan_dynamic_array_cleanup_production_readiness(result, options);
    result.planned_drop_declaration_state =
        build_planned_drop_declaration_state(emission);
    result.planned_drop_action_state =
        build_planned_drop_action_state(emission);
    result.drop_cleanup_authorization_state =
        build_drop_cleanup_authorization_state(emission);
    result.drop_readiness_snapshot = emission.drop_readiness_snapshot();
    result.drop_readiness_summary = emission.drop_readiness_summary();
    result.drop_readiness_blocker_summary =
        lowering::summarize_drop_readiness_blockers(result.drop_readiness_snapshot);
    result.runtime_indexed_cleanup_capability_state =
        build_runtime_indexed_cleanup_capability_state(emission);
    result.runtime_indexed_cleanup_emission_plan_state =
        build_runtime_indexed_cleanup_emission_plan_state(emission);
    result.runtime_indexed_cleanup_ir_render_state =
        build_runtime_indexed_cleanup_ir_render_state(emission);
    result.runtime_indexed_cleanup_module_ir_artifact_state =
        build_runtime_indexed_cleanup_module_ir_artifact_state(
            result.runtime_indexed_cleanup_ir_render_state
        );
    result.runtime_indexed_cleanup_module_ir_insertion_gate_state =
        build_runtime_indexed_cleanup_module_ir_insertion_gate_state(
            result.runtime_indexed_cleanup_ir_render_state,
            result.runtime_indexed_cleanup_module_ir_artifact_state,
            options
        );
    result.runtime_indexed_cleanup_module_ir_insertion_preview_state =
        build_runtime_indexed_cleanup_module_ir_insertion_preview_state(
            result.ir_text,
            result.runtime_indexed_cleanup_module_ir_artifact_state,
            result.runtime_indexed_cleanup_module_ir_insertion_gate_state
        );
    result.runtime_indexed_cleanup_module_ir_candidate_state =
        build_runtime_indexed_cleanup_module_ir_candidate_state(
            result.ir_text,
            result.runtime_indexed_cleanup_module_ir_artifact_state,
            result.runtime_indexed_cleanup_module_ir_insertion_preview_state
        );
    result.runtime_indexed_cleanup_module_ir_candidate_verification_state =
        build_runtime_indexed_cleanup_module_ir_candidate_verification_state(
            result.ir_text,
            result.runtime_indexed_cleanup_module_ir_artifact_state,
            result.runtime_indexed_cleanup_module_ir_candidate_state
        );
    auto const runtime_indexed_cleanup_function_module_base_ir_text = result.ir_text;
    result.runtime_indexed_cleanup_module_ir_mutation_state =
        apply_runtime_indexed_cleanup_module_ir_mutation(
            options,
            result.runtime_indexed_cleanup_module_ir_artifact_state,
            result.runtime_indexed_cleanup_module_ir_candidate_state,
            result.runtime_indexed_cleanup_module_ir_candidate_verification_state,
            result.ir_text
        );
    result.runtime_indexed_cleanup_function_cfg_rewrite_plan_state =
        build_runtime_indexed_cleanup_function_cfg_rewrite_plan_state(
            result.runtime_indexed_cleanup_emission_plan_state
        );
    result.runtime_indexed_cleanup_function_cfg_rewrite_verification_state =
        build_runtime_indexed_cleanup_function_cfg_rewrite_verification_state(
            runtime_indexed_cleanup_function_module_base_ir_text,
            result.runtime_indexed_cleanup_function_cfg_rewrite_plan_state
        );
    result.runtime_indexed_cleanup_function_ir_rewrite_candidate_state =
        build_runtime_indexed_cleanup_function_ir_rewrite_candidate_state(
            runtime_indexed_cleanup_function_module_base_ir_text,
            result.runtime_indexed_cleanup_function_cfg_rewrite_plan_state
        );
    result.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state =
        build_runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state(
            result.runtime_indexed_cleanup_function_ir_rewrite_candidate_state
        );
    result.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state =
        build_runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state(
            options,
            runtime_indexed_cleanup_function_module_base_ir_text,
            result.runtime_indexed_cleanup_function_ir_rewrite_candidate_state,
            result.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
        );
    result.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state =
        build_runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state(
            result.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state,
            result.runtime_indexed_cleanup_function_ir_rewrite_candidate_state,
            result.runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state
        );
    result.runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state =
        apply_runtime_indexed_cleanup_function_ir_module_rewrite_mutation(
            options,
            runtime_indexed_cleanup_function_module_base_ir_text,
            result.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state,
            result.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state,
            result.runtime_indexed_cleanup_function_ir_rewrite_candidate_state,
            result.ir_text
        );
    result.runtime_indexed_member_cleanup_function_ir_module_rewrite_mutation_state =
        apply_runtime_indexed_member_cleanup_function_ir_module_rewrite_mutation(
            emission,
            result.ir_text
        );
    result.runtime_indexed_cleanup_module_ir_production_readiness_state =
        build_runtime_indexed_cleanup_module_ir_production_readiness_state(
            result.runtime_indexed_cleanup_module_ir_insertion_gate_state,
            result.runtime_indexed_cleanup_module_ir_insertion_preview_state,
            result.runtime_indexed_cleanup_module_ir_candidate_state,
            result.runtime_indexed_cleanup_module_ir_candidate_verification_state,
            result.runtime_indexed_cleanup_module_ir_mutation_state,
            result.runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state,
            result.runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state
        );
    result.runtime_indexed_cleanup_audit_lines =
        std::move(emission.runtime_indexed_cleanup_audit_lines);
    result.runtime_indexed_member_cleanup_sibling_fields =
        std::move(emission.runtime_indexed_member_cleanup_sibling_fields);
    result.semantic_drop_lowering_authorizations = std::move(emission.semantic_drop_lowering_authorizations);
}

}  // namespace orison::pipeline
