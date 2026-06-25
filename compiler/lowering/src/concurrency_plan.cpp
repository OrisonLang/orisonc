#include "orison/lowering/concurrency_plan.hpp"

#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/source_type_queries.hpp"

#include <algorithm>
#include <string_view>

namespace orison::lowering {
namespace {

auto is_concurrency_expression(syntax::ExpressionSyntax const& expression) -> bool {
    return expression.kind == syntax::ExpressionKind::task ||
           expression.kind == syntax::ExpressionKind::thread;
}

auto expression_plan_kind(syntax::ExpressionSyntax const& expression) -> ConcurrencyPlanKind {
    return expression.kind == syntax::ExpressionKind::thread
        ? ConcurrencyPlanKind::thread
        : ConcurrencyPlanKind::task;
}

auto spawn_operation_for(ConcurrencyPlanKind kind) -> ConcurrencyRuntimeOperation {
    return kind == ConcurrencyPlanKind::thread
        ? ConcurrencyRuntimeOperation::spawn_thread
        : ConcurrencyRuntimeOperation::spawn_task;
}

auto semantic_kind_for(ConcurrencyPlanKind kind) -> semantics::ConcurrencyExpressionKind {
    return kind == ConcurrencyPlanKind::thread
        ? semantics::ConcurrencyExpressionKind::thread
        : semantics::ConcurrencyExpressionKind::task;
}

auto append_sanitized_symbol_part(std::string& symbol, std::string_view text) -> void {
    for (auto character : text) {
        if ((character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_') {
            symbol.push_back(character);
            continue;
        }
        symbol.push_back('_');
    }
}

auto concurrency_thunk_symbol_name(
    ConcurrencyPlanKind kind,
    std::string_view enclosing_symbol_name,
    std::size_t expression_line,
    std::size_t ordinal
) -> std::string {
    auto symbol = kind == ConcurrencyPlanKind::thread
        ? std::string {"__orison_thread_thunk."}
        : std::string {"__orison_task_thunk."};
    append_sanitized_symbol_part(symbol, enclosing_symbol_name);
    symbol += "." + std::to_string(expression_line) + "." + std::to_string(ordinal);
    return symbol;
}

auto max_expression_line(syntax::ExpressionSyntax const& expression) -> std::size_t;

auto max_statement_line(syntax::StatementSyntax const& statement) -> std::size_t {
    auto line = statement.line;
    line = std::max(line, max_expression_line(statement.expression));
    line = std::max(line, max_expression_line(statement.assignment_target));
    for (auto const& nested_statement : statement.nested_statements) {
        line = std::max(line, max_statement_line(nested_statement));
    }
    for (auto const& alternate_statement : statement.alternate_statements) {
        line = std::max(line, max_statement_line(alternate_statement));
    }
    for (auto const& switch_case : statement.switch_cases) {
        line = std::max(line, max_expression_line(switch_case.pattern));
        for (auto const& case_statement : switch_case.statements) {
            if (case_statement != nullptr) {
                line = std::max(line, max_statement_line(*case_statement));
            }
        }
    }
    return line;
}

auto max_expression_line(syntax::ExpressionSyntax const& expression) -> std::size_t {
    auto line = expression.line;
    for (auto const& argument : expression.arguments) {
        line = std::max(line, max_expression_line(argument));
    }
    for (auto const& nested_statement : expression.nested_statements) {
        if (nested_statement != nullptr) {
            line = std::max(line, max_statement_line(*nested_statement));
        }
    }
    if (expression.left != nullptr) {
        line = std::max(line, max_expression_line(*expression.left));
    }
    if (expression.right != nullptr) {
        line = std::max(line, max_expression_line(*expression.right));
    }
    if (expression.alternate != nullptr) {
        line = std::max(line, max_expression_line(*expression.alternate));
    }
    return line;
}

auto final_value_expression(
    syntax::ExpressionSyntax const& expression
) -> syntax::ExpressionSyntax const* {
    if (expression.nested_statements.empty() ||
        expression.nested_statements.back() == nullptr) {
        return nullptr;
    }

    auto const& final_statement = *expression.nested_statements.back();
    if (final_statement.kind == syntax::StatementKind::expression_statement ||
        final_statement.kind == syntax::StatementKind::return_statement) {
        return &final_statement.expression;
    }
    return nullptr;
}

auto infer_concurrency_result_type(
    syntax::ExpressionSyntax const& expression,
    LoweringEmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType> {
    auto inferred = infer_expression_type(expression, context, state);
    if (inferred.has_value()) {
        return inferred;
    }

    if (expression.kind == syntax::ExpressionKind::integer_literal) {
        return LoweredType {
            .type = "i64",
            .signedness = IntegerSignedness::signed_integer,
        };
    }

    if (expression.kind == syntax::ExpressionKind::binary && expression.left != nullptr &&
        expression.right != nullptr) {
        auto left = infer_concurrency_result_type(*expression.left, context, state);
        auto right = infer_concurrency_result_type(*expression.right, context, state);
        if (left.has_value() && right.has_value() && left->type == right->type &&
            left->signedness == right->signedness) {
            return left;
        }
    }

    return std::nullopt;
}

auto is_capture_in_expression_range(
    semantics::ConcurrencyCapture const& capture,
    semantics::ConcurrencyExpressionKind expected_kind,
    std::size_t first_line,
    std::size_t last_line
) -> bool {
    return capture.expression_kind == expected_kind &&
           capture.line >= first_line &&
           capture.line <= last_line;
}

}  // namespace

auto plan_concurrency_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view enclosing_symbol_name,
    std::size_t ordinal,
    LoweringEmissionContext const& context,
    FunctionLoweringState const& state,
    semantics::SemanticAnalysisResult const& semantics
) -> std::optional<ConcurrencyExpressionPlan> {
    if (!is_concurrency_expression(expression)) {
        return std::nullopt;
    }

    auto const* result_expression = final_value_expression(expression);
    if (result_expression == nullptr) {
        return std::nullopt;
    }

    auto result_type = infer_concurrency_result_type(*result_expression, context, state);
    if (!result_type.has_value() || result_type->type.empty() || result_type->type == "void") {
        return std::nullopt;
    }

    auto kind = expression_plan_kind(expression);
    auto expression_last_line = max_expression_line(expression);
    auto plan = ConcurrencyExpressionPlan {
        .kind = kind,
        .spawn_operation = spawn_operation_for(kind),
        .thunk_symbol_name = concurrency_thunk_symbol_name(
            kind,
            enclosing_symbol_name,
            expression.line,
            ordinal
        ),
        .result_type = *result_type,
    };

    auto semantic_kind = semantic_kind_for(kind);
    for (auto const& capture : semantics.concurrency_captures) {
        if (!is_capture_in_expression_range(capture, semantic_kind, expression.line, expression_last_line)) {
            continue;
        }

        auto lowered_capture_type = lowered_type_for_source_type_name(capture.type_name, context.lowering);
        plan.captures.push_back(ConcurrencyCapturePlan {
            .name = capture.name,
            .source_type_name = capture.type_name,
            .llvm_type = lowered_capture_type.has_value() ? lowered_capture_type->type : std::string {},
            .capture_kind = capture.capture_kind,
        });
    }

    return plan;
}

}  // namespace orison::lowering
