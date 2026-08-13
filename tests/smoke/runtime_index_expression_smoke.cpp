#include "orison/lowering/runtime_index_expression.hpp"

#include <cassert>
#include <memory>
#include <string>
#include <utility>

namespace {

auto name(std::string text) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::name;
    expression.text = std::move(text);
    return expression;
}

auto integer_literal(std::string text) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::integer_literal;
    expression.text = std::move(text);
    return expression;
}

auto unary(std::string op, orison::syntax::ExpressionSyntax inner) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::unary;
    expression.text = std::move(op);
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(inner));
    return expression;
}

auto binary(
    orison::syntax::ExpressionSyntax left,
    std::string op,
    orison::syntax::ExpressionSyntax right
) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::binary;
    expression.text = std::move(op);
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(left));
    expression.right = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(right));
    return expression;
}

auto cast(orison::syntax::ExpressionSyntax inner, std::string type_name) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::cast;
    expression.text = std::move(type_name);
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(inner));
    return expression;
}

auto call(std::string callee, orison::syntax::ExpressionSyntax argument) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::call;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(name(std::move(callee)));
    expression.arguments.push_back(std::move(argument));
    return expression;
}

auto member(orison::syntax::ExpressionSyntax left, std::string field) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::member_access;
    expression.text = std::move(field);
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(left));
    return expression;
}

auto null_safe_member(orison::syntax::ExpressionSyntax left, std::string field) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::null_safe_member_access;
    expression.text = std::move(field);
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(left));
    return expression;
}

auto index(orison::syntax::ExpressionSyntax left, orison::syntax::ExpressionSyntax index_expression)
    -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::index_access;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(left));
    expression.arguments.push_back(std::move(index_expression));
    return expression;
}

auto ternary(
    orison::syntax::ExpressionSyntax condition,
    orison::syntax::ExpressionSyntax then_expression,
    orison::syntax::ExpressionSyntax else_expression
) -> orison::syntax::ExpressionSyntax {
    auto expression = orison::syntax::ExpressionSyntax {};
    expression.kind = orison::syntax::ExpressionKind::ternary;
    expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(condition));
    expression.right = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(then_expression));
    expression.alternate = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(else_expression));
    return expression;
}

}  // namespace

int main() {
    using orison::lowering::runtime_index_expression_key;

    assert(runtime_index_expression_key(name("index")) == "index");
    assert(runtime_index_expression_key(integer_literal("7")) == "7");
    assert(runtime_index_expression_key(binary(name("index"), "+", name("zero"))) == "(index + zero)");
    assert(runtime_index_expression_key(cast(name("index"), "UInt64")) == "index as UInt64");
    assert(runtime_index_expression_key(unary("not", name("flag"))) == "not flag");
    assert(runtime_index_expression_key(unary("-", name("offset"))) == "-offset");
    assert(runtime_index_expression_key(call("normalize", name("index"))) == "normalize(index)");
    assert(runtime_index_expression_key(member(name("item"), "value")) == "item.value");
    assert(runtime_index_expression_key(null_safe_member(name("user"), "profile")) == "user?.profile");
    assert(runtime_index_expression_key(index(name("items"), name("index"))) == "items[index]");
    assert(
        runtime_index_expression_key(
            index(name("items"), binary(name("index"), "+", integer_literal("1")))
        ) == "items[(index + 1)]"
    );
    assert(
        runtime_index_expression_key(ternary(name("flag"), name("left"), name("right"))) ==
        "(flag ? left : right)"
    );
}
