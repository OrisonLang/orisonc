#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/deferred_cleanup.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/loop_lowering_support.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/syntax/module_parser.hpp"

#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace orison::lowering {

auto lower_common_builtin_nonvalue_statement(
    syntax::StatementSyntax const& statement,
    std::optional<LoweredType> binding_type,
    std::string_view unsupported_let_diagnostic,
    std::string_view unsupported_var_diagnostic,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    DeferredCleanupBlockLowerer lower_cleanup_block
) -> std::optional<StatementFlow>;

template <typename LowerStatement>
auto lower_nonvalue_statement_block(
    std::span<syntax::StatementSyntax const* const> statements,
    std::string_view terminated_statement_diagnostic,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    DeferredCleanupBlockLowerer lower_cleanup_block,
    LowerStatement&& lower_statement
) -> StatementFlow {
    auto flow = StatementFlow::falls_through;
    DeferredCleanupScope defer_scope(session.state);
    for (auto index = std::size_t {0}; index < statements.size(); ++index) {
        auto const* statement = statements[index];
        if (statement == nullptr) {
            return StatementFlow::failed;
        }
        if (flow == StatementFlow::terminated) {
            diagnostics.error(statement->line, std::string(terminated_statement_diagnostic));
            return StatementFlow::failed;
        }

        auto const is_last_statement = index + 1 == statements.size();
        flow = std::forward<LowerStatement>(lower_statement)(*statement, is_last_statement);
        if (flow == StatementFlow::failed) {
            return StatementFlow::failed;
        }
    }
    if (flow == StatementFlow::falls_through &&
        !emit_deferred_cleanup_to_depth_with_block_lowerer(
            defer_scope.cleanup_depth(),
            context,
            session,
            diagnostics,
            output,
            lower_cleanup_block
        )) {
        return StatementFlow::failed;
    }
    return flow;
}

template <typename InferBindingType, typename LowerRepeat, typename LowerFor, typename LowerUnsafe>
auto lower_common_nonvalue_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    DeferredCleanupBlockLowerer lower_cleanup_block,
    InferBindingType&& infer_binding_type,
    std::string_view unsupported_let_diagnostic,
    std::string_view unsupported_var_diagnostic,
    LowerRepeat&& lower_repeat,
    LowerFor&& lower_for,
    LowerUnsafe&& lower_unsafe
) -> std::optional<StatementFlow> {
    auto binding_type = std::optional<LoweredType> {};
    if (statement.kind == syntax::StatementKind::let_binding ||
        statement.kind == syntax::StatementKind::var_binding) {
        auto type = std::forward<InferBindingType>(infer_binding_type)(statement, context, session.state);
        binding_type = type;
    }
    if (auto builtin = lower_common_builtin_nonvalue_statement(
            statement,
            binding_type,
            unsupported_let_diagnostic,
            unsupported_var_diagnostic,
            context,
            session,
            diagnostics,
            output,
            lower_cleanup_block
        )) {
        return builtin;
    }
    if (statement.kind == syntax::StatementKind::repeat_statement) {
        return std::forward<LowerRepeat>(lower_repeat)(statement);
    }
    if (statement.kind == syntax::StatementKind::for_statement) {
        return std::forward<LowerFor>(lower_for)(statement);
    }
    if (statement.kind == syntax::StatementKind::unsafe_statement) {
        return std::forward<LowerUnsafe>(lower_unsafe)(statement);
    }
    return std::nullopt;
}

}  // namespace orison::lowering
