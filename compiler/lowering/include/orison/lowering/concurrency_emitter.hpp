#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/syntax/module_parser.hpp"

#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace orison::lowering {

enum class ConcurrencyPlanKind;
struct ConcurrencyExpressionPlan;

enum class ConcurrencyCaptureStoreEmissionResult {
    emitted,
    unsupported_capture_type,
    missing_capture_source,
};

auto emit_concurrency_handle_destroy(
    ConcurrencyBinding& binding,
    std::ostringstream& output
) -> void;

auto emit_concurrency_result_load_and_destroy(
    ConcurrencyBinding& binding,
    FunctionLoweringState& state,
    std::ostringstream& output
) -> LoweredExpression;

auto emit_thread_join_result(
    ConcurrencyBinding& binding,
    std::string_view expected_llvm_type,
    FunctionLoweringState& state,
    LoweringFailures& failures,
    std::ostringstream& output
) -> std::optional<LoweredExpression>;

auto emit_task_await_result(
    ConcurrencyBinding& binding,
    std::string_view expected_llvm_type,
    FunctionLoweringState& state,
    LoweringFailures& failures,
    std::ostringstream& output
) -> std::optional<LoweredExpression>;

auto emit_concurrency_spawn_failed(
    std::ostringstream& output
) -> void;

auto emit_concurrency_spawn_failure_check(
    std::string_view handle_name,
    std::string_view spawn_failed_block,
    std::string_view spawn_ok_block,
    FunctionLoweringState& state,
    std::ostringstream& output
) -> void;

auto emit_concurrency_spawn(
    ConcurrencyExpressionPlan const& plan,
    std::string_view handle_name,
    std::string_view environment_storage,
    std::string_view result_storage,
    std::ostringstream& output
) -> void;

auto emit_concurrency_capture_environment_stores(
    ConcurrencyExpressionPlan const& plan,
    std::string_view environment_storage,
    FunctionLoweringState& state,
    std::ostringstream& output
) -> ConcurrencyCaptureStoreEmissionResult;

auto register_concurrency_binding(
    ConcurrencyPlanKind kind,
    std::string const& binding_name,
    std::string handle_name,
    std::string result_storage,
    LoweredType result_type,
    FunctionLoweringState& state
) -> void;

auto queue_concurrency_function_definitions(
    ConcurrencyExpressionPlan const& plan,
    std::string entry_thunk_definition,
    std::string cleanup_definition,
    FunctionLoweringState& state
) -> void;

auto emit_concurrency_entry_thunk(
    ConcurrencyExpressionPlan const& plan,
    syntax::ExpressionSyntax const& expression,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& parent_session,
    diagnostics::DiagnosticBag& diagnostics
) -> std::optional<std::string>;

auto emit_concurrency_cleanup_thunk(
    ConcurrencyExpressionPlan const& plan
) -> std::string;

auto emit_abandoned_concurrency_handle_cleanup(
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> void;

}  // namespace orison::lowering
