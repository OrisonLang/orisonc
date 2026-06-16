#include "orison/lowering/statement_emitter.hpp"

#include "orison/lowering/call_emitter.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/member_call_receiver.hpp"

#include <span>
#include <string>
#include <utility>

namespace orison::lowering {
namespace {

auto lower_prefix_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    if (statement.kind == syntax::StatementKind::let_binding) {
        return lower_let_statement(
            statement,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            diagnostics,
            output
        );
    }
    if (statement.kind == syntax::StatementKind::var_binding) {
        return lower_var_statement(
            statement,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            diagnostics,
            output
        );
    }
    if (statement.kind == syntax::StatementKind::assignment_statement) {
        return lower_assignment_statement(statement, context, session, diagnostics, output);
    }
    return false;
}

auto lower_value_statement_block(
    std::span<syntax::StatementSyntax const* const> statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    FinalControlFlowLowerer lower_final_control_flow
) -> std::optional<LoweredExpression> {
    if (statements.empty()) {
        return std::nullopt;
    }

    for (auto index = std::size_t {0}; index + 1 < statements.size(); ++index) {
        auto const* statement = statements[index];
        if (statement == nullptr ||
            !lower_prefix_statement(
                *statement,
                expected_llvm_type,
                expected_signedness,
                context,
                session,
                diagnostics,
                output
            )) {
            return std::nullopt;
        }
    }

    auto const* final_statement = statements.back();
    if (final_statement == nullptr) {
        return std::nullopt;
    }
    if (final_statement->kind == syntax::StatementKind::if_statement ||
        final_statement->kind == syntax::StatementKind::switch_statement) {
        return lower_final_control_flow(
            *final_statement,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            diagnostics,
            output
        );
    }

    auto const* expression = value_expression_for(*final_statement);
    if (expression == nullptr) {
        return std::nullopt;
    }
    return lower_expression(
        *expression,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        output
    );
}

auto lower_void_call_statement(
    syntax::StatementSyntax const& statement,
    std::string const& function_name,
    LoweredFunctionSignature const& function,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    if (function.parameter_types.size() != statement.expression.arguments.size()) {
        session.failures.expression = ExpressionLoweringFailure {
            .reason = ExpressionLoweringFailureReason::call_arity_mismatch,
            .detail = function_name + " expects " +
                std::to_string(function.parameter_types.size()) + " arguments, got " +
                std::to_string(statement.expression.arguments.size()),
        };
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(statement.line, "lowering call statement failed: " + detail);
        return false;
    }

    auto arguments = lower_call_arguments(
        statement.expression,
        function,
        context,
        session,
        output
    );
    if (!arguments.has_value()) {
        if (session.failures.expression.reason == ExpressionLoweringFailureReason::none) {
            session.failures.expression = ExpressionLoweringFailure {
                .reason = ExpressionLoweringFailureReason::call_argument_failure,
                .detail = function_name,
            };
        }
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            "lowering call statement failed" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return false;
    }

    emit_void_call(function, *arguments, output);
    return true;
}

struct ResolvedMemberCall {
    MemberCallReceiverInference receiver;
    LoweredMethodLookup method;
};

auto resolve_member_call(
    syntax::ExpressionSyntax const& expression,
    LoweringEmissionContext const& context,
    FunctionLoweringState const& state
) -> ResolvedMemberCall {
    auto receiver = infer_member_call_receiver(expression, state);
    if (receiver.result != MemberCallReceiverInferenceResult::found) {
        return {.receiver = std::move(receiver)};
    }

    auto method = find_lowered_method_signature(
        context.lowering,
        receiver.receiver_type_name,
        receiver.method_name
    );
    return ResolvedMemberCall {
        .receiver = std::move(receiver),
        .method = std::move(method),
    };
}

auto member_call_target_name(ResolvedMemberCall const& resolved) -> std::string {
    return resolved.receiver.receiver_type_name + "." + resolved.receiver.method_name;
}

auto diagnose_member_call_statement(
    syntax::StatementSyntax const& statement,
    ResolvedMemberCall const& resolved,
    diagnostics::DiagnosticBag& diagnostics
) -> LoweredFunctionSignature const* {
    if (resolved.receiver.result == MemberCallReceiverInferenceResult::unsupported_shape) {
        diagnostics.error(statement.line, "lowering member call statement has unsupported receiver shape");
        return nullptr;
    }
    if (resolved.receiver.result == MemberCallReceiverInferenceResult::not_found) {
        diagnostics.error(statement.line, "lowering member call receiver type is unknown");
        return nullptr;
    }

    auto const target_name = member_call_target_name(resolved);
    if (resolved.method.result == LoweredMethodLookupResult::not_found) {
        diagnostics.error(
            statement.line,
            "lowering member call target is unknown: " + target_name
        );
        return nullptr;
    }
    if (resolved.method.result == LoweredMethodLookupResult::ambiguous) {
        diagnostics.error(
            statement.line,
            "lowering member call target is ambiguous: " + target_name
        );
        return nullptr;
    }

    if (resolved.method.method == nullptr ||
        !has_supported_function_signature_types(resolved.method.method->signature)) {
        diagnostics.error(
            statement.line,
            "lowering member call target is not lowerable: " + target_name
        );
        return nullptr;
    }

    return &resolved.method.method->signature;
}

auto lower_void_member_call_statement(
    syntax::StatementSyntax const& statement,
    syntax::ExpressionSyntax const& receiver_expression,
    std::string const& target_name,
    LoweredFunctionSignature const& function,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    auto const expected_argument_count = function.parameter_types.empty()
        ? std::size_t {0}
        : function.parameter_types.size() - 1;
    if (expected_argument_count != statement.expression.arguments.size()) {
        session.failures.expression = ExpressionLoweringFailure {
            .reason = ExpressionLoweringFailureReason::call_arity_mismatch,
            .detail = target_name + " expects " + std::to_string(expected_argument_count) +
                " arguments, got " + std::to_string(statement.expression.arguments.size()),
        };
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(statement.line, "lowering member call statement failed: " + detail);
        return false;
    }

    auto arguments = lower_member_call_arguments(
        receiver_expression,
        std::span<syntax::ExpressionSyntax const>(
            statement.expression.arguments.data(),
            statement.expression.arguments.size()
        ),
        function,
        context,
        session,
        output
    );
    if (!arguments.has_value()) {
        if (session.failures.expression.reason == ExpressionLoweringFailureReason::none) {
            session.failures.expression = ExpressionLoweringFailure {
                .reason = ExpressionLoweringFailureReason::call_argument_failure,
                .detail = target_name,
            };
        }
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            "lowering member call statement failed" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return false;
    }

    emit_void_call(function, *arguments, output);
    return true;
}

}  // namespace

auto value_expression_for(
    syntax::StatementSyntax const& statement
) -> syntax::ExpressionSyntax const* {
    if (statement.kind == syntax::StatementKind::return_statement ||
        statement.kind == syntax::StatementKind::expression_statement) {
        return &statement.expression;
    }
    return nullptr;
}

auto lower_let_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    auto lowered = lower_expression(
        statement.expression,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        output
    );
    if (!lowered.has_value()) {
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            "lowering does not yet support this let initializer" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return false;
    }

    auto local_name = next_llvm_local_value_name(statement.name, session.state.local_name_counts);
    output << "  " << local_name << " = add " << lowered->type << " 0, " << lowered->value << "\n";
    session.state.immutable_bindings[statement.name] = LoweredExpression {
        .type = lowered->type,
        .value = std::move(local_name),
        .signedness = lowered->signedness,
    };
    if (!statement.annotated_type.name.empty()) {
        session.state.source_type_names[statement.name] =
            render_source_type_name(statement.annotated_type);
    }
    return true;
}

auto lower_var_statement(
    syntax::StatementSyntax const& statement,
    std::string_view fallback_llvm_type,
    IntegerSignedness fallback_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    auto type = LoweredType {
        .type = std::string(fallback_llvm_type),
        .signedness = fallback_signedness,
    };
    if (!statement.annotated_type.name.empty()) {
        auto annotated_type = llvm_type_for(statement.annotated_type);
        if (!annotated_type.has_value() || *annotated_type == "void") {
            diagnostics.error(statement.line, "lowering does not yet support this var type");
            return false;
        }
        type = {
            .type = std::string(*annotated_type),
            .signedness = integer_signedness_for(statement.annotated_type),
        };
    } else if (auto inferred = infer_expression_type(statement.expression, context, session.state)) {
        type = std::move(*inferred);
    }

    auto lowered = lower_expression(
        statement.expression,
        type.type,
        type.signedness,
        context,
        session,
        output
    );
    if (!lowered.has_value()) {
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            "lowering does not yet support this var initializer" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return false;
    }

    auto storage = next_llvm_local_value_name(
        statement.name + ".addr",
        session.state.local_name_counts
    );
    output << "  " << storage << " = alloca " << type.type << "\n";
    output << "  store " << type.type << " " << lowered->value << ", ptr " << storage << "\n";
    session.state.mutable_bindings[statement.name] = MutableBinding {
        .type = std::move(type),
        .storage = std::move(storage),
    };
    if (!statement.annotated_type.name.empty()) {
        session.state.source_type_names[statement.name] =
            render_source_type_name(statement.annotated_type);
    }
    return true;
}

auto lower_assignment_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    if (statement.assignment_target.kind != syntax::ExpressionKind::name) {
        diagnostics.error(statement.line, "lowering only supports assignment to mutable local names");
        return false;
    }
    auto binding = session.state.mutable_bindings.find(statement.assignment_target.text);
    if (binding == session.state.mutable_bindings.end()) {
        diagnostics.error(statement.line, "lowering assignment target is not a mutable local");
        return false;
    }

    auto lowered = lower_expression(
        statement.expression,
        binding->second.type.type,
        binding->second.type.signedness,
        context,
        session,
        output
    );
    if (!lowered.has_value()) {
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            "lowering does not yet support this assignment value" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return false;
    }

    output << "  store " << binding->second.type.type << " " << lowered->value
           << ", ptr " << binding->second.storage << "\n";
    return true;
}

auto lower_call_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    if (statement.kind != syntax::StatementKind::expression_statement ||
        statement.expression.kind != syntax::ExpressionKind::call) {
        diagnostics.error(statement.line, "lowering call statement requires a call expression");
        return false;
    }
    if (statement.expression.left != nullptr &&
        statement.expression.left->kind == syntax::ExpressionKind::null_safe_member_access) {
        diagnostics.error(
            statement.line,
            "lowering null-safe member call statements is not yet supported"
        );
        return false;
    }
    if (statement.expression.left != nullptr &&
        statement.expression.left->kind == syntax::ExpressionKind::member_access) {
        auto resolved = resolve_member_call(statement.expression, context, session.state);
        auto function = diagnose_member_call_statement(statement, resolved, diagnostics);
        if (function == nullptr) {
            return false;
        }

        auto const target_name = member_call_target_name(resolved);
        if (function->return_type == "void") {
            return lower_void_member_call_statement(
                statement,
                *statement.expression.left->left,
                target_name,
                *function,
                context,
                session,
                diagnostics,
                output
            );
        }

        auto lowered = lower_expression(
            statement.expression,
            function->return_type,
            function->return_signedness,
            context,
            session,
            output
        );
        if (!lowered.has_value()) {
            auto detail = render_expression_lowering_failure(session.failures.expression);
            diagnostics.error(
                statement.line,
                "lowering does not yet support this call statement" +
                    (detail.empty() ? std::string {} : ": " + detail)
            );
            return false;
        }
        return true;
    }

    auto type = infer_expression_type(statement.expression, context, session.state);
    if (!type.has_value() || type->type.empty()) {
        diagnostics.error(statement.line, "lowering does not yet support this call statement result type");
        return false;
    }
    if (type->type == "void") {
        if (statement.expression.left == nullptr ||
            statement.expression.left->kind != syntax::ExpressionKind::name) {
            diagnostics.error(statement.line, "lowering call statement requires a direct function name");
            return false;
        }
        auto function = context.lowering.functions.find(statement.expression.left->text);
        if (function == context.lowering.functions.end()) {
            diagnostics.error(statement.line, "lowering call statement target is unknown");
            return false;
        }
        return lower_void_call_statement(
            statement,
            statement.expression.left->text,
            function->second,
            context,
            session,
            diagnostics,
            output
        );
    }
    auto lowered = lower_expression(
        statement.expression,
        type->type,
        type->signedness,
        context,
        session,
        output
    );
    if (!lowered.has_value()) {
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            "lowering does not yet support this call statement" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return false;
    }
    return true;
}

auto lower_loop_control_statement(
    syntax::StatementSyntax const& statement,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    if (statement.kind != syntax::StatementKind::break_statement &&
        statement.kind != syntax::StatementKind::continue_statement) {
        diagnostics.error(statement.line, "loop-control lowering requires break or continue");
        return false;
    }
    if (session.state.loop_targets.empty()) {
        diagnostics.error(statement.line, "loop-control lowering requires an active loop");
        return false;
    }

    auto const& targets = session.state.loop_targets.back();
    auto const& target = statement.kind == syntax::StatementKind::break_statement
        ? targets.break_target
        : targets.continue_target;
    emit_llvm_branch(output, target);
    return true;
}

auto lower_value_statement_block(
    std::vector<syntax::StatementSyntax> const& statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    FinalControlFlowLowerer lower_final_control_flow
) -> std::optional<LoweredExpression> {
    auto statement_pointers = std::vector<syntax::StatementSyntax const*> {};
    statement_pointers.reserve(statements.size());
    for (auto const& statement : statements) {
        statement_pointers.push_back(&statement);
    }
    return lower_value_statement_block(
        statement_pointers,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        diagnostics,
        output,
        lower_final_control_flow
    );
}

auto lower_value_statement_block(
    std::vector<std::unique_ptr<syntax::StatementSyntax>> const& statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    FinalControlFlowLowerer lower_final_control_flow
) -> std::optional<LoweredExpression> {
    auto statement_pointers = std::vector<syntax::StatementSyntax const*> {};
    statement_pointers.reserve(statements.size());
    for (auto const& statement : statements) {
        statement_pointers.push_back(statement.get());
    }
    return lower_value_statement_block(
        statement_pointers,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        diagnostics,
        output,
        lower_final_control_flow
    );
}

}  // namespace orison::lowering
