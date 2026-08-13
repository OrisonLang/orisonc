#include "orison/lowering/runtime_index_expression.hpp"

#include <cstddef>
#include <string>

namespace orison::lowering {

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
