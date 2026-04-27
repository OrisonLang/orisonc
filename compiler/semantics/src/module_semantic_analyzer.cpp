#include "orison/semantics/module_semantic_analyzer.hpp"

#include <string>
#include <vector>

namespace orison::semantics {
namespace {

class Analyzer {
public:
    explicit Analyzer(syntax::ModuleSyntax const& module) : module_(module) {}

    auto analyze() -> SemanticAnalysisResult {
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

        return SemanticAnalysisResult {
            .diagnostics = std::move(diagnostics_),
            .concurrency_captures = std::move(concurrency_captures),
        };
    }

private:
    struct Binding {
        std::string name;
        std::string type_name;
        bool mutable_binding = false;
        bool receiver_binding = false;
        bool parameter_binding = false;
        std::size_t scope_depth = 0;
    };

    auto render_type_name(syntax::TypeSyntax const& type) const -> std::string {
        std::string rendered = type.name;
        if (type.generic_arguments.empty()) {
            return rendered;
        }

        rendered += "<";
        for (std::size_t index = 0; index < type.generic_arguments.size(); ++index) {
            if (index > 0) {
                rendered += ", ";
            }
            rendered += render_type_name(type.generic_arguments[index]);
        }
        rendered += ">";
        return rendered;
    }

    auto is_obviously_safe_capture_type(std::string const& type_name) const -> bool {
        if (type_name.empty()) {
            return false;
        }

        if (type_name.rfind("shared.", 0) == 0) {
            return true;
        }

        static constexpr char const* safe_types[] = {
            "Int8",   "Int16",  "Int32",   "Int64",   "Int128", "IntSize", "UInt8",  "UInt16",
            "UInt32", "UInt64", "UInt128", "UIntSize", "Float32", "Float64", "Bool",   "Byte",
            "Char",   "Text",   "Address",
        };

        for (auto const* safe_type : safe_types) {
            if (type_name == safe_type) {
                return true;
            }
        }

        return false;
    }

    auto infer_expression_type_name(syntax::ExpressionSyntax const& expression) const -> std::string {
        switch (expression.kind) {
        case syntax::ExpressionKind::name: {
            auto const* binding = find_binding(expression.text);
            return binding == nullptr ? std::string {} : binding->type_name;
        }
        case syntax::ExpressionKind::string_literal:
            return "Text";
        case syntax::ExpressionKind::boolean_literal:
            return "Bool";
        case syntax::ExpressionKind::integer_literal:
            return "Int64";
        default:
            return {};
        }
    }

    auto is_value_return_statement(syntax::StatementSyntax const& statement) const -> bool {
        return statement.kind == syntax::StatementKind::return_statement &&
               (!statement.expression.text.empty() || statement.expression.left || statement.expression.right ||
                statement.expression.alternate || !statement.expression.arguments.empty() ||
                !statement.expression.nested_statements.empty());
    }

    auto expression_requires_value_boundary(syntax::ExpressionSyntax const& expression) const -> bool {
        return expression.kind == syntax::ExpressionKind::task || expression.kind == syntax::ExpressionKind::thread;
    }

    auto classify_concurrency_expression(syntax::ExpressionSyntax const& expression) const -> ConcurrencyExpressionKind {
        return expression.kind == syntax::ExpressionKind::thread ? ConcurrencyExpressionKind::thread
                                                                 : ConcurrencyExpressionKind::task;
    }

    void analyze_function(syntax::FunctionSyntax const& function) {
        scope_stack_.clear();
        push_scope();
        for (auto const& parameter : function.parameters) {
            declare_binding(
                parameter.name,
                render_type_name(parameter.type),
                false,
                parameter.name == "this",
                true
            );
        }

        for (auto const& statement : function.body_statements) {
            analyze_statement(statement, function.is_async);
        }

        pop_scope();
    }

    void analyze_statement(syntax::StatementSyntax const& statement, bool in_async_function) {
        if (statement.kind == syntax::StatementKind::let_binding || statement.kind == syntax::StatementKind::var_binding) {
            analyze_expression(statement.expression, in_async_function);
            auto type_name = !statement.annotated_type.name.empty()
                                 ? render_type_name(statement.annotated_type)
                                 : infer_expression_type_name(statement.expression);
            declare_binding(statement.name, type_name, statement.kind == syntax::StatementKind::var_binding);
            return;
        }

        if (statement.kind == syntax::StatementKind::for_statement) {
            analyze_expression(statement.expression, in_async_function);
            push_scope();
            declare_binding(statement.name, {}, true);
            for (auto const& nested_statement : statement.nested_statements) {
                analyze_statement(nested_statement, in_async_function);
            }
            pop_scope();
            return;
        }

        analyze_expression(statement.assignment_target, in_async_function);
        analyze_expression(statement.expression, in_async_function);

        push_scope();
        for (auto const& nested_statement : statement.nested_statements) {
            analyze_statement(nested_statement, in_async_function);
        }
        pop_scope();

        push_scope();
        for (auto const& alternate_statement : statement.alternate_statements) {
            analyze_statement(alternate_statement, in_async_function);
        }
        pop_scope();

        for (auto const& switch_case : statement.switch_cases) {
            analyze_expression(switch_case.pattern, in_async_function);
            push_scope();
            for (auto const& consequence : switch_case.statements) {
                analyze_statement(*consequence, in_async_function);
            }
            pop_scope();
        }
    }

    void analyze_expression(syntax::ExpressionSyntax const& expression, bool in_async_function) {
        if (expression.kind == syntax::ExpressionKind::name) {
            record_concurrency_capture(expression);
        }

        if (expression.kind == syntax::ExpressionKind::unary && expression.text == "await" && !in_async_function) {
            diagnostics_.error(expression.line, "await expression is only valid inside async functions");
        }

        if (expression.kind == syntax::ExpressionKind::task && !in_async_function) {
            diagnostics_.error(expression.line, "task expression is only valid inside async functions");
        }

        if (expression_requires_value_boundary(expression)) {
            auto const* final_statement =
                expression.nested_statements.empty() ? nullptr : expression.nested_statements.back().get();
            if (final_statement == nullptr ||
                (final_statement->kind != syntax::StatementKind::expression_statement &&
                 !is_value_return_statement(*final_statement))) {
                diagnostics_.error(
                    final_statement != nullptr ? final_statement->line : expression.line,
                    expression.text + " expression body must end with an expression statement or value return"
                );
            }

            auto saved_capture_depth = capture_scope_depth_;
            auto saved_capture_expression_kind = current_capture_expression_kind_;
            capture_scope_depth_ = scope_stack_.size();
            current_capture_expression_kind_ = classify_concurrency_expression(expression);

            push_scope();
            for (auto const& nested_statement : expression.nested_statements) {
                analyze_statement(*nested_statement, in_async_function);
            }
            pop_scope();

            capture_scope_depth_ = saved_capture_depth;
            current_capture_expression_kind_ = saved_capture_expression_kind;
            return;
        }

        for (auto const& argument : expression.arguments) {
            analyze_expression(argument, in_async_function);
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

    void push_scope() {
        scope_stack_.emplace_back();
    }

    void pop_scope() {
        if (!scope_stack_.empty()) {
            scope_stack_.pop_back();
        }
    }

    void declare_binding(
        std::string const& name,
        std::string type_name,
        bool mutable_binding,
        bool receiver_binding = false,
        bool parameter_binding = false
    ) {
        if (scope_stack_.empty()) {
            push_scope();
        }

        scope_stack_.back().push_back(Binding {
            .name = name,
            .type_name = std::move(type_name),
            .mutable_binding = mutable_binding,
            .receiver_binding = receiver_binding,
            .parameter_binding = parameter_binding,
            .scope_depth = scope_stack_.size() - 1,
        });
    }

    auto find_binding(std::string const& name) const -> Binding const* {
        for (auto scope_index = scope_stack_.rbegin(); scope_index != scope_stack_.rend(); ++scope_index) {
            for (auto binding = scope_index->rbegin(); binding != scope_index->rend(); ++binding) {
                if (binding->name == name) {
                    return &*binding;
                }
            }
        }
        return nullptr;
    }

    void record_concurrency_capture(syntax::ExpressionSyntax const& expression) {
        if (capture_scope_depth_ == no_capture_scope_depth) {
            return;
        }

        auto const* binding = find_binding(expression.text);
        if (binding == nullptr || binding->scope_depth >= capture_scope_depth_) {
            return;
        }

        auto capture_kind = ConcurrencyCaptureKind::immutable_outer_local;
        if (binding->receiver_binding) {
            capture_kind = ConcurrencyCaptureKind::receiver;
        } else if (binding->parameter_binding) {
            capture_kind = ConcurrencyCaptureKind::parameter;
        } else if (binding->mutable_binding) {
            capture_kind = ConcurrencyCaptureKind::mutable_outer_local;
        }

        concurrency_captures.push_back(ConcurrencyCapture {
            .line = expression.line,
            .name = expression.text,
            .type_name = binding->type_name,
            .expression_kind = current_capture_expression_kind_,
            .capture_kind = capture_kind,
        });

        if (capture_kind == ConcurrencyCaptureKind::receiver) {
            diagnostics_.error(expression.line, "concurrency expression cannot capture receiver 'this'");
            return;
        }

        if (capture_kind == ConcurrencyCaptureKind::mutable_outer_local) {
            diagnostics_.error(
                expression.line,
                "concurrency expression cannot capture mutable outer local '" + expression.text + "'"
            );
            return;
        }

        if ((capture_kind == ConcurrencyCaptureKind::parameter ||
             capture_kind == ConcurrencyCaptureKind::immutable_outer_local) &&
            !binding->type_name.empty() && !is_obviously_safe_capture_type(binding->type_name)) {
            diagnostics_.error(
                expression.line,
                "concurrency capture '" + expression.text + "' of type '" + binding->type_name +
                    "' requires future Transferable/Shareable analysis"
            );
        }
    }

    syntax::ModuleSyntax const& module_;
    diagnostics::DiagnosticBag diagnostics_;
    std::vector<ConcurrencyCapture> concurrency_captures;
    std::vector<std::vector<Binding>> scope_stack_;
    static constexpr std::size_t no_capture_scope_depth = static_cast<std::size_t>(-1);
    std::size_t capture_scope_depth_ = no_capture_scope_depth;
    ConcurrencyExpressionKind current_capture_expression_kind_ = ConcurrencyExpressionKind::task;
};

}  // namespace

auto ModuleSemanticAnalyzer::analyze(syntax::ModuleSyntax const& module) const -> SemanticAnalysisResult {
    return Analyzer(module).analyze();
}

}  // namespace orison::semantics
