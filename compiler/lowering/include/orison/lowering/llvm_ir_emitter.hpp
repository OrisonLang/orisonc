#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/syntax/module_parser.hpp"

#include <string>
#include <string_view>

namespace orison::lowering {

struct LlvmIrEmissionResult {
    diagnostics::DiagnosticBag diagnostics;
    std::string ir_text;

    auto has_errors() const -> bool;
    auto render(std::string_view path) const -> std::string;
};

class LlvmIrEmitter {
public:
    auto emit(
        syntax::ModuleSyntax const& module,
        semantics::SemanticAnalysisResult const& semantic_result
    ) const -> LlvmIrEmissionResult;
};

}  // namespace orison::lowering
