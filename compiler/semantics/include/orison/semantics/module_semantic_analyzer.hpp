#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/syntax/module_parser.hpp"

namespace orison::semantics {

class ModuleSemanticAnalyzer {
public:
    auto analyze(syntax::ModuleSyntax const& module) const -> diagnostics::DiagnosticBag;
};

}  // namespace orison::semantics
