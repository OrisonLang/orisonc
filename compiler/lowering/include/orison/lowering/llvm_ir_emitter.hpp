#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/module_prelude.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/syntax/module_parser.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace orison::lowering {

struct LlvmIrEmissionResult {
    diagnostics::DiagnosticBag diagnostics;
    std::string ir_text;
    std::vector<DropPreludeDeclaration> planned_drop_declarations;

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
