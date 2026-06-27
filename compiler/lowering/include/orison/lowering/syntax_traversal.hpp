#pragma once

#include "orison/syntax/module_parser.hpp"

namespace orison::lowering {

template <typename Visitor>
void walk_expression_tree(syntax::ExpressionSyntax const& expression, Visitor&& visitor);

template <typename Visitor>
void walk_statement_expressions(syntax::StatementSyntax const& statement, Visitor&& visitor) {
    walk_expression_tree(statement.expression, visitor);
    walk_expression_tree(statement.assignment_target, visitor);
    for (auto const& nested_statement : statement.nested_statements) {
        walk_statement_expressions(nested_statement, visitor);
    }
    for (auto const& alternate_statement : statement.alternate_statements) {
        walk_statement_expressions(alternate_statement, visitor);
    }
    for (auto const& switch_case : statement.switch_cases) {
        walk_expression_tree(switch_case.pattern, visitor);
        for (auto const& case_statement : switch_case.statements) {
            if (case_statement != nullptr) {
                walk_statement_expressions(*case_statement, visitor);
            }
        }
    }
}

template <typename Visitor>
void walk_expression_tree(syntax::ExpressionSyntax const& expression, Visitor&& visitor) {
    visitor(expression);
    for (auto const& argument : expression.arguments) {
        walk_expression_tree(argument, visitor);
    }
    for (auto const& nested_statement : expression.nested_statements) {
        if (nested_statement != nullptr) {
            walk_statement_expressions(*nested_statement, visitor);
        }
    }
    if (expression.left != nullptr) {
        walk_expression_tree(*expression.left, visitor);
    }
    if (expression.right != nullptr) {
        walk_expression_tree(*expression.right, visitor);
    }
    if (expression.alternate != nullptr) {
        walk_expression_tree(*expression.alternate, visitor);
    }
}

template <typename Visitor>
void walk_function_expressions(syntax::FunctionSyntax const& function, Visitor&& visitor) {
    for (auto const& statement : function.body_statements) {
        walk_statement_expressions(statement, visitor);
    }
}

template <typename Visitor>
void walk_module_expressions(syntax::ModuleSyntax const& module, Visitor&& visitor) {
    for (auto const& function : module.functions) {
        walk_function_expressions(function, visitor);
    }
    for (auto const& implementation : module.implementations) {
        for (auto const& method : implementation.methods) {
            walk_function_expressions(method, visitor);
        }
    }
    for (auto const& extension : module.extensions) {
        for (auto const& method : extension.methods) {
            walk_function_expressions(method, visitor);
        }
    }
}

}  // namespace orison::lowering
