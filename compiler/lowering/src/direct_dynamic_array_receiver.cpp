#include "orison/lowering/direct_dynamic_array_receiver.hpp"

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

auto returned_aggregate_projection_requires_named_binding(
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
    if (returned_aggregate_projection_requires_named_binding(receiver_expression, context.lowering)) {
        return direct_receiver_failure(
            failures,
            record_expression_failures,
            "DynamicArray receiver expression over returned aggregate with sibling descriptors requires named binding"
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

    auto lowered_receiver = lower_expression(
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
            .diagnostic = "DynamicArray receiver expression failed",
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
