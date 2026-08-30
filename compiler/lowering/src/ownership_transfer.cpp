#include "orison/lowering/ownership_transfer.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/aggregate_path.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/runtime_index_expression.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/target_layout.hpp"

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

auto append_source_line(std::ostringstream& report, std::size_t source_line) -> void {
    if (source_line != 0) {
        report << " source-line " << source_line;
    }
}

auto member_cleanup_target_symbol_from_preview_operations(
    std::vector<std::string> const& preview_operations
) -> std::string {
    auto constexpr prefix = std::string_view {"call-member-cleanup-target "};
    for (auto const& operation : preview_operations) {
        if (operation.starts_with(prefix)) {
            return operation.substr(prefix.size());
        }
    }
    return {};
}

auto decimal_index_owner_segment(
    syntax::ExpressionSyntax const& expression
) -> std::optional<std::string> {
    if (expression.kind != syntax::ExpressionKind::integer_literal || expression.text.empty()) {
        return std::nullopt;
    }
    if (!std::ranges::all_of(expression.text, [](char character) {
            return std::isdigit(static_cast<unsigned char>(character)) != 0;
        })) {
        return std::nullopt;
    }
    return "element" + expression.text;
}

auto fixed_array_length_value(std::string_view source_type_name) -> std::optional<std::string> {
    constexpr auto prefix = std::string_view {"Array<"};
    if (!source_type_name.starts_with(prefix) || !source_type_name.ends_with(">")) {
        return std::nullopt;
    }

    auto length_text = source_type_name.substr(
        prefix.size(),
        source_type_name.size() - prefix.size() - 1
    );
    auto const separator = length_text.rfind(',');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }

    length_text.remove_prefix(separator + 1);
    while (!length_text.empty() && length_text.front() == ' ') {
        length_text.remove_prefix(1);
    }
    while (!length_text.empty() && length_text.back() == ' ') {
        length_text.remove_suffix(1);
    }
    if (length_text.empty() || !std::ranges::all_of(length_text, [](char character) {
            return std::isdigit(static_cast<unsigned char>(character)) != 0;
        })) {
        return std::nullopt;
    }
    return std::string {length_text};
}

auto same_runtime_indexed_partial_owner_record(
    RuntimeIndexedPartialOwner const& lhs,
    RuntimeIndexedPartialOwner const& rhs
) -> bool {
    return lhs.owner_name == rhs.owner_name &&
        lhs.index_expression_text == rhs.index_expression_text &&
        lhs.element_source_type_name == rhs.element_source_type_name &&
        lhs.moved_source_type_name == rhs.moved_source_type_name &&
        lhs.moved_member_path == rhs.moved_member_path;
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
    bool production_cleanup_emission_enabled,
    bool member_cleanup_ir_mutation_requested,
    bool member_cleanup_production_gate_requested,
    bool member_cleanup_apply_authorization_requested,
    bool member_cleanup_rewrite_execution_requested
) -> void {
    if (std::ranges::any_of(
            state.runtime_indexed_partial_owners,
            [&](RuntimeIndexedPartialOwner const& recorded) {
                return same_runtime_indexed_partial_owner_record(recorded, owner);
            }
        )) {
        return;
    }

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
    auto member_rewrite_candidate =
        runtime_indexed_member_cleanup_function_rewrite_candidate(member_cfg_slice);
    auto member_edit_script_plan =
        runtime_indexed_member_cleanup_function_rewrite_edit_script_plan(member_rewrite_candidate);
    auto member_edit_script_validation =
        runtime_indexed_member_cleanup_function_rewrite_edit_script_validation(member_edit_script_plan);
    auto member_staged_apply_plan =
        runtime_indexed_member_cleanup_function_rewrite_staged_apply_plan(member_edit_script_validation);
    auto member_mutation_gate =
        runtime_indexed_member_cleanup_module_mutation_gate(
            member_cfg_slice,
            member_edit_script_validation,
            member_staged_apply_plan
        );
    auto member_production_readiness = runtime_indexed_member_cleanup_production_readiness(
        member_proof,
        member_targets,
        {},
        member_cfg_slice,
        member_mutation_gate
    );
    auto member_promotion_checklist = runtime_indexed_member_cleanup_promotion_checklist(
        member_rewrite_candidate,
        member_edit_script_plan,
        member_edit_script_validation,
        member_staged_apply_plan,
        member_mutation_gate,
        member_production_readiness
    );
    auto member_typed_promotion_gate = runtime_indexed_member_cleanup_typed_promotion_gate(
        member_promotion_checklist,
        member_cleanup_ir_mutation_requested,
        member_cleanup_production_gate_requested
    );
    auto member_promotion_seam = runtime_indexed_member_cleanup_promotion_seam(member_typed_promotion_gate);
    auto member_mutation_operation_plan =
        runtime_indexed_member_cleanup_mutation_operation_plan(member_promotion_seam, member_edit_script_plan);
    auto member_mutation_operation_validation =
        runtime_indexed_member_cleanup_mutation_operation_validation(member_mutation_operation_plan);
    auto member_mutation_conflict_detection =
        runtime_indexed_member_cleanup_mutation_conflict_detection(
            member_mutation_operation_plan,
            member_mutation_operation_validation
        );
    auto member_mutation_apply_authorization =
        runtime_indexed_member_cleanup_mutation_apply_authorization(
            member_mutation_operation_validation,
            member_mutation_conflict_detection,
            member_typed_promotion_gate.ir_mutation_enabled,
            member_typed_promotion_gate.production_gate_enabled,
            member_cleanup_apply_authorization_requested
        );
    auto member_mutation_apply_preview =
        runtime_indexed_member_cleanup_mutation_apply_preview(
            member_mutation_operation_plan,
            member_mutation_apply_authorization
        );
    auto member_mutation_post_apply_verification =
        runtime_indexed_member_cleanup_mutation_post_apply_verification(member_mutation_apply_preview);
    auto member_mutation_promotion_summary =
        runtime_indexed_member_cleanup_mutation_promotion_summary(
            member_mutation_operation_plan,
            member_mutation_operation_validation,
            member_mutation_conflict_detection,
            member_mutation_apply_authorization,
            member_mutation_apply_preview,
            member_mutation_post_apply_verification
        );
    auto member_mutation_production_readiness =
        runtime_indexed_member_cleanup_mutation_production_readiness(member_mutation_promotion_summary);
    auto member_mutation_readiness_verdict =
        runtime_indexed_member_cleanup_mutation_readiness_verdict(member_mutation_production_readiness);
    auto member_mutation_rewrite_authorization =
        runtime_indexed_member_cleanup_mutation_rewrite_authorization(
            member_mutation_readiness_verdict,
            member_cleanup_rewrite_execution_requested
        );
    auto member_mutation_rewrite_execution_plan =
        runtime_indexed_member_cleanup_mutation_rewrite_execution_plan(
            member_mutation_rewrite_authorization,
            member_cleanup_rewrite_execution_requested
        );
    auto member_mutation_rewrite_execution_verdict =
        runtime_indexed_member_cleanup_mutation_rewrite_execution_verdict(member_mutation_rewrite_execution_plan);
    auto member_mutation_rewrite_promotion_status =
        runtime_indexed_member_cleanup_mutation_rewrite_promotion_status(
            member_mutation_rewrite_authorization,
            member_mutation_rewrite_execution_plan,
            member_mutation_rewrite_execution_verdict
        );
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
    state.runtime_indexed_member_cleanup_function_rewrite_candidates.push_back(
        std::move(member_rewrite_candidate)
    );
    state.runtime_indexed_member_cleanup_function_rewrite_edit_script_plans.push_back(
        std::move(member_edit_script_plan)
    );
    state.runtime_indexed_member_cleanup_function_rewrite_edit_script_validations.push_back(
        std::move(member_edit_script_validation)
    );
    state.runtime_indexed_member_cleanup_function_rewrite_staged_apply_plans.push_back(
        std::move(member_staged_apply_plan)
    );
    state.runtime_indexed_member_cleanup_module_mutation_gates.push_back(std::move(member_mutation_gate));
    state.runtime_indexed_member_cleanup_production_readiness.push_back(
        std::move(member_production_readiness)
    );
    state.runtime_indexed_member_cleanup_promotion_checklists.push_back(std::move(member_promotion_checklist));
    state.runtime_indexed_member_cleanup_typed_promotion_gates.push_back(std::move(member_typed_promotion_gate));
    state.runtime_indexed_member_cleanup_promotion_seams.push_back(std::move(member_promotion_seam));
    state.runtime_indexed_member_cleanup_mutation_operation_plans.push_back(
        std::move(member_mutation_operation_plan)
    );
    state.runtime_indexed_member_cleanup_mutation_operation_validations.push_back(
        std::move(member_mutation_operation_validation)
    );
    state.runtime_indexed_member_cleanup_mutation_conflict_detections.push_back(
        std::move(member_mutation_conflict_detection)
    );
    state.runtime_indexed_member_cleanup_mutation_apply_authorizations.push_back(
        std::move(member_mutation_apply_authorization)
    );
    state.runtime_indexed_member_cleanup_mutation_apply_previews.push_back(
        std::move(member_mutation_apply_preview)
    );
    state.runtime_indexed_member_cleanup_mutation_post_apply_verifications.push_back(
        std::move(member_mutation_post_apply_verification)
    );
    state.runtime_indexed_member_cleanup_mutation_promotion_summaries.push_back(
        std::move(member_mutation_promotion_summary)
    );
    state.runtime_indexed_member_cleanup_mutation_production_readiness.push_back(
        std::move(member_mutation_production_readiness)
    );
    state.runtime_indexed_member_cleanup_mutation_readiness_verdicts.push_back(
        std::move(member_mutation_readiness_verdict)
    );
    state.runtime_indexed_member_cleanup_mutation_rewrite_authorizations.push_back(
        std::move(member_mutation_rewrite_authorization)
    );
    state.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans.push_back(
        std::move(member_mutation_rewrite_execution_plan)
    );
    state.runtime_indexed_member_cleanup_mutation_rewrite_execution_verdicts.push_back(
        std::move(member_mutation_rewrite_execution_verdict)
    );
    state.runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses.push_back(
        std::move(member_mutation_rewrite_promotion_status)
    );
}

auto runtime_indexed_partial_owner_for_constructor_argument(
    syntax::ExpressionSyntax const& argument,
    std::string_view expected_source_type,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<RuntimeIndexedPartialOwner> {
    if (!is_owned_transfer_source_type(expected_source_type, context)) {
        return std::nullopt;
    }

    auto path = collect_named_aggregate_path(argument);
    if (!path.has_value() || path->base_expression == nullptr) {
        return std::nullopt;
    }

    auto owner_source_type = state.source_type_names.find(path->base_expression->text);
    if (owner_source_type == state.source_type_names.end()) {
        return std::nullopt;
    }

    auto owner_name = path->base_expression->text;
    auto owner_storage = aggregate_storage_for_name(path->base_expression->text, state);
    auto owner_address_name = owner_storage.value_or(std::string {});
    auto owner_address_ir_lines = std::vector<std::string> {};
    auto current_source_type_name = owner_source_type->second;
    auto moved_source_type_name = std::optional<std::string> {};
    auto moved_member_path = std::vector<std::string> {};
    for (auto step_index = std::size_t {0}; step_index < path->steps.size(); ++step_index) {
        auto const& step = path->steps[step_index];
        if (step.kind == AggregatePathStepKind::member) {
            auto record = context.records.find(current_source_type_name);
            if (record == context.records.end()) {
                return std::nullopt;
            }
            auto const* field = find_record_field(record->second, step.field_name);
            if (field == nullptr) {
                return std::nullopt;
            }
            owner_name += ".";
            owner_name += step.field_name;
            if (!owner_address_name.empty()) {
                auto projected_owner_address_name = "%" + owner_name + ".runtime_cleanup.owner.addr";
                owner_address_ir_lines.push_back(
                    "  " + projected_owner_address_name + " = getelementptr " +
                    record->second.llvm_type_name + ", ptr " + owner_address_name +
                    ", i32 0, i32 " + std::to_string(field->index) + "\n"
                );
                owner_address_name = std::move(projected_owner_address_name);
            }
            current_source_type_name = field->source_type_name;
            continue;
        }

        if (step.index_expression == nullptr) {
            return std::nullopt;
        }

        auto dynamic_array_element_source_type =
            dynamic_array_element_source_type_name(current_source_type_name);
        auto element_source_type = dynamic_array_element_source_type.has_value()
            ? dynamic_array_element_source_type
            : array_element_source_type_name(current_source_type_name);
        if (!element_source_type.has_value()) {
            return std::nullopt;
        }
        auto runtime_index_owner_source_type_name = current_source_type_name;
        auto runtime_index_element_source_type_name = *element_source_type;

        if (!dynamic_array_element_source_type.has_value()) {
            if (auto element_segment = decimal_index_owner_segment(*step.index_expression)) {
                owner_name += ".";
                owner_name += *element_segment;
                auto current_llvm_type = llvm_type_for_source_type_name(current_source_type_name, context);
                if (!current_llvm_type.has_value()) {
                    return std::nullopt;
                }
                if (!owner_address_name.empty()) {
                    auto projected_owner_address_name = "%" + owner_name + ".runtime_cleanup.owner.addr";
                    owner_address_ir_lines.push_back(
                        "  " + projected_owner_address_name + " = getelementptr " +
                        *current_llvm_type + ", ptr " + owner_address_name +
                        ", i64 0, i64 " + *element_segment + "\n"
                    );
                    owner_address_name = std::move(projected_owner_address_name);
                }
                current_source_type_name = std::move(*element_source_type);
                continue;
            }
        }

        moved_source_type_name = *element_source_type;
        current_source_type_name = std::move(*element_source_type);
        auto const index_expression_text = runtime_index_expression_key(*step.index_expression);
        for (auto remaining_step_index = step_index + 1;
             remaining_step_index < path->steps.size();
             ++remaining_step_index) {
            auto const& remaining_step = path->steps[remaining_step_index];
            if (remaining_step.kind == AggregatePathStepKind::member) {
                auto record = context.records.find(current_source_type_name);
                if (record == context.records.end()) {
                    return std::nullopt;
                }
                auto const* field = find_record_field(record->second, remaining_step.field_name);
                if (field == nullptr) {
                    return std::nullopt;
                }
                current_source_type_name = field->source_type_name;
                moved_source_type_name = current_source_type_name;
                moved_member_path.push_back(remaining_step.field_name);
                continue;
            }

            if (remaining_step.index_expression == nullptr) {
                return std::nullopt;
            }
            auto nested_element_source_type = array_element_source_type_name(current_source_type_name);
            if (!nested_element_source_type.has_value()) {
                return std::nullopt;
            }
            current_source_type_name = std::move(*nested_element_source_type);
            moved_source_type_name = current_source_type_name;
            moved_member_path.push_back("[]");
        }

        if (!moved_source_type_name.has_value() || *moved_source_type_name != expected_source_type) {
            return std::nullopt;
        }
        auto element_llvm_type_name =
            llvm_type_for_source_type_name(runtime_index_element_source_type_name, context);
        if (!element_llvm_type_name.has_value()) {
            return std::nullopt;
        }
        auto owner_llvm_type_name =
            llvm_type_for_source_type_name(runtime_index_owner_source_type_name, context);
        auto owner_static_length_value = fixed_array_length_value(runtime_index_owner_source_type_name);
        auto element_size_bytes = lowered_type_size_bytes(*element_llvm_type_name, context);
        return RuntimeIndexedPartialOwner {
            .owner_name = std::move(owner_name),
            .index_expression_text = index_expression_text,
            .element_source_type_name = std::move(runtime_index_element_source_type_name),
            .element_llvm_type_name = std::move(*element_llvm_type_name),
            .owner_llvm_type_name =
                owner_llvm_type_name.has_value() ? std::move(*owner_llvm_type_name) : std::string {},
            .owner_address_name = std::move(owner_address_name),
            .owner_address_ir_lines = std::move(owner_address_ir_lines),
            .static_length_value =
                owner_static_length_value.has_value() ? std::move(*owner_static_length_value) : std::string {},
            .element_size_value = element_size_bytes.has_value()
                ? std::to_string(*element_size_bytes)
                : std::string {},
            .moved_source_type_name = std::move(*moved_source_type_name),
            .moved_member_path = std::move(moved_member_path),
            .cleanup_strategy = "skip-moved-element",
            .constructor_move_enabled = false,
        };
    }

    return std::nullopt;
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
        .source_line = owner.source_line,
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
        .source_line = plan.source_line,
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
        .source_line = proof.source_line,
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
            .source_line = sketch.source_line,
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
        .source_line = sketch.source_line,
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
        .source_line = gate.source_line,
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
    auto member_cleanup_target_symbol_name =
        member_cleanup_target_symbol_from_preview_operations(plan.preview_operations);
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
        .member_cleanup_target_symbol_name = std::move(member_cleanup_target_symbol_name),
        .topology_edges = std::move(topology_edges),
        .insertion_plan_ready = insertion_plan_ready,
        .block_topology_ready = block_topology_ready,
        .preview_operations_ready = preview_operations_ready,
        .report_only = true,
        .production_enabled = false,
        .source_line = plan.source_line,
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
           << " cleanup-target "
           << (plan.member_cleanup_target_symbol_name.empty() ? "missing" : plan.member_cleanup_target_symbol_name)
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
            "  ; member cleanup target " + plan.member_cleanup_target_symbol_name + "\n",
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
        .member_cleanup_target_symbol_name = plan.member_cleanup_target_symbol_name,
        .cfg_lines = std::move(cfg_lines),
        .composition_ready = composition_ready,
        .slice_rendered = composition_ready,
        .report_only = true,
        .production_enabled = false,
        .source_line = plan.source_line,
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
           << " cleanup-target "
           << (slice.member_cleanup_target_symbol_name.empty() ? "missing" : slice.member_cleanup_target_symbol_name)
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

auto runtime_indexed_member_cleanup_function_rewrite_candidate(
    RuntimeIndexedMemberCleanupCfgSlice const& slice
) -> RuntimeIndexedMemberCleanupFunctionRewriteCandidate {
    auto cfg_slice_ready = slice.slice_rendered && !slice.cfg_lines.empty();
    auto anchor_ready = cfg_slice_ready && !slice.insertion_anchor.empty();
    auto labels_ready =
        cfg_slice_ready &&
        !slice.entry_block_name.empty() &&
        !slice.skip_block_name.empty() &&
        !slice.sibling_drop_block_name.empty() &&
        !slice.preserve_block_name.empty() &&
        !slice.exit_block_name.empty();
    auto line_present = [&](std::string const& expected_line) {
        return std::find(slice.cfg_lines.begin(), slice.cfg_lines.end(), expected_line) !=
               slice.cfg_lines.end();
    };
    auto candidate_available = anchor_ready && labels_ready;
    auto candidate_verified =
        candidate_available &&
        line_present("; report-only runtime-index member cleanup anchor " + slice.insertion_anchor + "\n") &&
        line_present(slice.entry_block_name + ":\n") &&
        line_present(slice.skip_block_name + ":\n") &&
        line_present(slice.sibling_drop_block_name + ":\n") &&
        line_present(slice.preserve_block_name + ":\n") &&
        line_present(slice.exit_block_name + ":\n") &&
        !slice.member_cleanup_target_symbol_name.empty() &&
        line_present("  ; br label %" + slice.preserve_block_name + "\n") &&
        line_present("  ; br label %" + slice.exit_block_name + "\n");

    return RuntimeIndexedMemberCleanupFunctionRewriteCandidate {
        .owner_name = slice.owner_name,
        .index_expression_text = slice.index_expression_text,
        .element_source_type_name = slice.element_source_type_name,
        .moved_source_type_name = slice.moved_source_type_name,
        .moved_member_path = slice.moved_member_path,
        .insertion_anchor = slice.insertion_anchor,
        .entry_block_name = slice.entry_block_name,
        .skip_block_name = slice.skip_block_name,
        .sibling_drop_block_name = slice.sibling_drop_block_name,
        .preserve_block_name = slice.preserve_block_name,
        .exit_block_name = slice.exit_block_name,
        .member_cleanup_target_symbol_name = slice.member_cleanup_target_symbol_name,
        .replaced_terminator_text =
            anchor_ready ? "br label %" + slice.insertion_anchor : std::string {},
        .replacement_branch_text =
            labels_ready ? "br label %" + slice.entry_block_name : std::string {},
        .appended_cfg_preview_lines = slice.cfg_lines,
        .cfg_slice_ready = cfg_slice_ready,
        .anchor_ready = anchor_ready,
        .branch_rewrite_planned = candidate_available,
        .cfg_append_planned = candidate_available,
        .candidate_available = candidate_available,
        .candidate_verified = candidate_verified,
        .report_only = true,
        .production_enabled = false,
        .source_line = slice.source_line,
    };
}

auto runtime_indexed_member_cleanup_function_rewrite_candidate_report(
    RuntimeIndexedMemberCleanupFunctionRewriteCandidate const& candidate
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup function-rewrite-candidate owner " << candidate.owner_name
           << " index " << candidate.index_expression_text
           << " element " << candidate.element_source_type_name
           << " moved " << candidate.moved_source_type_name
           << " member-path " << dotted_path(candidate.moved_member_path)
           << " anchor " << (candidate.insertion_anchor.empty() ? "missing" : candidate.insertion_anchor)
           << " entry " << (candidate.entry_block_name.empty() ? "missing" : candidate.entry_block_name)
           << " sibling-drop "
           << (candidate.sibling_drop_block_name.empty() ? "missing" : candidate.sibling_drop_block_name)
           << " preserve "
           << (candidate.preserve_block_name.empty() ? "missing" : candidate.preserve_block_name)
           << " exit " << (candidate.exit_block_name.empty() ? "missing" : candidate.exit_block_name)
           << " cleanup-target "
           << (candidate.member_cleanup_target_symbol_name.empty() ? "missing" : candidate.member_cleanup_target_symbol_name)
           << " cfg-slice " << (candidate.cfg_slice_ready ? "ready" : "missing")
           << " anchor-state " << (candidate.anchor_ready ? "ready" : "missing")
           << " branch-rewrite " << (candidate.branch_rewrite_planned ? "planned" : "blocked")
           << " cfg-append " << (candidate.cfg_append_planned ? "planned" : "blocked")
           << " candidate " << (candidate.candidate_available ? "available" : "missing")
           << " verification " << (candidate.candidate_verified ? "verified" : "blocked")
           << " report-only " << (candidate.report_only ? "true" : "false")
           << " production " << (candidate.production_enabled ? "enabled" : "disabled")
           << " replaced-terminator "
           << (candidate.replaced_terminator_text.empty() ? "missing" : candidate.replaced_terminator_text)
           << " replacement-branch "
           << (candidate.replacement_branch_text.empty() ? "missing" : candidate.replacement_branch_text)
           << " appended-cfg-lines " << candidate.appended_cfg_preview_lines.size();
    return report.str();
}

auto runtime_indexed_member_cleanup_function_rewrite_edit_script_plan(
    RuntimeIndexedMemberCleanupFunctionRewriteCandidate const& candidate
) -> RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan {
    auto branch_replacement_ready =
        candidate.candidate_verified &&
        !candidate.replaced_terminator_text.empty() &&
        !candidate.replacement_branch_text.empty();
    auto cleanup_cfg_append_ready =
        candidate.candidate_verified &&
        !candidate.appended_cfg_preview_lines.empty();
    auto phi_retarget_ready =
        candidate.candidate_verified &&
        !candidate.insertion_anchor.empty() &&
        !candidate.exit_block_name.empty();
    auto edit_script_ready =
        branch_replacement_ready && cleanup_cfg_append_ready && phi_retarget_ready;

    return RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan {
        .owner_name = candidate.owner_name,
        .index_expression_text = candidate.index_expression_text,
        .element_source_type_name = candidate.element_source_type_name,
        .moved_source_type_name = candidate.moved_source_type_name,
        .moved_member_path = candidate.moved_member_path,
        .insertion_anchor = candidate.insertion_anchor,
        .entry_block_name = candidate.entry_block_name,
        .sibling_drop_block_name = candidate.sibling_drop_block_name,
        .preserve_block_name = candidate.preserve_block_name,
        .exit_block_name = candidate.exit_block_name,
        .member_cleanup_target_symbol_name = candidate.member_cleanup_target_symbol_name,
        .expected_branch_text =
            branch_replacement_ready ? candidate.replaced_terminator_text : std::string {},
        .replacement_branch_text =
            branch_replacement_ready ? candidate.replacement_branch_text : std::string {},
        .cleanup_cfg_append_placement =
            cleanup_cfg_append_ready ? "before-function-closing-brace" : std::string {},
        .expected_closing_text = cleanup_cfg_append_ready ? "\\n}\\n" : std::string {},
        .appended_cfg_preview_lines = candidate.appended_cfg_preview_lines,
        .phi_old_predecessor_block_name =
            phi_retarget_ready ? candidate.insertion_anchor : std::string {},
        .phi_new_predecessor_block_name =
            phi_retarget_ready ? candidate.exit_block_name : std::string {},
        .candidate_verified = candidate.candidate_verified,
        .branch_replacement_ready = branch_replacement_ready,
        .cleanup_cfg_append_ready = cleanup_cfg_append_ready,
        .phi_retarget_ready = phi_retarget_ready,
        .edit_script_ready = edit_script_ready,
        .report_only = true,
        .production_enabled = false,
        .source_line = candidate.source_line,
    };
}

auto runtime_indexed_member_cleanup_function_rewrite_edit_script_plan_report(
    RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan const& plan
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup function-rewrite-edit-script-plan owner " << plan.owner_name
           << " index " << plan.index_expression_text
           << " element " << plan.element_source_type_name
           << " moved " << plan.moved_source_type_name
           << " member-path " << dotted_path(plan.moved_member_path)
           << " anchor " << (plan.insertion_anchor.empty() ? "missing" : plan.insertion_anchor)
           << " entry " << (plan.entry_block_name.empty() ? "missing" : plan.entry_block_name)
           << " sibling-drop "
           << (plan.sibling_drop_block_name.empty() ? "missing" : plan.sibling_drop_block_name)
           << " preserve " << (plan.preserve_block_name.empty() ? "missing" : plan.preserve_block_name)
           << " exit " << (plan.exit_block_name.empty() ? "missing" : plan.exit_block_name)
           << " cleanup-target "
           << (plan.member_cleanup_target_symbol_name.empty() ? "missing" : plan.member_cleanup_target_symbol_name)
           << " candidate " << (plan.candidate_verified ? "verified" : "blocked")
           << " branch-replacement " << (plan.branch_replacement_ready ? "ready" : "missing")
           << " cleanup-cfg-append " << (plan.cleanup_cfg_append_ready ? "ready" : "missing")
           << " phi-retarget " << (plan.phi_retarget_ready ? "ready" : "missing")
           << " edit-script " << (plan.edit_script_ready ? "ready" : "blocked")
           << " report-only " << (plan.report_only ? "true" : "false")
           << " production " << (plan.production_enabled ? "enabled" : "disabled")
           << " expected-branch "
           << (plan.expected_branch_text.empty() ? "missing" : plan.expected_branch_text)
           << " replacement-branch "
           << (plan.replacement_branch_text.empty() ? "missing" : plan.replacement_branch_text)
           << " append-placement "
           << (plan.cleanup_cfg_append_placement.empty() ? "missing" : plan.cleanup_cfg_append_placement)
           << " expected-closing "
           << (plan.expected_closing_text.empty() ? "missing" : plan.expected_closing_text)
           << " phi-old "
           << (plan.phi_old_predecessor_block_name.empty() ? "missing" : plan.phi_old_predecessor_block_name)
           << " phi-new "
           << (plan.phi_new_predecessor_block_name.empty() ? "missing" : plan.phi_new_predecessor_block_name)
           << " appended-cfg-lines " << plan.appended_cfg_preview_lines.size();
    return report.str();
}

auto runtime_indexed_member_cleanup_function_rewrite_edit_script_validation(
    RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan const& plan
) -> RuntimeIndexedMemberCleanupFunctionRewriteEditScriptValidation {
    auto blockers = std::vector<std::string> {};
    auto branch_replacement_valid =
        plan.branch_replacement_ready &&
        !plan.expected_branch_text.empty() &&
        !plan.replacement_branch_text.empty();
    auto cleanup_cfg_append_valid =
        plan.cleanup_cfg_append_ready &&
        plan.cleanup_cfg_append_placement == "before-function-closing-brace" &&
        !plan.expected_closing_text.empty() &&
        !plan.appended_cfg_preview_lines.empty();
    auto phi_retarget_valid =
        plan.phi_retarget_ready &&
        !plan.phi_old_predecessor_block_name.empty() &&
        !plan.phi_new_predecessor_block_name.empty();
    if (!plan.edit_script_ready) {
        blockers.push_back("member-cleanup-edit-script");
    }
    if (!branch_replacement_valid) {
        blockers.push_back("member-cleanup-branch-replacement");
    }
    if (!cleanup_cfg_append_valid) {
        blockers.push_back("member-cleanup-cfg-append");
    }
    if (!phi_retarget_valid) {
        blockers.push_back("member-cleanup-phi-retarget");
    }
    blockers.push_back("production-member-cleanup-module-mutation");

    auto validation_ready =
        plan.edit_script_ready &&
        branch_replacement_valid &&
        cleanup_cfg_append_valid &&
        phi_retarget_valid;

    return RuntimeIndexedMemberCleanupFunctionRewriteEditScriptValidation {
        .owner_name = plan.owner_name,
        .index_expression_text = plan.index_expression_text,
        .element_source_type_name = plan.element_source_type_name,
        .moved_source_type_name = plan.moved_source_type_name,
        .moved_member_path = plan.moved_member_path,
        .insertion_anchor = plan.insertion_anchor,
        .entry_block_name = plan.entry_block_name,
        .exit_block_name = plan.exit_block_name,
        .blockers = std::move(blockers),
        .edit_script_ready = plan.edit_script_ready,
        .branch_replacement_valid = branch_replacement_valid,
        .cleanup_cfg_append_valid = cleanup_cfg_append_valid,
        .phi_retarget_valid = phi_retarget_valid,
        .validation_ready = validation_ready,
        .report_only = true,
        .production_enabled = false,
        .source_line = plan.source_line,
    };
}

auto runtime_indexed_member_cleanup_function_rewrite_edit_script_validation_report(
    RuntimeIndexedMemberCleanupFunctionRewriteEditScriptValidation const& validation
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup function-rewrite-edit-script-validation owner "
           << validation.owner_name
           << " index " << validation.index_expression_text
           << " element " << validation.element_source_type_name
           << " moved " << validation.moved_source_type_name
           << " member-path " << dotted_path(validation.moved_member_path)
           << " anchor " << (validation.insertion_anchor.empty() ? "missing" : validation.insertion_anchor)
           << " entry " << (validation.entry_block_name.empty() ? "missing" : validation.entry_block_name)
           << " exit " << (validation.exit_block_name.empty() ? "missing" : validation.exit_block_name)
           << " edit-script " << (validation.edit_script_ready ? "ready" : "blocked")
           << " branch-replacement " << (validation.branch_replacement_valid ? "valid" : "invalid")
           << " cleanup-cfg-append " << (validation.cleanup_cfg_append_valid ? "valid" : "invalid")
           << " phi-retarget " << (validation.phi_retarget_valid ? "valid" : "invalid")
           << " validation " << (validation.validation_ready ? "ready" : "blocked")
           << " report-only " << (validation.report_only ? "true" : "false")
           << " production " << (validation.production_enabled ? "enabled" : "disabled")
           << " blockers " << validation.blockers.size();
    for (auto const& blocker : validation.blockers) {
        report << " blocker " << blocker;
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_function_rewrite_edit_script_validation_diagnostics(
    RuntimeIndexedMemberCleanupFunctionRewriteEditScriptValidation const& validation
) -> std::vector<std::string> {
    auto diagnostics = std::vector<std::string> {};
    for (auto const& blocker : validation.blockers) {
        auto diagnostic = std::ostringstream {};
        diagnostic << "runtime-index member cleanup edit-script validation diagnostic owner "
                   << validation.owner_name
                   << " index " << validation.index_expression_text
                   << " element " << validation.element_source_type_name
                   << " moved " << validation.moved_source_type_name
                   << " member-path " << dotted_path(validation.moved_member_path)
                   << " blocker " << blocker
                   << " detail ";
        if (blocker == "member-cleanup-edit-script") {
            diagnostic << "member cleanup edit script is not ready";
        } else if (blocker == "member-cleanup-branch-replacement") {
            diagnostic << "member cleanup branch replacement is invalid";
        } else if (blocker == "member-cleanup-cfg-append") {
            diagnostic << "member cleanup CFG append is invalid";
        } else if (blocker == "member-cleanup-phi-retarget") {
            diagnostic << "member cleanup PHI retarget is invalid";
        } else if (blocker == "production-member-cleanup-module-mutation") {
            diagnostic << "member cleanup edit script is validated but production module mutation is disabled";
        } else {
            diagnostic << "member cleanup edit script validation is blocked";
        }
        diagnostics.push_back(diagnostic.str());
    }
    return diagnostics;
}

auto runtime_indexed_member_cleanup_function_rewrite_staged_apply_plan(
    RuntimeIndexedMemberCleanupFunctionRewriteEditScriptValidation const& validation
) -> RuntimeIndexedMemberCleanupFunctionRewriteStagedApplyPlan {
    auto blockers = std::vector<std::string> {};
    if (!validation.validation_ready) {
        blockers.push_back("member-cleanup-edit-script-validation");
    }
    blockers.push_back("production-member-cleanup-module-mutation");

    return RuntimeIndexedMemberCleanupFunctionRewriteStagedApplyPlan {
        .owner_name = validation.owner_name,
        .index_expression_text = validation.index_expression_text,
        .element_source_type_name = validation.element_source_type_name,
        .moved_source_type_name = validation.moved_source_type_name,
        .moved_member_path = validation.moved_member_path,
        .insertion_anchor = validation.insertion_anchor,
        .entry_block_name = validation.entry_block_name,
        .exit_block_name = validation.exit_block_name,
        .blockers = std::move(blockers),
        .validation_ready = validation.validation_ready,
        .branch_replacement_planned = validation.validation_ready,
        .cleanup_cfg_append_planned = validation.validation_ready,
        .phi_retarget_planned = validation.validation_ready,
        .staged_apply_ready = validation.validation_ready,
        .branch_replacement_applied = false,
        .cleanup_cfg_appended = false,
        .phi_retarget_applied = false,
        .report_only = true,
        .production_enabled = false,
        .source_line = validation.source_line,
    };
}

auto runtime_indexed_member_cleanup_function_rewrite_staged_apply_plan_report(
    RuntimeIndexedMemberCleanupFunctionRewriteStagedApplyPlan const& plan
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup function-rewrite-staged-apply-plan owner "
           << plan.owner_name
           << " index " << plan.index_expression_text
           << " element " << plan.element_source_type_name
           << " moved " << plan.moved_source_type_name
           << " member-path " << dotted_path(plan.moved_member_path)
           << " anchor " << (plan.insertion_anchor.empty() ? "missing" : plan.insertion_anchor)
           << " entry " << (plan.entry_block_name.empty() ? "missing" : plan.entry_block_name)
           << " exit " << (plan.exit_block_name.empty() ? "missing" : plan.exit_block_name)
           << " validation " << (plan.validation_ready ? "ready" : "blocked")
           << " branch-replacement " << (plan.branch_replacement_planned ? "planned" : "blocked")
           << " cleanup-cfg-append " << (plan.cleanup_cfg_append_planned ? "planned" : "blocked")
           << " phi-retarget " << (plan.phi_retarget_planned ? "planned" : "blocked")
           << " staged-apply " << (plan.staged_apply_ready ? "ready" : "blocked")
           << " branch-applied " << (plan.branch_replacement_applied ? "true" : "false")
           << " cfg-appended " << (plan.cleanup_cfg_appended ? "true" : "false")
           << " phi-applied " << (plan.phi_retarget_applied ? "true" : "false")
           << " report-only " << (plan.report_only ? "true" : "false")
           << " production " << (plan.production_enabled ? "enabled" : "disabled")
           << " blockers " << plan.blockers.size();
    for (auto const& blocker : plan.blockers) {
        report << " blocker " << blocker;
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_function_rewrite_staged_apply_plan_diagnostics(
    RuntimeIndexedMemberCleanupFunctionRewriteStagedApplyPlan const& plan
) -> std::vector<std::string> {
    auto diagnostics = std::vector<std::string> {};
    for (auto const& blocker : plan.blockers) {
        auto diagnostic = std::ostringstream {};
        diagnostic << "runtime-index member cleanup staged-apply diagnostic owner "
                   << plan.owner_name
                   << " index " << plan.index_expression_text
                   << " element " << plan.element_source_type_name
                   << " moved " << plan.moved_source_type_name
                   << " member-path " << dotted_path(plan.moved_member_path)
                   << " blocker " << blocker
                   << " detail ";
        if (blocker == "member-cleanup-edit-script-validation") {
            diagnostic << "member cleanup staged apply is blocked by edit script validation";
        } else if (blocker == "production-member-cleanup-module-mutation") {
            diagnostic << "member cleanup staged plan is ready but production module mutation is disabled";
        } else {
            diagnostic << "member cleanup staged apply is blocked";
        }
        diagnostics.push_back(diagnostic.str());
    }
    return diagnostics;
}

auto runtime_indexed_member_cleanup_module_mutation_gate(
    RuntimeIndexedMemberCleanupCfgSlice const& slice,
    RuntimeIndexedMemberCleanupFunctionRewriteEditScriptValidation const& validation,
    RuntimeIndexedMemberCleanupFunctionRewriteStagedApplyPlan const& staged_apply_plan
) -> RuntimeIndexedMemberCleanupModuleMutationGate {
    auto blockers = std::vector<std::string> {};
    if (!slice.slice_rendered) {
        blockers.push_back("member-cleanup-cfg-slice");
    }
    if (!validation.validation_ready) {
        blockers.push_back("member-cleanup-edit-script-validation");
    }
    if (!staged_apply_plan.staged_apply_ready) {
        blockers.push_back("member-cleanup-staged-apply");
    }
    blockers.push_back("member-cleanup-module-mutation");
    blockers.push_back("production-member-cleanup");

    return RuntimeIndexedMemberCleanupModuleMutationGate {
        .owner_name = slice.owner_name,
        .index_expression_text = slice.index_expression_text,
        .element_source_type_name = slice.element_source_type_name,
        .moved_source_type_name = slice.moved_source_type_name,
        .moved_member_path = slice.moved_member_path,
        .insertion_anchor = slice.insertion_anchor,
        .entry_block_name = slice.entry_block_name,
        .skip_block_name = slice.skip_block_name,
        .sibling_drop_block_name = slice.sibling_drop_block_name,
        .preserve_block_name = slice.preserve_block_name,
        .exit_block_name = slice.exit_block_name,
        .blockers = std::move(blockers),
        .cfg_slice_ready = slice.slice_rendered,
        .edit_script_validation_ready = validation.validation_ready,
        .staged_apply_ready = staged_apply_plan.staged_apply_ready,
        .module_mutation_enabled = false,
        .production_member_cleanup_enabled = false,
        .prerequisites_met = false,
        .production_enabled = false,
        .source_line = slice.source_line,
    };
}

auto runtime_indexed_member_cleanup_module_mutation_gate_report(
    RuntimeIndexedMemberCleanupModuleMutationGate const& gate
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup module-mutation-gate owner " << gate.owner_name
           << " index " << gate.index_expression_text
           << " element " << gate.element_source_type_name
           << " moved " << gate.moved_source_type_name
           << " member-path " << dotted_path(gate.moved_member_path)
           << " anchor " << (gate.insertion_anchor.empty() ? "missing" : gate.insertion_anchor)
           << " entry " << (gate.entry_block_name.empty() ? "missing" : gate.entry_block_name)
           << " skip " << (gate.skip_block_name.empty() ? "missing" : gate.skip_block_name)
           << " sibling-drop "
           << (gate.sibling_drop_block_name.empty() ? "missing" : gate.sibling_drop_block_name)
           << " preserve " << (gate.preserve_block_name.empty() ? "missing" : gate.preserve_block_name)
           << " exit " << (gate.exit_block_name.empty() ? "missing" : gate.exit_block_name)
           << " cfg-slice " << (gate.cfg_slice_ready ? "ready" : "missing")
           << " edit-script-validation "
           << (gate.edit_script_validation_ready ? "ready" : "missing")
           << " staged-apply " << (gate.staged_apply_ready ? "ready" : "missing")
           << " module-mutation " << (gate.module_mutation_enabled ? "enabled" : "disabled")
           << " production-member-cleanup "
           << (gate.production_member_cleanup_enabled ? "enabled" : "disabled")
           << " prerequisites " << (gate.prerequisites_met ? "met" : "missing")
           << " production " << (gate.production_enabled ? "enabled" : "disabled")
           << " blockers " << gate.blockers.size();
    for (auto const& blocker : gate.blockers) {
        report << " blocker " << blocker;
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_module_mutation_gate_diagnostics(
    RuntimeIndexedMemberCleanupModuleMutationGate const& gate
) -> std::vector<std::string> {
    auto diagnostics = std::vector<std::string> {};
    for (auto const& blocker : gate.blockers) {
        auto diagnostic = std::ostringstream {};
        diagnostic << "runtime-index member cleanup module-mutation diagnostic owner "
                   << gate.owner_name
                   << " index " << gate.index_expression_text
                   << " element " << gate.element_source_type_name
                   << " moved " << gate.moved_source_type_name
                   << " member-path " << dotted_path(gate.moved_member_path)
                   << " blocker " << blocker
                   << " detail ";
        if (blocker == "member-cleanup-cfg-slice") {
            diagnostic << "member cleanup module mutation is blocked by missing CFG slice";
        } else if (blocker == "member-cleanup-edit-script-validation") {
            diagnostic << "member cleanup module mutation is blocked by edit script validation";
        } else if (blocker == "member-cleanup-staged-apply") {
            diagnostic << "member cleanup module mutation is blocked by staged apply readiness";
        } else if (blocker == "member-cleanup-module-mutation") {
            diagnostic << "member cleanup module mutation is disabled";
        } else if (blocker == "production-member-cleanup") {
            diagnostic << "production member cleanup is disabled";
        } else {
            diagnostic << "member cleanup module mutation is blocked";
        }
        diagnostics.push_back(diagnostic.str());
    }
    return diagnostics;
}

auto runtime_indexed_member_cleanup_production_readiness(
    RuntimeIndexedMemberCleanupProof const& proof,
    std::vector<RuntimeIndexedMemberCleanupTarget> const& targets,
    std::vector<RuntimeIndexedMemberCleanupHelperDropBindings> const& helper_drop_bindings,
    RuntimeIndexedMemberCleanupCfgSlice const& slice,
    RuntimeIndexedMemberCleanupModuleMutationGate const& gate
) -> RuntimeIndexedMemberCleanupProductionReadiness {
    auto target_metadata_ready = !targets.empty();
    for (auto const& target : targets) {
        target_metadata_ready = target_metadata_ready && target.metadata_ready;
    }
    auto const helper_drop_bindings_required = !proof.moved_member_path.empty();
    auto helper_drop_bindings_ready = !helper_drop_bindings_required;
    if (helper_drop_bindings_required) {
        helper_drop_bindings_ready = target_metadata_ready && !helper_drop_bindings.empty();
        for (auto const& bindings : helper_drop_bindings) {
            helper_drop_bindings_ready = helper_drop_bindings_ready &&
                bindings.helper_definition_ready &&
                bindings.all_drop_definitions_available &&
                !bindings.helper_symbol_name.empty();
        }
    }

    auto proof_ready = proof.prerequisites_met && proof.member_scope_proven;
    auto cfg_slice_ready = slice.slice_rendered;
    auto module_mutation_ready = gate.module_mutation_enabled && gate.prerequisites_met;
    auto production_member_cleanup_ready = gate.production_member_cleanup_enabled;
    auto production_ready =
        proof_ready &&
        target_metadata_ready &&
        helper_drop_bindings_ready &&
        cfg_slice_ready &&
        module_mutation_ready &&
        production_member_cleanup_ready;
    auto production_gate_ready = production_ready;
    production_ready = production_gate_ready && gate.production_enabled;

    auto blockers = std::vector<std::string> {};
    if (!proof_ready) {
        blockers.push_back("member-cleanup-proof");
    }
    if (!target_metadata_ready) {
        blockers.push_back("member-drop-metadata");
    }
    if (helper_drop_bindings_required && !helper_drop_bindings_ready) {
        blockers.push_back("member-helper-drop-bindings");
    }
    if (!cfg_slice_ready) {
        blockers.push_back("member-cleanup-cfg-slice");
    }
    if (!module_mutation_ready) {
        blockers.push_back("member-cleanup-module-mutation");
    }
    if (!production_member_cleanup_ready) {
        blockers.push_back("production-member-cleanup");
    }

    return RuntimeIndexedMemberCleanupProductionReadiness {
        .owner_name = proof.owner_name,
        .index_expression_text = proof.index_expression_text,
        .element_source_type_name = proof.element_source_type_name,
        .moved_source_type_name = proof.moved_source_type_name,
        .moved_member_path = proof.moved_member_path,
        .blockers = std::move(blockers),
        .proof_ready = proof_ready,
        .target_metadata_ready = target_metadata_ready,
        .helper_drop_bindings_ready = helper_drop_bindings_ready,
        .cfg_slice_ready = cfg_slice_ready,
        .module_mutation_ready = module_mutation_ready,
        .production_member_cleanup_ready = production_member_cleanup_ready,
        .production_gate_ready = production_gate_ready,
        .production_enabled = gate.production_enabled,
        .production_ready = production_ready,
        .source_line = proof.source_line,
    };
}

auto runtime_indexed_member_cleanup_production_readiness_report(
    RuntimeIndexedMemberCleanupProductionReadiness const& readiness
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup production-readiness owner " << readiness.owner_name
           << " index " << readiness.index_expression_text
           << " element " << readiness.element_source_type_name
           << " moved " << readiness.moved_source_type_name
           << " member-path " << dotted_path(readiness.moved_member_path)
           << " proof " << (readiness.proof_ready ? "ready" : "missing")
           << " target-metadata " << (readiness.target_metadata_ready ? "ready" : "missing")
           << " helper-drop-bindings "
           << (readiness.helper_drop_bindings_ready ? "ready" : "missing")
           << " cfg-slice " << (readiness.cfg_slice_ready ? "ready" : "missing")
           << " module-mutation " << (readiness.module_mutation_ready ? "ready" : "blocked")
           << " production-member-cleanup "
           << (readiness.production_member_cleanup_ready ? "ready" : "blocked")
           << " production-gate " << (readiness.production_gate_ready ? "ready" : "blocked")
           << " production-enabled " << (readiness.production_enabled ? "true" : "false")
           << " production " << (readiness.production_ready ? "ready" : "blocked")
           << " blockers " << readiness.blockers.size();
    for (auto const& blocker : readiness.blockers) {
        report << " blocker " << blocker;
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_production_blocker_diagnostics(
    RuntimeIndexedMemberCleanupProductionReadiness const& readiness
) -> std::vector<std::string> {
    auto diagnostics = std::vector<std::string> {};
    for (auto const& blocker : readiness.blockers) {
        auto diagnostic = std::ostringstream {};
        diagnostic << "runtime-index member cleanup production blocker owner " << readiness.owner_name
                   << " index " << readiness.index_expression_text
                   << " element " << readiness.element_source_type_name
                   << " moved " << readiness.moved_source_type_name
                   << " member-path " << dotted_path(readiness.moved_member_path);
        append_source_line(diagnostic, readiness.source_line);
        diagnostic << " blocker " << blocker
                   << " detail ";
        if (blocker == "member-cleanup-proof") {
            diagnostic << "member cleanup proof is missing";
        } else if (blocker == "member-drop-metadata") {
            diagnostic << "member Drop metadata is missing";
        } else if (blocker == "member-helper-drop-bindings") {
            diagnostic << "member cleanup helper Drop bindings are missing";
        } else if (blocker == "member-cleanup-cfg-slice") {
            diagnostic << "member cleanup CFG slice is missing";
        } else if (blocker == "member-cleanup-module-mutation") {
            diagnostic << "member cleanup module mutation is disabled";
        } else if (blocker == "production-member-cleanup") {
            diagnostic << "production member cleanup is disabled";
        } else {
            diagnostic << "unknown blocker";
        }
        diagnostics.push_back(diagnostic.str());
    }
    return diagnostics;
}

auto runtime_indexed_member_cleanup_helper_drop_bindings_report(
    RuntimeIndexedMemberCleanupHelperDropBindings const& bindings
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup helper-drop-bindings owner " << bindings.owner_name
           << " index " << bindings.index_expression_text
           << " element " << bindings.element_source_type_name
           << " moved " << bindings.moved_source_type_name
           << " member-path " << dotted_path(bindings.moved_member_path)
           << " helper " << (bindings.helper_symbol_name.empty() ? "missing" : bindings.helper_symbol_name)
           << " sibling-bindings " << bindings.sibling_binding_count
           << " drop-definitions " << (bindings.all_drop_definitions_available ? "ready" : "missing")
           << " nested-path " << (bindings.nested_member_path ? "true" : "false")
           << " helper-definition " << (bindings.helper_definition_ready ? "ready" : "blocked")
           << " production " << (bindings.production_enabled ? "enabled" : "disabled");
    return report.str();
}

auto runtime_indexed_member_cleanup_promotion_checklist(
    RuntimeIndexedMemberCleanupFunctionRewriteCandidate const& candidate,
    RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan const& edit_script_plan,
    RuntimeIndexedMemberCleanupFunctionRewriteEditScriptValidation const& validation,
    RuntimeIndexedMemberCleanupFunctionRewriteStagedApplyPlan const& staged_apply_plan,
    RuntimeIndexedMemberCleanupModuleMutationGate const& mutation_gate,
    RuntimeIndexedMemberCleanupProductionReadiness const& production_readiness
) -> RuntimeIndexedMemberCleanupPromotionChecklist {
    auto blockers = std::vector<std::string> {};
    auto add_blocker = [&](std::string const& blocker) {
        if (std::find(blockers.begin(), blockers.end(), blocker) == blockers.end()) {
            blockers.push_back(blocker);
        }
    };
    if (!candidate.candidate_verified) {
        add_blocker("member-cleanup-rewrite-candidate");
    }
    if (!edit_script_plan.edit_script_ready) {
        add_blocker("member-cleanup-edit-script");
    }
    if (!validation.validation_ready) {
        add_blocker("member-cleanup-edit-script-validation");
    }
    if (!staged_apply_plan.staged_apply_ready) {
        add_blocker("member-cleanup-staged-apply");
    }
    for (auto const& blocker : mutation_gate.blockers) {
        add_blocker(blocker);
    }
    for (auto const& blocker : production_readiness.blockers) {
        add_blocker(blocker);
    }

    auto module_mutation_ready =
        mutation_gate.module_mutation_enabled &&
        mutation_gate.prerequisites_met &&
        mutation_gate.production_enabled;
    auto production_readiness_ready = production_readiness.production_ready;
    auto promotion_ready =
        candidate.candidate_verified &&
        edit_script_plan.edit_script_ready &&
        validation.validation_ready &&
        staged_apply_plan.staged_apply_ready &&
        module_mutation_ready &&
        production_readiness_ready;

    return RuntimeIndexedMemberCleanupPromotionChecklist {
        .owner_name = candidate.owner_name,
        .index_expression_text = candidate.index_expression_text,
        .element_source_type_name = candidate.element_source_type_name,
        .moved_source_type_name = candidate.moved_source_type_name,
        .moved_member_path = candidate.moved_member_path,
        .blockers = std::move(blockers),
        .rewrite_candidate_ready = candidate.candidate_verified,
        .edit_script_ready = edit_script_plan.edit_script_ready,
        .validation_ready = validation.validation_ready,
        .staged_apply_ready = staged_apply_plan.staged_apply_ready,
        .module_mutation_ready = module_mutation_ready,
        .production_readiness_ready = production_readiness_ready,
        .promotion_ready = promotion_ready,
        .report_only = true,
        .production_enabled = false,
        .source_line = candidate.source_line,
    };
}

auto runtime_indexed_member_cleanup_promotion_checklist_report(
    RuntimeIndexedMemberCleanupPromotionChecklist const& checklist
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup promotion-checklist owner " << checklist.owner_name
           << " index " << checklist.index_expression_text
           << " element " << checklist.element_source_type_name
           << " moved " << checklist.moved_source_type_name
           << " member-path " << dotted_path(checklist.moved_member_path)
           << " candidate " << (checklist.rewrite_candidate_ready ? "ready" : "blocked")
           << " edit-script " << (checklist.edit_script_ready ? "ready" : "blocked")
           << " validation " << (checklist.validation_ready ? "ready" : "blocked")
           << " staged-apply " << (checklist.staged_apply_ready ? "ready" : "blocked")
           << " module-mutation " << (checklist.module_mutation_ready ? "ready" : "blocked")
           << " production-readiness "
           << (checklist.production_readiness_ready ? "ready" : "blocked")
           << " promotion " << (checklist.promotion_ready ? "ready" : "blocked")
           << " report-only " << (checklist.report_only ? "true" : "false")
           << " production " << (checklist.production_enabled ? "enabled" : "disabled")
           << " blockers " << checklist.blockers.size();
    for (auto const& blocker : checklist.blockers) {
        report << " blocker " << blocker;
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_typed_promotion_gate(
    RuntimeIndexedMemberCleanupPromotionChecklist const& checklist,
    bool ir_mutation_requested,
    bool production_gate_requested
) -> RuntimeIndexedMemberCleanupTypedPromotionGate {
    auto blockers = checklist.blockers;
    auto append_blocker = [&blockers](std::string const& blocker) {
        if (!std::ranges::contains(blockers, blocker)) {
            blockers.push_back(blocker);
        }
    };

    auto checklist_ready =
        checklist.rewrite_candidate_ready &&
        checklist.edit_script_ready &&
        checklist.validation_ready &&
        checklist.staged_apply_ready;
    auto ir_mutation_enabled = checklist_ready && ir_mutation_requested;
    auto production_gate_enabled = checklist_ready && production_gate_requested;
    auto gate_ready = checklist_ready && ir_mutation_enabled && production_gate_enabled;

    if (!checklist_ready) {
        append_blocker("member-cleanup-promotion-checklist");
    }
    if (!ir_mutation_enabled) {
        append_blocker("member-cleanup-ir-mutation");
    } else {
        std::erase(blockers, "member-cleanup-ir-mutation");
    }
    if (!production_gate_enabled) {
        append_blocker("production-member-cleanup-ir-mutation");
    } else {
        std::erase(blockers, "production-member-cleanup-ir-mutation");
    }
    if (ir_mutation_enabled && production_gate_enabled) {
        std::erase(blockers, "member-cleanup-module-mutation");
        std::erase(blockers, "production-member-cleanup");
    }

    return RuntimeIndexedMemberCleanupTypedPromotionGate {
        .owner_name = checklist.owner_name,
        .index_expression_text = checklist.index_expression_text,
        .element_source_type_name = checklist.element_source_type_name,
        .moved_source_type_name = checklist.moved_source_type_name,
        .moved_member_path = checklist.moved_member_path,
        .blockers = std::move(blockers),
        .checklist_ready = checklist_ready,
        .ir_mutation_requested = ir_mutation_requested,
        .production_gate_requested = production_gate_requested,
        .ir_mutation_enabled = ir_mutation_enabled,
        .production_gate_enabled = production_gate_enabled,
        .gate_ready = gate_ready,
        .report_only = !gate_ready,
        .production_enabled = gate_ready,
        .source_line = checklist.source_line,
    };
}

auto runtime_indexed_member_cleanup_typed_promotion_gate_report(
    RuntimeIndexedMemberCleanupTypedPromotionGate const& gate
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup typed-promotion-gate owner " << gate.owner_name
           << " index " << gate.index_expression_text
           << " element " << gate.element_source_type_name
           << " moved " << gate.moved_source_type_name
           << " member-path " << dotted_path(gate.moved_member_path)
           << " checklist " << (gate.checklist_ready ? "ready" : "blocked")
           << " ir-mutation-requested " << (gate.ir_mutation_requested ? "true" : "false")
           << " production-gate-requested " << (gate.production_gate_requested ? "true" : "false")
           << " ir-mutation " << (gate.ir_mutation_enabled ? "enabled" : "disabled")
           << " production-gate " << (gate.production_gate_enabled ? "enabled" : "disabled")
           << " gate " << (gate.gate_ready ? "ready" : "blocked")
           << " report-only " << (gate.report_only ? "true" : "false")
           << " production " << (gate.production_enabled ? "enabled" : "disabled")
           << " blockers " << gate.blockers.size();
    for (auto const& blocker : gate.blockers) {
        report << " blocker " << blocker;
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_promotion_seam(
    RuntimeIndexedMemberCleanupTypedPromotionGate const& gate
) -> RuntimeIndexedMemberCleanupPromotionSeam {
    auto blockers = gate.blockers;
    auto add_blocker = [&](std::string const& blocker) {
        if (std::find(blockers.begin(), blockers.end(), blocker) == blockers.end()) {
            blockers.push_back(blocker);
        }
    };

    auto mutation_seam_selected = gate.checklist_ready;
    if (!gate.ir_mutation_enabled) {
        add_blocker("member-cleanup-ir-mutation");
    }
    if (!gate.production_gate_enabled) {
        add_blocker("production-member-cleanup-ir-mutation");
    }

    return RuntimeIndexedMemberCleanupPromotionSeam {
        .owner_name = gate.owner_name,
        .index_expression_text = gate.index_expression_text,
        .element_source_type_name = gate.element_source_type_name,
        .moved_source_type_name = gate.moved_source_type_name,
        .moved_member_path = gate.moved_member_path,
        .blockers = std::move(blockers),
        .checklist_ready = gate.checklist_ready,
        .mutation_seam_selected = mutation_seam_selected,
        .ir_mutation_enabled = gate.ir_mutation_enabled,
        .production_gate_enabled = gate.production_gate_enabled,
        .promotion_ready = gate.gate_ready,
        .report_only = gate.report_only,
        .production_enabled = gate.production_enabled,
        .source_line = gate.source_line,
    };
}

auto runtime_indexed_member_cleanup_promotion_seam(
    RuntimeIndexedMemberCleanupPromotionChecklist const& checklist
) -> RuntimeIndexedMemberCleanupPromotionSeam {
    return runtime_indexed_member_cleanup_promotion_seam(
        runtime_indexed_member_cleanup_typed_promotion_gate(checklist)
    );
}

auto runtime_indexed_member_cleanup_promotion_seam_report(
    RuntimeIndexedMemberCleanupPromotionSeam const& seam
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup promotion-seam owner " << seam.owner_name
           << " index " << seam.index_expression_text
           << " element " << seam.element_source_type_name
           << " moved " << seam.moved_source_type_name
           << " member-path " << dotted_path(seam.moved_member_path)
           << " checklist " << (seam.checklist_ready ? "ready" : "blocked")
           << " mutation-seam " << (seam.mutation_seam_selected ? "selected" : "blocked")
           << " ir-mutation " << (seam.ir_mutation_enabled ? "enabled" : "disabled")
           << " production-gate " << (seam.production_gate_enabled ? "enabled" : "disabled")
           << " promotion " << (seam.promotion_ready ? "ready" : "blocked")
           << " report-only " << (seam.report_only ? "true" : "false")
           << " production " << (seam.production_enabled ? "enabled" : "disabled")
           << " blockers " << seam.blockers.size();
    for (auto const& blocker : seam.blockers) {
        report << " blocker " << blocker;
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_mutation_operation_plan(
    RuntimeIndexedMemberCleanupPromotionSeam const& seam,
    RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan const& edit_script_plan
) -> RuntimeIndexedMemberCleanupMutationOperationPlan {
    auto operations = std::vector<RuntimeIndexedMemberCleanupMutationOperation> {};
    if (seam.mutation_seam_selected) {
        operations.push_back(RuntimeIndexedMemberCleanupMutationOperation {
            .kind = "branch-replacement",
            .anchor = edit_script_plan.insertion_anchor,
            .expected_text = edit_script_plan.expected_branch_text,
            .replacement_text = edit_script_plan.replacement_branch_text,
            .ready = edit_script_plan.branch_replacement_ready,
            .applied = false,
        });
        operations.push_back(RuntimeIndexedMemberCleanupMutationOperation {
            .kind = "cfg-append",
            .anchor = edit_script_plan.exit_block_name,
            .expected_text = edit_script_plan.expected_closing_text,
            .placement = edit_script_plan.cleanup_cfg_append_placement,
            .ready = edit_script_plan.cleanup_cfg_append_ready,
            .applied = false,
        });
        operations.push_back(RuntimeIndexedMemberCleanupMutationOperation {
            .kind = "phi-retarget",
            .anchor = edit_script_plan.exit_block_name,
            .old_predecessor = edit_script_plan.phi_old_predecessor_block_name,
            .new_predecessor = edit_script_plan.phi_new_predecessor_block_name,
            .ready = edit_script_plan.phi_retarget_ready,
            .applied = false,
        });
    }

    auto operations_ready = !operations.empty();
    auto operations_applied = !operations.empty();
    for (auto const& operation : operations) {
        operations_ready = operations_ready && operation.ready;
        operations_applied = operations_applied && operation.applied;
    }

    return RuntimeIndexedMemberCleanupMutationOperationPlan {
        .owner_name = seam.owner_name,
        .index_expression_text = seam.index_expression_text,
        .element_source_type_name = seam.element_source_type_name,
        .moved_source_type_name = seam.moved_source_type_name,
        .moved_member_path = seam.moved_member_path,
        .operations = std::move(operations),
        .blockers = seam.blockers,
        .seam_selected = seam.mutation_seam_selected,
        .operations_ready = operations_ready,
        .operations_applied = operations_applied,
        .report_only = true,
        .production_enabled = false,
        .source_line = seam.source_line,
    };
}

auto runtime_indexed_member_cleanup_mutation_operation_plan_report(
    RuntimeIndexedMemberCleanupMutationOperationPlan const& plan
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup mutation-operation-plan owner " << plan.owner_name
           << " index " << plan.index_expression_text
           << " element " << plan.element_source_type_name
           << " moved " << plan.moved_source_type_name
           << " member-path " << dotted_path(plan.moved_member_path);
    append_source_line(report, plan.source_line);
    report << " seam " << (plan.seam_selected ? "selected" : "blocked")
           << " operations " << plan.operations.size()
           << " operations-ready " << (plan.operations_ready ? "ready" : "blocked")
           << " operations-applied " << (plan.operations_applied ? "true" : "false")
           << " report-only " << (plan.report_only ? "true" : "false")
           << " production " << (plan.production_enabled ? "enabled" : "disabled")
           << " blockers " << plan.blockers.size();
    for (auto const& blocker : plan.blockers) {
        report << " blocker " << blocker;
    }
    for (auto const& operation : plan.operations) {
        report << " operation " << operation.kind
               << " ready " << (operation.ready ? "true" : "false")
               << " applied " << (operation.applied ? "true" : "false")
               << " anchor " << (operation.anchor.empty() ? "missing" : operation.anchor)
               << " expected " << (operation.expected_text.empty() ? "missing" : operation.expected_text)
               << " replacement " << (operation.replacement_text.empty() ? "missing" : operation.replacement_text)
               << " placement " << (operation.placement.empty() ? "missing" : operation.placement)
               << " old-pred " << (operation.old_predecessor.empty() ? "missing" : operation.old_predecessor)
               << " new-pred " << (operation.new_predecessor.empty() ? "missing" : operation.new_predecessor);
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_mutation_operation_validation(
    RuntimeIndexedMemberCleanupMutationOperationPlan const& plan
) -> RuntimeIndexedMemberCleanupMutationOperationValidation {
    auto operation_count_valid = plan.operations.size() == 3;
    auto operation_order_valid =
        operation_count_valid &&
        plan.operations[0].kind == "branch-replacement" &&
        plan.operations[1].kind == "cfg-append" &&
        plan.operations[2].kind == "phi-retarget";

    auto branch_replacement_fields_valid = false;
    auto cfg_append_fields_valid = false;
    auto phi_retarget_fields_valid = false;
    auto no_operations_applied = !plan.operations.empty();
    for (auto const& operation : plan.operations) {
        no_operations_applied = no_operations_applied && !operation.applied;
    }

    if (operation_order_valid) {
        auto const& branch_replacement = plan.operations[0];
        branch_replacement_fields_valid =
            branch_replacement.ready &&
            !branch_replacement.anchor.empty() &&
            !branch_replacement.expected_text.empty() &&
            !branch_replacement.replacement_text.empty();

        auto const& cfg_append = plan.operations[1];
        cfg_append_fields_valid =
            cfg_append.ready &&
            !cfg_append.anchor.empty() &&
            !cfg_append.expected_text.empty() &&
            !cfg_append.placement.empty();

        auto const& phi_retarget = plan.operations[2];
        phi_retarget_fields_valid =
            phi_retarget.ready &&
            !phi_retarget.anchor.empty() &&
            !phi_retarget.old_predecessor.empty() &&
            !phi_retarget.new_predecessor.empty();
    }

    auto validation_ready =
        plan.seam_selected &&
        operation_count_valid &&
        operation_order_valid &&
        branch_replacement_fields_valid &&
        cfg_append_fields_valid &&
        phi_retarget_fields_valid &&
        plan.operations_ready &&
        no_operations_applied;

    return RuntimeIndexedMemberCleanupMutationOperationValidation {
        .owner_name = plan.owner_name,
        .index_expression_text = plan.index_expression_text,
        .element_source_type_name = plan.element_source_type_name,
        .moved_source_type_name = plan.moved_source_type_name,
        .moved_member_path = plan.moved_member_path,
        .blockers = plan.blockers,
        .seam_selected = plan.seam_selected,
        .operation_count_valid = operation_count_valid,
        .operation_order_valid = operation_order_valid,
        .branch_replacement_fields_valid = branch_replacement_fields_valid,
        .cfg_append_fields_valid = cfg_append_fields_valid,
        .phi_retarget_fields_valid = phi_retarget_fields_valid,
        .operations_ready = plan.operations_ready,
        .no_operations_applied = no_operations_applied,
        .validation_ready = validation_ready,
        .report_only = true,
        .production_enabled = false,
        .source_line = plan.source_line,
    };
}

auto runtime_indexed_member_cleanup_mutation_operation_validation_report(
    RuntimeIndexedMemberCleanupMutationOperationValidation const& validation
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup mutation-operation-validation owner " << validation.owner_name
           << " index " << validation.index_expression_text
           << " element " << validation.element_source_type_name
           << " moved " << validation.moved_source_type_name
           << " member-path " << dotted_path(validation.moved_member_path);
    append_source_line(report, validation.source_line);
    report << " seam " << (validation.seam_selected ? "selected" : "blocked")
           << " count " << (validation.operation_count_valid ? "valid" : "blocked")
           << " order " << (validation.operation_order_valid ? "valid" : "blocked")
           << " branch-replacement-fields "
           << (validation.branch_replacement_fields_valid ? "valid" : "blocked")
           << " cfg-append-fields " << (validation.cfg_append_fields_valid ? "valid" : "blocked")
           << " phi-retarget-fields " << (validation.phi_retarget_fields_valid ? "valid" : "blocked")
           << " operations-ready " << (validation.operations_ready ? "ready" : "blocked")
           << " no-operations-applied " << (validation.no_operations_applied ? "true" : "false")
           << " validation " << (validation.validation_ready ? "ready" : "blocked")
           << " report-only " << (validation.report_only ? "true" : "false")
           << " production " << (validation.production_enabled ? "enabled" : "disabled")
           << " blockers " << validation.blockers.size();
    for (auto const& blocker : validation.blockers) {
        report << " blocker " << blocker;
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_mutation_conflict_detection(
    RuntimeIndexedMemberCleanupMutationOperationPlan const& plan,
    RuntimeIndexedMemberCleanupMutationOperationValidation const& validation
) -> RuntimeIndexedMemberCleanupMutationConflictDetection {
    auto branch_anchor_match_count = 0;
    auto closing_anchor_match_count = 0;
    auto phi_predecessor_match_count = 0;

    if (validation.validation_ready && plan.operations.size() == 3) {
        auto const& branch_replacement = plan.operations[0];
        auto const& cfg_append = plan.operations[1];
        auto const& phi_retarget = plan.operations[2];
        if (!branch_replacement.anchor.empty() && !branch_replacement.expected_text.empty()) {
            branch_anchor_match_count = 1;
        }
        if (!cfg_append.anchor.empty() && !cfg_append.expected_text.empty()) {
            closing_anchor_match_count = 1;
        }
        if (!phi_retarget.anchor.empty() && !phi_retarget.old_predecessor.empty()) {
            phi_predecessor_match_count = 1;
        }
    }

    auto branch_anchor_unique = branch_anchor_match_count == 1;
    auto closing_anchor_unique = closing_anchor_match_count == 1;
    auto phi_predecessor_unique = phi_predecessor_match_count == 1;
    auto conflict_free =
        validation.validation_ready &&
        branch_anchor_unique &&
        closing_anchor_unique &&
        phi_predecessor_unique;

    return RuntimeIndexedMemberCleanupMutationConflictDetection {
        .owner_name = plan.owner_name,
        .index_expression_text = plan.index_expression_text,
        .element_source_type_name = plan.element_source_type_name,
        .moved_source_type_name = plan.moved_source_type_name,
        .moved_member_path = plan.moved_member_path,
        .blockers = validation.blockers,
        .validation_ready = validation.validation_ready,
        .branch_anchor_match_count = branch_anchor_match_count,
        .closing_anchor_match_count = closing_anchor_match_count,
        .phi_predecessor_match_count = phi_predecessor_match_count,
        .branch_anchor_unique = branch_anchor_unique,
        .closing_anchor_unique = closing_anchor_unique,
        .phi_predecessor_unique = phi_predecessor_unique,
        .conflict_free = conflict_free,
        .apply_allowed = false,
        .report_only = true,
        .production_enabled = false,
        .source_line = plan.source_line,
    };
}

auto runtime_indexed_member_cleanup_mutation_conflict_detection_report(
    RuntimeIndexedMemberCleanupMutationConflictDetection const& detection
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup mutation-conflict-detection owner " << detection.owner_name
           << " index " << detection.index_expression_text
           << " element " << detection.element_source_type_name
           << " moved " << detection.moved_source_type_name
           << " member-path " << dotted_path(detection.moved_member_path);
    append_source_line(report, detection.source_line);
    report << " validation " << (detection.validation_ready ? "ready" : "blocked")
           << " branch-anchor-matches " << detection.branch_anchor_match_count
           << " branch-anchor " << (detection.branch_anchor_unique ? "unique" : "blocked")
           << " closing-anchor-matches " << detection.closing_anchor_match_count
           << " closing-anchor " << (detection.closing_anchor_unique ? "unique" : "blocked")
           << " phi-predecessor-matches " << detection.phi_predecessor_match_count
           << " phi-predecessor " << (detection.phi_predecessor_unique ? "unique" : "blocked")
           << " conflict-free " << (detection.conflict_free ? "true" : "false")
           << " apply-allowed " << (detection.apply_allowed ? "true" : "false")
           << " report-only " << (detection.report_only ? "true" : "false")
           << " production " << (detection.production_enabled ? "enabled" : "disabled")
           << " blockers " << detection.blockers.size();
    for (auto const& blocker : detection.blockers) {
        report << " blocker " << blocker;
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_mutation_apply_authorization(
    RuntimeIndexedMemberCleanupMutationOperationValidation const& validation,
    RuntimeIndexedMemberCleanupMutationConflictDetection const& detection,
    bool ir_mutation_requested,
    bool production_gate_enabled,
    bool apply_authorization_requested
) -> RuntimeIndexedMemberCleanupMutationApplyAuthorization {
    auto blockers = detection.blockers;
    auto append_blocker = [&blockers](std::string blocker) {
        if (!std::ranges::contains(blockers, blocker)) {
            blockers.push_back(std::move(blocker));
        }
    };
    if (ir_mutation_requested) {
        std::erase(blockers, "member-cleanup-ir-mutation");
    }
    if (production_gate_enabled) {
        std::erase(blockers, "production-member-cleanup-ir-mutation");
    }
    if (ir_mutation_requested && production_gate_enabled) {
        std::erase(blockers, "member-cleanup-module-mutation");
        std::erase(blockers, "production-member-cleanup");
    }
    auto authorization_ready =
        validation.validation_ready &&
        detection.conflict_free &&
        ir_mutation_requested &&
        production_gate_enabled;
    auto apply_authorized = authorization_ready && apply_authorization_requested;
    if (!validation.validation_ready) {
        append_blocker("member-cleanup-mutation-validation");
    }
    if (!detection.conflict_free) {
        append_blocker("member-cleanup-mutation-conflict");
    }
    if (!ir_mutation_requested) {
        append_blocker("member-cleanup-ir-mutation");
    }
    if (!production_gate_enabled) {
        append_blocker("production-member-cleanup-ir-mutation");
    }

    return RuntimeIndexedMemberCleanupMutationApplyAuthorization {
        .owner_name = detection.owner_name,
        .index_expression_text = detection.index_expression_text,
        .element_source_type_name = detection.element_source_type_name,
        .moved_source_type_name = detection.moved_source_type_name,
        .moved_member_path = detection.moved_member_path,
        .blockers = std::move(blockers),
        .validation_ready = validation.validation_ready,
        .conflict_free = detection.conflict_free,
        .ir_mutation_requested = ir_mutation_requested,
        .production_gate_enabled = production_gate_enabled,
        .apply_authorization_requested = apply_authorization_requested,
        .authorization_ready = authorization_ready,
        .apply_authorized = apply_authorized,
        .report_only = !apply_authorized,
        .production_enabled = apply_authorized,
        .source_line = detection.source_line,
    };
}

auto runtime_indexed_member_cleanup_mutation_apply_authorization_report(
    RuntimeIndexedMemberCleanupMutationApplyAuthorization const& authorization
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup mutation-apply-authorization owner "
           << authorization.owner_name
           << " index " << authorization.index_expression_text
           << " element " << authorization.element_source_type_name
           << " moved " << authorization.moved_source_type_name
           << " member-path " << dotted_path(authorization.moved_member_path);
    append_source_line(report, authorization.source_line);
    report << " validation " << (authorization.validation_ready ? "ready" : "blocked")
           << " conflict-free " << (authorization.conflict_free ? "true" : "false")
           << " ir-mutation " << (authorization.ir_mutation_requested ? "requested" : "blocked")
           << " production-gate " << (authorization.production_gate_enabled ? "enabled" : "disabled")
           << " apply-requested " << (authorization.apply_authorization_requested ? "true" : "false")
           << " authorization " << (authorization.authorization_ready ? "ready" : "blocked")
           << " apply-authorized " << (authorization.apply_authorized ? "true" : "false")
           << " report-only " << (authorization.report_only ? "true" : "false")
           << " production " << (authorization.production_enabled ? "enabled" : "disabled")
           << " blockers " << authorization.blockers.size();
    for (auto const& blocker : authorization.blockers) {
        report << " blocker " << blocker;
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_mutation_apply_preview(
    RuntimeIndexedMemberCleanupMutationOperationPlan const& plan,
    RuntimeIndexedMemberCleanupMutationApplyAuthorization const& authorization
) -> RuntimeIndexedMemberCleanupMutationApplyPreview {
    auto actions = std::vector<RuntimeIndexedMemberCleanupMutationApplyPreviewAction> {};
    for (auto const& operation : plan.operations) {
        auto detail = std::string {};
        if (operation.kind == "branch-replacement") {
            detail = "replace `" + operation.expected_text + "` with `" + operation.replacement_text + "`";
        } else if (operation.kind == "cfg-append") {
            detail = "append cleanup CFG " + operation.placement + " before closing anchor";
        } else if (operation.kind == "phi-retarget") {
            detail = "retarget PHI predecessor " + operation.old_predecessor + " to " + operation.new_predecessor;
        } else {
            detail = "unknown mutation operation";
        }
        actions.push_back(RuntimeIndexedMemberCleanupMutationApplyPreviewAction {
            .kind = operation.kind,
            .anchor = operation.anchor,
            .detail = std::move(detail),
            .ready = operation.ready,
            .applied = authorization.apply_authorized && operation.ready,
        });
    }

    auto preview_ready = authorization.validation_ready && authorization.conflict_free && !actions.empty();
    auto actions_applied = !actions.empty();
    for (auto const& action : actions) {
        preview_ready = preview_ready && action.ready;
        actions_applied = actions_applied && action.applied;
    }

    return RuntimeIndexedMemberCleanupMutationApplyPreview {
        .owner_name = authorization.owner_name,
        .index_expression_text = authorization.index_expression_text,
        .element_source_type_name = authorization.element_source_type_name,
        .moved_source_type_name = authorization.moved_source_type_name,
        .moved_member_path = authorization.moved_member_path,
        .actions = std::move(actions),
        .blockers = authorization.blockers,
        .authorization_ready = authorization.authorization_ready,
        .apply_authorized = authorization.apply_authorized,
        .preview_ready = preview_ready,
        .actions_applied = actions_applied,
        .report_only = !actions_applied,
        .production_enabled = actions_applied,
        .source_line = authorization.source_line,
    };
}

auto runtime_indexed_member_cleanup_mutation_apply_preview_report(
    RuntimeIndexedMemberCleanupMutationApplyPreview const& preview
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup mutation-apply-preview owner " << preview.owner_name
           << " index " << preview.index_expression_text
           << " element " << preview.element_source_type_name
           << " moved " << preview.moved_source_type_name
           << " member-path " << dotted_path(preview.moved_member_path);
    append_source_line(report, preview.source_line);
    report << " authorization " << (preview.authorization_ready ? "ready" : "blocked")
           << " apply-authorized " << (preview.apply_authorized ? "true" : "false")
           << " preview " << (preview.preview_ready ? "ready" : "blocked")
           << " actions " << preview.actions.size()
           << " actions-applied " << (preview.actions_applied ? "true" : "false")
           << " report-only " << (preview.report_only ? "true" : "false")
           << " production " << (preview.production_enabled ? "enabled" : "disabled")
           << " blockers " << preview.blockers.size();
    for (auto const& blocker : preview.blockers) {
        report << " blocker " << blocker;
    }
    for (auto const& action : preview.actions) {
        report << " action " << action.kind
               << " ready " << (action.ready ? "true" : "false")
               << " applied " << (action.applied ? "true" : "false")
               << " anchor " << (action.anchor.empty() ? "missing" : action.anchor)
               << " detail " << (action.detail.empty() ? "missing" : action.detail);
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_mutation_post_apply_verification(
    RuntimeIndexedMemberCleanupMutationApplyPreview const& preview
) -> RuntimeIndexedMemberCleanupMutationPostApplyVerification {
    auto expected_checks = std::vector<std::string> {};
    for (auto const& action : preview.actions) {
        if (action.kind == "branch-replacement") {
            expected_checks.push_back("branch-target " + action.anchor);
        } else if (action.kind == "cfg-append") {
            expected_checks.push_back("cfg-appended " + action.anchor);
        } else if (action.kind == "phi-retarget") {
            expected_checks.push_back("phi-predecessor " + action.anchor);
        }
    }

    auto blockers = preview.blockers;
    auto append_blocker = [&blockers](std::string blocker) {
        if (!std::ranges::contains(blockers, blocker)) {
            blockers.push_back(std::move(blocker));
        }
    };
    auto expected_checks_ready = preview.preview_ready && expected_checks.size() == preview.actions.size();
    if (!preview.preview_ready) {
        append_blocker("member-cleanup-mutation-apply-preview");
    }
    if (!preview.apply_authorized) {
        append_blocker("member-cleanup-mutation-apply-authorization");
    }
    if (!preview.actions_applied) {
        append_blocker("member-cleanup-mutation-actions-applied");
    }

    return RuntimeIndexedMemberCleanupMutationPostApplyVerification {
        .owner_name = preview.owner_name,
        .index_expression_text = preview.index_expression_text,
        .element_source_type_name = preview.element_source_type_name,
        .moved_source_type_name = preview.moved_source_type_name,
        .moved_member_path = preview.moved_member_path,
        .expected_checks = std::move(expected_checks),
        .blockers = std::move(blockers),
        .preview_ready = preview.preview_ready,
        .apply_authorized = preview.apply_authorized,
        .actions_applied = preview.actions_applied,
        .expected_checks_ready = expected_checks_ready,
        .verification_ready = expected_checks_ready && preview.apply_authorized && preview.actions_applied,
        .report_only = !(expected_checks_ready && preview.apply_authorized && preview.actions_applied),
        .production_enabled = expected_checks_ready && preview.apply_authorized && preview.actions_applied,
        .source_line = preview.source_line,
    };
}

auto runtime_indexed_member_cleanup_mutation_post_apply_verification_report(
    RuntimeIndexedMemberCleanupMutationPostApplyVerification const& verification
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup mutation-post-apply-verification owner "
           << verification.owner_name
           << " index " << verification.index_expression_text
           << " element " << verification.element_source_type_name
           << " moved " << verification.moved_source_type_name
           << " member-path " << dotted_path(verification.moved_member_path);
    append_source_line(report, verification.source_line);
    report << " preview " << (verification.preview_ready ? "ready" : "blocked")
           << " apply-authorized " << (verification.apply_authorized ? "true" : "false")
           << " actions-applied " << (verification.actions_applied ? "true" : "false")
           << " expected-checks " << verification.expected_checks.size()
           << " expected-checks-ready " << (verification.expected_checks_ready ? "true" : "false")
           << " verification " << (verification.verification_ready ? "ready" : "blocked")
           << " report-only " << (verification.report_only ? "true" : "false")
           << " production " << (verification.production_enabled ? "enabled" : "disabled")
           << " blockers " << verification.blockers.size();
    for (auto const& blocker : verification.blockers) {
        report << " blocker " << blocker;
    }
    for (auto const& check : verification.expected_checks) {
        report << " expected-check " << check;
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_mutation_promotion_summary(
    RuntimeIndexedMemberCleanupMutationOperationPlan const& plan,
    RuntimeIndexedMemberCleanupMutationOperationValidation const& validation,
    RuntimeIndexedMemberCleanupMutationConflictDetection const& detection,
    RuntimeIndexedMemberCleanupMutationApplyAuthorization const& authorization,
    RuntimeIndexedMemberCleanupMutationApplyPreview const& preview,
    RuntimeIndexedMemberCleanupMutationPostApplyVerification const& verification
) -> RuntimeIndexedMemberCleanupMutationPromotionSummary {
    auto blockers = std::vector<std::string> {};
    auto append_blocker = [&blockers](std::string const& blocker) {
        if (!std::ranges::contains(blockers, blocker)) {
            blockers.push_back(blocker);
        }
    };
    for (auto const& blocker : plan.blockers) {
        append_blocker(blocker);
    }
    for (auto const& blocker : validation.blockers) {
        append_blocker(blocker);
    }
    for (auto const& blocker : detection.blockers) {
        append_blocker(blocker);
    }
    for (auto const& blocker : authorization.blockers) {
        append_blocker(blocker);
    }
    for (auto const& blocker : preview.blockers) {
        append_blocker(blocker);
    }
    for (auto const& blocker : verification.blockers) {
        append_blocker(blocker);
    }
    if (authorization.apply_authorized) {
        std::erase(blockers, "member-cleanup-module-mutation");
        std::erase(blockers, "production-member-cleanup");
        std::erase(blockers, "member-cleanup-ir-mutation");
        std::erase(blockers, "production-member-cleanup-ir-mutation");
    }

    auto promotion_ready =
        plan.operations_ready &&
        validation.validation_ready &&
        detection.conflict_free &&
        authorization.authorization_ready &&
        preview.preview_ready &&
        verification.verification_ready;

    return RuntimeIndexedMemberCleanupMutationPromotionSummary {
        .owner_name = verification.owner_name,
        .index_expression_text = verification.index_expression_text,
        .element_source_type_name = verification.element_source_type_name,
        .moved_source_type_name = verification.moved_source_type_name,
        .moved_member_path = verification.moved_member_path,
        .blockers = std::move(blockers),
        .operation_count = static_cast<int>(plan.operations.size()),
        .action_count = static_cast<int>(preview.actions.size()),
        .expected_check_count = static_cast<int>(verification.expected_checks.size()),
        .operations_ready = plan.operations_ready,
        .validation_ready = validation.validation_ready,
        .conflict_free = detection.conflict_free,
        .authorization_ready = authorization.authorization_ready,
        .ir_mutation_requested = authorization.ir_mutation_requested,
        .production_gate_enabled = authorization.production_gate_enabled,
        .apply_authorized = authorization.apply_authorized,
        .preview_ready = preview.preview_ready,
        .post_apply_verification_ready = verification.verification_ready,
        .promotion_ready = promotion_ready,
        .report_only = true,
        .production_enabled = false,
        .source_line = verification.source_line,
    };
}

auto runtime_indexed_member_cleanup_mutation_promotion_summary_report(
    RuntimeIndexedMemberCleanupMutationPromotionSummary const& summary
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup mutation-promotion-summary owner "
           << summary.owner_name
           << " index " << summary.index_expression_text
           << " element " << summary.element_source_type_name
           << " moved " << summary.moved_source_type_name
           << " member-path " << dotted_path(summary.moved_member_path);
    append_source_line(report, summary.source_line);
    report << " operations " << summary.operation_count
           << " operations-ready " << (summary.operations_ready ? "ready" : "blocked")
           << " validation " << (summary.validation_ready ? "ready" : "blocked")
           << " conflict-free " << (summary.conflict_free ? "true" : "false")
           << " authorization " << (summary.authorization_ready ? "ready" : "blocked")
           << " preview " << (summary.preview_ready ? "ready" : "blocked")
           << " actions " << summary.action_count
           << " post-apply-verification "
           << (summary.post_apply_verification_ready ? "ready" : "blocked")
           << " expected-checks " << summary.expected_check_count
           << " promotion " << (summary.promotion_ready ? "ready" : "blocked")
           << " report-only " << (summary.report_only ? "true" : "false")
           << " production " << (summary.production_enabled ? "enabled" : "disabled")
           << " blockers " << summary.blockers.size();
    for (auto const& blocker : summary.blockers) {
        report << " blocker " << blocker;
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_mutation_production_readiness(
    RuntimeIndexedMemberCleanupMutationPromotionSummary const& summary
) -> RuntimeIndexedMemberCleanupMutationProductionReadiness {
    auto blockers = summary.blockers;
    auto append_blocker = [&blockers](std::string const& blocker) {
        if (!std::ranges::contains(blockers, blocker)) {
            blockers.push_back(blocker);
        }
    };

    auto ir_mutation_requested = summary.ir_mutation_requested;
    auto production_gate_enabled = summary.production_gate_enabled;
    auto readiness_ready = summary.promotion_ready && ir_mutation_requested && production_gate_enabled;
    if (!summary.promotion_ready) {
        append_blocker("member-cleanup-mutation-promotion");
    }
    if (!summary.post_apply_verification_ready) {
        append_blocker("member-cleanup-mutation-post-apply-verification");
    }
    if (!summary.authorization_ready) {
        append_blocker("member-cleanup-mutation-apply-authorization");
    }
    if (!ir_mutation_requested) {
        append_blocker("member-cleanup-ir-mutation");
    }
    if (!production_gate_enabled) {
        append_blocker("production-member-cleanup-ir-mutation");
    }

    return RuntimeIndexedMemberCleanupMutationProductionReadiness {
        .owner_name = summary.owner_name,
        .index_expression_text = summary.index_expression_text,
        .element_source_type_name = summary.element_source_type_name,
        .moved_source_type_name = summary.moved_source_type_name,
        .moved_member_path = summary.moved_member_path,
        .blockers = std::move(blockers),
        .promotion_ready = summary.promotion_ready,
        .post_apply_verification_ready = summary.post_apply_verification_ready,
        .authorization_ready = summary.authorization_ready,
        .ir_mutation_requested = ir_mutation_requested,
        .production_gate_enabled = production_gate_enabled,
        .readiness_ready = readiness_ready,
        .report_only = !readiness_ready,
        .production_enabled = readiness_ready,
        .source_line = summary.source_line,
    };
}

auto runtime_indexed_member_cleanup_mutation_production_readiness_report(
    RuntimeIndexedMemberCleanupMutationProductionReadiness const& readiness
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup mutation-production-readiness owner "
           << readiness.owner_name
           << " index " << readiness.index_expression_text
           << " element " << readiness.element_source_type_name
           << " moved " << readiness.moved_source_type_name
           << " member-path " << dotted_path(readiness.moved_member_path);
    append_source_line(report, readiness.source_line);
    report << " promotion " << (readiness.promotion_ready ? "ready" : "blocked")
           << " post-apply-verification "
           << (readiness.post_apply_verification_ready ? "ready" : "blocked")
           << " authorization " << (readiness.authorization_ready ? "ready" : "blocked")
           << " ir-mutation " << (readiness.ir_mutation_requested ? "requested" : "blocked")
           << " production-gate " << (readiness.production_gate_enabled ? "enabled" : "disabled")
           << " readiness " << (readiness.readiness_ready ? "ready" : "blocked")
           << " report-only " << (readiness.report_only ? "true" : "false")
           << " production " << (readiness.production_enabled ? "enabled" : "disabled")
           << " blockers " << readiness.blockers.size();
    for (auto const& blocker : readiness.blockers) {
        report << " blocker " << blocker;
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_mutation_production_readiness_diagnostics(
    RuntimeIndexedMemberCleanupMutationProductionReadiness const& readiness
) -> std::vector<std::string> {
    auto diagnostics = std::vector<std::string> {};
    for (auto const& blocker : readiness.blockers) {
        auto diagnostic = std::ostringstream {};
        diagnostic << "runtime-index member cleanup mutation production blocker owner "
                   << readiness.owner_name
                   << " index " << readiness.index_expression_text
                   << " element " << readiness.element_source_type_name
                   << " moved " << readiness.moved_source_type_name
                   << " member-path " << dotted_path(readiness.moved_member_path);
        append_source_line(diagnostic, readiness.source_line);
        diagnostic << " blocker " << blocker
                   << " detail ";
        if (blocker == "member-cleanup-rewrite-candidate") {
            diagnostic << "member cleanup rewrite candidate is missing";
        } else if (blocker == "member-cleanup-edit-script") {
            diagnostic << "member cleanup edit script is not ready";
        } else if (blocker == "member-cleanup-edit-script-validation") {
            diagnostic << "member cleanup edit script validation is blocked";
        } else if (blocker == "member-cleanup-staged-apply") {
            diagnostic << "member cleanup staged apply is blocked";
        } else if (blocker == "member-cleanup-cfg-slice") {
            diagnostic << "member cleanup CFG slice is missing";
        } else if (blocker == "member-cleanup-module-mutation") {
            diagnostic << "member cleanup module mutation is disabled";
        } else if (blocker == "production-member-cleanup") {
            diagnostic << "production member cleanup is disabled";
        } else if (blocker == "member-cleanup-proof") {
            diagnostic << "member cleanup proof is missing";
        } else if (blocker == "member-drop-metadata") {
            diagnostic << "member Drop metadata is missing";
        } else if (blocker == "member-cleanup-ir-mutation") {
            diagnostic << "member cleanup IR mutation is disabled";
        } else if (blocker == "production-member-cleanup-ir-mutation") {
            diagnostic << "production member cleanup IR mutation is disabled";
        } else if (blocker == "member-cleanup-mutation-validation") {
            diagnostic << "member cleanup mutation validation is blocked";
        } else if (blocker == "member-cleanup-mutation-conflict") {
            diagnostic << "member cleanup mutation conflict detection is blocked";
        } else if (blocker == "member-cleanup-mutation-apply-preview") {
            diagnostic << "member cleanup mutation apply preview is blocked";
        } else if (blocker == "member-cleanup-mutation-apply-authorization") {
            diagnostic << "member cleanup mutation apply authorization is blocked";
        } else if (blocker == "member-cleanup-mutation-actions-applied") {
            diagnostic << "member cleanup mutation actions are not applied";
        } else if (blocker == "member-cleanup-mutation-promotion") {
            diagnostic << "member cleanup mutation promotion is blocked";
        } else if (blocker == "member-cleanup-mutation-post-apply-verification") {
            diagnostic << "member cleanup mutation post-apply verification is blocked";
        } else {
            diagnostic << "member cleanup mutation production readiness is blocked";
        }
        diagnostics.push_back(diagnostic.str());
    }
    return diagnostics;
}

auto runtime_indexed_member_cleanup_mutation_readiness_verdict(
    RuntimeIndexedMemberCleanupMutationProductionReadiness const& readiness
) -> RuntimeIndexedMemberCleanupMutationReadinessVerdict {
    auto const blocker_count = static_cast<int>(readiness.blockers.size());
    return RuntimeIndexedMemberCleanupMutationReadinessVerdict {
        .owner_name = readiness.owner_name,
        .index_expression_text = readiness.index_expression_text,
        .element_source_type_name = readiness.element_source_type_name,
        .moved_source_type_name = readiness.moved_source_type_name,
        .moved_member_path = readiness.moved_member_path,
        .blocker_count = blocker_count,
        .diagnostic_count = blocker_count,
        .readiness_ready = readiness.readiness_ready,
        .guarded_rewrite_ready = readiness.readiness_ready && readiness.ir_mutation_requested &&
            readiness.production_gate_enabled,
        .report_only = true,
        .production_enabled = false,
        .source_line = readiness.source_line,
    };
}

auto runtime_indexed_member_cleanup_mutation_readiness_verdict_report(
    RuntimeIndexedMemberCleanupMutationReadinessVerdict const& verdict
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup mutation readiness verdict owner "
           << verdict.owner_name
           << " index " << verdict.index_expression_text
           << " element " << verdict.element_source_type_name
           << " moved " << verdict.moved_source_type_name
           << " member-path " << dotted_path(verdict.moved_member_path);
    append_source_line(report, verdict.source_line);
    report << " readiness " << (verdict.readiness_ready ? "ready" : "blocked")
           << " guarded-rewrite " << (verdict.guarded_rewrite_ready ? "ready" : "blocked")
           << " blockers " << verdict.blocker_count
           << " diagnostics " << verdict.diagnostic_count
           << " report-only " << (verdict.report_only ? "true" : "false")
           << " production " << (verdict.production_enabled ? "enabled" : "disabled");
    return report.str();
}

auto runtime_indexed_member_cleanup_mutation_rewrite_authorization(
    RuntimeIndexedMemberCleanupMutationReadinessVerdict const& verdict,
    bool rewrite_authorization_requested
) -> RuntimeIndexedMemberCleanupMutationRewriteAuthorization {
    auto blockers = std::vector<std::string> {};
    if (!verdict.readiness_ready) {
        blockers.push_back("member-cleanup-mutation-readiness-verdict");
    }
    if (!verdict.guarded_rewrite_ready) {
        blockers.push_back("member-cleanup-mutation-guarded-rewrite");
    }

    auto authorization_ready = verdict.readiness_ready && verdict.guarded_rewrite_ready;
    return RuntimeIndexedMemberCleanupMutationRewriteAuthorization {
        .owner_name = verdict.owner_name,
        .index_expression_text = verdict.index_expression_text,
        .element_source_type_name = verdict.element_source_type_name,
        .moved_source_type_name = verdict.moved_source_type_name,
        .moved_member_path = verdict.moved_member_path,
        .blockers = std::move(blockers),
        .verdict_ready = verdict.readiness_ready,
        .guarded_rewrite_ready = verdict.guarded_rewrite_ready,
        .authorization_ready = authorization_ready,
        .rewrite_authorization_requested = rewrite_authorization_requested,
        .rewrite_authorized = authorization_ready && rewrite_authorization_requested,
        .report_only = true,
        .production_enabled = false,
        .source_line = verdict.source_line,
    };
}

auto runtime_indexed_member_cleanup_mutation_rewrite_authorization_report(
    RuntimeIndexedMemberCleanupMutationRewriteAuthorization const& authorization
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup mutation rewrite authorization owner "
           << authorization.owner_name
           << " index " << authorization.index_expression_text
           << " element " << authorization.element_source_type_name
           << " moved " << authorization.moved_source_type_name
           << " member-path " << dotted_path(authorization.moved_member_path);
    append_source_line(report, authorization.source_line);
    report << " verdict " << (authorization.verdict_ready ? "ready" : "blocked")
           << " guarded-rewrite " << (authorization.guarded_rewrite_ready ? "ready" : "blocked")
           << " authorization " << (authorization.authorization_ready ? "ready" : "blocked")
           << " rewrite-requested "
           << (authorization.rewrite_authorization_requested ? "true" : "false")
           << " rewrite-authorized " << (authorization.rewrite_authorized ? "true" : "false")
           << " report-only " << (authorization.report_only ? "true" : "false")
           << " production " << (authorization.production_enabled ? "enabled" : "disabled")
           << " blockers " << authorization.blockers.size();
    for (auto const& blocker : authorization.blockers) {
        report << " blocker " << blocker;
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_mutation_rewrite_authorization_diagnostics(
    RuntimeIndexedMemberCleanupMutationRewriteAuthorization const& authorization
) -> std::vector<std::string> {
    auto diagnostics = std::vector<std::string> {};
    for (auto const& blocker : authorization.blockers) {
        auto diagnostic = std::ostringstream {};
        diagnostic << "runtime-index member cleanup mutation rewrite authorization blocker owner "
                   << authorization.owner_name
                   << " index " << authorization.index_expression_text
                   << " element " << authorization.element_source_type_name
                   << " moved " << authorization.moved_source_type_name
                   << " member-path " << dotted_path(authorization.moved_member_path);
        append_source_line(diagnostic, authorization.source_line);
        diagnostic << " blocker " << blocker
                   << " detail ";
        if (blocker == "member-cleanup-mutation-readiness-verdict") {
            diagnostic << "member cleanup mutation readiness verdict is blocked";
        } else if (blocker == "member-cleanup-mutation-guarded-rewrite") {
            diagnostic << "member cleanup mutation guarded rewrite is blocked";
        } else {
            diagnostic << "member cleanup mutation rewrite authorization is blocked";
        }
        diagnostics.push_back(diagnostic.str());
    }
    return diagnostics;
}

auto runtime_indexed_member_cleanup_mutation_rewrite_execution_plan(
    RuntimeIndexedMemberCleanupMutationRewriteAuthorization const& authorization,
    bool execution_requested
) -> RuntimeIndexedMemberCleanupMutationRewriteExecutionPlan {
    auto blockers = std::vector<std::string> {};
    if (!authorization.authorization_ready) {
        blockers.push_back("member-cleanup-mutation-rewrite-authorization");
    }
    if (!authorization.rewrite_authorized) {
        blockers.push_back("member-cleanup-mutation-rewrite-not-authorized");
    }

    auto execution_plan_ready = authorization.authorization_ready && authorization.rewrite_authorized;
    return RuntimeIndexedMemberCleanupMutationRewriteExecutionPlan {
        .owner_name = authorization.owner_name,
        .index_expression_text = authorization.index_expression_text,
        .element_source_type_name = authorization.element_source_type_name,
        .moved_source_type_name = authorization.moved_source_type_name,
        .moved_member_path = authorization.moved_member_path,
        .blockers = std::move(blockers),
        .authorization_ready = authorization.authorization_ready,
        .rewrite_authorized = authorization.rewrite_authorized,
        .execution_plan_ready = execution_plan_ready,
        .execution_requested = execution_requested,
        .execution_enabled = execution_plan_ready && execution_requested,
        .report_only = true,
        .production_enabled = false,
        .source_line = authorization.source_line,
    };
}

auto runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_report(
    RuntimeIndexedMemberCleanupMutationRewriteExecutionPlan const& plan
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup mutation rewrite execution-plan owner "
           << plan.owner_name
           << " index " << plan.index_expression_text
           << " element " << plan.element_source_type_name
           << " moved " << plan.moved_source_type_name
           << " member-path " << dotted_path(plan.moved_member_path);
    append_source_line(report, plan.source_line);
    report << " authorization " << (plan.authorization_ready ? "ready" : "blocked")
           << " rewrite-authorized " << (plan.rewrite_authorized ? "true" : "false")
           << " execution-plan " << (plan.execution_plan_ready ? "ready" : "blocked")
           << " execution-requested " << (plan.execution_requested ? "true" : "false")
           << " execution " << (plan.execution_enabled ? "enabled" : "disabled")
           << " report-only " << (plan.report_only ? "true" : "false")
           << " production " << (plan.production_enabled ? "enabled" : "disabled")
           << " blockers " << plan.blockers.size();
    for (auto const& blocker : plan.blockers) {
        report << " blocker " << blocker;
    }
    return report.str();
}

auto runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_diagnostics(
    RuntimeIndexedMemberCleanupMutationRewriteExecutionPlan const& plan
) -> std::vector<std::string> {
    auto diagnostics = std::vector<std::string> {};
    for (auto const& blocker : plan.blockers) {
        auto diagnostic = std::ostringstream {};
        diagnostic << "runtime-index member cleanup mutation rewrite execution-plan blocker owner "
                   << plan.owner_name
                   << " index " << plan.index_expression_text
                   << " element " << plan.element_source_type_name
                   << " moved " << plan.moved_source_type_name
                   << " member-path " << dotted_path(plan.moved_member_path);
        append_source_line(diagnostic, plan.source_line);
        diagnostic << " blocker " << blocker
                   << " detail ";
        if (blocker == "member-cleanup-mutation-rewrite-authorization") {
            diagnostic << "member cleanup mutation rewrite authorization is blocked";
        } else if (blocker == "member-cleanup-mutation-rewrite-not-authorized") {
            diagnostic << "member cleanup mutation rewrite is not authorized";
        } else {
            diagnostic << "member cleanup mutation rewrite execution plan is blocked";
        }
        diagnostics.push_back(diagnostic.str());
    }
    return diagnostics;
}

auto runtime_indexed_member_cleanup_mutation_rewrite_execution_verdict(
    RuntimeIndexedMemberCleanupMutationRewriteExecutionPlan const& plan
) -> RuntimeIndexedMemberCleanupMutationRewriteExecutionVerdict {
    auto const diagnostics = runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_diagnostics(plan);
    return RuntimeIndexedMemberCleanupMutationRewriteExecutionVerdict {
        .owner_name = plan.owner_name,
        .index_expression_text = plan.index_expression_text,
        .element_source_type_name = plan.element_source_type_name,
        .moved_source_type_name = plan.moved_source_type_name,
        .moved_member_path = plan.moved_member_path,
        .blocker_count = static_cast<int>(plan.blockers.size()),
        .diagnostic_count = static_cast<int>(diagnostics.size()),
        .execution_plan_ready = plan.execution_plan_ready,
        .execution_enabled = plan.execution_enabled,
        .report_only = true,
        .production_enabled = false,
        .source_line = plan.source_line,
    };
}

auto runtime_indexed_member_cleanup_mutation_rewrite_execution_verdict_report(
    RuntimeIndexedMemberCleanupMutationRewriteExecutionVerdict const& verdict
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup mutation rewrite execution verdict owner "
           << verdict.owner_name
           << " index " << verdict.index_expression_text
           << " element " << verdict.element_source_type_name
           << " moved " << verdict.moved_source_type_name
           << " member-path " << dotted_path(verdict.moved_member_path);
    append_source_line(report, verdict.source_line);
    report << " execution-plan " << (verdict.execution_plan_ready ? "ready" : "blocked")
           << " execution " << (verdict.execution_enabled ? "enabled" : "disabled")
           << " blockers " << verdict.blocker_count
           << " diagnostics " << verdict.diagnostic_count
           << " report-only " << (verdict.report_only ? "true" : "false")
           << " production " << (verdict.production_enabled ? "enabled" : "disabled");
    return report.str();
}

auto runtime_indexed_member_cleanup_mutation_rewrite_promotion_status(
    RuntimeIndexedMemberCleanupMutationRewriteAuthorization const& authorization,
    RuntimeIndexedMemberCleanupMutationRewriteExecutionPlan const& plan,
    RuntimeIndexedMemberCleanupMutationRewriteExecutionVerdict const& verdict
) -> RuntimeIndexedMemberCleanupMutationRewritePromotionStatus {
    auto const authorization_ready = authorization.authorization_ready && authorization.rewrite_authorized;
    auto const execution_plan_ready = plan.execution_plan_ready && plan.execution_enabled;
    auto const execution_verdict_ready = verdict.execution_plan_ready && verdict.execution_enabled;
    auto const promotion_ready = authorization_ready && execution_plan_ready && execution_verdict_ready;
    return RuntimeIndexedMemberCleanupMutationRewritePromotionStatus {
        .owner_name = verdict.owner_name,
        .index_expression_text = verdict.index_expression_text,
        .element_source_type_name = verdict.element_source_type_name,
        .moved_source_type_name = verdict.moved_source_type_name,
        .moved_member_path = verdict.moved_member_path,
        .blocker_count = verdict.blocker_count,
        .diagnostic_count = verdict.diagnostic_count,
        .authorization_ready = authorization_ready,
        .execution_plan_ready = execution_plan_ready,
        .execution_verdict_ready = execution_verdict_ready,
        .promotion_ready = promotion_ready,
        .report_only = true,
        .production_enabled = false,
        .source_line = verdict.source_line,
    };
}

auto runtime_indexed_member_cleanup_mutation_rewrite_promotion_status_report(
    RuntimeIndexedMemberCleanupMutationRewritePromotionStatus const& status
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup mutation rewrite promotion-status owner "
           << status.owner_name
           << " index " << status.index_expression_text
           << " element " << status.element_source_type_name
           << " moved " << status.moved_source_type_name
           << " member-path " << dotted_path(status.moved_member_path);
    append_source_line(report, status.source_line);
    report << " authorization " << (status.authorization_ready ? "ready" : "blocked")
           << " execution-plan " << (status.execution_plan_ready ? "ready" : "blocked")
           << " execution-verdict " << (status.execution_verdict_ready ? "ready" : "blocked")
           << " promotion " << (status.promotion_ready ? "ready" : "blocked")
           << " blockers " << status.blocker_count
           << " diagnostics " << status.diagnostic_count
           << " report-only " << (status.report_only ? "true" : "false")
           << " production " << (status.production_enabled ? "enabled" : "disabled");
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
        state.runtime_indexed_member_cleanup_cfg_slices.empty() &&
        state.runtime_indexed_member_cleanup_function_rewrite_candidates.empty() &&
        state.runtime_indexed_member_cleanup_function_rewrite_edit_script_plans.empty() &&
        state.runtime_indexed_member_cleanup_function_rewrite_edit_script_validations.empty() &&
        state.runtime_indexed_member_cleanup_function_rewrite_staged_apply_plans.empty() &&
        state.runtime_indexed_member_cleanup_module_mutation_gates.empty() &&
        state.runtime_indexed_member_cleanup_production_readiness.empty() &&
        state.runtime_indexed_member_cleanup_promotion_checklists.empty() &&
        state.runtime_indexed_member_cleanup_typed_promotion_gates.empty() &&
        state.runtime_indexed_member_cleanup_promotion_seams.empty() &&
        state.runtime_indexed_member_cleanup_mutation_operation_plans.empty() &&
        state.runtime_indexed_member_cleanup_mutation_operation_validations.empty() &&
        state.runtime_indexed_member_cleanup_mutation_conflict_detections.empty() &&
        state.runtime_indexed_member_cleanup_mutation_apply_authorizations.empty() &&
        state.runtime_indexed_member_cleanup_mutation_apply_previews.empty() &&
        state.runtime_indexed_member_cleanup_mutation_post_apply_verifications.empty() &&
        state.runtime_indexed_member_cleanup_mutation_promotion_summaries.empty() &&
        state.runtime_indexed_member_cleanup_mutation_production_readiness.empty() &&
        state.runtime_indexed_member_cleanup_mutation_readiness_verdicts.empty() &&
        state.runtime_indexed_member_cleanup_mutation_rewrite_authorizations.empty() &&
        state.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans.empty() &&
        state.runtime_indexed_member_cleanup_mutation_rewrite_execution_verdicts.empty() &&
        state.runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses.empty()) {
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
    for (auto const& candidate : state.runtime_indexed_member_cleanup_function_rewrite_candidates) {
        report.push_back(runtime_indexed_member_cleanup_function_rewrite_candidate_report(candidate));
    }
    for (auto const& plan : state.runtime_indexed_member_cleanup_function_rewrite_edit_script_plans) {
        report.push_back(runtime_indexed_member_cleanup_function_rewrite_edit_script_plan_report(plan));
    }
    for (auto const& validation : state.runtime_indexed_member_cleanup_function_rewrite_edit_script_validations) {
        report.push_back(runtime_indexed_member_cleanup_function_rewrite_edit_script_validation_report(validation));
        auto diagnostics =
            runtime_indexed_member_cleanup_function_rewrite_edit_script_validation_diagnostics(validation);
        report.insert(report.end(), diagnostics.begin(), diagnostics.end());
    }
    for (auto const& plan : state.runtime_indexed_member_cleanup_function_rewrite_staged_apply_plans) {
        report.push_back(runtime_indexed_member_cleanup_function_rewrite_staged_apply_plan_report(plan));
        auto diagnostics = runtime_indexed_member_cleanup_function_rewrite_staged_apply_plan_diagnostics(plan);
        report.insert(report.end(), diagnostics.begin(), diagnostics.end());
    }
    for (auto const& gate : state.runtime_indexed_member_cleanup_module_mutation_gates) {
        report.push_back(runtime_indexed_member_cleanup_module_mutation_gate_report(gate));
        auto diagnostics = runtime_indexed_member_cleanup_module_mutation_gate_diagnostics(gate);
        report.insert(report.end(), diagnostics.begin(), diagnostics.end());
    }
    for (auto const& readiness : state.runtime_indexed_member_cleanup_production_readiness) {
        report.push_back(runtime_indexed_member_cleanup_production_readiness_report(readiness));
        auto diagnostics = runtime_indexed_member_cleanup_production_blocker_diagnostics(readiness);
        report.insert(report.end(), diagnostics.begin(), diagnostics.end());
    }
    for (auto const& checklist : state.runtime_indexed_member_cleanup_promotion_checklists) {
        report.push_back(runtime_indexed_member_cleanup_promotion_checklist_report(checklist));
    }
    for (auto const& seam : state.runtime_indexed_member_cleanup_promotion_seams) {
        report.push_back(runtime_indexed_member_cleanup_promotion_seam_report(seam));
    }
    for (auto const& plan : state.runtime_indexed_member_cleanup_mutation_operation_plans) {
        report.push_back(runtime_indexed_member_cleanup_mutation_operation_plan_report(plan));
    }
    for (auto const& validation : state.runtime_indexed_member_cleanup_mutation_operation_validations) {
        report.push_back(runtime_indexed_member_cleanup_mutation_operation_validation_report(validation));
    }
    for (auto const& detection : state.runtime_indexed_member_cleanup_mutation_conflict_detections) {
        report.push_back(runtime_indexed_member_cleanup_mutation_conflict_detection_report(detection));
    }
    for (auto const& authorization : state.runtime_indexed_member_cleanup_mutation_apply_authorizations) {
        report.push_back(runtime_indexed_member_cleanup_mutation_apply_authorization_report(authorization));
    }
    for (auto const& preview : state.runtime_indexed_member_cleanup_mutation_apply_previews) {
        report.push_back(runtime_indexed_member_cleanup_mutation_apply_preview_report(preview));
    }
    for (auto const& verification : state.runtime_indexed_member_cleanup_mutation_post_apply_verifications) {
        report.push_back(runtime_indexed_member_cleanup_mutation_post_apply_verification_report(verification));
    }
    for (auto const& summary : state.runtime_indexed_member_cleanup_mutation_promotion_summaries) {
        report.push_back(runtime_indexed_member_cleanup_mutation_promotion_summary_report(summary));
    }
    for (auto const& readiness : state.runtime_indexed_member_cleanup_mutation_production_readiness) {
        report.push_back(runtime_indexed_member_cleanup_mutation_production_readiness_report(readiness));
        auto diagnostics = runtime_indexed_member_cleanup_mutation_production_readiness_diagnostics(readiness);
        report.insert(report.end(), diagnostics.begin(), diagnostics.end());
    }
    for (auto const& verdict : state.runtime_indexed_member_cleanup_mutation_readiness_verdicts) {
        report.push_back(runtime_indexed_member_cleanup_mutation_readiness_verdict_report(verdict));
    }
    for (auto const& authorization : state.runtime_indexed_member_cleanup_mutation_rewrite_authorizations) {
        report.push_back(runtime_indexed_member_cleanup_mutation_rewrite_authorization_report(authorization));
        auto diagnostics =
            runtime_indexed_member_cleanup_mutation_rewrite_authorization_diagnostics(authorization);
        report.insert(report.end(), diagnostics.begin(), diagnostics.end());
    }
    for (auto const& plan : state.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans) {
        report.push_back(runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_report(plan));
        auto diagnostics =
            runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_diagnostics(plan);
        report.insert(report.end(), diagnostics.begin(), diagnostics.end());
    }
    for (auto const& verdict : state.runtime_indexed_member_cleanup_mutation_rewrite_execution_verdicts) {
        report.push_back(runtime_indexed_member_cleanup_mutation_rewrite_execution_verdict_report(verdict));
    }
    for (auto const& status : state.runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses) {
        report.push_back(runtime_indexed_member_cleanup_mutation_rewrite_promotion_status_report(status));
    }
    for (auto const& gate : state.runtime_indexed_member_cleanup_typed_promotion_gates) {
        report.push_back(runtime_indexed_member_cleanup_typed_promotion_gate_report(gate));
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

auto plan_branch_local_cleanups(
    std::vector<OwnershipTransferState> const& branch_states
) -> std::vector<BranchLocalCleanupPlan> {
    auto owner_names = std::vector<std::string> {};
    auto seen = std::unordered_set<std::string> {};
    for (auto const& state : branch_states) {
        for (auto const& owner_name : state.consumed_owned_bindings) {
            if (seen.insert(owner_name).second) {
                owner_names.push_back(owner_name);
            }
        }
    }
    std::ranges::sort(owner_names);

    auto plans = std::vector<BranchLocalCleanupPlan> {};
    for (auto const& owner_name : owner_names) {
        auto consumed_arm_indices = std::vector<std::size_t> {};
        auto cleanup_arm_indices = std::vector<std::size_t> {};
        for (auto index = std::size_t {0}; index < branch_states.size(); ++index) {
            if (branch_states[index].consumed_owned_bindings.contains(owner_name)) {
                consumed_arm_indices.push_back(index);
            } else {
                cleanup_arm_indices.push_back(index);
            }
        }
        if (consumed_arm_indices.empty() || cleanup_arm_indices.empty()) {
            continue;
        }
        plans.push_back(BranchLocalCleanupPlan {
            .owner_name = owner_name,
            .consumed_arm_indices = std::move(consumed_arm_indices),
            .cleanup_arm_indices = std::move(cleanup_arm_indices),
            .cleanup_emission_supported = false,
            .blocker = "branch-local cleanup emission not implemented",
        });
    }
    return plans;
}

auto format_indices(std::vector<std::size_t> const& indices) -> std::string {
    auto output = std::ostringstream {};
    for (auto index = std::size_t {0}; index < indices.size(); ++index) {
        if (index != 0) {
            output << ",";
        }
        output << indices[index];
    }
    return output.str();
}

auto format_branch_local_cleanup_plan(
    BranchLocalCleanupPlan const& plan
) -> std::string {
    auto output = std::ostringstream {};
    output << "branch-local cleanup plan: owner " << plan.owner_name
           << " consumed-arms " << format_indices(plan.consumed_arm_indices)
           << " cleanup-arms " << format_indices(plan.cleanup_arm_indices)
           << " emission " << (plan.cleanup_emission_supported ? "supported" : "blocked");
    if (!plan.blocker.empty()) {
        output << " blocker " << plan.blocker;
    }
    return output.str();
}

auto format_branch_local_cleanup_plan_report(
    std::vector<BranchLocalCleanupPlan> const& plans
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(plans.size());
    for (auto const& plan : plans) {
        report.push_back(format_branch_local_cleanup_plan(plan));
    }
    return report;
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
                merged.runtime_indexed_member_cleanup_cfg_slices ||
            branch_states[index].runtime_indexed_member_cleanup_function_rewrite_candidates !=
                merged.runtime_indexed_member_cleanup_function_rewrite_candidates ||
            branch_states[index].runtime_indexed_member_cleanup_function_rewrite_edit_script_plans !=
                merged.runtime_indexed_member_cleanup_function_rewrite_edit_script_plans ||
            branch_states[index].runtime_indexed_member_cleanup_function_rewrite_edit_script_validations !=
                merged.runtime_indexed_member_cleanup_function_rewrite_edit_script_validations ||
            branch_states[index].runtime_indexed_member_cleanup_function_rewrite_staged_apply_plans !=
                merged.runtime_indexed_member_cleanup_function_rewrite_staged_apply_plans ||
            branch_states[index].runtime_indexed_member_cleanup_module_mutation_gates !=
                merged.runtime_indexed_member_cleanup_module_mutation_gates ||
            branch_states[index].runtime_indexed_member_cleanup_production_readiness !=
                merged.runtime_indexed_member_cleanup_production_readiness ||
            branch_states[index].runtime_indexed_member_cleanup_promotion_checklists !=
                merged.runtime_indexed_member_cleanup_promotion_checklists ||
            branch_states[index].runtime_indexed_member_cleanup_typed_promotion_gates !=
                merged.runtime_indexed_member_cleanup_typed_promotion_gates ||
            branch_states[index].runtime_indexed_member_cleanup_promotion_seams !=
                merged.runtime_indexed_member_cleanup_promotion_seams ||
            branch_states[index].runtime_indexed_member_cleanup_mutation_operation_plans !=
                merged.runtime_indexed_member_cleanup_mutation_operation_plans ||
            branch_states[index].runtime_indexed_member_cleanup_mutation_operation_validations !=
                merged.runtime_indexed_member_cleanup_mutation_operation_validations ||
            branch_states[index].runtime_indexed_member_cleanup_mutation_conflict_detections !=
                merged.runtime_indexed_member_cleanup_mutation_conflict_detections ||
            branch_states[index].runtime_indexed_member_cleanup_mutation_apply_authorizations !=
                merged.runtime_indexed_member_cleanup_mutation_apply_authorizations ||
            branch_states[index].runtime_indexed_member_cleanup_mutation_apply_previews !=
                merged.runtime_indexed_member_cleanup_mutation_apply_previews ||
            branch_states[index].runtime_indexed_member_cleanup_mutation_post_apply_verifications !=
                merged.runtime_indexed_member_cleanup_mutation_post_apply_verifications ||
            branch_states[index].runtime_indexed_member_cleanup_mutation_promotion_summaries !=
                merged.runtime_indexed_member_cleanup_mutation_promotion_summaries ||
            branch_states[index].runtime_indexed_member_cleanup_mutation_production_readiness !=
                merged.runtime_indexed_member_cleanup_mutation_production_readiness ||
            branch_states[index].runtime_indexed_member_cleanup_mutation_readiness_verdicts !=
                merged.runtime_indexed_member_cleanup_mutation_readiness_verdicts ||
            branch_states[index].runtime_indexed_member_cleanup_mutation_rewrite_authorizations !=
                merged.runtime_indexed_member_cleanup_mutation_rewrite_authorizations ||
            branch_states[index].runtime_indexed_member_cleanup_mutation_rewrite_execution_plans !=
                merged.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans ||
            branch_states[index].runtime_indexed_member_cleanup_mutation_rewrite_execution_verdicts !=
                merged.runtime_indexed_member_cleanup_mutation_rewrite_execution_verdicts ||
            branch_states[index].runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses !=
                merged.runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses) {
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
