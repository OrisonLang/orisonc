#include "orison/lowering/direct_dynamic_array_receiver.hpp"

#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/aggregate_path.hpp"
#include "orison/lowering/dynamic_array_cleanup_plan.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/fixed_array_bounds.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/runtime_index_expression.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/type_lowering.hpp"
#include "orison/semantics/drop_model.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>

namespace orison::lowering {
namespace {

enum class DescriptorProjectionStepKind {
    field,
    array_element,
};

struct DescriptorProjectionStep {
    DescriptorProjectionStepKind kind = DescriptorProjectionStepKind::field;
    std::string aggregate_llvm_type;
    std::string index_value;
};

struct DescriptorProjectionPath {
    std::string owner_name;
    std::string source_type_name;
    std::vector<DescriptorProjectionStep> steps;
};

struct LoweredSelectedDescriptorProjection {
    std::string pointer;
    std::string source_type_name;
};

auto direct_dynamic_array_receiver_element_drop_authorized(
    std::string_view element_source_type_name,
    LoweringEmissionContext const& context
) -> bool {
    auto symbol_name = semantics::drop_abi_symbol_name(element_source_type_name);
    return std::ranges::any_of(context.options.semantic_drop_lowering_authorizations, [&](auto const& authorization) {
        return authorization.authorized &&
            authorization.site.source_type_name == element_source_type_name &&
            authorization.site.abi_symbol_name == symbol_name;
    });
}

auto direct_projection_root_call(
    syntax::ExpressionSyntax const& expression
) -> syntax::ExpressionSyntax const* {
    auto const* current = &expression;
    auto saw_projection = false;
    while ((current->kind == syntax::ExpressionKind::member_access ||
            current->kind == syntax::ExpressionKind::index_access) &&
           current->left != nullptr) {
        saw_projection = true;
        current = current->left.get();
    }
    if (!saw_projection || current->kind != syntax::ExpressionKind::call ||
        current->left == nullptr || current->left->kind != syntax::ExpressionKind::name) {
        return nullptr;
    }
    return current;
}

auto dynamic_array_descriptor_count(
    std::string_view source_type_name,
    LoweringContext const& context,
    std::size_t depth = 0
) -> std::size_t {
    if (depth > 16) {
        return 2;
    }
    if (dynamic_array_element_source_type_name(source_type_name).has_value()) {
        return 1;
    }

    if (auto array_element_type = array_element_source_type_name(source_type_name)) {
        auto element_count = dynamic_array_descriptor_count(*array_element_type, context, depth + 1);
        return element_count == 0 ? 0 : 2;
    }

    auto record = context.records.find(std::string {source_type_name});
    if (record == context.records.end()) {
        return 0;
    }

    auto count = std::size_t {0};
    for (auto const& field : record->second.fields) {
        count += dynamic_array_descriptor_count(field.source_type_name, context, depth + 1);
        if (count > 1) {
            return count;
        }
    }
    return count;
}

auto returned_aggregate_projection_has_sibling_descriptors(
    syntax::ExpressionSyntax const& receiver_expression,
    LoweringContext const& context
) -> bool {
    auto const* root_call = direct_projection_root_call(receiver_expression);
    if (root_call == nullptr || root_call->left == nullptr) {
        return false;
    }

    auto function = context.functions.find(root_call->left->text);
    if (function == context.functions.end() || function->second.source_return_type_name.empty()) {
        return false;
    }

    return dynamic_array_descriptor_count(function->second.source_return_type_name, context) > 1;
}

auto aggregate_path_crosses_dynamic_array_element(
    AggregatePath const& aggregate_path,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> bool {
    if (aggregate_path.base_expression == nullptr) {
        return false;
    }

    auto cursor_source_type = source_type_name_for_expression(
        *aggregate_path.base_expression,
        context,
        state
    );
    if (!cursor_source_type.has_value()) {
        return false;
    }

    for (auto const& step : aggregate_path.steps) {
        if (step.kind == AggregatePathStepKind::member) {
            auto record = context.records.find(*cursor_source_type);
            if (record == context.records.end()) {
                return false;
            }

            auto const* field = find_record_field(record->second, step.field_name);
            if (field == nullptr || field->source_type_name.empty()) {
                return false;
            }
            cursor_source_type = field->source_type_name;
            continue;
        }

        if (dynamic_array_element_source_type_name(*cursor_source_type).has_value()) {
            return true;
        }

        auto array_element = array_element_source_type_name(*cursor_source_type);
        if (!array_element.has_value()) {
            return false;
        }
        cursor_source_type = std::move(*array_element);
    }

    return false;
}

auto temporary_aggregate_path_crosses_dynamic_array_element(
    syntax::ExpressionSyntax const& receiver_expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> bool {
    auto aggregate_path = collect_temporary_aggregate_path(receiver_expression);
    return aggregate_path.has_value() &&
        aggregate_path_crosses_dynamic_array_element(*aggregate_path, context, state);
}

auto named_aggregate_path_crosses_dynamic_array_element(
    syntax::ExpressionSyntax const& receiver_expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> bool {
    auto aggregate_path = collect_named_aggregate_path(receiver_expression);
    return aggregate_path.has_value() &&
        aggregate_path_crosses_dynamic_array_element(*aggregate_path, context, state);
}

auto lower_named_dynamic_array_element_projection_receiver(
    syntax::ExpressionSyntax const& receiver_expression,
    std::string_view receiver_type_name,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto aggregate_path = collect_named_aggregate_path(receiver_expression);
    if (!aggregate_path.has_value() || aggregate_path->base_expression == nullptr) {
        return std::nullopt;
    }

    auto const& base_name = aggregate_path->base_expression->text;
    auto base_storage = aggregate_storage_for_name(base_name, session.state);
    auto base_source_type = session.state.source_type_names.find(base_name);
    if (!base_storage.has_value() || base_source_type == session.state.source_type_names.end()) {
        return std::nullopt;
    }

    auto cursor = initialize_aggregate_path_cursor(
        *base_storage,
        base_source_type->second,
        context.lowering
    );
    if (!cursor.has_value()) {
        return std::nullopt;
    }

    auto owner_name = base_name;
    auto index_step_index = std::optional<std::size_t> {};
    for (auto step_index = std::size_t {0}; step_index < aggregate_path->steps.size(); ++step_index) {
        auto const& step = aggregate_path->steps[step_index];
        if (step.kind == AggregatePathStepKind::index) {
            if (!dynamic_array_element_source_type_name(cursor->source_type_name).has_value()) {
                return std::nullopt;
            }
            index_step_index = step_index;
            break;
        }

        auto result = advance_aggregate_path_member_with_temporary(
            *cursor,
            step.field_name,
            context.lowering,
            session.state.next_temporary_index,
            output
        );
        if (result.error != AggregatePathError::none) {
            return std::nullopt;
        }
        owner_name += ".";
        owner_name += step.field_name;
    }

    if (!index_step_index.has_value()) {
        return std::nullopt;
    }

    auto const& index_step = aggregate_path->steps[*index_step_index];
    if (index_step.index_expression == nullptr) {
        return std::nullopt;
    }

    auto element_source_type = dynamic_array_element_source_type_name(cursor->source_type_name);
    if (!element_source_type.has_value()) {
        return std::nullopt;
    }

    auto owner_cleanup_plan = plan_dynamic_array_descriptor_cleanup(
        owner_name,
        cursor->source_type_name,
        context.lowering
    );
    if (!owner_cleanup_plan.has_value()) {
        return std::nullopt;
    }

    auto lowered_index = lower_expression(
        *index_step.index_expression,
        "i64",
        IntegerSignedness::unsigned_integer,
        context,
        session,
        output
    );
    if (!lowered_index.has_value()) {
        return std::nullopt;
    }

    auto prefix = "%" + owner_name + ".dynamic_array_receiver_element_path" +
        std::to_string(session.state.next_temporary_index++);
    output << emit_dynamic_array_descriptor_load(
        prefix + ".descriptor",
        cursor->pointer
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
    auto value_block = llvm_block_name("dynamic_array.receiver_element_path.in_bounds", block_index);
    auto failure_block = llvm_block_name("dynamic_array.receiver_element_path.out_of_bounds", block_index);
    emit_llvm_conditional_branch(output, prefix + ".in_bounds", value_block, failure_block);
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
        *owner_cleanup_plan,
        prefix + ".element.addr",
        prefix + ".data",
        lowered_index->value
    );

    auto selected_cursor = initialize_aggregate_path_cursor(
        prefix + ".element.addr",
        *element_source_type,
        context.lowering
    );
    if (!selected_cursor.has_value()) {
        return std::nullopt;
    }

    for (auto step_index = *index_step_index + 1; step_index < aggregate_path->steps.size(); ++step_index) {
        auto const& step = aggregate_path->steps[step_index];
        if (step.kind == AggregatePathStepKind::member) {
            auto result = advance_aggregate_path_member_with_temporary(
                *selected_cursor,
                step.field_name,
                context.lowering,
                session.state.next_temporary_index,
                output
            );
            if (result.error != AggregatePathError::none) {
                return std::nullopt;
            }
            continue;
        }

        if (step.index_expression == nullptr) {
            return std::nullopt;
        }
        auto lowered_nested_index = lower_expression(
            *step.index_expression,
            "i64",
            IntegerSignedness::unsigned_integer,
            context,
            session,
            output
        );
        if (!lowered_nested_index.has_value()) {
            return std::nullopt;
        }
        auto result = advance_aggregate_path_index_with_temporary(
            *selected_cursor,
            lowered_nested_index->value,
            context.lowering,
            session.state.next_temporary_index,
            output
        );
        if (result.error != AggregatePathError::none) {
            return std::nullopt;
        }
    }

    if (selected_cursor->source_type_name != receiver_type_name) {
        return std::nullopt;
    }

    auto temporary_name = std::string {"%named_dynamic_array_receiver_descriptor"};
    temporary_name += std::to_string(session.state.next_temporary_index++);
    output << "  " << temporary_name << " = load " << dynamic_array_descriptor_llvm_type()
           << ", ptr " << selected_cursor->pointer << "\n";
    output << "  store " << dynamic_array_descriptor_llvm_type()
           << " zeroinitializer, ptr " << selected_cursor->pointer << "\n";

    return LoweredExpression {
        .type = std::string {dynamic_array_descriptor_llvm_type()},
        .value = std::move(temporary_name),
        .signedness = IntegerSignedness::not_integer,
    };
}

auto collect_descriptor_projection_paths(
    std::string owner_name,
    std::string_view source_type_name,
    std::string_view llvm_type,
    LoweringContext const& context,
    std::vector<DescriptorProjectionStep> steps = {}
) -> std::optional<std::vector<DescriptorProjectionPath>> {
    if (dynamic_array_element_source_type_name(source_type_name).has_value()) {
        return std::vector<DescriptorProjectionPath> {
            DescriptorProjectionPath {
                .owner_name = std::move(owner_name),
                .source_type_name = std::string {source_type_name},
                .steps = std::move(steps),
            },
        };
    }

    if (auto array_element_type = array_element_source_type_name(source_type_name)) {
        auto array_type = parse_llvm_array_type(llvm_type);
        if (!array_type.has_value()) {
            return std::nullopt;
        }

        auto paths = std::vector<DescriptorProjectionPath> {};
        for (auto index = std::size_t {0}; index < array_type->length; ++index) {
            auto element_steps = steps;
            element_steps.push_back(DescriptorProjectionStep {
                .kind = DescriptorProjectionStepKind::array_element,
                .aggregate_llvm_type = std::string {llvm_type},
                .index_value = std::to_string(index),
            });
            auto nested = collect_descriptor_projection_paths(
                owner_name + ".element" + std::to_string(index),
                *array_element_type,
                array_type->element_type,
                context,
                std::move(element_steps)
            );
            if (!nested.has_value()) {
                return std::nullopt;
            }
            paths.insert(
                paths.end(),
                std::make_move_iterator(nested->begin()),
                std::make_move_iterator(nested->end())
            );
        }
        return paths;
    }

    auto record = context.records.find(std::string {source_type_name});
    if (record == context.records.end()) {
        return std::vector<DescriptorProjectionPath> {};
    }

    auto paths = std::vector<DescriptorProjectionPath> {};
    for (auto const& field : record->second.fields) {
        auto field_steps = steps;
        field_steps.push_back(DescriptorProjectionStep {
            .kind = DescriptorProjectionStepKind::field,
            .aggregate_llvm_type = std::string {llvm_type},
            .index_value = std::to_string(field.index),
        });
        auto nested = collect_descriptor_projection_paths(
            owner_name + "." + field.name,
            field.source_type_name,
            field.llvm_type,
            context,
            std::move(field_steps)
        );
        if (!nested.has_value()) {
            return std::nullopt;
        }
        paths.insert(
            paths.end(),
            std::make_move_iterator(nested->begin()),
            std::make_move_iterator(nested->end())
        );
    }
    return paths;
}

auto lower_selected_descriptor_projection_path(
    AggregatePath const& aggregate_path,
    std::string_view aggregate_storage,
    std::string source_type_name,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredSelectedDescriptorProjection> {
    auto cursor = initialize_aggregate_path_cursor(
        std::string {aggregate_storage},
        std::move(source_type_name),
        context.lowering
    );
    if (!cursor.has_value()) {
        return std::nullopt;
    }

    for (auto const& step : aggregate_path.steps) {
        if (step.kind == AggregatePathStepKind::member) {
            auto result = advance_aggregate_path_member_with_temporary(
                *cursor,
                step.field_name,
                context.lowering,
                session.state.next_temporary_index,
                output
            );
            if (result.error != AggregatePathError::none) {
                return std::nullopt;
            }
            continue;
        }

        if (step.index_expression == nullptr) {
            return std::nullopt;
        }
        auto array_type = parse_llvm_array_type(cursor->llvm_type_name);
        if (!array_type.has_value()) {
            return std::nullopt;
        }
        auto index_text = decimal_integer_literal_text(*step.index_expression);
        auto lowered_index_value = std::string {};
        if (index_text.has_value()) {
            auto index_value = std::size_t {0};
            for (auto character : *index_text) {
                index_value = (index_value * 10) + static_cast<std::size_t>(character - '0');
            }
            if (!array_type.has_value() || index_value >= array_type->length) {
                return std::nullopt;
            }
            lowered_index_value = std::string {*index_text};
        } else {
            auto lowered_index = lower_expression(
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
            lowered_index_value = lowered_index->value;
            emit_fixed_array_runtime_index_bounds_check(
                "returned_aggregate_receiver_array_index",
                lowered_index_value,
                array_type->length,
                session,
                output
            );
        }

        auto result = advance_aggregate_path_index_with_temporary(
            *cursor,
            std::move(lowered_index_value),
            context.lowering,
            session.state.next_temporary_index,
            output
        );
        if (result.error != AggregatePathError::none) {
            return std::nullopt;
        }
    }

    if (!dynamic_array_element_source_type_name(cursor->source_type_name).has_value()) {
        return std::nullopt;
    }
    return LoweredSelectedDescriptorProjection {
        .pointer = std::move(cursor->pointer),
        .source_type_name = std::move(cursor->source_type_name),
    };
}

auto emit_descriptor_projection_pointer(
    std::string_view root_storage,
    DescriptorProjectionPath const& path,
    std::string_view pointer_prefix,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::string {
    auto pointer = std::string {root_storage};
    for (auto const& step : path.steps) {
        auto next_pointer = std::string {pointer_prefix} + ".path" +
            std::to_string(session.state.next_temporary_index++);
        output << "  " << next_pointer << " = getelementptr " << step.aggregate_llvm_type
               << ", ptr " << pointer;
        if (step.kind == DescriptorProjectionStepKind::field) {
            output << ", i32 0, i32 " << step.index_value << "\n";
        } else {
            output << ", i64 0, i64 " << step.index_value << "\n";
        }
        pointer = std::move(next_pointer);
    }
    return pointer;
}

auto lower_returned_aggregate_projection_receiver(
    syntax::ExpressionSyntax const& receiver_expression,
    std::string_view receiver_type_name,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto aggregate_path = collect_temporary_aggregate_path(receiver_expression);
    if (!aggregate_path.has_value() || aggregate_path->base_expression == nullptr) {
        return std::nullopt;
    }

    auto base_source_type = source_type_name_for_expression(
        *aggregate_path->base_expression,
        context.lowering,
        session.state
    );
    if (!base_source_type.has_value()) {
        return std::nullopt;
    }

    auto base_llvm_type = llvm_type_for_source_type_name(*base_source_type, context.lowering);
    if (!base_llvm_type.has_value() || *base_llvm_type == "void") {
        return std::nullopt;
    }

    auto aggregate_owner_name = std::string {"dynamic_array_receiver_aggregate_tmp"};
    aggregate_owner_name += std::to_string(session.state.next_temporary_index++);
    auto all_descriptor_paths = collect_descriptor_projection_paths(
        aggregate_owner_name,
        *base_source_type,
        *base_llvm_type,
        context.lowering
    );
    if (!all_descriptor_paths.has_value()) {
        return std::nullopt;
    }

    auto lowered_base = lower_expression(
        *aggregate_path->base_expression,
        *base_llvm_type,
        IntegerSignedness::not_integer,
        context,
        session,
        output,
        *base_source_type
    );
    if (!lowered_base.has_value()) {
        return std::nullopt;
    }

    auto aggregate_storage = "%" + aggregate_owner_name + ".addr";
    output << "  " << aggregate_storage << " = alloca " << *base_llvm_type << "\n";
    output << "  store " << *base_llvm_type << " " << lowered_base->value;
    output << ", ptr " << aggregate_storage << "\n";

    auto selected_path = lower_selected_descriptor_projection_path(
        *aggregate_path,
        aggregate_storage,
        *base_source_type,
        context,
        session,
        output
    );
    if (!selected_path.has_value() || selected_path->source_type_name != receiver_type_name) {
        return std::nullopt;
    }

    auto temporary_name = std::string {"%returned_aggregate_receiver_descriptor"};
    temporary_name += std::to_string(session.state.next_temporary_index++);
    output << "  " << temporary_name << " = load " << dynamic_array_descriptor_llvm_type()
           << ", ptr " << selected_path->pointer << "\n";
    output << "  store " << dynamic_array_descriptor_llvm_type()
           << " zeroinitializer, ptr " << selected_path->pointer << "\n";

    for (auto const& descriptor_path : *all_descriptor_paths) {
        auto sibling_pointer = emit_descriptor_projection_pointer(
            aggregate_storage,
            descriptor_path,
            "%" + descriptor_path.owner_name,
            session,
            output
        );
        auto cleanup_plan = plan_dynamic_array_descriptor_cleanup(
            descriptor_path.owner_name,
            descriptor_path.source_type_name,
            context.lowering
        );
        if (!cleanup_plan.has_value()) {
            return std::nullopt;
        }
        cleanup_plan->descriptor_storage_name = std::move(sibling_pointer);
        cleanup_plan->descriptor_storage_status =
            DynamicArrayDescriptorStorageStatus::lowered_local_descriptor;
        cleanup_plan->source_line = receiver_expression.line;
        session.state.dynamic_array_local_cleanup_plans.push_back(std::move(*cleanup_plan));
    }

    return LoweredExpression {
        .type = std::string {dynamic_array_descriptor_llvm_type()},
        .value = std::move(temporary_name),
        .signedness = IntegerSignedness::not_integer,
    };
}

auto direct_receiver_failure(
    LoweringFailures& failures,
    bool record_expression_failures,
    std::string message
) -> DirectDynamicArrayReceiverLowering {
    if (record_expression_failures) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            message
        );
    }
    return DirectDynamicArrayReceiverLowering {
        .receiver = std::nullopt,
        .diagnostic = std::move(message),
    };
}

}  // namespace

auto lower_direct_dynamic_array_receiver(
    syntax::ExpressionSyntax const& receiver_expression,
    LoweredFunctionSignature const& method_signature,
    std::string_view receiver_type_name,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    LoweringFailures& failures,
    std::ostringstream& output,
    bool record_expression_failures
) -> DirectDynamicArrayReceiverLowering {
    auto element_source_type = dynamic_array_element_source_type_name(receiver_type_name);
    if (!element_source_type.has_value()) {
        return {};
    }
    auto const requires_returned_aggregate_sibling_cleanup =
        returned_aggregate_projection_has_sibling_descriptors(receiver_expression, context.lowering);
    auto const receiver_is_temporary_aggregate_projection =
        collect_temporary_aggregate_path(receiver_expression).has_value();
    auto const requires_named_dynamic_array_element_transfer =
        named_aggregate_path_crosses_dynamic_array_element(receiver_expression, context.lowering, session.state);
    if (receiver_is_temporary_aggregate_projection &&
        temporary_aggregate_path_crosses_dynamic_array_element(receiver_expression, context.lowering, session.state)) {
        return direct_receiver_failure(
            failures,
            record_expression_failures,
            "DynamicArray receiver returned aggregate cleanup cannot enumerate descriptors through DynamicArray element projection"
        );
    }
    if (contains_runtime_indexed_projection(receiver_expression) &&
        receiver_is_temporary_aggregate_projection &&
        !requires_returned_aggregate_sibling_cleanup) {
        return direct_receiver_failure(
            failures,
            record_expression_failures,
            "DynamicArray receiver expression with runtime-indexed aggregate projection requires named binding"
        );
    }
    if (!is_scalar_or_nonowning_source_type(*element_source_type) &&
        !direct_dynamic_array_receiver_element_drop_authorized(*element_source_type, context)) {
        return direct_receiver_failure(
            failures,
            record_expression_failures,
            "DynamicArray receiver expression with owned elements requires authorized element drop"
        );
    }

    auto receiver_type = lowered_type_for_source_type_name(receiver_type_name, context.lowering);
    if (!receiver_type.has_value() || receiver_type->type == "void") {
        return direct_receiver_failure(
            failures,
            record_expression_failures,
            "DynamicArray receiver expression type is not lowerable: " + std::string {receiver_type_name}
        );
    }

    auto lowered_receiver = [&]() -> std::optional<LoweredExpression> {
        if (requires_returned_aggregate_sibling_cleanup) {
            return lower_returned_aggregate_projection_receiver(
                receiver_expression,
                receiver_type_name,
                context,
                session,
                output
            );
        }
        if (requires_named_dynamic_array_element_transfer) {
            return lower_named_dynamic_array_element_projection_receiver(
                receiver_expression,
                receiver_type_name,
                context,
                session,
                output
            );
        }
        return lower_expression(
            receiver_expression,
            receiver_type->type,
            receiver_type->signedness,
            context,
            session,
            output,
            receiver_type_name
        );
    }();
    if (!lowered_receiver.has_value()) {
        return DirectDynamicArrayReceiverLowering {
            .receiver = std::nullopt,
            .diagnostic = [&]() -> std::string {
                if (requires_returned_aggregate_sibling_cleanup) {
                    return "DynamicArray receiver returned aggregate sibling cleanup could not be planned";
                }
                if (requires_named_dynamic_array_element_transfer) {
                    return "DynamicArray receiver named aggregate cleanup could not transfer descriptors through DynamicArray element projection";
                }
                return "DynamicArray receiver expression failed";
            }(),
        };
    }

    auto cleanup_owner_name = std::string {"dynamic_array_receiver_tmp"};
    cleanup_owner_name += std::to_string(session.state.next_temporary_index++);
    auto descriptor_storage = "%" + cleanup_owner_name + ".addr";
    output << "  " << descriptor_storage << " = alloca " << receiver_type->type << "\n";
    output << "  store " << receiver_type->type << " " << lowered_receiver->value;
    output << ", ptr " << descriptor_storage << "\n";

    auto cleanup_plan = plan_dynamic_array_descriptor_cleanup(
        cleanup_owner_name,
        receiver_type_name,
        context.lowering
    );
    if (!cleanup_plan.has_value()) {
        return direct_receiver_failure(
            failures,
            record_expression_failures,
            "DynamicArray receiver cleanup could not be planned: " + std::string {receiver_type_name}
        );
    }
    cleanup_plan->descriptor_storage_name = descriptor_storage;
    cleanup_plan->descriptor_storage_status = DynamicArrayDescriptorStorageStatus::lowered_local_descriptor;
    cleanup_plan->source_line = receiver_expression.line;
    session.state.dynamic_array_local_cleanup_plans.push_back(std::move(*cleanup_plan));

    auto receiver_argument = LoweredExpression {
        .type = receiver_type->type,
        .value = lowered_receiver->value,
        .signedness = receiver_type->signedness,
    };
    if (!method_signature.parameter_types.empty() && method_signature.parameter_types.front() == "ptr") {
        receiver_argument = LoweredExpression {
            .type = "ptr",
            .value = descriptor_storage,
            .signedness = IntegerSignedness::not_integer,
        };
    }

    return DirectDynamicArrayReceiverLowering {
        .receiver = DirectDynamicArrayReceiver {
            .argument = std::move(receiver_argument),
            .cleanup_owner_name = std::move(cleanup_owner_name),
        },
        .diagnostic = {},
    };
}

}  // namespace orison::lowering
