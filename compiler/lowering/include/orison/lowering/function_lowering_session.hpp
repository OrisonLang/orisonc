#pragma once

#include "orison/lowering/function_lowering_state.hpp"
#include "orison/lowering/lowering_failures.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"

namespace orison::lowering {

struct FunctionLoweringSession {
    FunctionLoweringState& state;
    LoweringFailures& failures;
    semantics::SemanticAnalysisResult const* semantics = nullptr;
    std::string_view enclosing_symbol_name;
};

}  // namespace orison::lowering
