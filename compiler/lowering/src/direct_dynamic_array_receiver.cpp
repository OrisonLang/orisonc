#include "orison/lowering/direct_dynamic_array_receiver.hpp"

#include "orison/lowering/aggregate_path.hpp"
#include "orison/lowering/dynamic_array_cleanup_plan.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/lowering_context.hpp"
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
    std::size_t index = 0;
};

struct DescriptorProjectionPath {
    std::string owner_name;
    std::string source_type_name;
    std::vector<DescriptorProjectionStep> steps;
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

auto decimal_integer_literal_text(
    syntax::ExpressionSyntax const& expression
) -> std::optional<std::string_view> {
    auto const* index_expression = &expression;
    if (index_expression->kind == syntax::ExpressionKind::cast &&
        index_expression->left != nullptr) {
        index_expression = index_expression->left.get();
    }
    if (index_expression->kind != syntax::ExpressionKind::integer_literal ||
        index_expression->text.empty()) {
        return std::nullopt;
    }

    auto all_decimal_digits = true;
    for (auto const digit : index_expression->text) {
        all_decimal_digits = all_decimal_digits && digit >= '0' && digit <= '9';
    }
    return all_decimal_digits
        ? std::optional<std::string_view> {index_expression->text}
        : std::nullopt;
}

auto contains_runtime_indexed_projection(
    syntax::ExpressionSyntax const& expression
) -> bool {
    if (expression.kind == syntax::ExpressionKind::index_access &&
        !expression.arguments.empty() &&
        !decimal_integer_literal_text(expression.arguments.front()).has_value()) {
        return true;
    }
    if (expression.left != nullptr && contains_runtime_indexed_projection(*expression.left)) {
        return true;
    }
    if (expression.right != nullptr && contains_runtime_indexed_projection(*expression.right)) {
        return true;
    }
    if (expression.alternate != nullptr && contains_runtime_indexed_projection(*expression.alternate)) {
        return true;
    }
    return std::ranges::any_of(expression.arguments, contains_runtime_indexed_projection);
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

auto same_descriptor_projection_path(
    DescriptorProjectionPath const& left,
    DescriptorProjectionPath const& right
) -> bool {
    if (left.steps.size() != right.steps.size()) {
        return false;
    }
    for (auto index = std::size_t {0}; index < left.steps.size(); ++index) {
        if (left.steps[index].kind != right.steps[index].kind ||
            left.steps[index].index != right.steps[index].index) {
            return false;
        }
    }
    return true;
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
                .index = index,
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
            .index = field.index,
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

auto selected_descriptor_projection_path(
    AggregatePath const& aggregate_path,
    std::string owner_name,
    std::string source_type_name,
    std::string llvm_type,
    LoweringContext const& context
) -> std::optional<DescriptorProjectionPath> {
    auto steps = std::vector<DescriptorProjectionStep> {};
    for (auto const& step : aggregate_path.steps) {
        if (step.kind == AggregatePathStepKind::member) {
            auto record = context.records.find(source_type_name);
            if (record == context.records.end()) {
                return std::nullopt;
            }
            auto const* field = find_record_field(record->second, step.field_name);
            if (field == nullptr) {
                return std::nullopt;
            }
            steps.push_back(DescriptorProjectionStep {
                .kind = DescriptorProjectionStepKind::field,
                .aggregate_llvm_type = std::move(llvm_type),
                .index = field->index,
            });
            owner_name += ".";
            owner_name += field->name;
            source_type_name = field->source_type_name;
            llvm_type = field->llvm_type;
            continue;
        }

        if (step.index_expression == nullptr) {
            return std::nullopt;
        }
        auto index_text = decimal_integer_literal_text(*step.index_expression);
        if (!index_text.has_value()) {
            return std::nullopt;
        }
        auto index_value = std::size_t {0};
        for (auto character : *index_text) {
            index_value = (index_value * 10) + static_cast<std::size_t>(character - '0');
        }

        auto array_type = parse_llvm_array_type(llvm_type);
        auto array_element_type = array_element_source_type_name(source_type_name);
        if (!array_type.has_value() || !array_element_type.has_value() ||
            index_value >= array_type->length) {
            return std::nullopt;
        }
        steps.push_back(DescriptorProjectionStep {
            .kind = DescriptorProjectionStepKind::array_element,
            .aggregate_llvm_type = std::move(llvm_type),
            .index = index_value,
        });
        owner_name += ".element";
        owner_name += std::to_string(index_value);
        source_type_name = std::move(*array_element_type);
        llvm_type = std::move(array_type->element_type);
    }

    if (!dynamic_array_element_source_type_name(source_type_name).has_value()) {
        return std::nullopt;
    }
    return DescriptorProjectionPath {
        .owner_name = std::move(owner_name),
        .source_type_name = std::move(source_type_name),
        .steps = std::move(steps),
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
            output << ", i32 0, i32 " << step.index << "\n";
        } else {
            output << ", i64 0, i64 " << step.index << "\n";
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
    auto selected_path = selected_descriptor_projection_path(
        *aggregate_path,
        aggregate_owner_name,
        *base_source_type,
        *base_llvm_type,
        context.lowering
    );
    if (!selected_path.has_value() || selected_path->source_type_name != receiver_type_name) {
        return std::nullopt;
    }

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

    auto selected_pointer = emit_descriptor_projection_pointer(
        aggregate_storage,
        *selected_path,
        "%" + aggregate_owner_name + ".selected",
        session,
        output
    );

    for (auto const& descriptor_path : *all_descriptor_paths) {
        if (same_descriptor_projection_path(descriptor_path, *selected_path)) {
            continue;
        }

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

    auto temporary_name = std::string {"%returned_aggregate_receiver_descriptor"};
    temporary_name += std::to_string(session.state.next_temporary_index++);
    output << "  " << temporary_name << " = load " << dynamic_array_descriptor_llvm_type()
           << ", ptr " << selected_pointer << "\n";
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
    if (contains_runtime_indexed_projection(receiver_expression)) {
        return direct_receiver_failure(
            failures,
            record_expression_failures,
            "DynamicArray receiver expression with runtime-indexed aggregate projection requires named binding"
        );
    }
    auto const requires_returned_aggregate_sibling_cleanup =
        returned_aggregate_projection_has_sibling_descriptors(receiver_expression, context.lowering);
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

    auto lowered_receiver = requires_returned_aggregate_sibling_cleanup
        ? lower_returned_aggregate_projection_receiver(
              receiver_expression,
              receiver_type_name,
              context,
              session,
              output
          )
        : lower_expression(
              receiver_expression,
              receiver_type->type,
              receiver_type->signedness,
              context,
              session,
              output,
              receiver_type_name
          );
    if (!lowered_receiver.has_value()) {
        return DirectDynamicArrayReceiverLowering {
            .receiver = std::nullopt,
            .diagnostic = requires_returned_aggregate_sibling_cleanup
                ? "DynamicArray receiver returned aggregate sibling cleanup could not be planned"
                : "DynamicArray receiver expression failed",
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
