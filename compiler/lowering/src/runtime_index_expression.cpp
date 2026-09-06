#include "orison/lowering/runtime_index_expression.hpp"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace orison::lowering {

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

auto is_runtime_index_expression(
    syntax::ExpressionSyntax const& expression
) -> bool {
    return !decimal_integer_literal_text(expression).has_value();
}

auto contains_runtime_indexed_projection(
    syntax::ExpressionSyntax const& expression
) -> bool {
    if (expression.kind == syntax::ExpressionKind::index_access &&
        !expression.arguments.empty() &&
        is_runtime_index_expression(expression.arguments.front())) {
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

auto runtime_index_expression_key(
    syntax::ExpressionSyntax const& expression
) -> std::string {
    using syntax::ExpressionKind;
    switch (expression.kind) {
    case ExpressionKind::name:
    case ExpressionKind::integer_literal:
    case ExpressionKind::float_literal:
    case ExpressionKind::string_literal:
    case ExpressionKind::boolean_literal:
        return expression.text;
    case ExpressionKind::array_literal: {
        auto rendered = std::string {"["};
        for (auto index = std::size_t {0}; index < expression.arguments.size(); ++index) {
            if (index != 0) {
                rendered += ", ";
            }
            rendered += runtime_index_expression_key(expression.arguments[index]);
        }
        rendered += "]";
        return rendered;
    }
    case ExpressionKind::unary:
        if (expression.left == nullptr) {
            return expression.text.empty() ? std::string {"<computed>"} : expression.text;
        }
        if (expression.text == "not" || expression.text == "bit_not" || expression.text == "await") {
            return expression.text + " " + runtime_index_expression_key(*expression.left);
        }
        return expression.text + runtime_index_expression_key(*expression.left);
    case ExpressionKind::cast:
        if (expression.left == nullptr) {
            return expression.text.empty() ? std::string {"<computed>"} : expression.text;
        }
        return runtime_index_expression_key(*expression.left) + " as " + expression.text;
    case ExpressionKind::call: {
        if (expression.left == nullptr) {
            return expression.text.empty() ? std::string {"<computed>"} : expression.text;
        }
        auto rendered = runtime_index_expression_key(*expression.left) + "(";
        for (auto index = std::size_t {0}; index < expression.arguments.size(); ++index) {
            if (index != 0) {
                rendered += ", ";
            }
            rendered += runtime_index_expression_key(expression.arguments[index]);
        }
        rendered += ")";
        return rendered;
    }
    case ExpressionKind::member_access:
        if (expression.left == nullptr) {
            return expression.text;
        }
        return runtime_index_expression_key(*expression.left) + "." + expression.text;
    case ExpressionKind::null_safe_member_access:
        if (expression.left == nullptr) {
            return expression.text;
        }
        return runtime_index_expression_key(*expression.left) + "?." + expression.text;
    case ExpressionKind::index_access:
        if (expression.left == nullptr || expression.arguments.empty()) {
            return expression.text.empty() ? std::string {"<computed>"} : expression.text;
        }
        return runtime_index_expression_key(*expression.left) + "[" +
            runtime_index_expression_key(expression.arguments.front()) + "]";
    case ExpressionKind::binary:
        if (expression.left == nullptr || expression.right == nullptr) {
            return expression.text.empty() ? std::string {"<computed>"} : expression.text;
        }
        return "(" + runtime_index_expression_key(*expression.left) + " " + expression.text + " " +
            runtime_index_expression_key(*expression.right) + ")";
    case ExpressionKind::ternary:
        if (expression.left == nullptr || expression.right == nullptr || expression.alternate == nullptr) {
            return expression.text.empty() ? std::string {"<computed>"} : expression.text;
        }
        return "(" + runtime_index_expression_key(*expression.left) + " ? " +
            runtime_index_expression_key(*expression.right) + " : " +
            runtime_index_expression_key(*expression.alternate) + ")";
    case ExpressionKind::task:
    case ExpressionKind::thread:
        return expression.text.empty() ? std::string {"<computed>"} : expression.text;
    }
    return expression.text.empty() ? std::string {"<computed>"} : expression.text;
}

}  // namespace orison::lowering
