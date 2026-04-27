#include "orison/semantics/module_semantic_analyzer.hpp"

namespace orison::semantics {
namespace {

class Analyzer {
public:
    explicit Analyzer(syntax::ModuleSyntax const& module) : module_(module) {}

    auto analyze() -> diagnostics::DiagnosticBag {
        for (auto const& function : module_.functions) {
            analyze_function(function);
        }

        for (auto const& implementation : module_.implementations) {
            for (auto const& method : implementation.methods) {
                analyze_function(method);
            }
        }

        for (auto const& extension : module_.extensions) {
            for (auto const& method : extension.methods) {
                analyze_function(method);
            }
        }

        for (auto const& foreign_export : module_.foreign_exports) {
            analyze_function(foreign_export.function);
        }

        return diagnostics_;
    }

private:
    void analyze_function(syntax::FunctionSyntax const& function) {
        for (auto const& statement : function.body_statements) {
            analyze_statement(statement, function.is_async);
        }
    }

    void analyze_statement(syntax::StatementSyntax const& statement, bool in_async_function) {
        analyze_expression(statement.assignment_target, in_async_function);
        analyze_expression(statement.expression, in_async_function);

        for (auto const& nested_statement : statement.nested_statements) {
            analyze_statement(nested_statement, in_async_function);
        }

        for (auto const& alternate_statement : statement.alternate_statements) {
            analyze_statement(alternate_statement, in_async_function);
        }

        for (auto const& switch_case : statement.switch_cases) {
            analyze_expression(switch_case.pattern, in_async_function);
            for (auto const& consequence : switch_case.statements) {
                analyze_statement(*consequence, in_async_function);
            }
        }
    }

    void analyze_expression(syntax::ExpressionSyntax const& expression, bool in_async_function) {
        if (expression.kind == syntax::ExpressionKind::unary && expression.text == "await" && !in_async_function) {
            diagnostics_.error(expression.line, "await expression is only valid inside async functions");
        }

        for (auto const& argument : expression.arguments) {
            analyze_expression(argument, in_async_function);
        }

        for (auto const& nested_statement : expression.nested_statements) {
            analyze_statement(*nested_statement, in_async_function);
        }

        if (expression.left) {
            analyze_expression(*expression.left, in_async_function);
        }

        if (expression.right) {
            analyze_expression(*expression.right, in_async_function);
        }

        if (expression.alternate) {
            analyze_expression(*expression.alternate, in_async_function);
        }
    }

    syntax::ModuleSyntax const& module_;
    diagnostics::DiagnosticBag diagnostics_;
};

}  // namespace

auto ModuleSemanticAnalyzer::analyze(syntax::ModuleSyntax const& module) const -> diagnostics::DiagnosticBag {
    return Analyzer(module).analyze();
}

}  // namespace orison::semantics
