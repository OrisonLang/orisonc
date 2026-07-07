#pragma once

#include "orison/lowering/function_lowering_session.hpp"

#include <sstream>
#include <string_view>

namespace orison::lowering {

struct ConcurrencyExpressionPlan;

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
    FunctionLoweringState& state,
    std::ostringstream& output
) -> LoweredExpression;

auto emit_task_await_result(
    ConcurrencyBinding& binding,
    FunctionLoweringState& state,
    std::ostringstream& output
) -> LoweredExpression;

auto emit_concurrency_spawn_failed(
    std::ostringstream& output
) -> void;

auto emit_concurrency_spawn(
    ConcurrencyExpressionPlan const& plan,
    std::string_view handle_name,
    std::string_view environment_storage,
    std::string_view result_storage,
    std::ostringstream& output
) -> void;

auto emit_abandoned_concurrency_handle_cleanup(
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> void;

}  // namespace orison::lowering
