#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/source/source_file.hpp"

#include <string>
#include <vector>

namespace orison::syntax {

struct ModuleSyntax {
    std::string package_name;
    std::vector<std::string> top_level_declarations;
};

struct ParseResult {
    ModuleSyntax module;
    diagnostics::DiagnosticBag diagnostics;
};

class ModuleParser {
public:
    auto parse(source::SourceFile const& source_file) const -> ParseResult;
};

}  // namespace orison::syntax
