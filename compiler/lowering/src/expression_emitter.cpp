#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/aggregate_path.hpp"
#include "orison/lowering/branch_binding_scope.hpp"
#include "orison/lowering/call_emitter.hpp"
#include "orison/lowering/conditional_emitter.hpp"
#include "orison/lowering/conditional_plan.hpp"
#include "orison/lowering/concurrency_emitter.hpp"
#include "orison/lowering/concurrency_runtime.hpp"
#include "orison/lowering/dynamic_array_cleanup_plan.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/function_signature.hpp"
#include "orison/lowering/generic_call_resolution.hpp"
#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_failure_lifecycle.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/maybe_value_emitter.hpp"
#include "orison/lowering/null_safe_plan.hpp"
#include "orison/lowering/ownership_transfer.hpp"
#include "orison/lowering/runtime_index_expression.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/target_layout.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/lowering/type_lowering.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace orison::lowering {
namespace {

using EmissionContext = LoweringEmissionContext;

auto lowered_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output,
    std::optional<std::string_view> expected_source_type_name
) -> std::optional<LoweredExpression>;

auto moved_owned_dynamic_array_binding_name(
    std::string_view owner_name,
    FunctionLoweringState const& state
) -> std::optional<std::string> {
    auto name = std::string(owner_name);
    auto source_type = state.source_type_names.find(name);
    if (source_type == state.source_type_names.end() ||
        !dynamic_array_element_source_type_name(source_type->second).has_value() ||
        !is_owned_binding_consumed(state.ownership_transfers, name)) {
        return std::nullopt;
    }
    return name;
}

auto record_use_after_move_failure(
    LoweringFailures& failures,
    std::string_view owner_name
) -> void {
    record_expression_lowering_failure(
        failures,
        ExpressionLoweringFailureReason::use_after_move,
        std::string(owner_name)
    );
}

auto returned_dynamic_array_owner_name(
    syntax::ExpressionSyntax const& expression,
    std::optional<std::string_view> expected_source_type_name,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string> {
    if (!expected_source_type_name.has_value() ||
        !dynamic_array_element_source_type_name(*expected_source_type_name).has_value()) {
        return std::nullopt;
    }

    if (expression.kind == syntax::ExpressionKind::name) {
        auto source_type = state.source_type_names.find(expression.text);
        if (source_type != state.source_type_names.end() &&
            source_type->second == *expected_source_type_name) {
            return expression.text;
        }
        return std::nullopt;
    }

    if (expression.kind != syntax::ExpressionKind::call ||
        expression.left == nullptr ||
        expression.left->kind != syntax::ExpressionKind::name) {
        return std::nullopt;
    }

    auto function = context.lowering.functions.find(expression.left->text);
    if (function == context.lowering.functions.end() ||
        function->second.source_return_type_name != *expected_source_type_name ||
        function->second.parameter_source_type_names.size() != expression.arguments.size()) {
        return std::nullopt;
    }

    for (auto index = std::size_t {0}; index < expression.arguments.size(); ++index) {
        auto const& argument = expression.arguments[index];
        if (argument.kind != syntax::ExpressionKind::name ||
            function->second.parameter_source_type_names[index] != *expected_source_type_name) {
            continue;
        }
        auto source_type = state.source_type_names.find(argument.text);
        if (source_type != state.source_type_names.end() &&
            source_type->second == *expected_source_type_name) {
            return argument.text;
        }
    }
    return std::nullopt;
}

auto emit_dynamic_array_cleanup_for_owner(
    std::string_view owner_name,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> bool {
    if (is_owned_binding_consumed(session.state.ownership_transfers, owner_name)) {
        return true;
    }

    auto matching_cleanup_plans = std::vector<DynamicArrayDescriptorCleanupPlan> {};
    for (auto const& cleanup_plan : session.state.dynamic_array_local_cleanup_plans) {
        if (cleanup_plan.owner_name == owner_name) {
            matching_cleanup_plans.push_back(cleanup_plan);
        }
    }
    if (matching_cleanup_plans.empty()) {
        return true;
    }

    auto const cleanup_ordinal_start = session.state.next_temporary_index;
    auto const& final_cleanup_plan = matching_cleanup_plans.back();
    auto final_cleanup_ordinal = cleanup_ordinal_start + matching_cleanup_plans.size() - 1;
    auto label_prefix = final_cleanup_plan.owner_name + ".dynamic_array_cleanup" +
        std::to_string(final_cleanup_ordinal);
    auto cleanup_exit_block = is_scalar_or_nonowning_source_type(final_cleanup_plan.element_source_type_name)
        ? label_prefix + ".cleanup.entry"
        : label_prefix + ".drop.done";

    auto saved_cleanup_plans = session.state.dynamic_array_local_cleanup_plans;
    session.state.dynamic_array_local_cleanup_plans = std::move(matching_cleanup_plans);
    auto const emitted = emit_local_dynamic_array_cleanups(context, session, output);
    session.state.dynamic_array_local_cleanup_plans = std::move(saved_cleanup_plans);
    if (!emitted) {
        return false;
    }

    session.state.current_block = std::move(cleanup_exit_block);
    mark_owned_binding_consumed(session.state.ownership_transfers, std::string {owner_name});
    return true;
}

auto consumed_owned_record_member_path_name(
    AggregatePath const& path,
    std::string_view base_source_type_name,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string> {
    if (path.base_expression == nullptr ||
        path.base_expression->kind != syntax::ExpressionKind::name) {
        return std::nullopt;
    }
    if (auto moved_name = consumed_owned_binding_or_descendant_name(
            state.ownership_transfers,
            path.base_expression->text
        )) {
        return moved_name;
    }

    auto field_names = std::vector<std::string> {};
    field_names.reserve(path.steps.size());
    for (auto const& step : path.steps) {
        if (step.kind != AggregatePathStepKind::member) {
            return std::nullopt;
        }
        field_names.push_back(step.field_name);
        auto transfer = owned_record_member_path_transfer(
            path.base_expression->text,
            base_source_type_name,
            field_names,
            context
        );
        if (transfer.has_value() &&
            is_owned_binding_consumed(state.ownership_transfers, transfer->binding_name)) {
            return transfer->binding_name;
        }
    }
    return std::nullopt;
}

void mark_seeded_dynamic_array_cleanup_descendants_consumed(
    std::string_view owner_name,
    FunctionLoweringState const& state,
    OwnershipTransferState& transfers
) {
    auto owner_prefix = std::string {owner_name};
    owner_prefix += ".";
    for (auto const& cleanup_plan : state.dynamic_array_local_cleanup_plans) {
        if (cleanup_plan.owner_name == owner_name ||
            cleanup_plan.owner_name.starts_with(owner_prefix)) {
            mark_owned_binding_consumed(transfers, cleanup_plan.owner_name);
        }
    }
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

struct OwnedAggregatePathTransfer {
    std::string binding_name;
    std::string source_type_name;
    bool contains_index = false;
};

auto owned_named_aggregate_path_transfer(
    AggregatePath const& path,
    std::string_view base_source_type_name,
    LoweringContext const& context
) -> std::optional<OwnedAggregatePathTransfer> {
    if (path.base_expression == nullptr ||
        path.base_expression->kind != syntax::ExpressionKind::name ||
        path.steps.empty()) {
        return std::nullopt;
    }

    auto binding_name = path.base_expression->text;
    auto current_source_type_name = std::string {base_source_type_name};
    auto contains_index = false;
    for (auto const& step : path.steps) {
        if (step.kind == AggregatePathStepKind::member) {
            auto record = context.records.find(current_source_type_name);
            if (record == context.records.end()) {
                return std::nullopt;
            }

            auto const* field = find_record_field(record->second, step.field_name);
            if (field == nullptr) {
                return std::nullopt;
            }
            binding_name += ".";
            binding_name += step.field_name;
            current_source_type_name = field->source_type_name;
            continue;
        }

        if (step.index_expression == nullptr) {
            return std::nullopt;
        }
        auto element_source_type = array_element_source_type_name(current_source_type_name);
        auto element_segment = decimal_index_owner_segment(*step.index_expression);
        if (!element_source_type.has_value() || !element_segment.has_value()) {
            return std::nullopt;
        }
        binding_name += ".";
        binding_name += *element_segment;
        current_source_type_name = std::move(*element_source_type);
        contains_index = true;
    }

    if (!is_owned_transfer_source_type(current_source_type_name, context)) {
        return std::nullopt;
    }

    return OwnedAggregatePathTransfer {
        .binding_name = std::move(binding_name),
        .source_type_name = std::move(current_source_type_name),
        .contains_index = contains_index,
    };
}

auto consumed_owned_aggregate_path_name(
    AggregatePath const& path,
    std::string_view base_source_type_name,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string> {
    if (path.base_expression == nullptr ||
        path.base_expression->kind != syntax::ExpressionKind::name) {
        return std::nullopt;
    }
    if (is_owned_binding_consumed(state.ownership_transfers, path.base_expression->text)) {
        return path.base_expression->text;
    }

    auto runtime_owner_name = path.base_expression->text;
    auto runtime_current_source_type_name = std::string {base_source_type_name};
    for (auto const& step : path.steps) {
        if (step.kind == AggregatePathStepKind::member) {
            auto record = context.records.find(runtime_current_source_type_name);
            if (record == context.records.end()) {
                break;
            }
            auto const* field = find_record_field(record->second, step.field_name);
            if (field == nullptr) {
                break;
            }
            runtime_owner_name += ".";
            runtime_owner_name += step.field_name;
            runtime_current_source_type_name = field->source_type_name;
            continue;
        }

        if (step.index_expression == nullptr) {
            break;
        }
        auto const index_expression_text = runtime_index_expression_key(*step.index_expression);
        for (auto const& owner : state.ownership_transfers.runtime_indexed_partial_owners) {
            if (owner.constructor_move_enabled &&
                owner.owner_name == runtime_owner_name &&
                owner.index_expression_text == index_expression_text) {
                return runtime_owner_name + "[" + index_expression_text + "]";
            }
        }

        auto element_source_type = array_element_source_type_name(runtime_current_source_type_name);
        if (!element_source_type.has_value()) {
            break;
        }
        runtime_current_source_type_name = std::move(*element_source_type);
        if (auto element_segment = decimal_index_owner_segment(*step.index_expression)) {
            runtime_owner_name += ".";
            runtime_owner_name += *element_segment;
        } else {
            runtime_owner_name += "[";
            runtime_owner_name += index_expression_text;
            runtime_owner_name += "]";
        }
    }

    auto transfer = owned_named_aggregate_path_transfer(path, base_source_type_name, context);
    if (!transfer.has_value()) {
        return consumed_owned_record_member_path_name(path, base_source_type_name, context, state);
    }

    if (is_owned_binding_consumed(state.ownership_transfers, transfer->binding_name)) {
        return transfer->binding_name;
    }

    auto transfer_descendant_prefix = transfer->binding_name + ".";
    for (auto const& consumed_name : state.ownership_transfers.consumed_owned_bindings) {
        auto consumed_descendant_prefix = consumed_name + ".";
        if (consumed_name.starts_with(transfer_descendant_prefix) ||
            transfer->binding_name.starts_with(consumed_descendant_prefix)) {
            return consumed_name;
        }
    }
    return std::nullopt;
}

void mark_constructor_owned_argument_cleanup_consumed(
    syntax::ExpressionSyntax const& argument,
    std::string_view expected_source_type,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session
) {
    if (!is_owned_transfer_source_type(expected_source_type, context.lowering)) {
        return;
    }

    if (argument.kind == syntax::ExpressionKind::name) {
        auto actual_source_type = session.state.source_type_names.find(argument.text);
        if (actual_source_type == session.state.source_type_names.end() ||
            actual_source_type->second != expected_source_type) {
            return;
        }

        mark_seeded_dynamic_array_cleanup_descendants_consumed(
            argument.text,
            session.state,
            session.state.ownership_transfers
        );
        return;
    }

    auto path = collect_named_aggregate_path(argument);
    if (!path.has_value() || path->base_expression == nullptr) {
        return;
    }

    auto owner_source_type = session.state.source_type_names.find(path->base_expression->text);
    if (owner_source_type == session.state.source_type_names.end()) {
        return;
    }

    auto aggregate_transfer = owned_named_aggregate_path_transfer(
        *path,
        owner_source_type->second,
        context.lowering
    );
    if (aggregate_transfer.has_value() && aggregate_transfer->source_type_name == expected_source_type) {
        mark_owned_binding_consumed(session.state.ownership_transfers, aggregate_transfer->binding_name);
        mark_seeded_dynamic_array_cleanup_descendants_consumed(
            aggregate_transfer->binding_name,
            session.state,
            session.state.ownership_transfers
        );
        return;
    }

    auto field_names = std::vector<std::string> {};
    field_names.reserve(path->steps.size());
    for (auto step_index = std::size_t {0}; step_index < path->steps.size(); ++step_index) {
        auto const& step = path->steps[step_index];
        if (step.kind != AggregatePathStepKind::member) {
            return;
        }
        field_names.push_back(step.field_name);
    }

    auto transfer = owned_record_member_path_transfer(
        path->base_expression->text,
        owner_source_type->second,
        field_names,
        context.lowering
    );
    if (!transfer.has_value() || transfer->source_type_name != expected_source_type) {
        return;
    }

    mark_owned_binding_consumed(session.state.ownership_transfers, transfer->binding_name);
    mark_seeded_dynamic_array_cleanup_descendants_consumed(
        transfer->binding_name,
        session.state,
        session.state.ownership_transfers
    );
}

auto has_owned_literal_indexed_constructor_argument_transfer(
    syntax::ExpressionSyntax const& argument,
    std::string_view expected_source_type,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> bool {
    if (!is_owned_transfer_source_type(expected_source_type, context)) {
        return false;
    }

    auto path = collect_named_aggregate_path(argument);
    if (!path.has_value() || path->base_expression == nullptr) {
        return false;
    }

    auto owner_source_type = state.source_type_names.find(path->base_expression->text);
    if (owner_source_type == state.source_type_names.end()) {
        return false;
    }

    auto transfer = owned_named_aggregate_path_transfer(*path, owner_source_type->second, context);
    return transfer.has_value() &&
        transfer->contains_index &&
        transfer->source_type_name == expected_source_type;
}

auto record_runtime_indexed_constructor_ownership(
    syntax::ExpressionSyntax const& argument,
    std::string_view expected_source_type,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    bool constructor_move_enabled
) -> bool {
    auto owner = runtime_indexed_partial_owner_for_constructor_argument(
        argument,
        expected_source_type,
        context.lowering,
        session.state
    );
    if (!owner.has_value()) {
        return false;
    }
    owner->constructor_move_enabled = constructor_move_enabled;
    owner->source_line = argument.line;
    record_runtime_indexed_partial_owner(
        session.state.ownership_transfers,
        *owner,
        session.state.current_block,
        context.options.enable_runtime_indexed_cleanup_emission,
        context.options.enable_runtime_indexed_member_cleanup_ir_mutation_request,
        context.options.enable_runtime_indexed_member_cleanup_production_gate_request,
        context.options.enable_runtime_indexed_member_cleanup_apply_authorization_request,
        context.options.enable_runtime_indexed_member_cleanup_rewrite_execution_request
    );
    return true;
}

auto runtime_indexed_constructor_move_enabled(
    syntax::ExpressionSyntax const& argument,
    std::string_view expected_source_type,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session
) -> bool {
    if (!context.options.enable_runtime_indexed_cleanup_emission ||
        !context.options.enable_runtime_indexed_constructor_move) {
        return false;
    }

    auto owner = runtime_indexed_partial_owner_for_constructor_argument(
        argument,
        expected_source_type,
        context.lowering,
        session.state
    );
    if (!owner.has_value()) {
        return false;
    }
    if (context.options.enable_runtime_indexed_fixed_array_constructor_move_only &&
        owner->static_length_value.empty()) {
        return false;
    }

    owner->constructor_move_enabled = true;
    owner->source_line = argument.line;
    record_runtime_indexed_partial_owner(
        session.state.ownership_transfers,
        *owner,
        session.state.current_block,
        context.options.enable_runtime_indexed_cleanup_emission,
        context.options.enable_runtime_indexed_member_cleanup_ir_mutation_request,
        context.options.enable_runtime_indexed_member_cleanup_production_gate_request,
        context.options.enable_runtime_indexed_member_cleanup_apply_authorization_request,
        context.options.enable_runtime_indexed_member_cleanup_rewrite_execution_request
    );
    return true;
}

auto has_recorded_runtime_indexed_constructor_move(
    RuntimeIndexedPartialOwner const& candidate,
    FunctionLoweringState const& state
) -> bool {
    return std::ranges::any_of(
        state.ownership_transfers.runtime_indexed_partial_owners,
        [&](RuntimeIndexedPartialOwner const& recorded) {
            return recorded.constructor_move_enabled &&
                recorded.owner_name == candidate.owner_name &&
                recorded.index_expression_text == candidate.index_expression_text &&
                recorded.element_source_type_name == candidate.element_source_type_name &&
                recorded.moved_source_type_name == candidate.moved_source_type_name;
        }
    );
}

auto runtime_indexed_constructor_argument_key(
    RuntimeIndexedPartialOwner const& owner
) -> std::string {
    return owner.owner_name + "[" + owner.index_expression_text + "]";
}

auto runtime_indexed_constructor_argument_key(
    syntax::ExpressionSyntax const& argument,
    std::string_view expected_source_type,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string> {
    auto owner = runtime_indexed_partial_owner_for_constructor_argument(
        argument,
        expected_source_type,
        context,
        state
    );
    if (!owner.has_value()) {
        return std::nullopt;
    }
    return runtime_indexed_constructor_argument_key(*owner);
}

auto runtime_indexed_constructor_argument_active(
    FunctionLoweringState const& state,
    std::string_view key
) -> bool {
    return std::ranges::find(
        state.active_runtime_indexed_constructor_argument_keys,
        key
    ) != state.active_runtime_indexed_constructor_argument_keys.end();
}

struct RuntimeIndexedConstructorArgumentScope {
    FunctionLoweringState& state;
    bool active = false;

    RuntimeIndexedConstructorArgumentScope(
        FunctionLoweringState& lowering_state,
        std::optional<std::string> key
    ) : state(lowering_state) {
        if (!key.has_value()) {
            return;
        }
        active = true;
        state.active_runtime_indexed_constructor_argument_keys.push_back(std::move(*key));
    }

    ~RuntimeIndexedConstructorArgumentScope() {
        if (!active || state.active_runtime_indexed_constructor_argument_keys.empty()) {
            return;
        }
        state.active_runtime_indexed_constructor_argument_keys.pop_back();
    }
};

void retarget_runtime_indexed_constructor_cleanup_predecessor(
    FunctionLoweringState& state,
    std::optional<std::string> const& key,
    std::string const& predecessor_block_name
) {
    if (!key.has_value() || predecessor_block_name.empty()) {
        return;
    }
    for (auto& plan : state.ownership_transfers.runtime_indexed_cleanup_emission_plans) {
        auto const plan_key = plan.owner_name + "[" + plan.index_expression_text + "]";
        if (plan_key == *key) {
            plan.function_predecessor_block_name = predecessor_block_name;
        }
    }
}

void retarget_runtime_indexed_constructor_cleanup_index_operand(
    FunctionLoweringState& state,
    std::string_view key,
    std::string const& index_operand_value
) {
    if (key.empty() || index_operand_value.empty()) {
        return;
    }
    for (auto& plan : state.ownership_transfers.runtime_indexed_cleanup_emission_plans) {
        auto const plan_key = plan.owner_name + "[" + plan.index_expression_text + "]";
        if (plan_key != key) {
            continue;
        }
        plan.ir_plan.index_operand_value = index_operand_value;
        if (plan.ir_plan.complete) {
            plan.gated_ir_slice_lines = render_runtime_indexed_cleanup_ir_plan(plan.ir_plan);
            plan.gated_ir_slice_line_count = plan.gated_ir_slice_lines.size();
        }
    }
}

auto emit_runtime_indexed_constructor_source_slot_finalization(
    syntax::ExpressionSyntax const& argument,
    std::string_view expected_source_type,
    std::string_view expected_llvm_type,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> bool {
    if (!context.options.enable_runtime_indexed_cleanup_emission ||
        !context.options.enable_runtime_indexed_constructor_move) {
        return true;
    }

    auto candidate = runtime_indexed_partial_owner_for_constructor_argument(
        argument,
        expected_source_type,
        context.lowering,
        session.state
    );
    if (!candidate.has_value() ||
        !has_recorded_runtime_indexed_constructor_move(*candidate, session.state)) {
        return true;
    }
    if (candidate->static_length_value.empty()) {
        return true;
    }

    auto path = collect_named_aggregate_path(argument);
    if (!path.has_value() || path->base_expression == nullptr) {
        return false;
    }

    auto storage = named_aggregate_storage_for_name(path->base_expression->text, session.state);
    if (!storage.has_value() || !storage->source_type_name.has_value()) {
        return false;
    }

    auto cursor = initialize_aggregate_path_cursor(
        std::move(storage->storage),
        std::move(*storage->source_type_name),
        context.lowering
    );
    if (!cursor.has_value()) {
        return false;
    }

    for (auto const& step : path->steps) {
        if (step.kind == AggregatePathStepKind::member) {
            auto result = advance_aggregate_path_member_with_temporary(
                *cursor,
                step.field_name,
                context.lowering,
                session.state.next_temporary_index,
                output
            );
            if (result.error != AggregatePathError::none) {
                return false;
            }
            continue;
        }

        if (step.index_expression == nullptr) {
            return false;
        }
        auto lowered_index = lowered_expression(
            *step.index_expression,
            "i64",
            IntegerSignedness::unsigned_integer,
            context,
            session,
            output,
            std::nullopt
        );
        if (!lowered_index.has_value()) {
            return false;
        }

        auto result = advance_aggregate_path_index_with_temporary(
            *cursor,
            lowered_index->value,
            context.lowering,
            session.state.next_temporary_index,
            output
        );
        if (result.error != AggregatePathError::none) {
            return false;
        }
    }

    if (cursor->source_type_name != expected_source_type ||
        cursor->llvm_type_name != expected_llvm_type) {
        return false;
    }

    output << "  store " << expected_llvm_type << " zeroinitializer, ptr "
           << cursor->pointer << "\n";
    return true;
}

auto unsupported_indexed_constructor_ownership_detail(
    syntax::ExpressionSyntax const& argument,
    std::string_view expected_source_type,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session
) -> std::string {
    auto detail = std::string {
        "indexed constructor ownership move requires explicit partial ownership support"
    };
    if (!record_runtime_indexed_constructor_ownership(
            argument,
            expected_source_type,
            context,
            session,
            false
        )) {
        return detail;
    }
    detail += ": ";
    detail += runtime_indexed_partial_owner_report(
        session.state.ownership_transfers.runtime_indexed_partial_owners.back()
    );
    if (context.options.enable_runtime_indexed_fixed_array_constructor_move_only &&
        session.state.ownership_transfers.runtime_indexed_partial_owners.back().static_length_value.empty()) {
        detail += ": default runtime-index constructor move gate requires a static-length owner";
    }
    detail += ": ";
    detail += runtime_indexed_cleanup_skip_plan_report(
        session.state.ownership_transfers.runtime_indexed_cleanup_skip_plans.back()
    );
    detail += ": ";
    detail += runtime_indexed_cleanup_proof_gate_report(
        session.state.ownership_transfers.runtime_indexed_cleanup_proof_gates.back()
    );
    detail += ": ";
    detail += runtime_indexed_cleanup_emission_sketch_report(
        session.state.ownership_transfers.runtime_indexed_cleanup_emission_sketches.back()
    );
    detail += ": ";
    detail += runtime_indexed_cleanup_capability_report(
        session.state.ownership_transfers.runtime_indexed_cleanup_capabilities.back()
    );
    return detail;
}

auto is_indexed_owned_constructor_argument(
    syntax::ExpressionSyntax const& argument,
    std::string_view expected_source_type,
    LoweringContext const& context
) -> bool {
    if (!is_owned_transfer_source_type(expected_source_type, context)) {
        return false;
    }

    auto path = collect_named_aggregate_path(argument);
    if (!path.has_value()) {
        return false;
    }

    return std::ranges::any_of(path->steps, [](AggregatePathStep const& step) {
        return step.kind == AggregatePathStepKind::index;
    });
}

auto digit_value_for_base(char character, int base) -> std::optional<std::uint64_t> {
    auto value = std::optional<std::uint64_t> {};
    if (character >= '0' && character <= '9') {
        value = static_cast<std::uint64_t>(character - '0');
    } else if (character >= 'a' && character <= 'f') {
        value = static_cast<std::uint64_t>(character - 'a' + 10);
    } else if (character >= 'A' && character <= 'F') {
        value = static_cast<std::uint64_t>(character - 'A' + 10);
    }

    if (!value.has_value() || *value >= static_cast<std::uint64_t>(base)) {
        return std::nullopt;
    }
    return value;
}

auto normalized_integer_literal_text(std::string_view text) -> std::optional<std::string> {
    auto base = 10;
    auto digits = text;
    if (digits.starts_with("0x") || digits.starts_with("0X")) {
        base = 16;
        digits.remove_prefix(2);
    } else if (digits.starts_with("0b") || digits.starts_with("0B")) {
        base = 2;
        digits.remove_prefix(2);
    }
    if (digits.empty()) {
        return std::nullopt;
    }

    if (base == 10) {
        for (auto character : digits) {
            if (std::isdigit(static_cast<unsigned char>(character)) == 0) {
                return std::nullopt;
            }
        }
        return std::string {text};
    }

    auto value = std::uint64_t {0};
    for (auto character : digits) {
        auto digit = digit_value_for_base(character, base);
        if (!digit.has_value()) {
            return std::nullopt;
        }
        value = (value * static_cast<std::uint64_t>(base)) + *digit;
    }
    return std::to_string(value);
}

auto lowered_integer_literal(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness
) -> std::optional<LoweredExpression> {
    if (expression.kind != syntax::ExpressionKind::integer_literal) {
        return std::nullopt;
    }
    auto value = normalized_integer_literal_text(expression.text);
    if (!value.has_value()) {
        return std::nullopt;
    }

    return LoweredExpression {
        .type = std::string(expected_llvm_type),
        .value = std::move(*value),
        .signedness = expected_signedness,
    };
}

auto lowered_float_literal(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type
) -> std::optional<LoweredExpression> {
    if (expression.kind != syntax::ExpressionKind::float_literal ||
        (expected_llvm_type != "float" && expected_llvm_type != "double")) {
        return std::nullopt;
    }

    return LoweredExpression {
        .type = std::string(expected_llvm_type),
        .value = expression.text,
        .signedness = IntegerSignedness::not_integer,
    };
}

auto lowered_boolean_literal(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type
) -> std::optional<LoweredExpression> {
    if (expression.kind != syntax::ExpressionKind::boolean_literal || expected_llvm_type != "i1") {
        return std::nullopt;
    }

    return LoweredExpression {
        .type = "i1",
        .value = expression.text == "true" ? "1" : "0",
        .signedness = IntegerSignedness::not_integer,
    };
}

auto inferred_expression_type(
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType>;

auto signedness_for_expected_array_element(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> IntegerSignedness {
    auto inferred = inferred_expression_type(expression, context, state);
    if (inferred.has_value() && inferred->type == expected_llvm_type) {
        return inferred->signedness;
    }
    return IntegerSignedness::not_integer;
}

auto llvm_binary_instruction_for(
    std::string_view operator_text,
    IntegerSignedness signedness
) -> std::optional<std::string_view> {
    if (operator_text == "+") {
        return "add";
    }
    if (operator_text == "-") {
        return "sub";
    }
    if (operator_text == "*") {
        return "mul";
    }
    if (operator_text == "/") {
        return signedness == IntegerSignedness::signed_integer ? "sdiv" : "udiv";
    }
    if (operator_text == "%") {
        if (signedness == IntegerSignedness::not_integer) {
            return std::nullopt;
        }
        return signedness == IntegerSignedness::signed_integer ? "srem" : "urem";
    }
    if (operator_text == "bit_and") {
        return "and";
    }
    if (operator_text == "bit_or") {
        return "or";
    }
    if (operator_text == "bit_xor") {
        return "xor";
    }
    if (operator_text == "shift_left") {
        return "shl";
    }
    if (operator_text == "shift_right") {
        return signedness == IntegerSignedness::signed_integer ? "ashr" : "lshr";
    }
    return std::nullopt;
}

auto llvm_integer_comparison_predicate_for(
    std::string_view operator_text,
    IntegerSignedness signedness
) -> std::optional<std::string_view> {
    if (operator_text == "==") {
        return "eq";
    }
    if (operator_text == "!=") {
        return "ne";
    }
    if (operator_text == "<") {
        return signedness == IntegerSignedness::signed_integer ? "slt" : "ult";
    }
    if (operator_text == "<=") {
        return signedness == IntegerSignedness::signed_integer ? "sle" : "ule";
    }
    if (operator_text == ">") {
        return signedness == IntegerSignedness::signed_integer ? "sgt" : "ugt";
    }
    if (operator_text == ">=") {
        return signedness == IntegerSignedness::signed_integer ? "sge" : "uge";
    }
    return std::nullopt;
}

auto llvm_boolean_comparison_predicate_for(
    std::string_view operator_text
) -> std::optional<std::string_view> {
    if (operator_text == "==") {
        return "eq";
    }
    if (operator_text == "!=") {
        return "ne";
    }
    return std::nullopt;
}

auto is_integer_llvm_type_impl(std::string_view type) -> bool {
    return type == "i8" || type == "i16" || type == "i32" || type == "i64";
}

auto is_decimal_integer_text(std::string_view text) -> bool {
    return !text.empty() && std::ranges::all_of(text, [](char value) {
        return std::isdigit(static_cast<unsigned char>(value)) != 0;
    });
}

auto lowered_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output,
    std::optional<std::string_view> expected_source_type_name = std::nullopt
) -> std::optional<LoweredExpression>;

auto inferred_expression_type(
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType>;

auto is_low_level_intrinsic_name(std::string_view name) -> bool {
    return name == "address_of" || name == "raw_read" || name == "raw_write" ||
           name == "raw_offset" || name == "volatile_read" || name == "volatile_write";
}

auto is_write_intrinsic_name(std::string_view name) -> bool {
    return name == "raw_write" || name == "volatile_write";
}

auto is_concurrency_expression(syntax::ExpressionSyntax const& expression) -> bool {
    return expression.kind == syntax::ExpressionKind::task ||
           expression.kind == syntax::ExpressionKind::thread ||
           (expression.kind == syntax::ExpressionKind::unary && expression.text == "await");
}

auto pointer_pointee_source_type_name(std::string_view type_name) -> std::optional<std::string> {
    constexpr auto prefix = std::string_view {"Pointer<"};
    if (!type_name.starts_with(prefix) || !type_name.ends_with(">") ||
        type_name.size() <= prefix.size() + 1) {
        return std::nullopt;
    }

    return std::string(type_name.substr(prefix.size(), type_name.size() - prefix.size() - 1));
}

auto lowered_type_for_source_type_name(std::string_view source_type_name) -> std::optional<LoweredType> {
    auto type = syntax::TypeSyntax {
        .name = std::string(source_type_name),
    };
    auto lowered = llvm_type_for(type);
    if (!lowered.has_value() || *lowered == "void") {
        return std::nullopt;
    }

    return LoweredType {
        .type = std::string(*lowered),
        .signedness = integer_signedness_for(type),
    };
}

auto source_type_name_for_expression(
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string> {
    return source_type_name_for_expression(expression, context.lowering, state);
}

auto pointee_lowered_type_for_pointer_expression(
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType> {
    auto source_type = source_type_name_for_expression(expression, context, state);
    if (!source_type.has_value()) {
        return std::nullopt;
    }

    auto pointee_source_type = pointer_pointee_source_type_name(*source_type);
    return pointee_source_type.has_value()
        ? lowered_type_for_source_type_name(*pointee_source_type)
        : std::nullopt;
}

struct ResolvedMemberCall {
    MemberCallReceiverInference receiver;
    LoweredMethodLookup method;
};

struct NullSafeIncoming {
    std::string value;
    std::string block;
};

auto resolve_member_call(
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> ResolvedMemberCall {
    auto receiver = infer_member_call_receiver(expression, context.lowering, state);
    if (receiver.result != MemberCallReceiverInferenceResult::found) {
        return {.receiver = std::move(receiver)};
    }

    auto method = find_lowered_method_signature(
        context.lowering,
        receiver.receiver_type_name,
        receiver.method_name
    );
    return ResolvedMemberCall {
        .receiver = std::move(receiver),
        .method = std::move(method),
    };
}

auto inferred_expression_type(
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType> {
    if (expression.kind == syntax::ExpressionKind::name) {
        auto binding = state.immutable_bindings.find(expression.text);
        if (binding != state.immutable_bindings.end()) {
            return LoweredType {
                .type = binding->second.type,
                .signedness = binding->second.signedness,
            };
        }
        auto mutable_binding = state.mutable_bindings.find(expression.text);
        return mutable_binding == state.mutable_bindings.end()
            ? std::nullopt
            : std::optional<LoweredType> {mutable_binding->second.type};
    }

    if ((expression.kind == syntax::ExpressionKind::member_access ||
         expression.kind == syntax::ExpressionKind::index_access) &&
        expression.left != nullptr) {
        auto source_type = source_type_name_for_expression(expression, context, state);
        if (!source_type.has_value()) {
            return std::nullopt;
        }
        return lowered_type_for_source_type_name(*source_type, context.lowering);
    }

    if (expression.kind == syntax::ExpressionKind::call &&
        expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::member_access &&
        expression.left->left != nullptr &&
        expression.left->left->kind == syntax::ExpressionKind::name &&
        expression.left->text == "length" &&
        expression.arguments.empty()) {
        auto const& owner_name = expression.left->left->text;
        auto source_type = state.source_type_names.find(owner_name);
        if (source_type != state.source_type_names.end() &&
            (dynamic_array_element_source_type_name(source_type->second).has_value() ||
             view_element_source_type_name(source_type->second).has_value())) {
            return LoweredType {
                .type = "i64",
                .signedness = IntegerSignedness::signed_integer,
            };
        }
    }

    if (expression.kind == syntax::ExpressionKind::cast) {
        return lowered_type_for_source_type_name(expression.text, context.lowering);
    }

    if (expression.kind == syntax::ExpressionKind::unary && expression.left != nullptr) {
        if (expression.text == "not") {
            return LoweredType {
                .type = "i1",
                .signedness = IntegerSignedness::not_integer,
            };
        }
        if (expression.text == "bit_not") {
            auto operand = inferred_expression_type(*expression.left, context, state);
            if (operand.has_value() && is_integer_llvm_type_impl(operand->type) &&
                operand->signedness != IntegerSignedness::not_integer) {
                return operand;
            }
        }
        if (expression.text == "-") {
            auto operand = inferred_expression_type(*expression.left, context, state);
            if (operand.has_value() && is_integer_llvm_type_impl(operand->type) &&
                operand->signedness == IntegerSignedness::signed_integer) {
                return operand;
            }
        }
    }

    if (expression.kind == syntax::ExpressionKind::binary && expression.left != nullptr &&
        expression.right != nullptr) {
        if (expression.text == "and" || expression.text == "or") {
            return LoweredType {
                .type = "i1",
                .signedness = IntegerSignedness::not_integer,
            };
        }

        auto left = inferred_expression_type(*expression.left, context, state);
        auto right = inferred_expression_type(*expression.right, context, state);
        if (!left.has_value() || !right.has_value() || left->type != right->type ||
            left->signedness != right->signedness) {
            return std::nullopt;
        }

        if (left->type == "i1" && left->signedness == IntegerSignedness::not_integer &&
            llvm_boolean_comparison_predicate_for(expression.text).has_value()) {
            return LoweredType {
                .type = "i1",
                .signedness = IntegerSignedness::not_integer,
            };
        }
        if (left->signedness != IntegerSignedness::not_integer &&
            llvm_integer_comparison_predicate_for(expression.text, left->signedness).has_value()) {
            return LoweredType {
                .type = "i1",
                .signedness = IntegerSignedness::not_integer,
            };
        }
        if (llvm_binary_instruction_for(expression.text, left->signedness).has_value()) {
            return left;
        }
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name && expression.left->text == "Pointer" &&
        expression.arguments.size() == 1) {
        return LoweredType {
            .type = "ptr",
            .signedness = IntegerSignedness::not_integer,
        };
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name) {
        if (auto source_type = source_type_name_for_expression(expression, context, state)) {
            auto lowered_source_type = lowered_type_for_source_type_name(*source_type, context.lowering);
            if (lowered_source_type.has_value()) {
                return lowered_source_type;
            }
        }

        auto record = context.lowering.records.find(expression.left->text);
        if (record != context.lowering.records.end()) {
            return LoweredType {
                .type = record->second.llvm_type_name,
                .signedness = IntegerSignedness::not_integer,
            };
        }

        auto const& intrinsic_name = expression.left->text;
        if (is_write_intrinsic_name(intrinsic_name)) {
            return LoweredType {
                .type = "void",
                .signedness = IntegerSignedness::not_integer,
            };
        }
        if (intrinsic_name == "address_of") {
            return LoweredType {
                .type = "i64",
                .signedness = IntegerSignedness::not_integer,
            };
        }
        if (intrinsic_name == "raw_offset" && !expression.arguments.empty()) {
            auto source_type = inferred_expression_type(expression.arguments.front(), context, state);
            if (source_type.has_value() && (source_type->type == "ptr" || source_type->type == "i64")) {
                return source_type;
            }
        }
        if ((intrinsic_name == "raw_read" || intrinsic_name == "volatile_read") &&
            !expression.arguments.empty()) {
            auto pointee_type =
                pointee_lowered_type_for_pointer_expression(expression.arguments.front(), context, state);
            if (pointee_type.has_value()) {
                return pointee_type;
            }
        }

        auto function = context.lowering.functions.find(expression.left->text);
        if (function == context.lowering.functions.end()) {
            return std::nullopt;
        }
        return LoweredType {
            .type = function->second.return_type,
            .signedness = function->second.return_signedness,
        };
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::member_access) {
        if (expression.left->text == "join" &&
            expression.left->left != nullptr &&
            expression.left->left->kind == syntax::ExpressionKind::name) {
            auto thread_binding = state.thread_bindings.find(expression.left->left->text);
            if (thread_binding != state.thread_bindings.end()) {
                return thread_binding->second.result_type;
            }
        }

        auto resolved = resolve_member_call(expression, context, state);
        if (resolved.receiver.result != MemberCallReceiverInferenceResult::found ||
            resolved.method.result != LoweredMethodLookupResult::found ||
            resolved.method.method == nullptr ||
            !has_supported_function_signature_types(resolved.method.method->signature)) {
            return std::nullopt;
        }

        return LoweredType {
            .type = resolved.method.method->signature.return_type,
            .signedness = resolved.method.method->signature.return_signedness,
        };
    }

    if (expression.kind == syntax::ExpressionKind::unary &&
        expression.text == "await" &&
        expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name) {
        auto task_binding = state.task_bindings.find(expression.left->text);
        if (task_binding != state.task_bindings.end()) {
            return task_binding->second.result_type;
        }
    }

    if (expression.kind == syntax::ExpressionKind::array_literal) {
        if (expression.arguments.empty()) {
            return std::nullopt;
        }

        auto element_type = inferred_expression_type(expression.arguments.front(), context, state);
        if (!element_type.has_value()) {
            return std::nullopt;
        }
        for (auto index = std::size_t {1}; index < expression.arguments.size(); ++index) {
            auto next_type = inferred_expression_type(expression.arguments[index], context, state);
            if (!next_type.has_value() || next_type->type != element_type->type ||
                next_type->signedness != element_type->signedness) {
                return std::nullopt;
            }
        }

        return LoweredType {
            .type = "[" + std::to_string(expression.arguments.size()) + " x " + element_type->type + "]",
            .signedness = IntegerSignedness::not_integer,
        };
    }

    if (expression.kind == syntax::ExpressionKind::ternary) {
        auto source_type = source_type_name_for_expression(expression, context, state);
        return source_type.has_value()
            ? lowered_type_for_source_type_name(*source_type, context.lowering)
            : std::nullopt;
    }

    return std::nullopt;
}

auto lower_address_to_pointer(
    LoweredExpression address,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> LoweredExpression {
    auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << temporary_name << " = inttoptr i64 " << address.value << " to ptr\n";
    return LoweredExpression {
        .type = "ptr",
        .value = std::move(temporary_name),
        .signedness = IntegerSignedness::not_integer,
    };
}

auto lower_pointer_to_address(
    std::string pointer_value,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> LoweredExpression {
    auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << temporary_name << " = ptrtoint ptr " << pointer_value << " to i64\n";
    return LoweredExpression {
        .type = "i64",
        .value = std::move(temporary_name),
        .signedness = IntegerSignedness::not_integer,
    };
}

auto emit_pointer_load(
    std::string_view result_type,
    IntegerSignedness result_signedness,
    std::string_view pointer_value,
    bool is_volatile,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> LoweredExpression {
    auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << temporary_name << " = load ";
    if (is_volatile) {
        output << "volatile ";
    }
    output << result_type << ", ptr " << pointer_value << "\n";
    return LoweredExpression {
        .type = std::string(result_type),
        .value = std::move(temporary_name),
        .signedness = result_signedness,
    };
}

void emit_pointer_store(
    std::string_view value_type,
    std::string_view value,
    std::string_view pointer_value,
    bool is_volatile,
    std::ostringstream& output
) {
    output << "  store ";
    if (is_volatile) {
        output << "volatile ";
    }
    output << value_type << " " << value << ", ptr " << pointer_value << "\n";
}

auto emit_address_offset(
    std::string_view address_value,
    std::string_view offset_value,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> LoweredExpression {
    auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << temporary_name << " = add i64 " << address_value << ", " << offset_value << "\n";
    return LoweredExpression {
        .type = "i64",
        .value = std::move(temporary_name),
        .signedness = IntegerSignedness::not_integer,
    };
}

auto emit_pointer_offset(
    LoweredType const& pointee_type,
    std::string_view pointer_value,
    std::string_view offset_value,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> LoweredExpression {
    auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << temporary_name << " = getelementptr " << pointee_type.type << ", ptr "
           << pointer_value << ", i64 " << offset_value << "\n";
    return LoweredExpression {
        .type = "ptr",
        .value = std::move(temporary_name),
        .signedness = IntegerSignedness::not_integer,
    };
}

auto lower_pointer_operand(
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto inferred = inferred_expression_type(expression, context, session.state);
    if (inferred.has_value() && inferred->type == "ptr") {
        return lowered_expression(
            expression,
            "ptr",
            IntegerSignedness::not_integer,
            context,
            session,
            output
        );
    }

    auto source_type = source_type_name_for_expression(expression, context, session.state);
    if (inferred.has_value() && inferred->type == "i64" &&
        source_type.has_value() && *source_type == "Address") {
        auto address = lowered_expression(
            expression,
            "i64",
            IntegerSignedness::not_integer,
            context,
            session,
            output
        );
        if (!address.has_value()) {
            return std::nullopt;
        }
        return lower_address_to_pointer(std::move(*address), session, output);
    }

    return lowered_expression(
        expression,
        "ptr",
        IntegerSignedness::not_integer,
        context,
        session,
        output
    );
}

auto lower_address_operand(
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto inferred = inferred_expression_type(expression, context, session.state);
    if (inferred.has_value() && inferred->type == "ptr") {
        auto pointer = lowered_expression(
            expression,
            "ptr",
            IntegerSignedness::not_integer,
            context,
            session,
            output
        );
        if (!pointer.has_value()) {
            return std::nullopt;
        }

        return lower_pointer_to_address(std::move(pointer->value), session, output);
    }

    return lowered_expression(
        expression,
        "i64",
        IntegerSignedness::not_integer,
        context,
        session,
        output
    );
}

auto lower_array_literal_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    LoweringFailures& failures,
    std::ostringstream& output,
    std::optional<std::string_view> expected_source_type_name
) -> std::optional<LoweredExpression> {
    if (expression.kind != syntax::ExpressionKind::array_literal) {
        return std::nullopt;
    }

    auto array_type = parse_llvm_array_type(expected_llvm_type);
    if (!array_type.has_value()) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::type_mismatch,
            "array literal expected LLVM array type " + std::string(expected_llvm_type)
        );
        return std::nullopt;
    }
    if (expression.arguments.size() != array_type->length) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "array literal element count"
        );
        return std::nullopt;
    }
    if (array_type->length == 0) {
        return LoweredExpression {
            .type = std::string(expected_llvm_type),
            .value = "zeroinitializer",
            .signedness = IntegerSignedness::not_integer,
        };
    }

    auto element_source_type_name = expected_source_type_name.has_value()
        ? array_element_source_type_name(*expected_source_type_name)
        : std::optional<std::string> {};
    auto element_expected_source_type = element_source_type_name.has_value()
        ? std::optional<std::string_view> {*element_source_type_name}
        : std::optional<std::string_view> {};

    auto aggregate_value = std::string {"undef"};
    for (auto index = std::size_t {0}; index < expression.arguments.size(); ++index) {
        auto const& element = expression.arguments[index];
        auto lowered_element = lowered_expression(
            element,
            array_type->element_type,
            signedness_for_expected_array_element(
                element,
                array_type->element_type,
                context,
                session.state
            ),
            context,
            session,
            output,
            element_expected_source_type
        );
        if (!lowered_element.has_value()) {
            return std::nullopt;
        }

        auto aggregate_name = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << aggregate_name << " = insertvalue " << expected_llvm_type << " "
               << aggregate_value << ", " << array_type->element_type << " "
               << lowered_element->value << ", " << index << "\n";
        aggregate_value = std::move(aggregate_name);
    }

    return LoweredExpression {
        .type = std::string(expected_llvm_type),
        .value = std::move(aggregate_value),
        .signedness = IntegerSignedness::not_integer,
    };
}

auto lower_record_constructor_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    LoweringFailures& failures,
    std::ostringstream& output,
    std::optional<std::string_view> expected_source_type_name
) -> std::optional<LoweredExpression> {
    if (expression.left == nullptr || expression.left->kind != syntax::ExpressionKind::name) {
        return std::nullopt;
    }

    auto layout = expected_source_type_name.has_value()
        ? context.lowering.records.find(std::string(*expected_source_type_name))
        : context.lowering.records.end();
    if (layout == context.lowering.records.end()) {
        layout = context.lowering.records.find(expression.left->text);
    }
    if (layout == context.lowering.records.end()) {
        return std::nullopt;
    }
    if (layout->second.name != expression.left->text &&
        !layout->second.name.starts_with(expression.left->text + "<")) {
        return std::nullopt;
    }
    if (layout->second.llvm_type_name != expected_llvm_type) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::type_mismatch,
            expression.left->text + " has LLVM type " + layout->second.llvm_type_name +
                ", expected " + std::string(expected_llvm_type)
        );
        return std::nullopt;
    }
    if (layout->second.fields.size() != expression.arguments.size()) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::call_arity_mismatch,
            expression.left->text + " expects " + std::to_string(layout->second.fields.size()) +
                " arguments, got " + std::to_string(expression.arguments.size())
        );
        return std::nullopt;
    }
    if (layout->second.fields.empty()) {
        return LoweredExpression {
            .type = layout->second.llvm_type_name,
            .value = "zeroinitializer",
            .signedness = IntegerSignedness::not_integer,
        };
    }

    auto aggregate_value = std::string {"undef"};
    for (auto index = std::size_t {0}; index < layout->second.fields.size(); ++index) {
        auto const& field = layout->second.fields[index];
        if (field.llvm_type.empty() || field.llvm_type == "void") {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                "record constructor field layout"
            );
            return std::nullopt;
        }

        if (is_indexed_owned_constructor_argument(
                expression.arguments[index],
                field.source_type_name,
                context.lowering
            ) && !has_owned_literal_indexed_constructor_argument_transfer(
                expression.arguments[index],
                field.source_type_name,
                context.lowering,
                session.state
            ) && !runtime_indexed_constructor_move_enabled(
                expression.arguments[index],
                field.source_type_name,
                context,
                session
            )) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                unsupported_indexed_constructor_ownership_detail(
                    expression.arguments[index],
                    field.source_type_name,
                    context,
                    session
                )
            );
            return std::nullopt;
        }

        auto runtime_indexed_argument_key = runtime_indexed_constructor_argument_key(
            expression.arguments[index],
            field.source_type_name,
            context.lowering,
            session.state
        );
        auto lowered_field = std::optional<LoweredExpression> {};
        {
            auto runtime_indexed_argument_scope = RuntimeIndexedConstructorArgumentScope {
                session.state,
                runtime_indexed_argument_key
            };
            lowered_field = lowered_expression(
                expression.arguments[index],
                field.llvm_type,
                integer_signedness_for(syntax::TypeSyntax {.name = field.source_type_name}),
                context,
                session,
                output,
                field.source_type_name.empty()
                    ? std::optional<std::string_view> {}
                    : std::optional<std::string_view> {field.source_type_name}
            );
        }
        if (!lowered_field.has_value()) {
            return std::nullopt;
        }
        retarget_runtime_indexed_constructor_cleanup_predecessor(
            session.state,
            runtime_indexed_argument_key,
            session.state.current_block
        );

        auto aggregate_name = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << aggregate_name << " = insertvalue " << layout->second.llvm_type_name << " "
               << aggregate_value << ", " << field.llvm_type << " " << lowered_field->value << ", "
               << index << "\n";
        aggregate_value = std::move(aggregate_name);
        mark_constructor_owned_argument_cleanup_consumed(
            expression.arguments[index],
            field.source_type_name,
            context,
            session
        );
        if (!emit_runtime_indexed_constructor_source_slot_finalization(
                expression.arguments[index],
                field.source_type_name,
                field.llvm_type,
                context,
                session,
                output
            )) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                "runtime-index constructor source slot finalization"
            );
            return std::nullopt;
        }
    }

    return LoweredExpression {
        .type = layout->second.llvm_type_name,
        .value = std::move(aggregate_value),
        .signedness = IntegerSignedness::not_integer,
    };
}

auto find_choice_variant(
    LoweredChoiceLayout const& layout,
    std::string_view variant_name
) -> LoweredChoiceVariant const* {
    for (auto const& variant : layout.variants) {
        if (variant.name == variant_name) {
            return &variant;
        }
    }
    return nullptr;
}

auto choice_payload_field_type(LoweredChoiceLayout const& layout) -> std::optional<std::string_view> {
    constexpr auto prefix = std::string_view {"{ i32,"};
    if (!layout.llvm_type_name.starts_with(prefix) || !layout.llvm_type_name.ends_with("}")) {
        return std::nullopt;
    }
    auto field = std::string_view(layout.llvm_type_name).substr(
        prefix.size(),
        layout.llvm_type_name.size() - prefix.size() - 1
    );
    while (!field.empty() && field.front() == ' ') {
        field.remove_prefix(1);
    }
    while (!field.empty() && field.back() == ' ') {
        field.remove_suffix(1);
    }
    if (field.empty()) {
        return std::nullopt;
    }
    return field;
}

auto record_unsupported_choice_constructor_abi_failure(
    LoweringFailures& failures,
    LoweredChoiceLayout const& layout,
    std::string_view constructor_name
) -> void {
    auto source_type_name = layout.source_type_name.empty()
        ? layout.name
        : layout.source_type_name;
    auto reason = layout.unsupported_abi_reason.empty()
        ? std::string("choice type does not yet have a lowered choice ABI")
        : layout.unsupported_abi_reason;
    record_expression_lowering_failure(
        failures,
        ExpressionLoweringFailureReason::unsupported_choice_abi,
        "choice constructor '" + std::string(constructor_name) + "' for '" +
            source_type_name + "': " + reason
    );
}

auto record_unsupported_aggregate_path_failure(
    LoweringFailures& failures,
    std::string_view operation,
    AggregatePathError error
) -> void {
    record_expression_lowering_failure(
        failures,
        ExpressionLoweringFailureReason::unsupported_aggregate_path,
        std::string(operation) + ": " + std::string(render_aggregate_path_error(error))
    );
}

struct ChoiceConstructorLayoutLookup {
    LoweredChoiceLayout const* layout = nullptr;
    bool ambiguous = false;
};

auto choice_constructor_layout_for(
    std::string_view variant_name,
    std::string_view expected_llvm_type,
    LoweringContext const& context,
    std::optional<std::string_view> expected_source_type_name
) -> ChoiceConstructorLayoutLookup {
    auto const* match = static_cast<LoweredChoiceLayout const*>(nullptr);
    for (auto const& [choice_name, layout] : context.choices) {
        (void)choice_name;
        if (layout.llvm_type_name != expected_llvm_type || find_choice_variant(layout, variant_name) == nullptr) {
            continue;
        }
        if (expected_source_type_name.has_value() && layout.source_type_name != *expected_source_type_name) {
            continue;
        }
        if (match != nullptr) {
            return ChoiceConstructorLayoutLookup {
                .ambiguous = true,
            };
        }
        match = &layout;
    }
    return ChoiceConstructorLayoutLookup {
        .layout = match,
    };
}

auto lower_choice_constructor_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    LoweringFailures& failures,
    std::ostringstream& output,
    std::optional<std::string_view> expected_source_type_name
) -> std::optional<LoweredExpression> {
    auto const* constructor_name = static_cast<std::string const*>(nullptr);
    auto const* arguments = static_cast<std::vector<syntax::ExpressionSyntax> const*>(nullptr);
    if (expression.kind == syntax::ExpressionKind::name) {
        constructor_name = &expression.text;
    } else if (expression.kind == syntax::ExpressionKind::call &&
               expression.left != nullptr &&
               expression.left->kind == syntax::ExpressionKind::name) {
        constructor_name = &expression.left->text;
        arguments = &expression.arguments;
    }
    if (constructor_name == nullptr) {
        return std::nullopt;
    }

    auto layout_lookup = choice_constructor_layout_for(
        *constructor_name,
        expected_llvm_type,
        context.lowering,
        expected_source_type_name
    );
    if (layout_lookup.ambiguous) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "choice constructor '" + *constructor_name +
                "' is ambiguous for lowered choice ABI type '" + std::string(expected_llvm_type) + "'"
        );
        return std::nullopt;
    }
    auto const* layout = layout_lookup.layout;
    if (layout == nullptr) {
        return std::nullopt;
    }
    auto const* variant = find_choice_variant(*layout, *constructor_name);
    if (variant == nullptr) {
        return std::nullopt;
    }

    auto const argument_count = arguments == nullptr ? std::size_t {0} : arguments->size();
    if (argument_count != variant->payloads.size()) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::call_arity_mismatch,
            *constructor_name + " expects " + std::to_string(variant->payloads.size()) +
                " arguments, got " + std::to_string(argument_count)
        );
        return std::nullopt;
    }

    if (layout->llvm_type_name == "i32") {
        return LoweredExpression {
            .type = layout->llvm_type_name,
            .value = std::to_string(variant->tag),
            .signedness = IntegerSignedness::unsigned_integer,
        };
    }

    auto tag_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << tag_name << " = insertvalue " << layout->llvm_type_name << " undef, i32 "
           << variant->tag << ", 0\n";
    auto aggregate_value = std::move(tag_name);
    if (!variant->payloads.empty()) {
        auto payload_field_type = choice_payload_field_type(*layout);
        if (!payload_field_type.has_value()) {
            record_unsupported_choice_constructor_abi_failure(failures, *layout, *constructor_name);
            return std::nullopt;
        }
        if (variant->lowered_payload_type.empty()) {
            record_unsupported_choice_constructor_abi_failure(failures, *layout, *constructor_name);
            return std::nullopt;
        }

        auto payload_value = std::string {};
        if (variant->payloads.size() == 1) {
            auto const& payload = variant->payloads.front();
            if (is_indexed_owned_constructor_argument(
                    arguments->front(),
                    payload.source_type_name,
                    context.lowering
                ) && !has_owned_literal_indexed_constructor_argument_transfer(
                    arguments->front(),
                    payload.source_type_name,
                    context.lowering,
                    session.state
                ) && !runtime_indexed_constructor_move_enabled(
                    arguments->front(),
                    payload.source_type_name,
                    context,
                    session
                )) {
                record_expression_lowering_failure(
                    failures,
                    ExpressionLoweringFailureReason::unsupported_expression,
                    unsupported_indexed_constructor_ownership_detail(
                        arguments->front(),
                        payload.source_type_name,
                        context,
                        session
                    )
                );
                return std::nullopt;
            }

            auto runtime_indexed_argument_key = runtime_indexed_constructor_argument_key(
                arguments->front(),
                payload.source_type_name,
                context.lowering,
                session.state
            );
            auto lowered_payload = std::optional<LoweredExpression> {};
            {
                auto runtime_indexed_argument_scope = RuntimeIndexedConstructorArgumentScope {
                    session.state,
                    runtime_indexed_argument_key
                };
                lowered_payload = lowered_expression(
                    arguments->front(),
                    payload.llvm_type,
                    integer_signedness_for(syntax::TypeSyntax {.name = payload.source_type_name}),
                    context,
                    session,
                    output
                );
            }
            if (!lowered_payload.has_value()) {
                return std::nullopt;
            }
            retarget_runtime_indexed_constructor_cleanup_predecessor(
                session.state,
                runtime_indexed_argument_key,
                session.state.current_block
            );
            mark_constructor_owned_argument_cleanup_consumed(
                arguments->front(),
                payload.source_type_name,
                context,
                session
            );
            if (!emit_runtime_indexed_constructor_source_slot_finalization(
                    arguments->front(),
                    payload.source_type_name,
                    payload.llvm_type,
                    context,
                    session,
                    output
                )) {
                record_expression_lowering_failure(
                    failures,
                    ExpressionLoweringFailureReason::unsupported_expression,
                    "runtime-index constructor source slot finalization"
                );
                return std::nullopt;
            }
            payload_value = lowered_payload->value;
        } else {
            payload_value = "undef";
            for (auto index = std::size_t {0}; index < variant->payloads.size(); ++index) {
                auto const& payload = variant->payloads[index];
                if (is_indexed_owned_constructor_argument(
                        (*arguments)[index],
                        payload.source_type_name,
                        context.lowering
                    ) && !has_owned_literal_indexed_constructor_argument_transfer(
                        (*arguments)[index],
                        payload.source_type_name,
                        context.lowering,
                        session.state
                    ) && !runtime_indexed_constructor_move_enabled(
                        (*arguments)[index],
                        payload.source_type_name,
                        context,
                        session
                    )) {
                    record_expression_lowering_failure(
                        failures,
                        ExpressionLoweringFailureReason::unsupported_expression,
                        unsupported_indexed_constructor_ownership_detail(
                            (*arguments)[index],
                            payload.source_type_name,
                            context,
                            session
                        )
                    );
                    return std::nullopt;
                }

                auto runtime_indexed_argument_key = runtime_indexed_constructor_argument_key(
                    (*arguments)[index],
                    payload.source_type_name,
                    context.lowering,
                    session.state
                );
                auto lowered_payload = std::optional<LoweredExpression> {};
                {
                    auto runtime_indexed_argument_scope = RuntimeIndexedConstructorArgumentScope {
                        session.state,
                        runtime_indexed_argument_key
                    };
                    lowered_payload = lowered_expression(
                        (*arguments)[index],
                        payload.llvm_type,
                        integer_signedness_for(syntax::TypeSyntax {.name = payload.source_type_name}),
                        context,
                        session,
                        output
                    );
                }
                if (!lowered_payload.has_value()) {
                    return std::nullopt;
                }
                retarget_runtime_indexed_constructor_cleanup_predecessor(
                    session.state,
                    runtime_indexed_argument_key,
                    session.state.current_block
                );
                mark_constructor_owned_argument_cleanup_consumed(
                    (*arguments)[index],
                    payload.source_type_name,
                    context,
                    session
                );
                if (!emit_runtime_indexed_constructor_source_slot_finalization(
                        (*arguments)[index],
                        payload.source_type_name,
                        payload.llvm_type,
                        context,
                        session,
                        output
                    )) {
                    record_expression_lowering_failure(
                        failures,
                        ExpressionLoweringFailureReason::unsupported_expression,
                        "runtime-index constructor source slot finalization"
                    );
                    return std::nullopt;
                }
                auto payload_part_name = next_llvm_temporary_name(session.state.next_temporary_index);
                output << "  " << payload_part_name << " = insertvalue " << variant->lowered_payload_type
                       << " " << payload_value << ", " << payload.llvm_type << " "
                       << lowered_payload->value << ", " << index << "\n";
                payload_value = std::move(payload_part_name);
            }
        }

        if (*payload_field_type != variant->lowered_payload_type) {
            auto payload_storage = next_llvm_temporary_name(session.state.next_temporary_index);
            output << "  " << payload_storage << " = alloca " << *payload_field_type << ", align 8\n";
            output << "  store " << variant->lowered_payload_type << " " << payload_value << ", ptr "
                   << payload_storage << ", align 8\n";
            payload_value = next_llvm_temporary_name(session.state.next_temporary_index);
            output << "  " << payload_value << " = load " << *payload_field_type << ", ptr "
                   << payload_storage << ", align 8\n";
        }
        auto payload_name = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << payload_name << " = insertvalue " << layout->llvm_type_name << " "
               << aggregate_value << ", " << *payload_field_type << " " << payload_value
               << ", 1\n";
        aggregate_value = std::move(payload_name);
    }

    return LoweredExpression {
        .type = layout->llvm_type_name,
        .value = std::move(aggregate_value),
        .signedness = IntegerSignedness::not_integer,
    };
}

struct AggregateAddressBase {
    std::string pointer_value;
    std::string source_type_name;
};

auto resolve_aggregate_address_base(
    syntax::ExpressionSyntax const& base_expression,
    std::string_view base_source_type,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<AggregateAddressBase> {
    if (auto record_name = pointer_pointee_source_type_name(base_source_type)) {
        auto base_pointer = lower_pointer_operand(base_expression, context, session, output);
        if (!base_pointer.has_value()) {
            return std::nullopt;
        }
        return AggregateAddressBase {
            .pointer_value = std::move(base_pointer->value),
            .source_type_name = std::move(*record_name),
        };
    }

    if (base_expression.kind != syntax::ExpressionKind::name) {
        return std::nullopt;
    }

    auto binding = session.state.mutable_bindings.find(base_expression.text);
    if (binding == session.state.mutable_bindings.end()) {
        return std::nullopt;
    }
    return AggregateAddressBase {
        .pointer_value = binding->second.storage,
        .source_type_name = std::string(base_source_type),
    };
}

auto advance_address_of_aggregate_path_step(
    AggregatePathCursor& cursor,
    AggregatePathStep const& step,
    LoweringEmissionContext const& context,
    LoweringFailures& failures,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> bool {
    if (step.kind == AggregatePathStepKind::member) {
        auto result = advance_aggregate_path_member_with_temporary(
            cursor,
            step.field_name,
            context.lowering,
            session.state.next_temporary_index,
            output
        );
        if (result.error != AggregatePathError::none) {
            record_unsupported_aggregate_path_failure(failures, "address_of member path", result.error);
            return false;
        }
        return true;
    }

    if (step.index_expression == nullptr) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_aggregate_path,
            "address_of indexed field layout"
        );
        return false;
    }

    auto lowered_index = lowered_expression(
        *step.index_expression,
        "i64",
        IntegerSignedness::unsigned_integer,
        context,
        session,
        output
    );
    if (!lowered_index.has_value()) {
        return false;
    }

    auto result = advance_aggregate_path_index_with_temporary(
        cursor,
        lowered_index->value,
        context.lowering,
        session.state.next_temporary_index,
        output
    );
    if (result.error != AggregatePathError::none) {
        record_unsupported_aggregate_path_failure(failures, "address_of index path", result.error);
        return false;
    }
    return true;
}

auto lower_pointer_record_field_address(
    syntax::ExpressionSyntax const& operand,
    LoweringEmissionContext const& context,
    LoweringFailures& failures,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto path = collect_aggregate_path(operand);
    if (path.steps.empty() || path.base_expression == nullptr) {
        return std::nullopt;
    }

    auto const& base_expression = *path.base_expression;
    auto base_source_type = source_type_name_for_expression(base_expression, context, session.state);
    if (!base_source_type.has_value()) {
        return std::nullopt;
    }

    auto base = resolve_aggregate_address_base(
        base_expression,
        *base_source_type,
        context,
        session,
        output
    );
    if (!base.has_value()) {
        return std::nullopt;
    }

    auto cursor = initialize_aggregate_path_cursor(
        std::move(base->pointer_value),
        std::move(base->source_type_name),
        context.lowering
    );
    if (!cursor.has_value()) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_aggregate_path,
            "address_of field source record layout"
        );
        return std::nullopt;
    }

    for (auto const& step : path.steps) {
        if (!advance_address_of_aggregate_path_step(
                *cursor,
                step,
                context,
                failures,
                session,
                output
            )) {
            return std::nullopt;
        }
    }

    return lower_pointer_to_address(cursor->pointer, session, output);
}

auto lower_address_of_intrinsic(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    LoweringEmissionContext const& context,
    LoweringFailures& failures,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (expected_llvm_type != "i64" || expression.arguments.size() != 1) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "address_of"
        );
        return std::nullopt;
    }

    auto const& operand = expression.arguments.front();
    if (auto field_address = lower_pointer_record_field_address(
            operand,
            context,
            failures,
            session,
            output
        )) {
        return field_address;
    }

    if (operand.kind != syntax::ExpressionKind::name) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "address_of currently only lowers mutable local names and aggregate fields"
        );
        return std::nullopt;
    }

    auto binding = session.state.mutable_bindings.find(operand.text);
    if (binding == session.state.mutable_bindings.end()) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "address_of currently only lowers mutable local names and aggregate fields"
        );
        return std::nullopt;
    }

    return lower_pointer_to_address(binding->second.storage, session, output);
}

auto lower_pointer_constructor_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    LoweringFailures& failures,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (expected_llvm_type != "ptr") {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "Pointer construction"
        );
        return std::nullopt;
    }
    if (expression.arguments.size() != 1) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "Pointer construction"
        );
        return std::nullopt;
    }

    auto source = lowered_expression(
        expression.arguments.front(),
        "i64",
        IntegerSignedness::not_integer,
        context,
        session,
        output
    );
    if (!source.has_value()) {
        return std::nullopt;
    }

    return lower_address_to_pointer(std::move(*source), session, output);
}

auto lower_raw_offset_intrinsic(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    LoweringFailures& failures,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (expression.arguments.size() != 2) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "raw_offset"
        );
        return std::nullopt;
    }

    auto offset = lowered_expression(
        expression.arguments[1],
        "i64",
        IntegerSignedness::unsigned_integer,
        context,
        session,
        output
    );
    if (!offset.has_value()) {
        return std::nullopt;
    }

    if (expected_llvm_type == "i64") {
        auto address = lower_address_operand(expression.arguments.front(), context, session, output);
        if (!address.has_value()) {
            return std::nullopt;
        }

        return emit_address_offset(address->value, offset->value, session, output);
    }

    if (expected_llvm_type != "ptr") {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::type_mismatch,
            "raw_offset expected ptr or i64 result"
        );
        return std::nullopt;
    }

    auto pointee_type =
        pointee_lowered_type_for_pointer_expression(expression.arguments.front(), context, session.state);
    if (!pointee_type.has_value()) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "raw_offset requires a known Pointer<T> source"
        );
        return std::nullopt;
    }

    auto pointer = lower_pointer_operand(expression.arguments.front(), context, session, output);
    if (!pointer.has_value()) {
        return std::nullopt;
    }

    return emit_pointer_offset(*pointee_type, pointer->value, offset->value, session, output);
}

auto lower_read_intrinsic(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    bool is_volatile,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    LoweringFailures& failures,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (expected_llvm_type == "void" || expression.arguments.size() != 1) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            is_volatile ? "volatile_read" : "raw_read"
        );
        return std::nullopt;
    }

    auto pointer = lower_pointer_operand(expression.arguments.front(), context, session, output);
    if (!pointer.has_value()) {
        return std::nullopt;
    }

    return emit_pointer_load(
        expected_llvm_type,
        expected_signedness,
        pointer->value,
        is_volatile,
        session,
        output
    );
}

auto lower_write_intrinsic(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    bool is_volatile,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    LoweringFailures& failures,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (expected_llvm_type != "void" || expression.arguments.size() != 2) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            is_volatile ? "volatile_write" : "raw_write"
        );
        return std::nullopt;
    }

    auto value_type =
        pointee_lowered_type_for_pointer_expression(expression.arguments.front(), context, session.state);
    if (!value_type.has_value()) {
        value_type = inferred_expression_type(expression.arguments[1], context, session.state);
    }
    if (!value_type.has_value() || value_type->type.empty() || value_type->type == "void") {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::cannot_infer_operand_type,
            is_volatile ? "volatile_write value" : "raw_write value"
        );
        return std::nullopt;
    }

    auto pointer = lower_pointer_operand(expression.arguments.front(), context, session, output);
    if (!pointer.has_value()) {
        return std::nullopt;
    }
    auto value = lowered_expression(
        expression.arguments[1],
        value_type->type,
        value_type->signedness,
        context,
        session,
        output
    );
    if (!value.has_value()) {
        return std::nullopt;
    }

    emit_pointer_store(value_type->type, value->value, pointer->value, is_volatile, output);
    return LoweredExpression {
        .type = "void",
        .value = "",
        .signedness = IntegerSignedness::not_integer,
    };
}

auto lower_aggregate_path_read_from_storage(
    syntax::ExpressionSyntax const& expression,
    AggregatePath const& path,
    std::string storage,
    std::string_view base_source_type_name,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto inferred = inferred_expression_type(expression, context, session.state);
    if (!inferred.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            expression.text
        );
        return std::nullopt;
    }
    if (inferred->type != expected_llvm_type) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::type_mismatch,
            expression.text
        );
        return std::nullopt;
    }
    if (inferred->signedness != expected_signedness) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::signedness_mismatch,
            expression.text
        );
        return std::nullopt;
    }

    auto cursor = initialize_aggregate_path_cursor(
        std::move(storage),
        std::string(base_source_type_name),
        context.lowering
    );
    if (!cursor.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            expression.text
        );
        return std::nullopt;
    }

    if (auto moved_member_name = consumed_owned_aggregate_path_name(
            path,
            base_source_type_name,
            context.lowering,
            session.state
        )) {
        record_use_after_move_failure(session.failures, *moved_member_name);
        return std::nullopt;
    }

    for (auto const& step : path.steps) {
        if (step.kind == AggregatePathStepKind::member) {
            auto result = advance_aggregate_path_member_with_temporary(
                *cursor,
                step.field_name,
                context.lowering,
                session.state.next_temporary_index,
                output
            );
            if (result.error != AggregatePathError::none) {
                record_unsupported_aggregate_path_failure(
                    session.failures,
                    "aggregate member read '" + expression.text + "'",
                    result.error
                );
                return std::nullopt;
            }
            continue;
        }

        if (step.index_expression == nullptr) {
            record_expression_lowering_failure(
                session.failures,
                ExpressionLoweringFailureReason::unsupported_aggregate_path,
                expression.text
            );
            return std::nullopt;
        }

        auto lowered_index = lowered_expression(
            *step.index_expression,
            "i64",
            IntegerSignedness::unsigned_integer,
            context,
            session,
            output
        );
        if (!lowered_index.has_value()) {
            return std::nullopt;
        }

        auto result = advance_aggregate_path_index_with_temporary(
            *cursor,
            lowered_index->value,
            context.lowering,
            session.state.next_temporary_index,
            output
        );
        if (result.error != AggregatePathError::none) {
            record_unsupported_aggregate_path_failure(
                session.failures,
                "aggregate index read '" + expression.text + "'",
                result.error
            );
            return std::nullopt;
        }
    }

    auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
    return emit_aggregate_path_cursor_load(
        *cursor,
        expected_llvm_type,
        expected_signedness,
        std::move(temporary_name),
        output
    );
}

auto lower_addressable_aggregate_path_read(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto path = collect_named_aggregate_path(expression);
    if (!path.has_value()) {
        return std::nullopt;
    }

    auto storage = named_aggregate_storage_for_name(path->base_expression->text, session.state);
    if (!storage.has_value()) {
        return std::nullopt;
    }

    if (!storage->source_type_name.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            expression.text
        );
        return std::nullopt;
    }

    return lower_aggregate_path_read_from_storage(
        expression,
        *path,
        std::move(storage->storage),
        *storage->source_type_name,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        output
    );
}

auto lower_temporary_aggregate_path_read(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto path = collect_temporary_aggregate_path(expression);
    if (!path.has_value()) {
        return std::nullopt;
    }

    auto base_source_type =
        source_type_name_for_expression(*path->base_expression, context, session.state);
    if (!base_source_type.has_value()) {
        return std::nullopt;
    }

    auto base_llvm_type = llvm_type_for_source_type_name(*base_source_type, context.lowering);
    if (!base_llvm_type.has_value() || *base_llvm_type == "void") {
        return std::nullopt;
    }

    auto lowered_base = lowered_expression(
        *path->base_expression,
        *base_llvm_type,
        IntegerSignedness::not_integer,
        context,
        session,
        output
    );
    if (!lowered_base.has_value()) {
        return std::nullopt;
    }

    auto storage = spill_aggregate_value_to_temporary_storage(*lowered_base, session, output);
    if (!storage.has_value()) {
        return std::nullopt;
    }

    return lower_aggregate_path_read_from_storage(
        expression,
        *path,
        std::move(*storage),
        *base_source_type,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        output
    );
}

auto lower_dynamic_array_index_read(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (!context.options.enable_dynamic_array_index_lowering ||
        expression.kind != syntax::ExpressionKind::index_access ||
        expression.left == nullptr ||
        expression.left->kind != syntax::ExpressionKind::name ||
        expression.arguments.size() != 1) {
        return std::nullopt;
    }

    auto const& owner_name = expression.left->text;
    if (auto moved_name = moved_owned_dynamic_array_binding_name(owner_name, session.state)) {
        record_use_after_move_failure(session.failures, *moved_name);
        return std::nullopt;
    }
    auto source_type = session.state.source_type_names.find(owner_name);
    if (source_type == session.state.source_type_names.end()) {
        return std::nullopt;
    }
    auto element_source_type = dynamic_array_element_source_type_name(source_type->second);
    if (!element_source_type.has_value()) {
        return std::nullopt;
    }
    auto const index_expression_text = runtime_index_expression_key(expression.arguments.front());
    auto const runtime_index_constructor_move_recorded =
        std::ranges::any_of(
            session.state.ownership_transfers.runtime_indexed_partial_owners,
            [&](RuntimeIndexedPartialOwner const& owner) {
                return owner.constructor_move_enabled &&
                    owner.owner_name == owner_name &&
                    owner.index_expression_text == index_expression_text &&
                    owner.element_source_type_name == *element_source_type;
            }
        );
    auto const runtime_index_constructor_argument_key =
        owner_name + "[" + index_expression_text + "]";
    if (runtime_index_constructor_move_recorded &&
        !runtime_indexed_constructor_argument_active(
            session.state,
            runtime_index_constructor_argument_key
        )) {
        record_use_after_move_failure(
            session.failures,
            runtime_index_constructor_argument_key
        );
        return std::nullopt;
    }
    if (is_owned_transfer_source_type(*element_source_type, context.lowering) &&
        !runtime_index_constructor_move_recorded) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "DynamicArray index read of owned element requires a non-owning projection"
        );
        return std::nullopt;
    }

    auto storage = aggregate_storage_for_name(owner_name, session.state);
    if (!storage.has_value()) {
        return std::nullopt;
    }

    auto plan = plan_dynamic_array_descriptor_cleanup(
        owner_name,
        source_type->second,
        context.lowering
    );
    if (!plan.has_value()) {
        return std::nullopt;
    }
    auto element_signedness = integer_signedness_for(syntax::TypeSyntax {
        .name = *element_source_type,
    });
    if (plan->element_llvm_type != expected_llvm_type || element_signedness != expected_signedness) {
        return std::nullopt;
    }

    auto lowered_index = lowered_expression(
        expression.arguments.front(),
        "i64",
        IntegerSignedness::unsigned_integer,
        context,
        session,
        output
    );
    if (!lowered_index.has_value()) {
        return std::nullopt;
    }
    retarget_runtime_indexed_constructor_cleanup_index_operand(
        session.state,
        runtime_index_constructor_argument_key,
        lowered_index->value
    );

    auto prefix = "%" + owner_name + ".dynamic_array_index" +
        std::to_string(session.state.next_temporary_index++);
    output << emit_dynamic_array_descriptor_load(
        prefix + ".descriptor",
        *storage
    );
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".length",
        prefix + ".descriptor",
        DynamicArrayDescriptorField::length
    );
    output << emit_dynamic_array_bounds_check(
        prefix + ".in_bounds",
        lowered_index->value,
        prefix + ".length",
        DynamicArrayBoundsCheckKind::index_within_length
    );
    auto block_index = next_llvm_block_index(session.state.next_block_index);
    auto value_block = llvm_block_name("dynamic_array.index.in_bounds", block_index);
    auto failure_block = llvm_block_name("dynamic_array.index.out_of_bounds", block_index);
    emit_llvm_conditional_branch(
        output,
        prefix + ".in_bounds",
        value_block,
        failure_block
    );
    emit_llvm_block_label(output, failure_block);
    output << "  call void @__orison_dynamic_array_bounds_failed()\n";
    emit_llvm_unreachable(output);
    emit_llvm_block_label(output, value_block);
    session.state.current_block = value_block;
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".data",
        prefix + ".descriptor",
        DynamicArrayDescriptorField::data
    );
    output << emit_dynamic_array_element_address(
        *plan,
        prefix + ".element.addr",
        prefix + ".data",
        lowered_index->value
    );
    output << "  " << prefix << ".value = load " << plan->element_llvm_type;
    output << ", ptr " << prefix << ".element.addr\n";

    return LoweredExpression {
        .type = plan->element_llvm_type,
        .value = prefix + ".value",
        .signedness = element_signedness,
    };
}

auto lower_dynamic_array_element_path_read(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto path = collect_named_aggregate_path(expression);
    if (!path.has_value() ||
        path->base_expression == nullptr ||
        path->steps.size() < 2 ||
        path->steps.front().kind != AggregatePathStepKind::index ||
        path->steps.front().index_expression == nullptr) {
        return std::nullopt;
    }

    auto const& owner_name = path->base_expression->text;
    if (auto moved_name = moved_owned_dynamic_array_binding_name(owner_name, session.state)) {
        record_use_after_move_failure(session.failures, *moved_name);
        return std::nullopt;
    }

    auto source_type = session.state.source_type_names.find(owner_name);
    if (source_type == session.state.source_type_names.end()) {
        return std::nullopt;
    }
    auto element_source_type = dynamic_array_element_source_type_name(source_type->second);
    if (!element_source_type.has_value() ||
        !is_owned_transfer_source_type(*element_source_type, context.lowering)) {
        return std::nullopt;
    }
    auto const index_expression_text = runtime_index_expression_key(*path->steps.front().index_expression);
    auto const runtime_index_constructor_move_recorded =
        std::ranges::any_of(
            session.state.ownership_transfers.runtime_indexed_partial_owners,
            [&](RuntimeIndexedPartialOwner const& owner) {
                return owner.constructor_move_enabled &&
                    owner.owner_name == owner_name &&
                    owner.index_expression_text == index_expression_text &&
                    owner.element_source_type_name == *element_source_type;
            }
        );
    auto const runtime_index_constructor_argument_key =
        owner_name + "[" + index_expression_text + "]";
    if (runtime_index_constructor_move_recorded &&
        !runtime_indexed_constructor_argument_active(
            session.state,
            runtime_index_constructor_argument_key
        )) {
        record_use_after_move_failure(
            session.failures,
            runtime_index_constructor_argument_key
        );
        return std::nullopt;
    }
    auto projected_source_type = source_type_name_for_expression(expression, context.lowering, session.state);
    auto const runtime_index_member_cleanup_rewrite_enabled =
        runtime_index_constructor_move_recorded &&
        context.options.enable_runtime_indexed_member_cleanup_ir_mutation_request &&
        context.options.enable_runtime_indexed_member_cleanup_production_gate_request &&
        context.options.enable_runtime_indexed_member_cleanup_apply_authorization_request &&
        context.options.enable_runtime_indexed_member_cleanup_rewrite_execution_request;
    if (projected_source_type.has_value() &&
        is_owned_transfer_source_type(*projected_source_type, context.lowering) &&
        !runtime_index_member_cleanup_rewrite_enabled) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "DynamicArray element path read of owned projection requires a non-owning scalar projection"
        );
        return std::nullopt;
    }

    auto storage = aggregate_storage_for_name(owner_name, session.state);
    if (!storage.has_value()) {
        return std::nullopt;
    }

    auto plan = plan_dynamic_array_descriptor_cleanup(
        owner_name,
        source_type->second,
        context.lowering
    );
    if (!plan.has_value()) {
        return std::nullopt;
    }

    auto lowered_index = lowered_expression(
        *path->steps.front().index_expression,
        "i64",
        IntegerSignedness::unsigned_integer,
        context,
        session,
        output
    );
    if (!lowered_index.has_value()) {
        return std::nullopt;
    }

    auto prefix = "%" + owner_name + ".dynamic_array_element_path" +
        std::to_string(session.state.next_temporary_index++);
    output << emit_dynamic_array_descriptor_load(
        prefix + ".descriptor",
        *storage
    );
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".length",
        prefix + ".descriptor",
        DynamicArrayDescriptorField::length
    );
    output << emit_dynamic_array_bounds_check(
        prefix + ".in_bounds",
        lowered_index->value,
        prefix + ".length",
        DynamicArrayBoundsCheckKind::index_within_length
    );
    auto block_index = next_llvm_block_index(session.state.next_block_index);
    auto value_block = llvm_block_name("dynamic_array.element_path.in_bounds", block_index);
    auto failure_block = llvm_block_name("dynamic_array.element_path.out_of_bounds", block_index);
    emit_llvm_conditional_branch(
        output,
        prefix + ".in_bounds",
        value_block,
        failure_block
    );
    emit_llvm_block_label(output, failure_block);
    output << "  call void @__orison_dynamic_array_bounds_failed()\n";
    emit_llvm_unreachable(output);
    emit_llvm_block_label(output, value_block);
    session.state.current_block = value_block;
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".data",
        prefix + ".descriptor",
        DynamicArrayDescriptorField::data
    );
    output << emit_dynamic_array_element_address(
        *plan,
        prefix + ".element.addr",
        prefix + ".data",
        lowered_index->value
    );

    auto element_path = *path;
    element_path.steps.erase(element_path.steps.begin());
    return lower_aggregate_path_read_from_storage(
        expression,
        element_path,
        prefix + ".element.addr",
        *element_source_type,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        output
    );
}

auto lower_dynamic_array_length_call(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (!context.options.enable_dynamic_array_length_lowering ||
        expression.kind != syntax::ExpressionKind::call ||
        expression.left == nullptr ||
        expression.left->kind != syntax::ExpressionKind::member_access ||
        expression.left->left == nullptr ||
        expression.left->left->kind != syntax::ExpressionKind::name ||
        expression.left->text != "length" ||
        !expression.arguments.empty() ||
        expected_llvm_type != "i64" ||
        expected_signedness != IntegerSignedness::signed_integer) {
        return std::nullopt;
    }

    auto const& owner_name = expression.left->left->text;
    if (auto moved_name = moved_owned_dynamic_array_binding_name(owner_name, session.state)) {
        record_use_after_move_failure(session.failures, *moved_name);
        return std::nullopt;
    }
    auto source_type = session.state.source_type_names.find(owner_name);
    if (source_type == session.state.source_type_names.end() ||
        !dynamic_array_element_source_type_name(source_type->second).has_value()) {
        return std::nullopt;
    }

    auto storage = aggregate_storage_for_name(owner_name, session.state);
    if (!storage.has_value()) {
        return std::nullopt;
    }

    auto prefix = "%" + owner_name + ".dynamic_array_length" +
        std::to_string(session.state.next_temporary_index++);
    output << emit_dynamic_array_descriptor_load(
        prefix + ".descriptor",
        *storage
    );
    output << emit_dynamic_array_descriptor_field_projection(
        prefix + ".value",
        prefix + ".descriptor",
        DynamicArrayDescriptorField::length
    );

    return LoweredExpression {
        .type = "i64",
        .value = prefix + ".value",
        .signedness = IntegerSignedness::signed_integer,
    };
}

auto emit_view_descriptor_field_projection(
    std::string_view result_name,
    std::string_view descriptor_value_name,
    std::size_t field_index
) -> std::string {
    auto output = std::ostringstream {};
    output << "  " << result_name << " = extractvalue ";
    output << view_descriptor_llvm_type() << " " << descriptor_value_name;
    output << ", " << field_index << "\n";
    return output.str();
}

auto lower_view_index_read(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (expression.kind != syntax::ExpressionKind::index_access ||
        expression.left == nullptr ||
        expression.arguments.size() != 1) {
        return std::nullopt;
    }

    auto base_source_type = source_type_name_for_expression(*expression.left, context, session.state);
    if (!base_source_type.has_value()) {
        return std::nullopt;
    }
    auto element_source_type = view_element_source_type_name(*base_source_type);
    if (!element_source_type.has_value()) {
        return std::nullopt;
    }

    auto element_llvm_type = llvm_type_for_source_type_name(*element_source_type, context.lowering);
    auto element_signedness = integer_signedness_for(syntax::TypeSyntax {
        .name = *element_source_type,
    });
    if (!element_llvm_type.has_value() ||
        *element_llvm_type != expected_llvm_type ||
        element_signedness != expected_signedness) {
        return std::nullopt;
    }

    auto lowered_base = lowered_expression(
        *expression.left,
        std::string {view_descriptor_llvm_type()},
        IntegerSignedness::not_integer,
        context,
        session,
        output
    );
    if (!lowered_base.has_value()) {
        return std::nullopt;
    }

    auto lowered_index = lowered_expression(
        expression.arguments.front(),
        "i64",
        IntegerSignedness::unsigned_integer,
        context,
        session,
        output
    );
    if (!lowered_index.has_value()) {
        return std::nullopt;
    }

    auto prefix = "%view_index" + std::to_string(session.state.next_temporary_index++);
    output << emit_view_descriptor_field_projection(prefix + ".data", lowered_base->value, 0);
    output << emit_view_descriptor_field_projection(prefix + ".length", lowered_base->value, 1);
    output << emit_dynamic_array_bounds_check(
        prefix + ".in_bounds",
        lowered_index->value,
        prefix + ".length",
        DynamicArrayBoundsCheckKind::index_within_length
    );
    auto block_index = next_llvm_block_index(session.state.next_block_index);
    auto value_block = llvm_block_name("view.index.in_bounds", block_index);
    auto failure_block = llvm_block_name("view.index.out_of_bounds", block_index);
    emit_llvm_conditional_branch(output, prefix + ".in_bounds", value_block, failure_block);
    emit_llvm_block_label(output, failure_block);
    output << "  call void @__orison_dynamic_array_bounds_failed()\n";
    emit_llvm_unreachable(output);
    emit_llvm_block_label(output, value_block);
    session.state.current_block = value_block;
    output << "  " << prefix << ".element.addr = getelementptr " << *element_llvm_type;
    output << ", ptr " << prefix << ".data, i64 " << lowered_index->value << "\n";
    output << "  " << prefix << ".value = load " << *element_llvm_type;
    output << ", ptr " << prefix << ".element.addr\n";

    return LoweredExpression {
        .type = *element_llvm_type,
        .value = prefix + ".value",
        .signedness = element_signedness,
    };
}

auto lower_view_length_call(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (expression.kind != syntax::ExpressionKind::call ||
        expression.left == nullptr ||
        expression.left->kind != syntax::ExpressionKind::member_access ||
        expression.left->text != "length" ||
        expression.left->left == nullptr ||
        !expression.arguments.empty() ||
        expected_llvm_type != "i64" ||
        expected_signedness != IntegerSignedness::signed_integer) {
        return std::nullopt;
    }

    auto base_source_type = source_type_name_for_expression(*expression.left->left, context, session.state);
    if (!base_source_type.has_value() ||
        !view_element_source_type_name(*base_source_type).has_value()) {
        return std::nullopt;
    }

    auto lowered_base = lowered_expression(
        *expression.left->left,
        std::string {view_descriptor_llvm_type()},
        IntegerSignedness::not_integer,
        context,
        session,
        output
    );
    if (!lowered_base.has_value()) {
        return std::nullopt;
    }

    auto prefix = "%view_length" + std::to_string(session.state.next_temporary_index++);
    output << emit_view_descriptor_field_projection(prefix + ".value", lowered_base->value, 1);

    return LoweredExpression {
        .type = "i64",
        .value = prefix + ".value",
        .signedness = IntegerSignedness::signed_integer,
    };
}

auto emit_null_safe_empty_result(
    MaybeValueAbi const& result_abi,
    FunctionLoweringState& state,
    std::ostringstream& output
) -> NullSafeIncoming {
    auto empty = emit_empty_maybe_value(result_abi, state.next_temporary_index, output);
    auto incoming_block = state.current_block;
    return NullSafeIncoming {
        .value = std::move(empty.value),
        .block = std::move(incoming_block),
    };
}

auto lower_null_safe_member_call_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (expression.kind != syntax::ExpressionKind::call || expression.left == nullptr ||
        expression.left->kind != syntax::ExpressionKind::null_safe_member_access ||
        expression.left->left == nullptr) {
        return std::nullopt;
    }

    auto resolved = resolve_member_call(expression, context, session.state);
    auto const receiver_name = expression.left->left != nullptr ? expression.left->left->text : std::string {};
    if (resolved.receiver.result == MemberCallReceiverInferenceResult::unsupported_shape) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "member call receiver shape"
        );
        return std::nullopt;
    }
    if (resolved.receiver.result == MemberCallReceiverInferenceResult::not_found) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unknown_member_call_receiver,
            receiver_name
        );
        return std::nullopt;
    }

    auto receiver_maybe_type_name = resolved.receiver.receiver_type_name;
    auto receiver_payload_type_name = maybe_payload_source_type_name(receiver_maybe_type_name);
    if (!receiver_payload_type_name.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "null-safe member call receiver ABI: " + resolved.receiver.receiver_type_name + "." +
                resolved.receiver.method_name
        );
        return std::nullopt;
    }

    auto method_lookup = find_lowered_method_signature(
        context.lowering,
        *receiver_payload_type_name,
        resolved.receiver.method_name
    );
    auto const target_name = *receiver_payload_type_name + "." + resolved.receiver.method_name;
    if (method_lookup.result == LoweredMethodLookupResult::not_found) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unknown_member_call_target,
            target_name
        );
        return std::nullopt;
    }
    if (method_lookup.result == LoweredMethodLookupResult::ambiguous) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::ambiguous_member_call_target,
            target_name
        );
        return std::nullopt;
    }
    if (method_lookup.method == nullptr ||
        !has_supported_function_signature_types(method_lookup.method->signature)) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "member call target is not lowerable: " + target_name
        );
        return std::nullopt;
    }

    auto const& method = method_lookup.method->signature;
    if (method.source_return_type_name.empty()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "null-safe member call return source type: " + target_name
        );
        return std::nullopt;
    }

    auto result_maybe_type_name = "Maybe<" + method.source_return_type_name + ">";
    auto result_abi = maybe_value_abi_for_source_type(result_maybe_type_name, context.lowering);
    if (!result_abi.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "null-safe member call result ABI: " + target_name
        );
        return std::nullopt;
    }
    if (result_abi->llvm_type != expected_llvm_type) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::type_mismatch,
            result_maybe_type_name + " has LLVM type " + result_abi->llvm_type +
                ", expected " + std::string(expected_llvm_type)
        );
        return std::nullopt;
    }

    auto receiver_abi = maybe_value_abi_for_source_type(receiver_maybe_type_name, context.lowering);
    if (!receiver_abi.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "null-safe member call receiver ABI: " + target_name
        );
        return std::nullopt;
    }

    auto lowered_receiver = lowered_expression(
        *expression.left->left,
        receiver_abi->llvm_type,
        IntegerSignedness::not_integer,
        context,
        session,
        output,
        receiver_maybe_type_name
    );
    if (!lowered_receiver.has_value()) {
        return std::nullopt;
    }

    auto merge_block = llvm_block_name(
        "nullsafe.call.merge",
        next_llvm_block_index(session.state.next_block_index)
    );
    auto block_index = next_llvm_block_index(session.state.next_block_index);
    auto some_block = llvm_block_name("nullsafe.call.some", block_index);
    auto empty_block = llvm_block_name("nullsafe.call.empty", block_index);

    auto tag_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << tag_name << " = extractvalue " << receiver_abi->llvm_type << " "
           << lowered_receiver->value << ", 0\n";
    output << "  br i1 " << tag_name << ", label %" << some_block
           << ", label %" << empty_block << "\n";

    session.state.current_block = empty_block;
    output << empty_block << ":\n";
    auto empty_incoming = emit_null_safe_empty_result(*result_abi, session.state, output);
    output << "  br label %" << merge_block << "\n";

    session.state.current_block = some_block;
    output << some_block << ":\n";
    auto payload_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << payload_name << " = extractvalue " << receiver_abi->llvm_type << " "
           << lowered_receiver->value << ", 1\n";
    auto receiver_payload = LoweredExpression {
        .type = receiver_abi->payload_llvm_type,
        .value = std::move(payload_name),
        .signedness = IntegerSignedness::not_integer,
    };

    auto expected_argument_count = method.parameter_types.size() - 1;
    if (expected_argument_count != expression.arguments.size()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::call_arity_mismatch,
            target_name + " expects " + std::to_string(expected_argument_count) +
                " arguments, got " + std::to_string(expression.arguments.size())
        );
        return std::nullopt;
    }

    auto arguments = lower_member_call_arguments(
        std::move(receiver_payload),
        std::span<syntax::ExpressionSyntax const>(
            expression.arguments.data(),
            expression.arguments.size()
        ),
        method,
        context,
        session,
        output
    );
    if (!arguments.has_value()) {
        if (session.failures.expression.reason == ExpressionLoweringFailureReason::none) {
            record_expression_lowering_failure(
                session.failures,
                ExpressionLoweringFailureReason::call_argument_failure,
                target_name
            );
        }
        return std::nullopt;
    }

    auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
    auto call_result = emit_value_call(std::move(temporary_name), method, *arguments, output);
    auto some_result = emit_some_maybe_value(
        *result_abi,
        call_result,
        session.state.next_temporary_index,
        output
    );
    if (!some_result.has_value()) {
        return std::nullopt;
    }
    auto some_incoming = NullSafeIncoming {
        .value = std::move(some_result->value),
        .block = session.state.current_block,
    };
    output << "  br label %" << merge_block << "\n";

    output << merge_block << ":\n";
    auto phi_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << phi_name << " = phi " << result_abi->llvm_type << " ["
           << empty_incoming.value << ", %" << empty_incoming.block << "], ["
           << some_incoming.value << ", %" << some_incoming.block << "]\n";
    session.state.current_block = merge_block;
    return LoweredExpression {
        .type = result_abi->llvm_type,
        .value = std::move(phi_name),
        .signedness = IntegerSignedness::not_integer,
    };
}

auto lower_null_safe_member_access_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (expression.kind != syntax::ExpressionKind::null_safe_member_access) {
        return std::nullopt;
    }

    auto plan_result = plan_null_safe_member_access(expression, context.lowering, session.state);
    if (!plan_result.plan.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            render_null_safe_plan_failure(plan_result.failure)
        );
        return std::nullopt;
    }

    auto const& plan = *plan_result.plan;
    auto result_abi = maybe_value_abi_for_source_type(plan.result_maybe_type_name, context.lowering);
    if (!result_abi.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "null-safe result ABI"
        );
        return std::nullopt;
    }
    if (result_abi->llvm_type != expected_llvm_type) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::type_mismatch,
            plan.result_maybe_type_name + " has LLVM type " + result_abi->llvm_type +
                ", expected " + std::string(expected_llvm_type)
        );
        return std::nullopt;
    }

    auto base_llvm_type = llvm_type_for_source_type_name(plan.base_maybe_type_name, context.lowering);
    if (!base_llvm_type.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "null-safe base ABI"
        );
        return std::nullopt;
    }

    auto current_maybe = lowered_expression(
        *plan.base_expression,
        *base_llvm_type,
        IntegerSignedness::not_integer,
        context,
        session,
        output
    );
    if (!current_maybe.has_value()) {
        return std::nullopt;
    }

    auto current_maybe_type_name = plan.base_maybe_type_name;
    auto incoming = std::vector<NullSafeIncoming> {};
    auto merge_block = llvm_block_name(
        "nullsafe.merge",
        next_llvm_block_index(session.state.next_block_index)
    );
    for (auto index = std::size_t {0}; index < plan.segments.size(); ++index) {
        auto current_abi = maybe_value_abi_for_source_type(current_maybe_type_name, context.lowering);
        if (!current_abi.has_value() || current_maybe->type != current_abi->llvm_type) {
            record_expression_lowering_failure(
                session.failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                "null-safe segment ABI"
            );
            return std::nullopt;
        }

        auto block_index = next_llvm_block_index(session.state.next_block_index);
        auto some_block = llvm_block_name("nullsafe.some", block_index);
        auto empty_block = llvm_block_name("nullsafe.empty", block_index);

        auto tag_name = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << tag_name << " = extractvalue " << current_abi->llvm_type << " "
               << current_maybe->value << ", 0\n";
        output << "  br i1 " << tag_name << ", label %" << some_block
               << ", label %" << empty_block << "\n";

        session.state.current_block = empty_block;
        output << empty_block << ":\n";
        incoming.push_back(emit_null_safe_empty_result(*result_abi, session.state, output));
        output << "  br label %" << merge_block << "\n";

        session.state.current_block = some_block;
        output << some_block << ":\n";
        auto payload_name = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << payload_name << " = extractvalue " << current_abi->llvm_type << " "
               << current_maybe->value << ", 1\n";
        auto payload = LoweredExpression {
            .type = current_abi->payload_llvm_type,
            .value = std::move(payload_name),
            .signedness = IntegerSignedness::not_integer,
        };

        auto const& segment = plan.segments[index];
        auto const* layout = context.lowering.records.contains(segment.receiver_type_name)
            ? &context.lowering.records.at(segment.receiver_type_name)
            : nullptr;
        auto const* field = layout != nullptr ? find_record_field(*layout, segment.field_name) : nullptr;
        auto field_llvm_type = llvm_type_for_source_type_name(segment.field_type_name, context.lowering);
        if (field == nullptr || !field_llvm_type.has_value()) {
            record_expression_lowering_failure(
                session.failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                "null-safe field extraction"
            );
            return std::nullopt;
        }

        auto field_name = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << field_name << " = extractvalue " << payload.type << " "
               << payload.value << ", " << field->index << "\n";
        auto field_value = LoweredExpression {
            .type = *field_llvm_type,
            .value = std::move(field_name),
            .signedness = integer_signedness_for(syntax::TypeSyntax {.name = segment.field_type_name}),
        };

        auto field_maybe_payload = maybe_payload_source_type_name(segment.field_type_name);
        if (field_maybe_payload.has_value()) {
            current_maybe = std::move(field_value);
            current_maybe_type_name = segment.field_type_name;
        } else {
            auto segment_maybe_type_name = std::string {"Maybe<"} + segment.field_type_name + ">";
            auto segment_abi = maybe_value_abi_for_source_type(segment_maybe_type_name, context.lowering);
            if (!segment_abi.has_value()) {
                record_expression_lowering_failure(
                    session.failures,
                    ExpressionLoweringFailureReason::unsupported_expression,
                    "null-safe segment result ABI"
                );
                return std::nullopt;
            }
            current_maybe = emit_some_maybe_value(
                *segment_abi,
                field_value,
                session.state.next_temporary_index,
                output
            );
            if (!current_maybe.has_value()) {
                return std::nullopt;
            }
            current_maybe_type_name = std::move(segment_maybe_type_name);
        }

        if (index + 1 == plan.segments.size()) {
            incoming.push_back(NullSafeIncoming {
                .value = current_maybe->value,
                .block = session.state.current_block,
            });
            output << "  br label %" << merge_block << "\n";

            output << merge_block << ":\n";
            auto phi_name = next_llvm_temporary_name(session.state.next_temporary_index);
            output << "  " << phi_name << " = phi " << result_abi->llvm_type << " ";
            for (auto incoming_index = std::size_t {0}; incoming_index < incoming.size(); ++incoming_index) {
                if (incoming_index > 0) {
                    output << ", ";
                }
                output << "[" << incoming[incoming_index].value << ", %"
                       << incoming[incoming_index].block << "]";
            }
            output << "\n";
            session.state.current_block = std::move(merge_block);
            return LoweredExpression {
                .type = result_abi->llvm_type,
                .value = std::move(phi_name),
                .signedness = IntegerSignedness::not_integer,
            };
        }
    }

    return std::nullopt;
}

auto lowered_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output,
    std::optional<std::string_view> expected_source_type_name
) -> std::optional<LoweredExpression> {
    auto& state = session.state;
    auto& failures = session.failures;
    if (auto literal = lowered_integer_literal(expression, expected_llvm_type, expected_signedness)) {
        return literal;
    }
    if (auto literal = lowered_float_literal(expression, expected_llvm_type)) {
        return literal;
    }
    if (auto literal = lowered_boolean_literal(expression, expected_llvm_type)) {
        return literal;
    }
    if (auto array_literal = lower_array_literal_expression(
            expression,
            expected_llvm_type,
            context,
            session,
            failures,
            output,
            expected_source_type_name
        )) {
        return array_literal;
    }
    if (expression.kind == syntax::ExpressionKind::string_literal && expected_llvm_type == "ptr") {
        auto const* constant = context.string_constants.find(expression.text);
        if (constant == nullptr) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::missing_string_constant,
                expression.text
            );
            return std::nullopt;
        }
        return LoweredExpression {
            .type = "ptr",
            .value = "@" + constant->name,
            .signedness = IntegerSignedness::not_integer,
        };
    }

    if (expression.kind == syntax::ExpressionKind::name && expression.text == "Empty") {
        if (auto maybe_abi = maybe_value_abi_for_llvm_type(expected_llvm_type, context.lowering)) {
            return emit_empty_maybe_value(*maybe_abi, state.next_temporary_index, output);
        }
    }

    if (auto choice_constructor = lower_choice_constructor_expression(
            expression,
            expected_llvm_type,
            context,
            session,
            failures,
            output,
            expected_source_type_name
        )) {
        return choice_constructor;
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name && expression.left->text == "Some") {
        auto maybe_abi = expected_source_type_name.has_value()
            ? maybe_value_abi_for_source_type(*expected_source_type_name, context.lowering)
            : maybe_value_abi_for_llvm_type(expected_llvm_type, context.lowering);
        if (maybe_abi.has_value() && maybe_abi->llvm_type != expected_llvm_type) {
            maybe_abi = std::nullopt;
        }
        if (maybe_abi.has_value() && expression.arguments.size() == 1) {
            auto payload_source_type_name = maybe_abi->payload_source_type_name.empty()
                ? std::optional<std::string_view> {}
                : std::optional<std::string_view> {maybe_abi->payload_source_type_name};
            auto payload = lowered_expression(
                expression.arguments.front(),
                maybe_abi->payload_llvm_type,
                signedness_for_expected_array_element(
                    expression.arguments.front(),
                    maybe_abi->payload_llvm_type,
                    context,
                    state
                ),
                context,
                session,
                output,
                payload_source_type_name
            );
            if (!payload.has_value()) {
                return std::nullopt;
            }
            return emit_some_maybe_value(*maybe_abi, *payload, state.next_temporary_index, output);
        }
    }

    if (expression.kind == syntax::ExpressionKind::name) {
        if (auto moved_name = consumed_owned_binding_or_descendant_name(
                state.ownership_transfers,
                expression.text
            )) {
            record_use_after_move_failure(failures, *moved_name);
            return std::nullopt;
        }
        if (auto moved_name = moved_owned_dynamic_array_binding_name(expression.text, state)) {
            record_use_after_move_failure(failures, *moved_name);
            return std::nullopt;
        }
        auto binding = state.immutable_bindings.find(expression.text);
        if (binding == state.immutable_bindings.end()) {
            auto mutable_binding = state.mutable_bindings.find(expression.text);
            if (mutable_binding == state.mutable_bindings.end()) {
                record_expression_lowering_failure(failures, ExpressionLoweringFailureReason::unknown_name, expression.text);
                return std::nullopt;
            }
            if (mutable_binding->second.type.type != expected_llvm_type) {
                record_expression_lowering_failure(
                    failures,
                    ExpressionLoweringFailureReason::type_mismatch,
                    expression.text + " has LLVM type " + mutable_binding->second.type.type +
                        ", expected " + std::string(expected_llvm_type)
                );
                return std::nullopt;
            }
            if (mutable_binding->second.type.signedness != expected_signedness) {
                record_expression_lowering_failure(
                    failures,
                    ExpressionLoweringFailureReason::signedness_mismatch,
                    expression.text
                );
                return std::nullopt;
            }
            auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
            output << "  " << temporary_name << " = load " << mutable_binding->second.type.type
                   << ", ptr " << mutable_binding->second.storage << "\n";
            return LoweredExpression {
                .type = mutable_binding->second.type.type,
                .value = std::move(temporary_name),
                .signedness = mutable_binding->second.type.signedness,
            };
        }
        if (binding->second.type != expected_llvm_type) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::type_mismatch,
                expression.text + " has LLVM type " + binding->second.type +
                    ", expected " + std::string(expected_llvm_type)
            );
            return std::nullopt;
        }
        if (binding->second.signedness != expected_signedness) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::signedness_mismatch,
                expression.text
            );
            return std::nullopt;
        }
        return binding->second;
    }

    if (auto dynamic_array_element_path_read = lower_dynamic_array_element_path_read(
            expression,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            output
        )) {
        return dynamic_array_element_path_read;
    }

    if (auto dynamic_array_index_read = lower_dynamic_array_index_read(
            expression,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            output
        )) {
        return dynamic_array_index_read;
    }

    if (auto aggregate_path_read = lower_addressable_aggregate_path_read(
            expression,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            output
        )) {
        return aggregate_path_read;
    }

    if (auto aggregate_path_read = lower_temporary_aggregate_path_read(
            expression,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            output
        )) {
        return aggregate_path_read;
    }

    if (auto dynamic_array_length_call = lower_dynamic_array_length_call(
            expression,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            output
        )) {
        return dynamic_array_length_call;
    }

    if (auto view_index_read = lower_view_index_read(
            expression,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            output
        )) {
        return view_index_read;
    }

    if (auto view_length_call = lower_view_length_call(
            expression,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            output
        )) {
        return view_length_call;
    }

    if (auto null_safe_access = lower_null_safe_member_access_expression(
            expression,
            expected_llvm_type,
            context,
            session,
            output
        )) {
        return null_safe_access;
    }

    if (expression.kind == syntax::ExpressionKind::member_access && expression.left != nullptr) {
        auto base_source_type = source_type_name_for_expression(*expression.left, context, state);
        if (!base_source_type.has_value()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }

        auto layout = context.lowering.records.find(*base_source_type);
        if (layout == context.lowering.records.end()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }

        auto const* field = find_record_field(layout->second, expression.text);
        if (field == nullptr || field->source_type_name.empty()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }

        if (auto path = collect_named_aggregate_path(expression)) {
            auto path_base_source_type = path->base_expression != nullptr
                ? source_type_name_for_expression(*path->base_expression, context, state)
                : std::optional<std::string> {};
            if (path_base_source_type.has_value()) {
                if (auto moved_member_name = consumed_owned_aggregate_path_name(
                        *path,
                        *path_base_source_type,
                        context.lowering,
                        state
                    )) {
                    record_use_after_move_failure(session.failures, *moved_member_name);
                    return std::nullopt;
                }
            }
        }

        auto base_llvm_type = llvm_type_for_source_type_name(*base_source_type, context.lowering);
        auto field_llvm_type = llvm_type_for_source_type_name(field->source_type_name, context.lowering);
        auto field_signedness = integer_signedness_for(syntax::TypeSyntax {
            .name = field->source_type_name,
        });
        if (!base_llvm_type.has_value() || !field_llvm_type.has_value()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }

        auto lowered_base = lowered_expression(
            *expression.left,
            *base_llvm_type,
            IntegerSignedness::not_integer,
            context,
            session,
            output
        );
        if (!lowered_base.has_value()) {
            return std::nullopt;
        }

        auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
        output << "  " << temporary_name << " = extractvalue " << lowered_base->type << " "
               << lowered_base->value << ", " << field->index << "\n";
        return LoweredExpression {
            .type = *field_llvm_type,
            .value = std::move(temporary_name),
            .signedness = field_signedness,
        };
    }

    if (expression.kind == syntax::ExpressionKind::index_access && expression.left != nullptr &&
        expression.arguments.size() == 1) {
        auto base_source_type = source_type_name_for_expression(*expression.left, context, state);
        if (!base_source_type.has_value()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }

        auto element_source_type = array_element_source_type_name(*base_source_type);
        auto is_view_index = false;
        if (!element_source_type.has_value()) {
            element_source_type = view_element_source_type_name(*base_source_type);
            is_view_index = element_source_type.has_value();
        }
        if (!element_source_type.has_value()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }

        auto base_llvm_type = llvm_type_for_source_type_name(*base_source_type, context.lowering);
        auto element_llvm_type = llvm_type_for_source_type_name(*element_source_type, context.lowering);
        auto element_signedness = integer_signedness_for(syntax::TypeSyntax {
            .name = *element_source_type,
        });
        if (!base_llvm_type.has_value() || !element_llvm_type.has_value()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }

        if (*element_llvm_type != expected_llvm_type || element_signedness != expected_signedness) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::type_mismatch,
                expression.text
            );
            return std::nullopt;
        }

        auto lowered_base = lowered_expression(
            *expression.left,
            *base_llvm_type,
            IntegerSignedness::not_integer,
            context,
            session,
            output
        );
        if (!lowered_base.has_value()) {
            return std::nullopt;
        }

        auto lowered_index = lowered_expression(
            expression.arguments.front(),
            "i64",
            IntegerSignedness::unsigned_integer,
            context,
            session,
            output
        );
        if (!lowered_index.has_value()) {
            return std::nullopt;
        }

        auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
        if (is_view_index) {
            auto element_pointer_name = next_llvm_temporary_name(state.next_temporary_index);
            auto data_pointer_name = next_llvm_temporary_name(state.next_temporary_index);
            output << "  " << data_pointer_name << " = extractvalue " << lowered_base->type << " "
                   << lowered_base->value << ", 0\n";
            output << "  " << element_pointer_name << " = getelementptr " << *element_llvm_type
                   << ", ptr " << data_pointer_name << ", i64 " << lowered_index->value << "\n";
            output << "  " << temporary_name << " = load " << *element_llvm_type << ", ptr "
                   << element_pointer_name << "\n";
        } else if (is_decimal_integer_text(lowered_index->value)) {
            output << "  " << temporary_name << " = extractvalue " << lowered_base->type << " "
                   << lowered_base->value << ", " << lowered_index->value << "\n";
        } else {
            auto storage_name =
                spill_aggregate_value_to_temporary_storage(*lowered_base, session, output);
            if (!storage_name.has_value()) {
                return std::nullopt;
            }
            auto element_pointer_name = next_llvm_temporary_name(state.next_temporary_index);
            output << "  " << element_pointer_name << " = getelementptr " << lowered_base->type
                   << ", ptr " << *storage_name << ", i64 0, i64 " << lowered_index->value << "\n";
            output << "  " << temporary_name << " = load " << *element_llvm_type << ", ptr "
                   << element_pointer_name << "\n";
        }
        return LoweredExpression {
            .type = *element_llvm_type,
            .value = std::move(temporary_name),
            .signedness = element_signedness,
        };
    }

    if (expression.kind == syntax::ExpressionKind::cast && expression.left != nullptr) {
        auto cast_type = lowered_type_for_source_type_name(expression.text, context.lowering);
        if (!cast_type.has_value() || cast_type->type != expected_llvm_type) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_cast,
                expression.text
            );
            return std::nullopt;
        }
        if (expression.left->kind == syntax::ExpressionKind::array_literal) {
            return lowered_expression(
                *expression.left,
                expected_llvm_type,
                expected_signedness,
                context,
                session,
                output,
                expression.text
            );
        }
        syntax::TypeSyntax target_type {
            .name = expression.text,
        };
        if (auto integer = lowered_integer_literal(
                *expression.left,
                expected_llvm_type,
                integer_signedness_for(target_type)
            )) {
            return integer;
        }
        if (expression.left->kind == syntax::ExpressionKind::unary && expression.left->text == "-" &&
            cast_type->signedness == IntegerSignedness::signed_integer &&
            is_integer_llvm_type_impl(expected_llvm_type)) {
            return lowered_expression(
                *expression.left,
                expected_llvm_type,
                cast_type->signedness,
                context,
                session,
                output
            );
        }
        if (expression.left->kind == syntax::ExpressionKind::unary && expression.left->text == "-" &&
            cast_type->signedness == IntegerSignedness::unsigned_integer &&
            is_integer_llvm_type_impl(expected_llvm_type)) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_cast,
                "negative value to " + expression.text
            );
            return std::nullopt;
        }
        if (auto nested = lowered_expression(
                *expression.left,
                expected_llvm_type,
                cast_type->signedness,
                context,
                session,
                output,
                expression.text
            )) {
            return nested;
        }
        auto lowered_float = lowered_float_literal(*expression.left, expected_llvm_type);
        if (!lowered_float.has_value()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_cast,
                expression.text
            );
        }
        return lowered_float;
    }

    if (expression.kind == syntax::ExpressionKind::unary && expression.text == "not" &&
        expression.left != nullptr && expected_llvm_type == "i1") {
        auto operand = lowered_expression(
            *expression.left,
            "i1",
            IntegerSignedness::not_integer,
            context,
            session,
            output
        );
        if (!operand.has_value()) {
            return std::nullopt;
        }

        auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
        output << "  " << temporary_name << " = xor i1 " << operand->value << ", true\n";
        return LoweredExpression {
            .type = "i1",
            .value = std::move(temporary_name),
            .signedness = IntegerSignedness::not_integer,
        };
    }

    if (expression.kind == syntax::ExpressionKind::unary && expression.text == "bit_not" &&
        expression.left != nullptr && is_integer_llvm_type_impl(expected_llvm_type) &&
        expected_signedness != IntegerSignedness::not_integer) {
        auto operand = lowered_expression(
            *expression.left,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            output
        );
        if (!operand.has_value()) {
            return std::nullopt;
        }

        auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
        output << "  " << temporary_name << " = xor " << operand->type << " " << operand->value
               << ", -1\n";
        return LoweredExpression {
            .type = operand->type,
            .value = std::move(temporary_name),
            .signedness = operand->signedness,
        };
    }

    if (expression.kind == syntax::ExpressionKind::unary && expression.text == "-" &&
        expression.left != nullptr && is_integer_llvm_type_impl(expected_llvm_type) &&
        expected_signedness == IntegerSignedness::signed_integer) {
        auto operand = lowered_expression(
            *expression.left,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            output
        );
        if (!operand.has_value()) {
            return std::nullopt;
        }

        auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
        output << "  " << temporary_name << " = sub " << operand->type << " 0, "
               << operand->value << "\n";
        return LoweredExpression {
            .type = operand->type,
            .value = std::move(temporary_name),
            .signedness = operand->signedness,
        };
    }

    if (expression.kind == syntax::ExpressionKind::unary && expression.text == "-" &&
        expression.left != nullptr) {
        if (expected_signedness == IntegerSignedness::unsigned_integer &&
            expected_source_type_name.has_value() &&
            expression.left->kind == syntax::ExpressionKind::integer_literal) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_cast,
                "negative value to " + std::string(*expected_source_type_name)
            );
            return std::nullopt;
        }
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_operator,
            expression.text
        );
        return std::nullopt;
    }

    if (expression.kind == syntax::ExpressionKind::ternary && expression.left != nullptr &&
        expression.right != nullptr && expression.alternate != nullptr) {
        auto condition = lowered_expression(
            *expression.left,
            "i1",
            IntegerSignedness::not_integer,
            context,
            session,
            output
        );
        if (!condition.has_value()) {
            return std::nullopt;
        }

        auto plan = plan_conditional(
            ConditionalPlanKind::ternary,
            next_llvm_block_index(state.next_block_index)
        );
        auto then_returned_owner = returned_dynamic_array_owner_name(
            *expression.right,
            expected_source_type_name,
            context,
            state
        );
        auto else_returned_owner = returned_dynamic_array_owner_name(
            *expression.alternate,
            expected_source_type_name,
            context,
            state
        );
        auto branch_owned_dynamic_array_return = then_returned_owner.has_value() &&
            else_returned_owner.has_value() &&
            *then_returned_owner != *else_returned_owner;
        auto binding_scope = std::optional<BranchBindingScope> {};
        if (branch_owned_dynamic_array_return) {
            binding_scope.emplace(state);
        }
        struct ArmContext {
            syntax::ExpressionSyntax const& then_expression;
            syntax::ExpressionSyntax const& else_expression;
            std::string_view expected_llvm_type;
            IntegerSignedness expected_signedness;
            EmissionContext const& context;
            FunctionLoweringSession& session;
            std::ostringstream& output;
            std::optional<std::string_view> expected_source_type_name;
            std::optional<std::string> then_returned_owner;
            std::optional<std::string> else_returned_owner;
            BranchBindingScope* binding_scope = nullptr;
            std::vector<OwnershipTransferState> ownership_transfers_by_arm;
        };
        auto arm_context = ArmContext {
            .then_expression = *expression.right,
            .else_expression = *expression.alternate,
            .expected_llvm_type = expected_llvm_type,
            .expected_signedness = expected_signedness,
            .context = context,
            .session = session,
            .output = output,
            .expected_source_type_name = expected_source_type_name,
            .then_returned_owner = then_returned_owner,
            .else_returned_owner = else_returned_owner,
            .binding_scope = binding_scope.has_value() ? &*binding_scope : nullptr,
        };
        auto result = emit_conditional_value(
            plan,
            condition->value,
            state,
            output,
            ConditionalLoweringCallbacks {
                .context = &arm_context,
                .lower_then = [](void* opaque) {
                    auto& arm = *static_cast<ArmContext*>(opaque);
                    auto value = lowered_expression(
                        arm.then_expression,
                        arm.expected_llvm_type,
                        arm.expected_signedness,
                        arm.context,
                        arm.session,
                        arm.output,
                        arm.expected_source_type_name
                    );
                    if (!value.has_value()) {
                        return value;
                    }
                    if (arm.else_returned_owner.has_value() &&
                        !emit_dynamic_array_cleanup_for_owner(
                            *arm.else_returned_owner,
                            arm.context,
                            arm.session,
                            arm.output
                        )) {
                        return std::optional<LoweredExpression> {};
                    }
                    if (arm.binding_scope != nullptr) {
                        arm.ownership_transfers_by_arm.push_back(arm.session.state.ownership_transfers);
                    }
                    return value;
                },
                .between_arms = [](void* opaque) {
                    auto& arm = *static_cast<ArmContext*>(opaque);
                    if (arm.binding_scope != nullptr) {
                        arm.binding_scope->reset();
                    }
                },
                .lower_else = [](void* opaque) {
                    auto& arm = *static_cast<ArmContext*>(opaque);
                    auto value = lowered_expression(
                        arm.else_expression,
                        arm.expected_llvm_type,
                        arm.expected_signedness,
                        arm.context,
                        arm.session,
                        arm.output,
                        arm.expected_source_type_name
                    );
                    if (!value.has_value()) {
                        return value;
                    }
                    if (arm.then_returned_owner.has_value() &&
                        !emit_dynamic_array_cleanup_for_owner(
                            *arm.then_returned_owner,
                            arm.context,
                            arm.session,
                            arm.output
                        )) {
                        return std::optional<LoweredExpression> {};
                    }
                    if (arm.binding_scope != nullptr) {
                        arm.ownership_transfers_by_arm.push_back(arm.session.state.ownership_transfers);
                    }
                    return value;
                },
            }
        );
        if (result.failure == ConditionalEmissionFailure::branch_mismatch) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::branch_type_mismatch,
                "ternary branches"
            );
            return std::nullopt;
        }
        if (branch_owned_dynamic_array_return) {
            auto merged_transfers = merge_ownership_transfer_states(arm_context.ownership_transfers_by_arm);
            if (!merged_transfers.has_value()) {
                record_expression_lowering_failure(
                    failures,
                    ExpressionLoweringFailureReason::branch_type_mismatch,
                    "ternary ownership transfers"
                );
                return std::nullopt;
            }
            binding_scope->commit_ownership_transfers(std::move(*merged_transfers));
        }
        return result.value;
    }

    if (expression.kind == syntax::ExpressionKind::binary && expression.left != nullptr &&
        expression.right != nullptr) {
        if (expected_llvm_type == "i1") {
            if (expression.text == "and" || expression.text == "or") {
                auto left = lowered_expression(
                    *expression.left,
                    "i1",
                    IntegerSignedness::not_integer,
                    context,
                    session,
                    output
                );
                auto right = lowered_expression(
                    *expression.right,
                    "i1",
                    IntegerSignedness::not_integer,
                    context,
                    session,
                    output
                );
                if (!left.has_value() || !right.has_value()) {
                    return std::nullopt;
                }

                auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
                output << "  " << temporary_name << " = " << expression.text << " i1 ";
                output << left->value << ", " << right->value << "\n";
                return LoweredExpression {
                    .type = "i1",
                    .value = std::move(temporary_name),
                    .signedness = IntegerSignedness::not_integer,
                };
            }

            auto boolean_left_type = inferred_expression_type(*expression.left, context, state);
            auto boolean_right_type = inferred_expression_type(*expression.right, context, state);
            auto const has_boolean_operands =
                boolean_left_type.has_value() &&
                boolean_right_type.has_value() &&
                boolean_left_type->type == "i1" &&
                boolean_right_type->type == "i1" &&
                boolean_left_type->signedness == IntegerSignedness::not_integer &&
                boolean_right_type->signedness == IntegerSignedness::not_integer;
            if (has_boolean_operands) {
                auto boolean_predicate = llvm_boolean_comparison_predicate_for(expression.text);
                if (!boolean_predicate.has_value()) {
                    record_expression_lowering_failure(
                        failures,
                        ExpressionLoweringFailureReason::unsupported_operator,
                        expression.text
                    );
                    return std::nullopt;
                }
                auto left = lowered_expression(
                    *expression.left,
                    "i1",
                    IntegerSignedness::not_integer,
                    context,
                    session,
                    output
                );
                auto right = lowered_expression(
                    *expression.right,
                    "i1",
                    IntegerSignedness::not_integer,
                    context,
                    session,
                    output
                );
                if (!left.has_value() || !right.has_value()) {
                    return std::nullopt;
                }

                auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
                output << "  " << temporary_name << " = icmp " << *boolean_predicate << " i1 ";
                output << left->value << ", " << right->value << "\n";
                return LoweredExpression {
                    .type = "i1",
                    .value = std::move(temporary_name),
                    .signedness = IntegerSignedness::not_integer,
                };
            }

            auto operand_type = inferred_expression_type(*expression.left, context, state);
            if (!operand_type.has_value()) {
                operand_type = inferred_expression_type(*expression.right, context, state);
            }
            if (!operand_type.has_value() || !is_integer_llvm_type_impl(operand_type->type) ||
                operand_type->signedness == IntegerSignedness::not_integer) {
                record_expression_lowering_failure(
                    failures,
                    ExpressionLoweringFailureReason::cannot_infer_operand_type,
                    expression.text
                );
                return std::nullopt;
            }

            auto predicate = llvm_integer_comparison_predicate_for(expression.text, operand_type->signedness);
            if (predicate.has_value()) {
                auto left = lowered_expression(
                    *expression.left,
                    operand_type->type,
                    operand_type->signedness,
                    context,
                    session,
                    output
                );
                auto right = lowered_expression(
                    *expression.right,
                    operand_type->type,
                    operand_type->signedness,
                    context,
                    session,
                    output
                );
                if (!left.has_value() || !right.has_value() || left->type != right->type ||
                    left->signedness != right->signedness) {
                    return std::nullopt;
                }

                auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
                output << "  " << temporary_name << " = icmp " << *predicate << " " << left->type << " ";
                output << left->value << ", " << right->value << "\n";
                return LoweredExpression {
                    .type = "i1",
                    .value = std::move(temporary_name),
                    .signedness = IntegerSignedness::not_integer,
                };
            }
        }

        auto instruction = llvm_binary_instruction_for(expression.text, expected_signedness);
        if (!instruction.has_value()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_operator,
                expression.text
            );
            return std::nullopt;
        }
        auto left = lowered_expression(
            *expression.left,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            output
        );
        auto right = lowered_expression(
            *expression.right,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            output
        );
        if (!left.has_value() || !right.has_value() || left->type != right->type ||
            left->signedness != right->signedness) {
            return std::nullopt;
        }

        auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
        output << "  " << temporary_name << " = " << *instruction << " " << left->type << " " << left->value << ", ";
        output << right->value << "\n";
        return LoweredExpression {
            .type = left->type,
            .value = std::move(temporary_name),
            .signedness = left->signedness,
        };
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::null_safe_member_access) {
        return lower_null_safe_member_call_expression(expression, expected_llvm_type, context, session, output);
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name && expression.left->text == "Pointer") {
        return lower_pointer_constructor_expression(
            expression,
            expected_llvm_type,
            context,
            session,
            failures,
            output
        );
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name &&
        is_low_level_intrinsic_name(expression.left->text)) {
        auto const& intrinsic_name = expression.left->text;
        if (intrinsic_name == "address_of") {
            return lower_address_of_intrinsic(
                expression,
                expected_llvm_type,
                context,
                failures,
                session,
                output
            );
        }
        if (intrinsic_name == "raw_offset") {
            return lower_raw_offset_intrinsic(
                expression,
                expected_llvm_type,
                context,
                session,
                failures,
                output
            );
        }
        if (intrinsic_name == "raw_read" || intrinsic_name == "volatile_read") {
            return lower_read_intrinsic(
                expression,
                expected_llvm_type,
                expected_signedness,
                intrinsic_name == "volatile_read",
                context,
                session,
                failures,
                output
            );
        }
        if (intrinsic_name == "raw_write" || intrinsic_name == "volatile_write") {
            return lower_write_intrinsic(
                expression,
                expected_llvm_type,
                intrinsic_name == "volatile_write",
                context,
                session,
                failures,
                output
            );
        }
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::member_access) {
        if (expression.left->text == "join" &&
            expression.left->left != nullptr &&
            expression.left->left->kind == syntax::ExpressionKind::name) {
            auto thread_binding = state.thread_bindings.find(expression.left->left->text);
            if (thread_binding != state.thread_bindings.end()) {
                return emit_thread_join_result(
                    thread_binding->second,
                    expected_llvm_type,
                    state,
                    failures,
                    output
                );
            }
        }

        auto resolved = resolve_member_call(expression, context, state);
        auto const receiver_name = expression.left->left != nullptr ? expression.left->left->text : std::string {};
        auto const target_name = resolved.receiver.receiver_type_name + "." + resolved.receiver.method_name;
        if (resolved.receiver.result == MemberCallReceiverInferenceResult::unsupported_shape) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                "member call receiver shape"
            );
            return std::nullopt;
        }
        if (resolved.receiver.result == MemberCallReceiverInferenceResult::not_found) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unknown_member_call_receiver,
                receiver_name
            );
            return std::nullopt;
        }
        auto const* method_signature = resolved.method.method == nullptr
            ? nullptr
            : &resolved.method.method->signature;
        if (method_signature == nullptr ||
            resolved.method.result != LoweredMethodLookupResult::found ||
            method_signature->return_type != expected_llvm_type) {
            method_signature = find_matching_generic_method_specialization(
                resolved.receiver.receiver_type_name,
                resolved.receiver.method_name,
                expression,
                expected_llvm_type,
                context.lowering,
                session.state
            );
        }

        if (method_signature == nullptr) {
            if (auto detail = generic_method_specialization_ambiguity_detail(
                    resolved.receiver.receiver_type_name,
                    resolved.receiver.method_name,
                    expression,
                    expected_llvm_type,
                    context.lowering,
                    session.state
                )) {
                record_expression_lowering_failure(
                    failures,
                    ExpressionLoweringFailureReason::ambiguous_generic_specialization,
                    *detail
                );
                return std::nullopt;
            }
        }
        if (method_signature == nullptr && resolved.method.result == LoweredMethodLookupResult::not_found) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unknown_member_call_target,
                target_name
            );
            return std::nullopt;
        }
        if (method_signature == nullptr && resolved.method.result == LoweredMethodLookupResult::ambiguous) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::ambiguous_member_call_target,
                target_name
            );
            return std::nullopt;
        }
        if (method_signature == nullptr || !has_supported_function_signature_types(*method_signature)) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                "member call target is not lowerable: " + target_name
            );
            return std::nullopt;
        }

        if (method_signature->return_type != expected_llvm_type) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::call_return_type_mismatch,
                target_name + " returns " + method_signature->return_type + ", expected " +
                    std::string(expected_llvm_type)
            );
            return std::nullopt;
        }

        auto const expected_argument_count = method_signature->parameter_types.size() - 1;
        if (expected_argument_count != expression.arguments.size()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::call_arity_mismatch,
                target_name + " expects " + std::to_string(expected_argument_count) +
                    " arguments, got " + std::to_string(expression.arguments.size())
            );
            return std::nullopt;
        }

        auto arguments = lower_member_call_arguments(
            *expression.left->left,
            std::span<syntax::ExpressionSyntax const>(
                expression.arguments.data(),
                expression.arguments.size()
            ),
            *method_signature,
            context,
            session,
            output
        );
        if (!arguments.has_value()) {
            if (failures.expression.reason == ExpressionLoweringFailureReason::none) {
                record_expression_lowering_failure(
                    failures,
                    ExpressionLoweringFailureReason::call_argument_failure,
                    target_name
                );
            }
            return std::nullopt;
        }

        auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
        return emit_value_call(std::move(temporary_name), *method_signature, *arguments, output);
    }

    if (expression.kind == syntax::ExpressionKind::unary &&
        expression.text == "await" &&
        expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name) {
        auto task_binding = state.task_bindings.find(expression.left->text);
        if (task_binding != state.task_bindings.end()) {
            return emit_task_await_result(
                task_binding->second,
                expected_llvm_type,
                state,
                failures,
                output
            );
        }
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name) {
        if (auto record_constructor = lower_record_constructor_expression(
                expression,
                expected_llvm_type,
                context,
                session,
                failures,
                output,
                expected_source_type_name
            )) {
            return record_constructor;
        }

        auto function = context.lowering.functions.find(expression.left->text);
        auto const* function_signature = function == context.lowering.functions.end()
            ? nullptr
            : &function->second;
        if (function_signature == nullptr ||
            function_signature->return_type != expected_llvm_type ||
            function_signature->parameter_types.size() != expression.arguments.size()) {
            if (auto specialization = find_matching_generic_specialization(
                    expression.left->text,
                    expression,
                    expected_llvm_type,
                    context.lowering,
                    session.state
                )) {
                function_signature = specialization;
            }
        }

        if (function_signature == nullptr) {
            if (auto detail = generic_specialization_ambiguity_detail(
                    expression.left->text,
                    expression,
                    expected_llvm_type,
                    context.lowering,
                    session.state
                )) {
                record_expression_lowering_failure(
                    failures,
                    ExpressionLoweringFailureReason::ambiguous_generic_specialization,
                    *detail
                );
                return std::nullopt;
            }
            auto resolver = GenericCallSourceResolver {
                .lowering_context = &context.lowering,
                .state = &session.state,
            };
            if (auto detail = generic_call_argument_inference_failure_detail(expression, resolver)) {
                record_expression_lowering_failure(
                    failures,
                    ExpressionLoweringFailureReason::call_argument_failure,
                    *detail
                );
                return std::nullopt;
            }
            if (auto detail = generic_record_constructor_inference_failure_detail(
                    expression,
                    context.lowering,
                    session.state
                )) {
                record_expression_lowering_failure(
                    failures,
                    ExpressionLoweringFailureReason::generic_record_constructor_inference_failed,
                    *detail
                );
                return std::nullopt;
            }
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unknown_function,
                expression.left->text
            );
            return std::nullopt;
        }
        if (function_signature->return_type != expected_llvm_type) {
            if (auto detail = generic_specialization_ambiguity_detail(
                    expression.left->text,
                    expression,
                    expected_llvm_type,
                    context.lowering,
                    session.state
                )) {
                record_expression_lowering_failure(
                    failures,
                    ExpressionLoweringFailureReason::ambiguous_generic_specialization,
                    *detail
                );
                return std::nullopt;
            }
            auto resolver = GenericCallSourceResolver {
                .lowering_context = &context.lowering,
                .state = &session.state,
            };
            if (auto detail = generic_call_argument_inference_failure_detail(expression, resolver)) {
                record_expression_lowering_failure(
                    failures,
                    ExpressionLoweringFailureReason::call_argument_failure,
                    *detail
                );
                return std::nullopt;
            }
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::call_return_type_mismatch,
                expression.left->text + " returns " + function_signature->return_type +
                    ", expected " + std::string(expected_llvm_type)
            );
            return std::nullopt;
        }
        if (function_signature->parameter_types.size() != expression.arguments.size()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::call_arity_mismatch,
                expression.left->text + " expects " +
                    std::to_string(function_signature->parameter_types.size()) + " arguments, got " +
                    std::to_string(expression.arguments.size())
            );
            return std::nullopt;
        }

        auto arguments = lower_call_arguments(expression, *function_signature, context, session, output);
        if (!arguments.has_value()) {
            if (failures.expression.reason == ExpressionLoweringFailureReason::none) {
                record_expression_lowering_failure(
                    failures,
                    ExpressionLoweringFailureReason::call_argument_failure,
                    expression.left->text
                );
            }
            return std::nullopt;
        }

        auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
        return emit_value_call(std::move(temporary_name), *function_signature, *arguments, output);
    }

    record_expression_lowering_failure(
        failures,
        is_concurrency_expression(expression)
            ? ExpressionLoweringFailureReason::unsupported_concurrency_expression
            : ExpressionLoweringFailureReason::unsupported_expression,
        expression.text
    );
    return std::nullopt;
}

}  // namespace

auto lower_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output,
    std::optional<std::string_view> expected_source_type_name
) -> std::optional<LoweredExpression> {
    auto& failures = session.failures;
    reset_expression_lowering_failure(failures);
    return lowered_expression(
        expression,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        output,
        expected_source_type_name
    );
}

auto infer_expression_type(
    syntax::ExpressionSyntax const& expression,
    LoweringEmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType> {
    return inferred_expression_type(expression, context, state);
}

auto lower_integer_literal(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness
) -> std::optional<LoweredExpression> {
    return lowered_integer_literal(expression, expected_llvm_type, expected_signedness);
}

auto lower_boolean_literal(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type
) -> std::optional<LoweredExpression> {
    return lowered_boolean_literal(expression, expected_llvm_type);
}

auto is_integer_llvm_type(std::string_view type) -> bool {
    return is_integer_llvm_type_impl(type);
}

}  // namespace orison::lowering
