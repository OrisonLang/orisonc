#pragma once

#include "orison/syntax/module_parser.hpp"

#include <string>
#include <vector>

namespace orison::pipeline {

void collect_link_libraries(
    syntax::ModuleSyntax const& module,
    std::vector<std::string>& libraries
);

}  // namespace orison::pipeline
