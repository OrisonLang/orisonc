#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/semantics/drop_model.hpp"
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

struct DynamicArrayDescriptorOrigin {
    std::string owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    std::size_t line = 0;
};

struct SemanticAnalysisResult {
    diagnostics::DiagnosticBag diagnostics;
    std::vector<ConcurrencyCapture> concurrency_captures;
    std::vector<PlannedDropSite> planned_drop_sites;
    std::vector<DynamicArrayDescriptorOrigin> dynamic_array_descriptor_origins;

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

auto format_dynamic_array_descriptor_origin(DynamicArrayDescriptorOrigin const& origin) -> std::string;

auto format_dynamic_array_descriptor_origin_report(
    std::vector<DynamicArrayDescriptorOrigin> const& origins
) -> std::vector<std::string>;

class ModuleSemanticAnalyzer {
public:
    auto analyze(syntax::ModuleSyntax const& module) const -> SemanticAnalysisResult;
};

}  // namespace orison::semantics
