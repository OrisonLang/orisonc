#include "orison/syntax/module_parser.hpp"

#include <cctype>
#include <sstream>
#include <string>
#include <string_view>

namespace orison::syntax {
namespace {

auto trim(std::string_view text) -> std::string_view {
    std::size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
        ++start;
    }

    std::size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }

    return text.substr(start, end - start);
}

auto starts_with_keyword(std::string_view text, std::string_view keyword) -> bool {
    if (!text.starts_with(keyword)) {
        return false;
    }

    if (text.size() == keyword.size()) {
        return true;
    }

    auto next = static_cast<unsigned char>(text[keyword.size()]);
    return std::isspace(next) != 0;
}

auto split_first_word(std::string_view text) -> std::string {
    std::size_t end = 0;
    while (end < text.size() && std::isspace(static_cast<unsigned char>(text[end])) == 0) {
        ++end;
    }
    return std::string(text.substr(0, end));
}

}  // namespace

auto ModuleParser::parse(source::SourceFile const& source_file) const -> ParseResult {
    ParseResult result;

    std::istringstream lines(source_file.content());
    std::string line;
    std::size_t line_number = 0;

    while (std::getline(lines, line)) {
        ++line_number;
        auto text = trim(line);
        if (text.empty() || text.starts_with('#')) {
            continue;
        }

        if (!line.empty() && std::isspace(static_cast<unsigned char>(line.front())) != 0) {
            continue;
        }

        if (starts_with_keyword(text, "package")) {
            auto package_name = trim(text.substr(std::string_view("package").size()));
            if (package_name.empty()) {
                result.diagnostics.error(line_number, "package declaration requires a package name");
                continue;
            }

            if (!result.module.package_name.empty()) {
                result.diagnostics.error(line_number, "duplicate package declaration");
                continue;
            }

            result.module.package_name = std::string(package_name);
            continue;
        }

        constexpr std::string_view declaration_keywords[] {
            "import",
            "export",
            "type",
            "record",
            "choice",
            "interface",
            "implements",
            "extend",
            "function",
            "foreign",
            "async",
        };

        bool recognized = false;
        for (auto keyword : declaration_keywords) {
            if (starts_with_keyword(text, keyword)) {
                result.module.top_level_declarations.push_back(split_first_word(text));
                recognized = true;
                break;
            }
        }

        if (!recognized) {
            result.diagnostics.error(
                line_number,
                "expected a top-level declaration keyword such as package, import, record, or function"
            );
        }
    }

    if (result.module.package_name.empty()) {
        result.diagnostics.error(1, "source file must start with a package declaration");
    }

    return result;
}

}  // namespace orison::syntax
