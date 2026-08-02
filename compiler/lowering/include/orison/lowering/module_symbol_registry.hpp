#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>

namespace orison::lowering {

class ModuleSymbolRegistry {
public:
    auto register_symbol(
        std::string const& symbol_name,
        std::string category,
        std::size_t line,
        diagnostics::DiagnosticBag& diagnostics
    ) -> bool;

    auto validate_symbol(
        std::string const& symbol_name,
        std::string category,
        std::size_t line,
        diagnostics::DiagnosticBag& diagnostics
    ) const -> bool;

    auto validate_foreign_declaration(
        std::string const& symbol_name,
        std::size_t line,
        diagnostics::DiagnosticBag& diagnostics
    ) const -> bool;

    auto validate_foreign_export(
        std::string const& symbol_name,
        std::size_t line,
        diagnostics::DiagnosticBag& diagnostics
    ) const -> bool;

private:
    struct Binding {
        std::string category;
    };

    std::unordered_map<std::string, Binding> symbols_;
};

}  // namespace orison::lowering
