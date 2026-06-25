#pragma once

#include "orison/lowering/concurrency_runtime.hpp"
#include "orison/lowering/function_lowering_state.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace orison::lowering {

enum class ConcurrencyPlanKind {
    task,
    thread,
};

struct ConcurrencyCapturePlan {
    std::string name;
    std::string source_type_name;
    std::string llvm_type;
    semantics::ConcurrencyCaptureKind capture_kind =
        semantics::ConcurrencyCaptureKind::parameter;
};

struct ConcurrencyExpressionPlan {
    ConcurrencyPlanKind kind = ConcurrencyPlanKind::task;
    ConcurrencyRuntimeOperation spawn_operation = ConcurrencyRuntimeOperation::spawn_task;
    std::string thunk_symbol_name;
    LoweredType result_type;
    std::vector<ConcurrencyCapturePlan> captures;
};

auto plan_concurrency_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view enclosing_symbol_name,
    std::size_t ordinal,
    LoweringEmissionContext const& context,
    FunctionLoweringState const& state,
    semantics::SemanticAnalysisResult const& semantics
) -> std::optional<ConcurrencyExpressionPlan>;

}  // namespace orison::lowering
