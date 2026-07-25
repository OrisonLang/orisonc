#include "orison/lowering/source_type_queries.hpp"

#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/null_safe_plan.hpp"
#include "orison/lowering/statement_pointer_adapter.hpp"
#include "orison/lowering/type_lowering.hpp"

#include <array>
#include <utility>

namespace orison::lowering {

auto split_top_level_generic_arguments(std::string_view text) -> std::vector<std::string> {
    auto arguments = std::vector<std::string> {};
    auto depth = std::size_t {0};
    auto start = std::size_t {0};
    for (auto index = std::size_t {0}; index < text.size(); ++index) {
        auto const character = text[index];
        if (character == '<') {
            ++depth;
            continue;
        }
        if (character == '>') {
            if (depth > 0) {
                --depth;
            }
            continue;
        }
        if (character == ',' && depth == 0) {
            auto argument = std::string {text.substr(start, index - start)};
            if (!argument.empty() && argument.front() == ' ') {
                argument.erase(argument.begin());
            }
            if (!argument.empty() && argument.back() == ' ') {
                argument.pop_back();
            }
            arguments.push_back(std::move(argument));
            start = index + 1;
        }
    }

    if (start < text.size()) {
        auto argument = std::string {text.substr(start)};
        if (!argument.empty() && argument.front() == ' ') {
            argument.erase(argument.begin());
        }
        if (!argument.empty() && argument.back() == ' ') {
            argument.pop_back();
        }
        arguments.push_back(std::move(argument));
    }
    return arguments;
}

auto parse_llvm_array_type(std::string_view type) -> std::optional<ParsedLlvmArrayType> {
    if (!type.starts_with("[") || !type.ends_with("]") || type.size() < 6) {
        return std::nullopt;
    }

    auto depth = std::size_t {0};
    auto separator = std::size_t {0};
    for (auto index = std::size_t {1}; index + 1 < type.size(); ++index) {
        auto const character = type[index];
        if (character == '[') {
            ++depth;
            continue;
        }
        if (character == ']') {
            if (depth > 0) {
                --depth;
            }
            continue;
        }
        if (depth == 0 && character == 'x' && index > 1 && index + 2 < type.size() &&
            type[index - 1] == ' ' && type[index + 1] == ' ') {
            separator = index;
            break;
        }
    }
    if (separator == 0) {
        return std::nullopt;
    }

    auto length_text = std::string {type.substr(1, separator - 2)};
    auto element_type = std::string {type.substr(separator + 2, type.size() - separator - 3)};
    if (length_text.empty() || element_type.empty()) {
        return std::nullopt;
    }

    auto length = std::size_t {0};
    for (auto character : length_text) {
        if (character < '0' || character > '9') {
            return std::nullopt;
        }
        length = (length * 10) + static_cast<std::size_t>(character - '0');
    }

    return ParsedLlvmArrayType {
        .element_type = std::move(element_type),
        .length = length,
    };
}

auto array_element_source_type_name(std::string_view type_name) -> std::optional<std::string> {
    constexpr auto prefix = std::string_view {"Array<"};
    if (!type_name.starts_with(prefix) || !type_name.ends_with(">") ||
        type_name.size() <= prefix.size() + 1) {
        return std::nullopt;
    }

    auto arguments = split_top_level_generic_arguments(
        type_name.substr(prefix.size(), type_name.size() - prefix.size() - 1)
    );
    if (arguments.size() != 2 || arguments[0].empty()) {
        return std::nullopt;
    }
    return arguments[0];
}

auto dynamic_array_element_source_type_name(std::string_view type_name) -> std::optional<std::string> {
    constexpr auto prefix = std::string_view {"DynamicArray<"};
    if (!type_name.starts_with(prefix) || !type_name.ends_with(">") ||
        type_name.size() <= prefix.size() + 1) {
        return std::nullopt;
    }

    auto arguments = split_top_level_generic_arguments(
        type_name.substr(prefix.size(), type_name.size() - prefix.size() - 1)
    );
    if (arguments.size() != 1 || arguments[0].empty()) {
        return std::nullopt;
    }
    return arguments[0];
}

auto view_source_type_parts(std::string_view type_name)
    -> std::optional<std::pair<DynamicSequenceKind, std::string>> {
    auto normalized = type_name;
    auto kind = DynamicSequenceKind::view;
    if (normalized.starts_with("shared.")) {
        normalized.remove_prefix(std::string_view {"shared."}.size());
        kind = DynamicSequenceKind::shared_view;
    } else if (normalized.starts_with("exclusive.")) {
        normalized.remove_prefix(std::string_view {"exclusive."}.size());
        kind = DynamicSequenceKind::exclusive_view;
    }

    constexpr auto prefix = std::string_view {"View<"};
    if (!normalized.starts_with(prefix) || !normalized.ends_with(">") ||
        normalized.size() <= prefix.size() + 1) {
        return std::nullopt;
    }

    auto arguments = split_top_level_generic_arguments(
        normalized.substr(prefix.size(), normalized.size() - prefix.size() - 1)
    );
    if (arguments.size() != 1 || arguments[0].empty()) {
        return std::nullopt;
    }
    return std::pair {kind, arguments[0]};
}

auto view_element_source_type_name(std::string_view type_name) -> std::optional<std::string> {
    auto parts = view_source_type_parts(type_name);
    if (!parts.has_value()) {
        return std::nullopt;
    }
    return parts->second;
}

auto dynamic_sequence_source_type(std::string_view type_name) -> std::optional<DynamicSequenceSourceType> {
    if (auto element_type = dynamic_array_element_source_type_name(type_name)) {
        return DynamicSequenceSourceType {
            .kind = DynamicSequenceKind::dynamic_array,
            .element_source_type_name = std::move(*element_type),
            .owns_storage = true,
            .permits_element_mutation = true,
        };
    }

    auto view_parts = view_source_type_parts(type_name);
    if (!view_parts.has_value()) {
        return std::nullopt;
    }

    return DynamicSequenceSourceType {
        .kind = view_parts->first,
        .element_source_type_name = std::move(view_parts->second),
        .owns_storage = false,
        .permits_element_mutation = view_parts->first == DynamicSequenceKind::exclusive_view,
    };
}

auto view_descriptor_llvm_type() -> std::string_view {
    return "{ ptr, i64 }";
}

auto dynamic_array_descriptor_llvm_type() -> std::string_view {
    return "{ ptr, i64, i64 }";
}

auto dynamic_array_lowering_invariants() -> DynamicArrayLoweringInvariants {
    return DynamicArrayLoweringInvariants {
        .descriptor_llvm_type = dynamic_array_descriptor_llvm_type(),
    };
}

auto dynamic_array_iterable_cleanup_owner_proof_status(
    DynamicArrayDescriptorStorageStatus status
) -> DynamicArrayIterableCleanupOwnerProofStatus {
    switch (status) {
        case DynamicArrayDescriptorStorageStatus::predicted_owner_local:
            return DynamicArrayIterableCleanupOwnerProofStatus::predicted_owner_local;
        case DynamicArrayDescriptorStorageStatus::audit_parameter_descriptor:
            return DynamicArrayIterableCleanupOwnerProofStatus::audit_parameter_descriptor;
        case DynamicArrayDescriptorStorageStatus::bound_parameter_descriptor:
            return DynamicArrayIterableCleanupOwnerProofStatus::proven_bound_parameter_descriptor;
        case DynamicArrayDescriptorStorageStatus::lowered_local_descriptor:
            return DynamicArrayIterableCleanupOwnerProofStatus::proven_lowered_local_descriptor;
    }
    return DynamicArrayIterableCleanupOwnerProofStatus::missing_cleanup_plan;
}

auto dynamic_array_iterable_cleanup_owner_proven(
    DynamicArrayIterableCleanupOwnerProofStatus status
) -> bool {
    return status == DynamicArrayIterableCleanupOwnerProofStatus::proven_bound_parameter_descriptor ||
        status == DynamicArrayIterableCleanupOwnerProofStatus::proven_lowered_local_descriptor;
}

auto attach_dynamic_array_iterable_cleanup_owner_proof(
    DynamicArrayIterableDescriptorPlan& plan,
    FunctionLoweringState const& state
) -> void {
    if (plan.owner_name.empty()) {
        plan.cleanup_owner_proof_status =
            DynamicArrayIterableCleanupOwnerProofStatus::missing_cleanup_plan;
        return;
    }

    for (auto const& cleanup_plan : state.dynamic_array_local_cleanup_plans) {
        if (cleanup_plan.owner_name != plan.owner_name ||
            cleanup_plan.source_type_name != plan.source_type_name) {
            continue;
        }
        if (!plan.descriptor_storage.empty() &&
            !cleanup_plan.descriptor_storage_name.empty() &&
            cleanup_plan.descriptor_storage_name != plan.descriptor_storage) {
            continue;
        }

        plan.cleanup_owner_proof_status =
            dynamic_array_iterable_cleanup_owner_proof_status(cleanup_plan.descriptor_storage_status);
        plan.cleanup_owner_proven =
            dynamic_array_iterable_cleanup_owner_proven(plan.cleanup_owner_proof_status);
        return;
    }

    plan.cleanup_owner_proof_status =
        DynamicArrayIterableCleanupOwnerProofStatus::missing_cleanup_plan;
}

auto plan_dynamic_array_iterable_descriptor(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> DynamicArrayIterableDescriptorPlan {
    auto plan = DynamicArrayIterableDescriptorPlan {};
    auto source_type_name = source_type_name_for_expression(expression, context, state);
    if (!source_type_name.has_value()) {
        return plan;
    }

    auto sequence = dynamic_sequence_source_type(*source_type_name);
    if (!sequence.has_value() || sequence->kind != DynamicSequenceKind::dynamic_array) {
        plan.source_type_name = std::move(*source_type_name);
        return plan;
    }

    plan.source_type_name = std::move(*source_type_name);
    plan.element_source_type_name = std::move(sequence->element_source_type_name);

    if (expression.kind != syntax::ExpressionKind::name) {
        plan.kind = DynamicArrayIterableDescriptorPlanKind::computed_owner_unproven;
        attach_dynamic_array_iterable_cleanup_owner_proof(plan, state);
        return plan;
    }

    plan.owner_name = expression.text;
    auto storage = aggregate_storage_for_name(expression.text, state);
    if (!storage.has_value()) {
        plan.kind = DynamicArrayIterableDescriptorPlanKind::missing_named_descriptor_storage;
        attach_dynamic_array_iterable_cleanup_owner_proof(plan, state);
        return plan;
    }

    plan.kind = DynamicArrayIterableDescriptorPlanKind::named_descriptor_owner;
    plan.descriptor_storage = std::move(*storage);
    plan.can_lower_now = true;
    attach_dynamic_array_iterable_cleanup_owner_proof(plan, state);
    return plan;
}

auto dynamic_array_iterable_cleanup_owner_proof_report(
    DynamicArrayIterableDescriptorPlan const& plan
) -> std::string {
    switch (plan.cleanup_owner_proof_status) {
        case DynamicArrayIterableCleanupOwnerProofStatus::not_dynamic_array:
            return "cleanup owner proof not required";
        case DynamicArrayIterableCleanupOwnerProofStatus::missing_cleanup_plan:
            return "cleanup owner proof missing";
        case DynamicArrayIterableCleanupOwnerProofStatus::predicted_owner_local:
            return "cleanup owner predicted from semantic descriptor origin";
        case DynamicArrayIterableCleanupOwnerProofStatus::audit_parameter_descriptor:
            return "cleanup owner audit-only parameter descriptor";
        case DynamicArrayIterableCleanupOwnerProofStatus::proven_bound_parameter_descriptor:
            return "cleanup owner proven from bound parameter descriptor";
        case DynamicArrayIterableCleanupOwnerProofStatus::proven_lowered_local_descriptor:
            return "cleanup owner proven from lowered local descriptor";
    }
    return "cleanup owner proof unknown";
}

auto dynamic_array_iterable_descriptor_plan_report(
    DynamicArrayIterableDescriptorPlan const& plan
) -> std::string {
    auto cleanup_report = dynamic_array_iterable_cleanup_owner_proof_report(plan);
    switch (plan.kind) {
        case DynamicArrayIterableDescriptorPlanKind::not_dynamic_array:
            return "not a DynamicArray iterable";
        case DynamicArrayIterableDescriptorPlanKind::named_descriptor_owner:
            return "named DynamicArray descriptor owner '" + plan.owner_name + "' lowers from " +
                plan.descriptor_storage + " [" + cleanup_report + "]";
        case DynamicArrayIterableDescriptorPlanKind::missing_named_descriptor_storage:
            return "named DynamicArray iterable '" + plan.owner_name +
                "' has no bound descriptor storage [" + cleanup_report + "]";
        case DynamicArrayIterableDescriptorPlanKind::computed_owner_unproven:
            return "computed DynamicArray iterable of type '" + plan.source_type_name +
                "' requires a proven single descriptor owner before lowering [" + cleanup_report + "]";
    }
    return "unknown DynamicArray iterable descriptor plan";
}

auto plan_computed_dynamic_array_iterable_ownership_transfer(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> ComputedDynamicArrayIterableOwnershipPlan {
    auto plan = ComputedDynamicArrayIterableOwnershipPlan {};
    auto source_type_name = source_type_name_for_expression(expression, context, state);
    if (!source_type_name.has_value()) {
        return plan;
    }

    auto sequence = dynamic_sequence_source_type(*source_type_name);
    if (!sequence.has_value() || sequence->kind != DynamicSequenceKind::dynamic_array ||
        expression.kind == syntax::ExpressionKind::name) {
        plan.source_type_name = std::move(*source_type_name);
        return plan;
    }

    plan.source_type_name = std::move(*source_type_name);
    plan.element_source_type_name = std::move(sequence->element_source_type_name);
    if (expression.kind != syntax::ExpressionKind::ternary || expression.right == nullptr ||
        expression.alternate == nullptr || expression.right->kind != syntax::ExpressionKind::name ||
        expression.alternate->kind != syntax::ExpressionKind::name) {
        plan.kind = ComputedDynamicArrayIterableOwnershipPlanKind::unsupported_computed_shape;
        return plan;
    }

    auto then_descriptor = plan_dynamic_array_iterable_descriptor(*expression.right, context, state);
    auto else_descriptor = plan_dynamic_array_iterable_descriptor(*expression.alternate, context, state);
    if (then_descriptor.source_type_name != plan.source_type_name ||
        else_descriptor.source_type_name != plan.source_type_name ||
        then_descriptor.owner_name.empty() ||
        else_descriptor.owner_name.empty()) {
        plan.kind = ComputedDynamicArrayIterableOwnershipPlanKind::unsupported_computed_shape;
        return plan;
    }

    plan.branch_owner_names.push_back(then_descriptor.owner_name);
    plan.branch_owner_names.push_back(else_descriptor.owner_name);
    plan.branch_cleanup_owner_proof_statuses.push_back(then_descriptor.cleanup_owner_proof_status);
    plan.branch_cleanup_owner_proof_statuses.push_back(else_descriptor.cleanup_owner_proof_status);

    auto branch_states = std::vector<OwnershipTransferState> {};
    branch_states.reserve(2);
    auto then_state = state.ownership_transfers;
    mark_owned_binding_consumed(then_state, then_descriptor.owner_name);
    branch_states.push_back(std::move(then_state));
    auto else_state = state.ownership_transfers;
    mark_owned_binding_consumed(else_state, else_descriptor.owner_name);
    branch_states.push_back(std::move(else_state));

    auto merged = merge_ownership_transfer_states(branch_states);
    if (!merged.has_value()) {
        plan.kind = ComputedDynamicArrayIterableOwnershipPlanKind::ternary_branch_owner_mismatch;
        return plan;
    }

    plan.merged_transfers = std::move(*merged);
    plan.ownership_join_matches = true;
    plan.cleanup_owner_proven =
        then_descriptor.cleanup_owner_proven && else_descriptor.cleanup_owner_proven &&
        then_descriptor.owner_name == else_descriptor.owner_name;
    plan.kind = plan.cleanup_owner_proven
        ? ComputedDynamicArrayIterableOwnershipPlanKind::ternary_single_owner_proven
        : ComputedDynamicArrayIterableOwnershipPlanKind::ternary_single_owner_unproven;
    return plan;
}

auto computed_dynamic_array_iterable_ownership_plan_report(
    ComputedDynamicArrayIterableOwnershipPlan const& plan
) -> std::string {
    auto output = std::string {"computed DynamicArray ownership plan "};
    switch (plan.kind) {
        case ComputedDynamicArrayIterableOwnershipPlanKind::not_computed_dynamic_array:
            output += "not computed dynamic array";
            return output;
        case ComputedDynamicArrayIterableOwnershipPlanKind::unsupported_computed_shape:
            output += "unsupported computed shape";
            break;
        case ComputedDynamicArrayIterableOwnershipPlanKind::ternary_branch_owner_mismatch:
            output += "ternary branch owner mismatch";
            break;
        case ComputedDynamicArrayIterableOwnershipPlanKind::ternary_single_owner_unproven:
            output += "ternary single owner unproven";
            break;
        case ComputedDynamicArrayIterableOwnershipPlanKind::ternary_single_owner_proven:
            output += "ternary single owner proven";
            break;
    }
    if (!plan.source_type_name.empty()) {
        output += " source ";
        output += plan.source_type_name;
    }
    if (!plan.element_source_type_name.empty()) {
        output += " element ";
        output += plan.element_source_type_name;
    }
    if (!plan.branch_owner_names.empty()) {
        output += " owners";
        for (auto const& owner : plan.branch_owner_names) {
            output += " ";
            output += owner;
        }
    }
    output += plan.ownership_join_matches ? " [ownership join ok]" : " [ownership join blocked]";
    output += plan.cleanup_owner_proven ? " [cleanup owner proven]" : " [cleanup owner blocked]";
    output += " (metadata only)";
    return output;
}

auto plan_computed_dynamic_array_iterable_descriptor_handoff(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> ComputedDynamicArrayIterableDescriptorHandoffPlan {
    auto plan = ComputedDynamicArrayIterableDescriptorHandoffPlan {};
    plan.ownership_plan = plan_computed_dynamic_array_iterable_ownership_transfer(
        expression,
        context,
        state
    );
    plan.source_type_name = plan.ownership_plan.source_type_name;
    plan.element_source_type_name = plan.ownership_plan.element_source_type_name;
    plan.lowering_enabled = false;

    switch (plan.ownership_plan.kind) {
        case ComputedDynamicArrayIterableOwnershipPlanKind::not_computed_dynamic_array:
            plan.kind = ComputedDynamicArrayIterableDescriptorHandoffPlanKind::not_computed_dynamic_array;
            return plan;
        case ComputedDynamicArrayIterableOwnershipPlanKind::unsupported_computed_shape:
            plan.kind = ComputedDynamicArrayIterableDescriptorHandoffPlanKind::unsupported_computed_shape;
            return plan;
        case ComputedDynamicArrayIterableOwnershipPlanKind::ternary_branch_owner_mismatch:
            plan.kind = ComputedDynamicArrayIterableDescriptorHandoffPlanKind::ownership_join_blocked;
            return plan;
        case ComputedDynamicArrayIterableOwnershipPlanKind::ternary_single_owner_unproven:
            plan.kind = ComputedDynamicArrayIterableDescriptorHandoffPlanKind::cleanup_owner_unproven;
            if (!plan.ownership_plan.branch_owner_names.empty()) {
                plan.source_owner_name = plan.ownership_plan.branch_owner_names.front();
                plan.handoff_owner_name = plan.source_owner_name;
            }
            return plan;
        case ComputedDynamicArrayIterableOwnershipPlanKind::ternary_single_owner_proven:
            break;
    }

    if (plan.ownership_plan.branch_owner_names.empty()) {
        plan.kind = ComputedDynamicArrayIterableDescriptorHandoffPlanKind::cleanup_owner_unproven;
        return plan;
    }

    plan.source_owner_name = plan.ownership_plan.branch_owner_names.front();
    plan.handoff_owner_name = plan.source_owner_name;

    auto owner_expression = syntax::ExpressionSyntax {};
    owner_expression.kind = syntax::ExpressionKind::name;
    owner_expression.text = plan.source_owner_name;
    auto descriptor_plan = plan_dynamic_array_iterable_descriptor(owner_expression, context, state);
    plan.descriptor_storage_name = std::move(descriptor_plan.descriptor_storage);
    plan.descriptor_storage_available = !plan.descriptor_storage_name.empty();
    plan.cleanup_owner_proven = descriptor_plan.cleanup_owner_proven;
    plan.kind = plan.descriptor_storage_available && plan.cleanup_owner_proven
        ? ComputedDynamicArrayIterableDescriptorHandoffPlanKind::single_cleanup_owner_handoff_planned
        : ComputedDynamicArrayIterableDescriptorHandoffPlanKind::cleanup_owner_unproven;
    return plan;
}

auto computed_dynamic_array_iterable_descriptor_handoff_plan_report(
    ComputedDynamicArrayIterableDescriptorHandoffPlan const& plan
) -> std::string {
    auto output = std::string {"computed DynamicArray descriptor handoff plan "};
    switch (plan.kind) {
        case ComputedDynamicArrayIterableDescriptorHandoffPlanKind::not_computed_dynamic_array:
            output += "not computed dynamic array";
            return output;
        case ComputedDynamicArrayIterableDescriptorHandoffPlanKind::unsupported_computed_shape:
            output += "unsupported computed shape";
            break;
        case ComputedDynamicArrayIterableDescriptorHandoffPlanKind::ownership_join_blocked:
            output += "ownership join blocked";
            break;
        case ComputedDynamicArrayIterableDescriptorHandoffPlanKind::cleanup_owner_unproven:
            output += "cleanup owner unproven";
            break;
        case ComputedDynamicArrayIterableDescriptorHandoffPlanKind::single_cleanup_owner_handoff_planned:
            output += "single cleanup owner handoff planned";
            break;
    }
    if (!plan.source_type_name.empty()) {
        output += " source ";
        output += plan.source_type_name;
    }
    if (!plan.element_source_type_name.empty()) {
        output += " element ";
        output += plan.element_source_type_name;
    }
    if (!plan.source_owner_name.empty()) {
        output += " owner ";
        output += plan.source_owner_name;
    }
    if (!plan.handoff_owner_name.empty()) {
        output += " handoff ";
        output += plan.handoff_owner_name;
    }
    if (!plan.descriptor_storage_name.empty()) {
        output += " descriptor ";
        output += plan.descriptor_storage_name;
    }
    output += plan.descriptor_storage_available ? " [descriptor storage available]" :
        " [descriptor storage blocked]";
    output += plan.cleanup_owner_proven ? " [cleanup owner proven]" : " [cleanup owner blocked]";
    output += plan.lowering_enabled ? " [lowering enabled]" : " [lowering disabled]";
    output += " (metadata only)";
    return output;
}

auto plan_computed_dynamic_array_iterable_cleanup_sequence(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> ComputedDynamicArrayIterableCleanupSequencePlan {
    auto plan = ComputedDynamicArrayIterableCleanupSequencePlan {};
    plan.handoff_plan = plan_computed_dynamic_array_iterable_descriptor_handoff(
        expression,
        context,
        state
    );
    plan.source_type_name = plan.handoff_plan.source_type_name;
    plan.element_source_type_name = plan.handoff_plan.element_source_type_name;
    plan.cleanup_owner_name = plan.handoff_plan.handoff_owner_name;
    plan.descriptor_storage_name = plan.handoff_plan.descriptor_storage_name;
    plan.cleanup_sequence_enabled = false;

    switch (plan.handoff_plan.kind) {
        case ComputedDynamicArrayIterableDescriptorHandoffPlanKind::not_computed_dynamic_array:
            plan.kind = ComputedDynamicArrayIterableCleanupSequencePlanKind::not_computed_dynamic_array;
            return plan;
        case ComputedDynamicArrayIterableDescriptorHandoffPlanKind::unsupported_computed_shape:
            plan.kind = ComputedDynamicArrayIterableCleanupSequencePlanKind::unsupported_computed_shape;
            return plan;
        case ComputedDynamicArrayIterableDescriptorHandoffPlanKind::ownership_join_blocked:
            plan.kind = ComputedDynamicArrayIterableCleanupSequencePlanKind::ownership_join_blocked;
            return plan;
        case ComputedDynamicArrayIterableDescriptorHandoffPlanKind::cleanup_owner_unproven:
            plan.kind = ComputedDynamicArrayIterableCleanupSequencePlanKind::cleanup_owner_unproven;
            return plan;
        case ComputedDynamicArrayIterableDescriptorHandoffPlanKind::single_cleanup_owner_handoff_planned:
            break;
    }

    if (plan.cleanup_owner_name.empty() || plan.descriptor_storage_name.empty()) {
        plan.kind = ComputedDynamicArrayIterableCleanupSequencePlanKind::cleanup_owner_unproven;
        return plan;
    }

    plan.kind = ComputedDynamicArrayIterableCleanupSequencePlanKind::loop_cleanup_sequence_planned;
    plan.loop_entry_cleanup_owner_name = plan.cleanup_owner_name + ".loop.entry";
    plan.loop_exit_cleanup_owner_name = plan.cleanup_owner_name;
    plan.loop_body_has_cleanup_responsibility = true;
    plan.function_cleanup_resumes_after_loop = true;
    return plan;
}

auto computed_dynamic_array_iterable_cleanup_sequence_plan_report(
    ComputedDynamicArrayIterableCleanupSequencePlan const& plan
) -> std::string {
    auto output = std::string {"computed DynamicArray cleanup sequence plan "};
    switch (plan.kind) {
        case ComputedDynamicArrayIterableCleanupSequencePlanKind::not_computed_dynamic_array:
            output += "not computed dynamic array";
            return output;
        case ComputedDynamicArrayIterableCleanupSequencePlanKind::unsupported_computed_shape:
            output += "unsupported computed shape";
            break;
        case ComputedDynamicArrayIterableCleanupSequencePlanKind::ownership_join_blocked:
            output += "ownership join blocked";
            break;
        case ComputedDynamicArrayIterableCleanupSequencePlanKind::cleanup_owner_unproven:
            output += "cleanup owner unproven";
            break;
        case ComputedDynamicArrayIterableCleanupSequencePlanKind::loop_cleanup_sequence_planned:
            output += "loop cleanup sequence planned";
            break;
    }
    if (!plan.source_type_name.empty()) {
        output += " source ";
        output += plan.source_type_name;
    }
    if (!plan.element_source_type_name.empty()) {
        output += " element ";
        output += plan.element_source_type_name;
    }
    if (!plan.cleanup_owner_name.empty()) {
        output += " owner ";
        output += plan.cleanup_owner_name;
    }
    if (!plan.descriptor_storage_name.empty()) {
        output += " descriptor ";
        output += plan.descriptor_storage_name;
    }
    if (!plan.loop_entry_cleanup_owner_name.empty()) {
        output += " loop-entry ";
        output += plan.loop_entry_cleanup_owner_name;
    }
    if (!plan.loop_exit_cleanup_owner_name.empty()) {
        output += " loop-exit ";
        output += plan.loop_exit_cleanup_owner_name;
    }
    output += plan.loop_body_has_cleanup_responsibility ? " [loop cleanup owns descriptor]" :
        " [loop cleanup blocked]";
    output += plan.function_cleanup_resumes_after_loop ? " [function cleanup resumes]" :
        " [function cleanup blocked]";
    output += plan.cleanup_sequence_enabled ? " [cleanup sequence enabled]" :
        " [cleanup sequence disabled]";
    output += " (metadata only)";
    return output;
}

auto plan_computed_dynamic_array_iterable_descriptor_render(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> ComputedDynamicArrayIterableDescriptorRenderPlan {
    auto plan = ComputedDynamicArrayIterableDescriptorRenderPlan {};
    plan.cleanup_sequence_plan = plan_computed_dynamic_array_iterable_cleanup_sequence(
        expression,
        context,
        state
    );
    plan.source_type_name = plan.cleanup_sequence_plan.source_type_name;
    plan.element_source_type_name = plan.cleanup_sequence_plan.element_source_type_name;
    plan.cleanup_owner_name = plan.cleanup_sequence_plan.cleanup_owner_name;
    plan.descriptor_storage_name = plan.cleanup_sequence_plan.descriptor_storage_name;
    plan.render_enabled = false;

    switch (plan.cleanup_sequence_plan.kind) {
        case ComputedDynamicArrayIterableCleanupSequencePlanKind::not_computed_dynamic_array:
            plan.kind = ComputedDynamicArrayIterableDescriptorRenderPlanKind::not_computed_dynamic_array;
            return plan;
        case ComputedDynamicArrayIterableCleanupSequencePlanKind::unsupported_computed_shape:
            plan.kind = ComputedDynamicArrayIterableDescriptorRenderPlanKind::unsupported_computed_shape;
            return plan;
        case ComputedDynamicArrayIterableCleanupSequencePlanKind::ownership_join_blocked:
            plan.kind = ComputedDynamicArrayIterableDescriptorRenderPlanKind::ownership_join_blocked;
            return plan;
        case ComputedDynamicArrayIterableCleanupSequencePlanKind::cleanup_owner_unproven:
            plan.kind = ComputedDynamicArrayIterableDescriptorRenderPlanKind::cleanup_owner_unproven;
            return plan;
        case ComputedDynamicArrayIterableCleanupSequencePlanKind::loop_cleanup_sequence_planned:
            break;
    }

    if (plan.cleanup_owner_name.empty() || plan.descriptor_storage_name.empty()) {
        plan.kind = ComputedDynamicArrayIterableDescriptorRenderPlanKind::cleanup_owner_unproven;
        return plan;
    }

    auto prefix = "%" + plan.cleanup_owner_name + ".computed_for";
    plan.descriptor_value_name = prefix + ".descriptor";
    plan.data_pointer_name = prefix + ".data";
    plan.length_name = prefix + ".length";
    plan.rendered_ir.push_back(
        emit_dynamic_array_descriptor_load(plan.descriptor_value_name, plan.descriptor_storage_name)
    );
    plan.rendered_ir.push_back(
        emit_dynamic_array_descriptor_field_projection(
            plan.data_pointer_name,
            plan.descriptor_value_name,
            DynamicArrayDescriptorField::data
        )
    );
    plan.rendered_ir.push_back(
        emit_dynamic_array_descriptor_field_projection(
            plan.length_name,
            plan.descriptor_value_name,
            DynamicArrayDescriptorField::length
        )
    );
    plan.descriptor_load_planned = true;
    plan.data_projection_planned = true;
    plan.length_projection_planned = true;
    plan.kind = ComputedDynamicArrayIterableDescriptorRenderPlanKind::descriptor_render_planned;
    return plan;
}

auto computed_dynamic_array_iterable_descriptor_render_plan_report(
    ComputedDynamicArrayIterableDescriptorRenderPlan const& plan
) -> std::string {
    auto output = std::string {"computed DynamicArray descriptor render plan "};
    switch (plan.kind) {
        case ComputedDynamicArrayIterableDescriptorRenderPlanKind::not_computed_dynamic_array:
            output += "not computed dynamic array";
            return output;
        case ComputedDynamicArrayIterableDescriptorRenderPlanKind::unsupported_computed_shape:
            output += "unsupported computed shape";
            break;
        case ComputedDynamicArrayIterableDescriptorRenderPlanKind::ownership_join_blocked:
            output += "ownership join blocked";
            break;
        case ComputedDynamicArrayIterableDescriptorRenderPlanKind::cleanup_owner_unproven:
            output += "cleanup owner unproven";
            break;
        case ComputedDynamicArrayIterableDescriptorRenderPlanKind::descriptor_render_planned:
            output += "descriptor load projection planned";
            break;
    }
    if (!plan.source_type_name.empty()) {
        output += " source ";
        output += plan.source_type_name;
    }
    if (!plan.element_source_type_name.empty()) {
        output += " element ";
        output += plan.element_source_type_name;
    }
    if (!plan.cleanup_owner_name.empty()) {
        output += " owner ";
        output += plan.cleanup_owner_name;
    }
    if (!plan.descriptor_storage_name.empty()) {
        output += " descriptor ";
        output += plan.descriptor_storage_name;
    }
    if (!plan.descriptor_value_name.empty()) {
        output += " value ";
        output += plan.descriptor_value_name;
    }
    if (!plan.data_pointer_name.empty()) {
        output += " data ";
        output += plan.data_pointer_name;
    }
    if (!plan.length_name.empty()) {
        output += " length ";
        output += plan.length_name;
    }
    output += plan.descriptor_load_planned ? " [descriptor load planned]" :
        " [descriptor load blocked]";
    output += plan.data_projection_planned ? " [data projection planned]" :
        " [data projection blocked]";
    output += plan.length_projection_planned ? " [length projection planned]" :
        " [length projection blocked]";
    output += plan.render_enabled ? " [render enabled]" : " [render disabled]";
    output += " (metadata only)";
    return output;
}

auto plan_computed_dynamic_array_iterable_loop_control_render(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> ComputedDynamicArrayIterableLoopControlRenderPlan {
    auto plan = ComputedDynamicArrayIterableLoopControlRenderPlan {};
    plan.descriptor_render_plan = plan_computed_dynamic_array_iterable_descriptor_render(
        expression,
        context,
        state
    );
    plan.source_type_name = plan.descriptor_render_plan.source_type_name;
    plan.element_source_type_name = plan.descriptor_render_plan.element_source_type_name;
    plan.cleanup_owner_name = plan.descriptor_render_plan.cleanup_owner_name;
    plan.render_enabled = false;

    switch (plan.descriptor_render_plan.kind) {
        case ComputedDynamicArrayIterableDescriptorRenderPlanKind::not_computed_dynamic_array:
            plan.kind = ComputedDynamicArrayIterableLoopControlRenderPlanKind::not_computed_dynamic_array;
            return plan;
        case ComputedDynamicArrayIterableDescriptorRenderPlanKind::unsupported_computed_shape:
            plan.kind = ComputedDynamicArrayIterableLoopControlRenderPlanKind::unsupported_computed_shape;
            return plan;
        case ComputedDynamicArrayIterableDescriptorRenderPlanKind::ownership_join_blocked:
            plan.kind = ComputedDynamicArrayIterableLoopControlRenderPlanKind::ownership_join_blocked;
            return plan;
        case ComputedDynamicArrayIterableDescriptorRenderPlanKind::cleanup_owner_unproven:
            plan.kind = ComputedDynamicArrayIterableLoopControlRenderPlanKind::cleanup_owner_unproven;
            return plan;
        case ComputedDynamicArrayIterableDescriptorRenderPlanKind::descriptor_render_planned:
            break;
    }

    if (plan.cleanup_owner_name.empty() || plan.descriptor_render_plan.length_name.empty()) {
        plan.kind = ComputedDynamicArrayIterableLoopControlRenderPlanKind::cleanup_owner_unproven;
        return plan;
    }

    auto prefix = "%" + plan.cleanup_owner_name + ".computed_for";
    auto block_prefix = plan.cleanup_owner_name + ".computed_for";
    plan.condition_block_name = block_prefix + ".condition";
    plan.body_block_name = block_prefix + ".body";
    plan.continue_block_name = block_prefix + ".continue";
    plan.exit_block_name = block_prefix + ".exit";
    plan.index_name = prefix + ".index";
    plan.next_index_name = prefix + ".next.index";
    plan.bounds_check_name = prefix + ".more";

    plan.rendered_ir.push_back("  br label %" + plan.condition_block_name + "\n");
    plan.rendered_ir.push_back(plan.condition_block_name + ":\n");
    plan.rendered_ir.push_back(
        "  " + plan.index_name + " = phi i64 [ 0, %entry ], [ " + plan.next_index_name +
        ", %" + plan.continue_block_name + " ]\n"
    );
    plan.rendered_ir.push_back(
        emit_dynamic_array_bounds_check(
            plan.bounds_check_name,
            plan.index_name,
            plan.descriptor_render_plan.length_name,
            DynamicArrayBoundsCheckKind::index_within_length
        )
    );
    plan.rendered_ir.push_back(
        "  br i1 " + plan.bounds_check_name + ", label %" + plan.body_block_name +
        ", label %" + plan.exit_block_name + "\n"
    );
    plan.entry_branch_planned = true;
    plan.index_phi_planned = true;
    plan.bounds_check_planned = true;
    plan.conditional_branch_planned = true;
    plan.kind = ComputedDynamicArrayIterableLoopControlRenderPlanKind::loop_control_render_planned;
    return plan;
}

auto computed_dynamic_array_iterable_loop_control_render_plan_report(
    ComputedDynamicArrayIterableLoopControlRenderPlan const& plan
) -> std::string {
    auto output = std::string {"computed DynamicArray loop control render plan "};
    switch (plan.kind) {
        case ComputedDynamicArrayIterableLoopControlRenderPlanKind::not_computed_dynamic_array:
            output += "not computed dynamic array";
            return output;
        case ComputedDynamicArrayIterableLoopControlRenderPlanKind::unsupported_computed_shape:
            output += "unsupported computed shape";
            break;
        case ComputedDynamicArrayIterableLoopControlRenderPlanKind::ownership_join_blocked:
            output += "ownership join blocked";
            break;
        case ComputedDynamicArrayIterableLoopControlRenderPlanKind::cleanup_owner_unproven:
            output += "cleanup owner unproven";
            break;
        case ComputedDynamicArrayIterableLoopControlRenderPlanKind::loop_control_render_planned:
            output += "loop control render planned";
            break;
    }
    if (!plan.source_type_name.empty()) {
        output += " source ";
        output += plan.source_type_name;
    }
    if (!plan.element_source_type_name.empty()) {
        output += " element ";
        output += plan.element_source_type_name;
    }
    if (!plan.cleanup_owner_name.empty()) {
        output += " owner ";
        output += plan.cleanup_owner_name;
    }
    if (!plan.condition_block_name.empty()) {
        output += " condition ";
        output += plan.condition_block_name;
    }
    if (!plan.body_block_name.empty()) {
        output += " body ";
        output += plan.body_block_name;
    }
    if (!plan.continue_block_name.empty()) {
        output += " continue ";
        output += plan.continue_block_name;
    }
    if (!plan.exit_block_name.empty()) {
        output += " exit ";
        output += plan.exit_block_name;
    }
    if (!plan.index_name.empty()) {
        output += " index ";
        output += plan.index_name;
    }
    if (!plan.bounds_check_name.empty()) {
        output += " bounds ";
        output += plan.bounds_check_name;
    }
    output += plan.entry_branch_planned ? " [entry branch planned]" :
        " [entry branch blocked]";
    output += plan.index_phi_planned ? " [index phi planned]" : " [index phi blocked]";
    output += plan.bounds_check_planned ? " [bounds check planned]" :
        " [bounds check blocked]";
    output += plan.conditional_branch_planned ? " [conditional branch planned]" :
        " [conditional branch blocked]";
    output += plan.render_enabled ? " [render enabled]" : " [render disabled]";
    output += " (metadata only)";
    return output;
}

auto is_scalar_or_nonowning_source_type(std::string_view source_type_name) -> bool {
    constexpr auto scalar_names = std::array<std::string_view, 25> {
        "Address",
        "Bool",
        "Byte",
        "Char",
        "Float32",
        "Float64",
        "Int8",
        "Int16",
        "Int32",
        "Int64",
        "Int128",
        "IntSize",
        "UInt8",
        "UInt16",
        "UInt32",
        "UInt64",
        "UInt128",
        "UIntSize",
        "Unit",
        "Pointer",
        "Pointer<",
        "View",
        "View<",
        "shared.",
        "exclusive.",
    };

    for (auto scalar_name : scalar_names) {
        if (source_type_name == scalar_name || source_type_name.starts_with(scalar_name)) {
            return true;
        }
    }
    return false;
}

auto pointer_pointee_source_type_name(std::string_view type_name) -> std::optional<std::string> {
    constexpr auto prefix = std::string_view {"Pointer<"};
    if (!type_name.starts_with(prefix) || !type_name.ends_with(">") ||
        type_name.size() <= prefix.size() + 1) {
        return std::nullopt;
    }

    return std::string(type_name.substr(prefix.size(), type_name.size() - prefix.size() - 1));
}

auto maybe_payload_source_type_name(std::string_view type_name) -> std::optional<std::string> {
    constexpr auto prefix = std::string_view {"Maybe<"};
    if (!type_name.starts_with(prefix) || !type_name.ends_with(">") ||
        type_name.size() <= prefix.size() + 1) {
        return std::nullopt;
    }

    auto arguments = split_top_level_generic_arguments(
        type_name.substr(prefix.size(), type_name.size() - prefix.size() - 1)
    );
    if (arguments.size() != 1 || arguments.front().empty()) {
        return std::nullopt;
    }
    return arguments.front();
}

auto source_type_name_for_llvm_type(
    std::string_view llvm_type,
    LoweringContext const& context
) -> std::optional<std::string> {
    if (llvm_type == "i1") {
        return std::string {"Bool"};
    }
    if (llvm_type == "i8") {
        return std::string {"UInt8"};
    }
    if (llvm_type == "i16") {
        return std::string {"UInt16"};
    }
    if (llvm_type == "i32") {
        return std::string {"UInt32"};
    }
    if (llvm_type == "i64") {
        return std::string {"UInt64"};
    }
    if (llvm_type == "float") {
        return std::string {"Float32"};
    }
    if (llvm_type == "double") {
        return std::string {"Float64"};
    }

    for (auto const& [record_name, layout] : context.records) {
        if (layout.llvm_type_name == llvm_type) {
            return record_name;
        }
    }
    auto choice_source_type = std::optional<std::string> {};
    for (auto const& [choice_name, layout] : context.choices) {
        (void)choice_name;
        if (layout.llvm_type_name != llvm_type) {
            continue;
        }
        if (choice_source_type.has_value()) {
            return std::nullopt;
        }
        choice_source_type = layout.source_type_name;
    }
    if (choice_source_type.has_value()) {
        return choice_source_type;
    }

    if (auto array = parse_llvm_array_type(llvm_type)) {
        auto element_source_type = source_type_name_for_llvm_type(array->element_type, context);
        if (!element_source_type.has_value()) {
            return std::nullopt;
        }

        return std::string {"Array<"} + *element_source_type + ", " + std::to_string(array->length) + ">";
    }

    return std::nullopt;
}

auto find_record_field(
    LoweredRecordLayout const& layout,
    std::string_view field_name
) -> LoweredRecordField const* {
    for (auto const& field : layout.fields) {
        if (field.name == field_name) {
            return &field;
        }
    }
    return nullptr;
}

auto source_type_name_for_record_constructor(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string> {
    if (expression.kind != syntax::ExpressionKind::call || expression.left == nullptr ||
        expression.left->kind != syntax::ExpressionKind::name) {
        return std::nullopt;
    }

    auto inferred = std::optional<std::string> {};
    auto generic_prefix = expression.left->text + "<";
    for (auto const& [record_name, layout] : context.records) {
        if (!record_name.starts_with(generic_prefix) || layout.fields.size() != expression.arguments.size()) {
            continue;
        }

        auto matches = true;
        for (auto index = std::size_t {0}; index < expression.arguments.size(); ++index) {
            auto argument_type = source_type_name_for_expression(expression.arguments[index], context, state);
            if (!argument_type.has_value() || *argument_type != layout.fields[index].source_type_name) {
                matches = false;
                break;
            }
        }
        if (!matches) {
            continue;
        }
        if (inferred.has_value()) {
            return std::nullopt;
        }
        inferred = record_name;
    }
    if (inferred.has_value()) {
        return inferred;
    }

    auto exact_record = context.records.find(expression.left->text);
    if (exact_record != context.records.end()) {
        return expression.left->text;
    }
    return std::nullopt;
}

auto generic_record_constructor_inference_failure_detail(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string> {
    if (expression.kind != syntax::ExpressionKind::call || expression.left == nullptr ||
        expression.left->kind != syntax::ExpressionKind::name) {
        return std::nullopt;
    }

    auto generic_record = context.generic_record_parameters.find(expression.left->text);
    if (generic_record == context.generic_record_parameters.end() || generic_record->second.empty()) {
        return std::nullopt;
    }
    if (source_type_name_for_record_constructor(expression, context, state).has_value()) {
        return std::nullopt;
    }

    auto message = std::string {};
    if (generic_record->second.size() == 1) {
        message = "generic parameter '" + generic_record->second.front() + "' cannot be inferred";
    } else {
        message = "generic parameters ";
        for (auto index = std::size_t {0}; index < generic_record->second.size(); ++index) {
            if (index > 0) {
                message += ", ";
            }
            message += "'";
            message += generic_record->second[index];
            message += "'";
        }
        message += " cannot be inferred";
    }
    message += " for record '";
    message += expression.left->text;
    message += "'";
    return message;
}

auto lowered_type_for_source_type_name(
    std::string_view type_name,
    LoweringContext const& context
) -> std::optional<LoweredType> {
    constexpr auto pointer_prefix = std::string_view {"Pointer<"};
    if (type_name.starts_with(pointer_prefix) && type_name.ends_with(">") &&
        type_name.size() > pointer_prefix.size() + 1) {
        return LoweredType {
            .type = "ptr",
            .signedness = IntegerSignedness::not_integer,
        };
    }

    if (view_element_source_type_name(type_name).has_value()) {
        return LoweredType {
            .type = std::string {view_descriptor_llvm_type()},
            .signedness = IntegerSignedness::not_integer,
        };
    }

    auto type = syntax::TypeSyntax {.name = std::string(type_name)};
    if (auto lowered = llvm_type_for(type); lowered.has_value() && *lowered != "void") {
        return LoweredType {
            .type = std::string(*lowered),
            .signedness = integer_signedness_for(type),
        };
    }

    if (auto record = context.records.find(std::string(type_name)); record != context.records.end()) {
        return LoweredType {
            .type = record->second.llvm_type_name,
            .signedness = IntegerSignedness::not_integer,
        };
    }
    if (auto choice = context.choices.find(std::string(type_name));
        choice != context.choices.end() && !choice->second.llvm_type_name.empty()) {
        return LoweredType {
            .type = choice->second.llvm_type_name,
            .signedness = IntegerSignedness::not_integer,
        };
    }

    if (auto payload_type_name = maybe_payload_source_type_name(type_name)) {
        auto payload_type = lowered_type_for_source_type_name(*payload_type_name, context);
        if (payload_type.has_value()) {
            return LoweredType {
                .type = "{ i1, " + payload_type->type + " }",
                .signedness = IntegerSignedness::not_integer,
            };
        }
    }

    constexpr auto prefix = std::string_view {"Array<"};
    if (type_name.starts_with(prefix) && type_name.ends_with(">") &&
        type_name.size() > prefix.size() + 1) {
        auto arguments = split_top_level_generic_arguments(
            type_name.substr(prefix.size(), type_name.size() - prefix.size() - 1)
        );
        if (arguments.size() == 2 && !arguments[1].empty()) {
            auto element_type = lowered_type_for_source_type_name(arguments[0], context);
            if (element_type.has_value()) {
                return LoweredType {
                    .type = "[" + arguments[1] + " x " + element_type->type + "]",
                    .signedness = IntegerSignedness::not_integer,
                };
            }
        }
    }

    return std::nullopt;
}

auto llvm_type_for_source_type_name(
    std::string_view type_name,
    LoweringContext const& context
) -> std::optional<std::string> {
    auto type = lowered_type_for_source_type_name(type_name, context);
    if (!type.has_value()) {
        return std::nullopt;
    }
    return type->type;
}

auto source_type_name_for_expression(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string> {
    if (expression.kind == syntax::ExpressionKind::name) {
        auto source_type = state.source_type_names.find(expression.text);
        if (source_type != state.source_type_names.end()) {
            return source_type->second;
        }
    }

    if (expression.kind == syntax::ExpressionKind::cast && !expression.text.empty()) {
        return lowered_type_for_source_type_name(expression.text, context).has_value()
            ? std::optional<std::string> {expression.text}
            : std::nullopt;
    }

    if (expression.kind == syntax::ExpressionKind::array_literal) {
        if (expression.arguments.empty()) {
            return std::nullopt;
        }

        auto element_source_type = source_type_name_for_expression(expression.arguments.front(), context, state);
        if (!element_source_type.has_value()) {
            return std::nullopt;
        }

        for (auto index = std::size_t {1}; index < expression.arguments.size(); ++index) {
            auto next_element_source_type =
                source_type_name_for_expression(expression.arguments[index], context, state);
            if (!next_element_source_type.has_value() || *next_element_source_type != *element_source_type) {
                return std::nullopt;
            }
        }

        return std::string {"Array<"} + *element_source_type + ", " +
               std::to_string(expression.arguments.size()) + ">";
    }

    if (expression.kind == syntax::ExpressionKind::member_access && expression.left != nullptr) {
        auto base_source_type = source_type_name_for_expression(*expression.left, context, state);
        if (!base_source_type.has_value()) {
            return std::nullopt;
        }

        auto record_source_type = pointer_pointee_source_type_name(*base_source_type);
        auto layout = context.records.find(record_source_type.value_or(*base_source_type));
        if (layout == context.records.end()) {
            return std::nullopt;
        }

        auto const* field = find_record_field(layout->second, expression.text);
        if (field == nullptr || field->source_type_name.empty()) {
            return std::nullopt;
        }
        return field->source_type_name;
    }

    if (expression.kind == syntax::ExpressionKind::null_safe_member_access) {
        auto plan_result = plan_null_safe_member_access(expression, context, state);
        return plan_result.plan.has_value()
            ? std::optional<std::string> {plan_result.plan->result_maybe_type_name}
            : std::nullopt;
    }

    if (expression.kind == syntax::ExpressionKind::index_access && expression.left != nullptr &&
        expression.arguments.size() == 1) {
        auto base_source_type = source_type_name_for_expression(*expression.left, context, state);
        if (!base_source_type.has_value()) {
            return std::nullopt;
        }

        auto indexed_source_type = pointer_pointee_source_type_name(*base_source_type);
        auto array_element = array_element_source_type_name(indexed_source_type.value_or(*base_source_type));
        if (array_element.has_value()) {
            return std::move(array_element);
        }
        auto view_element = view_element_source_type_name(indexed_source_type.value_or(*base_source_type));
        if (view_element.has_value()) {
            return view_element;
        }
        return dynamic_array_element_source_type_name(indexed_source_type.value_or(*base_source_type));
    }

    if (expression.kind == syntax::ExpressionKind::ternary && expression.right != nullptr &&
        expression.alternate != nullptr) {
        auto then_source_type = source_type_name_for_expression(*expression.right, context, state);
        auto else_source_type = source_type_name_for_expression(*expression.alternate, context, state);
        if (!then_source_type.has_value() || !else_source_type.has_value() ||
            *then_source_type != *else_source_type) {
            return std::nullopt;
        }
        return then_source_type;
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name && expression.left->text == "raw_offset" &&
        !expression.arguments.empty()) {
        return source_type_name_for_expression(expression.arguments.front(), context, state);
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name && expression.left->text == "Pointer" &&
        expression.arguments.size() == 1) {
        auto const& source = expression.arguments.front();
        if (source.kind != syntax::ExpressionKind::call || source.left == nullptr ||
            source.left->kind != syntax::ExpressionKind::name || source.left->text != "address_of" ||
            source.arguments.size() != 1) {
            return std::nullopt;
        }

        auto pointee_source_type = source_type_name_for_expression(source.arguments.front(), context, state);
        if (!pointee_source_type.has_value()) {
            return std::nullopt;
        }
        return std::string {"Pointer<"} + *pointee_source_type + ">";
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name) {
        if (auto constructor_type = source_type_name_for_record_constructor(expression, context, state)) {
            return constructor_type;
        }

        auto function = context.functions.find(expression.left->text);
        if (function == context.functions.end() || function->second.return_type.empty() ||
            function->second.return_type == "void") {
            return std::nullopt;
        }
        if (!function->second.source_return_type_name.empty()) {
            return function->second.source_return_type_name;
        }

        return source_type_name_for_llvm_type(function->second.return_type, context);
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        (expression.left->kind == syntax::ExpressionKind::member_access ||
         expression.left->kind == syntax::ExpressionKind::null_safe_member_access) &&
        expression.left->left != nullptr) {
        auto receiver_type = source_type_name_for_expression(*expression.left->left, context, state);
        if (!receiver_type.has_value()) {
            return std::nullopt;
        }

        auto lookup_receiver_type = *receiver_type;
        if (expression.left->kind == syntax::ExpressionKind::null_safe_member_access) {
            auto payload_type = maybe_payload_source_type_name(*receiver_type);
            if (!payload_type.has_value()) {
                return std::nullopt;
            }
            lookup_receiver_type = std::move(*payload_type);
        }

        auto method = find_lowered_method_signature(context, lookup_receiver_type, expression.left->text);
        if (method.result != LoweredMethodLookupResult::found || method.method == nullptr ||
            method.method->signature.return_type.empty() || method.method->signature.return_type == "void") {
            return std::nullopt;
        }

        auto return_source_type = !method.method->signature.source_return_type_name.empty()
            ? std::optional<std::string> {method.method->signature.source_return_type_name}
            : source_type_name_for_llvm_type(method.method->signature.return_type, context);
        if (!return_source_type.has_value()) {
            return std::nullopt;
        }
        return expression.left->kind == syntax::ExpressionKind::null_safe_member_access
            ? std::optional<std::string> {"Maybe<" + *return_source_type + ">"}
            : return_source_type;
    }

    return std::nullopt;
}

auto source_type_name_for_initializer(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state,
    std::string_view lowered_llvm_type
) -> std::optional<std::string> {
    if (auto source_type = source_type_name_for_expression(expression, context, state)) {
        return source_type;
    }

    return source_type_name_for_llvm_type(lowered_llvm_type, context);
}

auto source_type_name_for_value_statement_pointer_block(
    std::vector<syntax::StatementSyntax const*> const& statements,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string> {
    if (statements.empty()) {
        return std::nullopt;
    }

    auto local_state = state;
    for (auto index = std::size_t {0}; index + 1 < statements.size(); ++index) {
        auto const* statement = statements[index];
        if (statement == nullptr) {
            return std::nullopt;
        }
        if (statement->kind != syntax::StatementKind::let_binding &&
            statement->kind != syntax::StatementKind::var_binding) {
            return std::nullopt;
        }

        if (!statement->annotated_type.name.empty()) {
            local_state.source_type_names[statement->name] = render_source_type_name(statement->annotated_type);
            continue;
        }

        auto initializer_source_type =
            source_type_name_for_expression(statement->expression, context, local_state);
        if (!initializer_source_type.has_value()) {
            return std::nullopt;
        }
        local_state.source_type_names[statement->name] = std::move(*initializer_source_type);
    }

    auto const* final_statement = statements.back();
    if (final_statement == nullptr) {
        return std::nullopt;
    }
    return source_type_name_for_value_statement(*final_statement, context, local_state);
}

auto source_type_name_for_value_statement(
    syntax::StatementSyntax const& statement,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string> {
    if (statement.kind == syntax::StatementKind::expression_statement ||
        statement.kind == syntax::StatementKind::return_statement) {
        return source_type_name_for_expression(statement.expression, context, state);
    }

    if (statement.kind == syntax::StatementKind::if_statement) {
        auto then_source_type =
            source_type_name_for_value_statement_block(statement.nested_statements, context, state);
        auto else_source_type =
            source_type_name_for_value_statement_block(statement.alternate_statements, context, state);
        if (!then_source_type.has_value() || !else_source_type.has_value() ||
            *then_source_type != *else_source_type) {
            return std::nullopt;
        }
        return then_source_type;
    }

    if (statement.kind == syntax::StatementKind::switch_statement) {
        auto result = std::optional<std::string> {};
        for (auto const& switch_case : statement.switch_cases) {
            auto case_source_type =
                source_type_name_for_value_statement_block(switch_case.statements, context, state);
            if (!case_source_type.has_value()) {
                return std::nullopt;
            }
            if (!result.has_value()) {
                result = std::move(case_source_type);
                continue;
            }
            if (*result != *case_source_type) {
                return std::nullopt;
            }
        }
        return result;
    }

    return std::nullopt;
}

auto source_type_name_for_value_statement_block(
    std::vector<syntax::StatementSyntax> const& statements,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string> {
    auto statement_pointers = statement_pointers_for(statements);
    return source_type_name_for_value_statement_pointer_block(statement_pointers, context, state);
}

auto source_type_name_for_value_statement_block(
    std::vector<std::unique_ptr<syntax::StatementSyntax>> const& statements,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string> {
    auto statement_pointers = statement_pointers_for(statements);
    return source_type_name_for_value_statement_pointer_block(statement_pointers, context, state);
}

}  // namespace orison::lowering
