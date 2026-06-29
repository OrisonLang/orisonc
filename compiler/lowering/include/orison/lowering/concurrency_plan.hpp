#pragma once

#include "orison/lowering/concurrency_runtime.hpp"
#include "orison/lowering/function_lowering_state.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/drop_metadata.hpp"
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
    std::size_t field_index = 0;
    semantics::ConcurrencyCaptureKind capture_kind =
        semantics::ConcurrencyCaptureKind::parameter;
};

struct ConcurrencyEnvironmentLayout {
    std::string llvm_type;
    std::size_t size_bytes = 0;
    std::vector<ConcurrencyCapturePlan> fields;
};

struct ConcurrencyResultStorageLayout {
    std::string llvm_type;
    std::size_t size_bytes = 0;
};

struct ConcurrencyCleanupFieldPlan {
    std::string name;
    std::string source_type_name;
    std::string llvm_type;
    std::string drop_symbol_name;
    std::size_t field_index = 0;
    semantics::ConcurrencyCaptureKind capture_kind =
        semantics::ConcurrencyCaptureKind::parameter;
};

struct ConcurrencyDropCleanupPlan {
    std::string cleanup_symbol_name;
    std::vector<PlannedDropAction> actions;
    bool emit_drop_calls = false;
};

struct ConcurrencyCleanupPlan {
    std::vector<ConcurrencyCleanupFieldPlan> drop_candidates;
    ConcurrencyDropCleanupPlan drop_cleanup;
};

struct ConcurrencyExpressionPlan {
    ConcurrencyPlanKind kind = ConcurrencyPlanKind::task;
    ConcurrencyRuntimeOperation spawn_operation = ConcurrencyRuntimeOperation::spawn_task;
    std::string thunk_symbol_name;
    std::string cleanup_symbol_name;
    LoweredType result_type;
    ConcurrencyEnvironmentLayout environment_layout;
    ConcurrencyResultStorageLayout result_storage;
    ConcurrencyCleanupPlan cleanup;
    std::vector<ConcurrencyCapturePlan> captures;
};

auto format_concurrency_drop_cleanup_plan(
    ConcurrencyDropCleanupPlan const& plan
) -> std::vector<std::string>;

auto plan_concurrency_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view enclosing_symbol_name,
    std::size_t ordinal,
    LoweringEmissionContext const& context,
    FunctionLoweringState const& state,
    semantics::SemanticAnalysisResult const& semantics
) -> std::optional<ConcurrencyExpressionPlan>;

auto plan_concurrency_planned_drops(
    syntax::ModuleSyntax const& module,
    LoweringEmissionContext const& context,
    semantics::SemanticAnalysisResult const& semantics
) -> std::vector<PlannedDropDeclaration>;

auto plan_concurrency_planned_drop_actions(
    syntax::ModuleSyntax const& module,
    LoweringEmissionContext const& context,
    semantics::SemanticAnalysisResult const& semantics
) -> std::vector<PlannedDropAction>;

}  // namespace orison::lowering
