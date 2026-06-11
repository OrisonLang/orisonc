#include "orison/lowering/control_flow_emitter.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/lowering/type_lowering.hpp"

#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace orison::lowering {
namespace {

using EmissionContext = ExpressionEmissionContext;
using FunctionLoweringState = ExpressionLoweringState;

auto return_expression_for(syntax::StatementSyntax const& statement) -> syntax::ExpressionSyntax const* {
    if (statement.kind == syntax::StatementKind::return_statement) {
        return &statement.expression;
    }
    if (statement.kind == syntax::StatementKind::expression_statement) {
        return &statement.expression;
    }

    return nullptr;
}

auto lower_let_binding(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringState& state,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    auto lowered = lower_expression(statement.expression, expected_llvm_type, expected_signedness, context, state, output);
    if (!lowered.has_value()) {
        auto detail = render_expression_lowering_failure(state.failure);
        diagnostics.error(
            statement.line,
            "lowering does not yet support this let initializer" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return false;
    }

    auto local_name = next_llvm_local_value_name(statement.name, state.local_name_counts);
    output << "  " << local_name << " = add " << lowered->type << " 0, " << lowered->value << "\n";
    state.immutable_bindings[statement.name] = LoweredExpression {
        .type = lowered->type,
        .value = std::move(local_name),
        .signedness = lowered->signedness,
    };
    return true;
}

auto lower_final_if_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringState& state,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression>;

auto lower_final_switch_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringState& state,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression>;

auto lower_value_statement_block(
    std::vector<syntax::StatementSyntax> const& statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringState& state,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (statements.empty()) {
        return std::nullopt;
    }

    for (auto index = std::size_t {0}; index + 1 < statements.size(); ++index) {
        auto const& statement = statements[index];
        if (statement.kind != syntax::StatementKind::let_binding ||
            !lower_let_binding(
                statement,
                expected_llvm_type,
                expected_signedness,
                context,
                state,
                diagnostics,
                output
            )) {
            return std::nullopt;
        }
    }

    auto const& final_statement = statements.back();
    if (final_statement.kind == syntax::StatementKind::if_statement) {
        return lower_final_if_statement(
            final_statement,
            expected_llvm_type,
            expected_signedness,
            context,
            state,
            diagnostics,
            output
        );
    }
    if (final_statement.kind == syntax::StatementKind::switch_statement) {
        return lower_final_switch_statement(
            final_statement,
            expected_llvm_type,
            expected_signedness,
            context,
            state,
            diagnostics,
            output
        );
    }

    auto const* expression = return_expression_for(final_statement);
    if (expression == nullptr) {
        return std::nullopt;
    }
    return lower_expression(
        *expression,
        expected_llvm_type,
        expected_signedness,
        context,
        state,
        output
    );
}

auto lower_final_if_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringState& state,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (statement.kind != syntax::StatementKind::if_statement || statement.nested_statements.empty() ||
        statement.alternate_statements.empty()) {
        return std::nullopt;
    }

    auto condition = lower_expression(
        statement.expression,
        "i1",
        IntegerSignedness::not_integer,
        context,
        state,
        output
    );
    if (!condition.has_value()) {
        return std::nullopt;
    }

    auto block_index = next_llvm_block_index(state.next_block_index);
    auto then_block = llvm_block_name("if.then", block_index);
    auto else_block = llvm_block_name("if.else", block_index);
    auto merge_block = llvm_block_name("if.merge", block_index);
    output << "  br i1 " << condition->value << ", label %" << then_block << ", label %" << else_block << "\n";

    output << then_block << ":\n";
    state.current_block = then_block;
    auto outer_bindings = state.immutable_bindings;
    auto then_value = lower_value_statement_block(
        statement.nested_statements,
        expected_llvm_type,
        expected_signedness,
        context,
        state,
        diagnostics,
        output
    );
    if (!then_value.has_value()) {
        return std::nullopt;
    }
    auto then_exit_block = state.current_block;
    output << "  br label %" << merge_block << "\n";

    output << else_block << ":\n";
    state.current_block = else_block;
    state.immutable_bindings = outer_bindings;
    auto else_value = lower_value_statement_block(
        statement.alternate_statements,
        expected_llvm_type,
        expected_signedness,
        context,
        state,
        diagnostics,
        output
    );
    if (!else_value.has_value() || then_value->type != else_value->type ||
        then_value->signedness != else_value->signedness) {
        return std::nullopt;
    }
    auto else_exit_block = state.current_block;
    output << "  br label %" << merge_block << "\n";

    output << merge_block << ":\n";
    state.current_block = merge_block;
    state.immutable_bindings = std::move(outer_bindings);
    auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
    output << "  " << temporary_name << " = phi " << then_value->type << " [" << then_value->value;
    output << ", %" << then_exit_block << "], [" << else_value->value << ", %" << else_exit_block << "]\n";
    return LoweredExpression {
        .type = then_value->type,
        .value = std::move(temporary_name),
        .signedness = then_value->signedness,
    };
}

auto lower_switch_case_statement_block(
    std::vector<std::unique_ptr<syntax::StatementSyntax>> const& statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringState& state,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (statements.empty()) {
        return std::nullopt;
    }

    for (auto index = std::size_t {0}; index + 1 < statements.size(); ++index) {
        auto const* statement = statements[index].get();
        if (statement == nullptr || statement->kind != syntax::StatementKind::let_binding ||
            !lower_let_binding(
                *statement,
                expected_llvm_type,
                expected_signedness,
                context,
                state,
                diagnostics,
                output
            )) {
            return std::nullopt;
        }
    }

    auto const* final_statement = statements.back().get();
    if (final_statement == nullptr) {
        return std::nullopt;
    }
    if (final_statement->kind == syntax::StatementKind::if_statement) {
        return lower_final_if_statement(
            *final_statement,
            expected_llvm_type,
            expected_signedness,
            context,
            state,
            diagnostics,
            output
        );
    }
    if (final_statement->kind == syntax::StatementKind::switch_statement) {
        return lower_final_switch_statement(
            *final_statement,
            expected_llvm_type,
            expected_signedness,
            context,
            state,
            diagnostics,
            output
        );
    }

    auto const* expression = return_expression_for(*final_statement);
    if (expression == nullptr) {
        return std::nullopt;
    }
    return lower_expression(
        *expression,
        expected_llvm_type,
        expected_signedness,
        context,
        state,
        output
    );
}

auto lowered_switch_pattern(
    syntax::ExpressionSyntax const& pattern,
    LoweredType const& subject_type
) -> std::optional<LoweredExpression> {
    if (auto literal = lower_integer_literal(pattern, subject_type.type, subject_type.signedness)) {
        return literal;
    }
    if (auto literal = lower_boolean_literal(pattern, subject_type.type)) {
        return literal;
    }
    if (pattern.kind != syntax::ExpressionKind::cast || pattern.left == nullptr) {
        return std::nullopt;
    }

    syntax::TypeSyntax target_type {
        .name = pattern.text,
    };
    auto cast_type = llvm_type_for(target_type);
    if (!cast_type.has_value() || *cast_type != subject_type.type) {
        return std::nullopt;
    }
    return lower_integer_literal(*pattern.left, subject_type.type, subject_type.signedness);
}

auto lower_final_switch_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringState& state,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (statement.kind != syntax::StatementKind::switch_statement || statement.switch_cases.empty()) {
        return std::nullopt;
    }

    auto subject_type = infer_expression_type(statement.expression, context, state);
    if (!subject_type.has_value() ||
        (subject_type->type != "i1" && !is_integer_llvm_type(subject_type->type))) {
        return std::nullopt;
    }
    auto subject = lower_expression(
        statement.expression,
        subject_type->type,
        subject_type->signedness,
        context,
        state,
        output
    );
    if (!subject.has_value()) {
        return std::nullopt;
    }

    struct LoweredSwitchCase {
        syntax::SwitchCaseSyntax const* syntax = nullptr;
        std::string block;
        std::optional<LoweredExpression> pattern;
    };

    auto block_index = next_llvm_block_index(state.next_block_index);
    auto merge_block = llvm_block_name("switch.merge", block_index);
    auto fallback_block = llvm_block_name("switch.unreachable", block_index);
    auto lowered_cases = std::vector<LoweredSwitchCase> {};
    lowered_cases.reserve(statement.switch_cases.size());
    auto default_case_index = std::optional<std::size_t> {};
    auto value_case_index = std::size_t {0};

    for (auto const& switch_case : statement.switch_cases) {
        auto lowered_case = LoweredSwitchCase {
            .syntax = &switch_case,
        };
        if (switch_case.is_default) {
            if (default_case_index.has_value()) {
                return std::nullopt;
            }
            lowered_case.block = llvm_block_name("switch.default", block_index);
            default_case_index = lowered_cases.size();
        } else {
            lowered_case.block = llvm_block_name("switch.case", block_index, value_case_index++);
            lowered_case.pattern = lowered_switch_pattern(switch_case.pattern, *subject_type);
            if (!lowered_case.pattern.has_value()) {
                return std::nullopt;
            }
        }
        lowered_cases.push_back(std::move(lowered_case));
    }

    auto const& default_block =
        default_case_index.has_value() ? lowered_cases[*default_case_index].block : fallback_block;
    output << "  switch " << subject->type << " " << subject->value << ", label %" << default_block << " [\n";
    for (auto const& lowered_case : lowered_cases) {
        if (lowered_case.pattern.has_value()) {
            output << "    " << subject->type << " " << lowered_case.pattern->value;
            output << ", label %" << lowered_case.block << "\n";
        }
    }
    output << "  ]\n";

    auto outer_bindings = state.immutable_bindings;
    auto incoming_values = std::vector<std::pair<LoweredExpression, std::string>> {};
    incoming_values.reserve(lowered_cases.size());
    for (auto const& lowered_case : lowered_cases) {
        output << lowered_case.block << ":\n";
        state.current_block = lowered_case.block;
        state.immutable_bindings = outer_bindings;
        auto case_value = lower_switch_case_statement_block(
            lowered_case.syntax->statements,
            expected_llvm_type,
            expected_signedness,
            context,
            state,
            diagnostics,
            output
        );
        if (!case_value.has_value()) {
            return std::nullopt;
        }
        auto case_exit_block = state.current_block;
        output << "  br label %" << merge_block << "\n";
        incoming_values.emplace_back(std::move(*case_value), std::move(case_exit_block));
    }

    if (!default_case_index.has_value()) {
        output << fallback_block << ":\n";
        output << "  unreachable\n";
    }

    if (incoming_values.empty()) {
        return std::nullopt;
    }
    auto const& result_type = incoming_values.front().first;
    for (auto const& [incoming_value, incoming_block] : incoming_values) {
        static_cast<void>(incoming_block);
        if (incoming_value.type != result_type.type || incoming_value.signedness != result_type.signedness) {
            return std::nullopt;
        }
    }

    output << merge_block << ":\n";
    state.current_block = merge_block;
    state.immutable_bindings = std::move(outer_bindings);
    auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
    output << "  " << temporary_name << " = phi " << result_type.type << " ";
    for (auto index = std::size_t {0}; index < incoming_values.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << "[" << incoming_values[index].first.value << ", %" << incoming_values[index].second << "]";
    }
    output << "\n";
    return LoweredExpression {
        .type = result_type.type,
        .value = std::move(temporary_name),
        .signedness = result_type.signedness,
    };
}

}  // namespace

auto value_expression_for(
    syntax::StatementSyntax const& statement
) -> syntax::ExpressionSyntax const* {
    return return_expression_for(statement);
}

auto lower_let_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    ExpressionEmissionContext const& context,
    ExpressionLoweringState& state,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    return lower_let_binding(
        statement,
        expected_llvm_type,
        expected_signedness,
        context,
        state,
        diagnostics,
        output
    );
}

auto lower_final_control_flow_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    ExpressionEmissionContext const& context,
    ExpressionLoweringState& state,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (statement.kind == syntax::StatementKind::if_statement) {
        return lower_final_if_statement(
            statement,
            expected_llvm_type,
            expected_signedness,
            context,
            state,
            diagnostics,
            output
        );
    }
    if (statement.kind == syntax::StatementKind::switch_statement) {
        return lower_final_switch_statement(
            statement,
            expected_llvm_type,
            expected_signedness,
            context,
            state,
            diagnostics,
            output
        );
    }
    return std::nullopt;
}

}  // namespace orison::lowering
