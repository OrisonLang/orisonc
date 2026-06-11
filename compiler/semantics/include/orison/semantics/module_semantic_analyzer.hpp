#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace orison::semantics {

enum class ConcurrencyExpressionKind {
    task,
    thread,
};

enum class ConcurrencyCaptureKind {
    parameter,
    immutable_outer_local,
    mutable_outer_local,
    receiver,
};

struct ConcurrencyCapture {
    std::size_t line = 0;
    std::string name;
    std::string type_name;
    ConcurrencyExpressionKind expression_kind = ConcurrencyExpressionKind::task;
    ConcurrencyCaptureKind capture_kind = ConcurrencyCaptureKind::parameter;
};

struct SemanticAnalysisResult {
    diagnostics::DiagnosticBag diagnostics;
    std::vector<ConcurrencyCapture> concurrency_captures;

    auto has_errors() const -> bool {
        return diagnostics.has_errors();
    }

    auto entries() const -> std::vector<diagnostics::Diagnostic> const& {
        return diagnostics.entries();
    }

    auto render(std::string_view path) const -> std::string {
        return diagnostics.render(path);
    }
};

class ModuleSemanticAnalyzer {
public:
    auto analyze(syntax::ModuleSyntax const& module) const -> SemanticAnalysisResult;
};

}  // namespace orison::semantics
