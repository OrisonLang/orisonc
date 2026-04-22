#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/source/source_file.hpp"

#include <string>
#include <vector>

namespace orison::syntax {

struct RecordSyntax {
    std::string name;
};

struct FunctionSyntax {
    std::string name;
    std::string return_type;
};

struct ModuleSyntax {
    std::string package_name;
    std::vector<RecordSyntax> records;
    std::vector<FunctionSyntax> functions;
    std::size_t top_level_declaration_count = 0;
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
