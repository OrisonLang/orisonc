#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/token.hpp"

#include <vector>

namespace orison::syntax {

struct LexResult {
    std::vector<Token> tokens;
    diagnostics::DiagnosticBag diagnostics;
};

class Lexer {
public:
    auto lex(source::SourceFile const& source_file) const -> LexResult;
};

}  // namespace orison::syntax
