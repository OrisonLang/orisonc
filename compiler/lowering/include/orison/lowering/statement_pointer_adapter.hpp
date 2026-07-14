#pragma once

#include "orison/syntax/module_parser.hpp"

#include <memory>
#include <span>
#include <vector>

namespace orison::lowering {

inline auto statement_pointers_for(std::vector<syntax::StatementSyntax> const& statements)
    -> std::vector<syntax::StatementSyntax const*> {
    auto pointers = std::vector<syntax::StatementSyntax const*> {};
    pointers.reserve(statements.size());
    for (auto const& statement : statements) {
        pointers.push_back(&statement);
    }
    return pointers;
}

inline auto statement_pointers_for(std::vector<std::unique_ptr<syntax::StatementSyntax>> const& statements)
    -> std::vector<syntax::StatementSyntax const*> {
    auto pointers = std::vector<syntax::StatementSyntax const*> {};
    pointers.reserve(statements.size());
    for (auto const& statement : statements) {
        pointers.push_back(statement.get());
    }
    return pointers;
}

inline auto statement_pointer_span(std::vector<syntax::StatementSyntax const*> const& statements)
    -> std::span<syntax::StatementSyntax const* const> {
    return {statements.data(), statements.size()};
}

}  // namespace orison::lowering
