#include "orison/lowering/module_symbol_registry.hpp"

#include <utility>

namespace orison::lowering {

auto ModuleSymbolRegistry::register_symbol(
    std::string const& symbol_name,
    std::string category,
    std::size_t line,
    diagnostics::DiagnosticBag& diagnostics
) -> bool {
    if (symbol_name.empty()) {
        return true;
    }

    auto attempted_category = std::move(category);
    auto [existing, inserted] =
        symbols_.emplace(symbol_name, Binding {.category = attempted_category});
    if (inserted) {
        return true;
    }

    diagnostics.error(
        line,
        "LLVM symbol '" + symbol_name + "' for " + attempted_category +
            " collides with " + existing->second.category
    );
    return false;
}

auto ModuleSymbolRegistry::validate_symbol(
    std::string const& symbol_name,
    std::string category,
    std::size_t line,
    diagnostics::DiagnosticBag& diagnostics
) const -> bool {
    if (symbol_name.empty()) {
        return true;
    }

    auto existing = symbols_.find(symbol_name);
    if (existing == symbols_.end()) {
        return true;
    }

    diagnostics.error(
        line,
        "LLVM symbol '" + symbol_name + "' for " + category + " collides with " +
            existing->second.category
    );
    return false;
}

auto ModuleSymbolRegistry::validate_foreign_declaration(
    std::string const& symbol_name,
    std::size_t line,
    diagnostics::DiagnosticBag& diagnostics
) const -> bool {
    return validate_symbol(symbol_name, "foreign declaration", line, diagnostics);
}

auto ModuleSymbolRegistry::validate_foreign_export(
    std::string const& symbol_name,
    std::size_t line,
    diagnostics::DiagnosticBag& diagnostics
) const -> bool {
    return validate_symbol(symbol_name, "foreign export", line, diagnostics);
}

}  // namespace orison::lowering
