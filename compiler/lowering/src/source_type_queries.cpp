#include "orison/lowering/source_type_queries.hpp"

#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/null_safe_plan.hpp"
#include "orison/lowering/statement_pointer_adapter.hpp"
#include "orison/lowering/type_lowering.hpp"

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

auto view_element_source_type_name(std::string_view type_name) -> std::optional<std::string> {
    auto normalized = type_name;
    if (normalized.starts_with("shared.")) {
        normalized.remove_prefix(std::string_view {"shared."}.size());
    } else if (normalized.starts_with("exclusive.")) {
        normalized.remove_prefix(std::string_view {"exclusive."}.size());
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
    return arguments[0];
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
            .type = "ptr",
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
        return array_element.has_value()
            ? std::move(array_element)
            : view_element_source_type_name(indexed_source_type.value_or(*base_source_type));
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
        if (context.records.contains(expression.left->text)) {
            return expression.left->text;
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
