#include "orison/lowering/syntax_traversal.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

auto expression(orison::syntax::ExpressionKind kind, std::string text) -> orison::syntax::ExpressionSyntax {
    return orison::syntax::ExpressionSyntax {
        .kind = kind,
        .text = std::move(text),
    };
}

auto expression_statement(orison::syntax::ExpressionSyntax expression) -> orison::syntax::StatementSyntax {
    return orison::syntax::StatementSyntax {
        .kind = orison::syntax::StatementKind::expression_statement,
        .expression = std::move(expression),
    };
}

auto statement_ptr(orison::syntax::StatementSyntax statement)
    -> std::unique_ptr<orison::syntax::StatementSyntax> {
    return std::make_unique<orison::syntax::StatementSyntax>(std::move(statement));
}

auto count_visit(std::vector<std::string> const& visited, std::string const& text) -> std::size_t {
    return static_cast<std::size_t>(std::count(visited.begin(), visited.end(), text));
}

}  // namespace

int main() {
    auto module = orison::syntax::ModuleSyntax {};

    auto function = orison::syntax::FunctionSyntax {};
    function.name = "scan";

    auto assignment = orison::syntax::StatementSyntax {
        .kind = orison::syntax::StatementKind::assignment_statement,
        .assignment_target = expression(orison::syntax::ExpressionKind::name, "target"),
        .expression = expression(orison::syntax::ExpressionKind::name, "assigned"),
    };
    function.body_statements.push_back(std::move(assignment));

    auto switch_statement = orison::syntax::StatementSyntax {
        .kind = orison::syntax::StatementKind::switch_statement,
        .expression = expression(orison::syntax::ExpressionKind::name, "subject"),
    };
    auto switch_case = orison::syntax::SwitchCaseSyntax {
        .pattern = expression(orison::syntax::ExpressionKind::integer_literal, "case_pattern"),
    };
    switch_case.statements.push_back(statement_ptr(expression_statement(
        expression(orison::syntax::ExpressionKind::thread, "case_thread")
    )));
    switch_statement.switch_cases.push_back(std::move(switch_case));
    function.body_statements.push_back(std::move(switch_statement));

    auto task = expression(orison::syntax::ExpressionKind::task, "outer_task");
    task.nested_statements.push_back(statement_ptr(expression_statement(
        expression(orison::syntax::ExpressionKind::thread, "nested_thread")
    )));
    function.body_statements.push_back(expression_statement(std::move(task)));

    module.functions.push_back(std::move(function));

    auto implementation = orison::syntax::ImplementationSyntax {};
    auto implementation_method = orison::syntax::FunctionSyntax {};
    implementation_method.name = "impl_method";
    auto await_expression = expression(orison::syntax::ExpressionKind::unary, "await");
    await_expression.left = std::make_unique<orison::syntax::ExpressionSyntax>(
        expression(orison::syntax::ExpressionKind::name, "pending")
    );
    implementation_method.body_statements.push_back(expression_statement(std::move(await_expression)));
    implementation.methods.push_back(std::move(implementation_method));
    module.implementations.push_back(std::move(implementation));

    auto extension = orison::syntax::ExtensionSyntax {};
    auto extension_method = orison::syntax::FunctionSyntax {};
    extension_method.name = "ext_method";
    auto join_member = expression(orison::syntax::ExpressionKind::member_access, "join");
    join_member.left = std::make_unique<orison::syntax::ExpressionSyntax>(
        expression(orison::syntax::ExpressionKind::name, "worker")
    );
    auto join_call = expression(orison::syntax::ExpressionKind::call, "join_call");
    join_call.left = std::make_unique<orison::syntax::ExpressionSyntax>(std::move(join_member));
    extension_method.body_statements.push_back(expression_statement(std::move(join_call)));
    extension.methods.push_back(std::move(extension_method));
    module.extensions.push_back(std::move(extension));

    auto visited = std::vector<std::string> {};
    orison::lowering::walk_module_expressions(
        module,
        [&visited](orison::syntax::ExpressionSyntax const& visited_expression) {
            if (!visited_expression.text.empty()) {
                visited.push_back(visited_expression.text);
            }
        }
    );

    assert(count_visit(visited, "target") == 1);
    assert(count_visit(visited, "assigned") == 1);
    assert(count_visit(visited, "subject") == 1);
    assert(count_visit(visited, "case_pattern") == 1);
    assert(count_visit(visited, "case_thread") == 1);
    assert(count_visit(visited, "outer_task") == 1);
    assert(count_visit(visited, "nested_thread") == 1);
    assert(count_visit(visited, "await") == 1);
    assert(count_visit(visited, "pending") == 1);
    assert(count_visit(visited, "join_call") == 1);
    assert(count_visit(visited, "join") == 1);
    assert(count_visit(visited, "worker") == 1);

    return 0;
}
