#include "link_library_collection.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

namespace orison::pipeline {
namespace {

auto unquoted_text(std::string_view text) -> std::string {
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return std::string(text.substr(1, text.size() - 2));
    }
    return std::string(text);
}

}  // namespace

void collect_link_libraries(
    syntax::ModuleSyntax const& module,
    std::vector<std::string>& libraries
) {
    for (auto const& foreign_import : module.foreign_imports) {
        if (unquoted_text(foreign_import.abi) != "c" || foreign_import.library_name.empty()) {
            continue;
        }
        auto library = unquoted_text(foreign_import.library_name);
        if (std::find(libraries.begin(), libraries.end(), library) == libraries.end()) {
            libraries.push_back(std::move(library));
        }
    }
}

}  // namespace orison::pipeline
