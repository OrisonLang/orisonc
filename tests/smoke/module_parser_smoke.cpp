#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

namespace {

void test_parse_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_success.or";
    {
        std::ofstream output(path);
        output << "package demo.app\n\n";
        output << "record User\n";
        output << "    name: Text\n\n";
        output << "function main() -> Int32\n";
        output << "    0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.package_name == "demo.app");
    assert(result.module.top_level_declaration_count == 2);
    assert(result.module.records.size() == 1);
    assert(result.module.records.front().name == "User");
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().name == "main");
    assert(result.module.functions.front().return_type == "Int32");
}

void test_parse_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_failure.or";
    {
        std::ofstream output(path);
        output << "record User\n";
        output << "    name: Text\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(result.diagnostics.entries().front().message == "source file must start with a package declaration");
}

void test_function_header_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_header_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.bad\n";
        output << "function main() Int32\n";
        output << "    0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    assert(result.diagnostics.entries().front().message == "expected '->' after function parameter list");
}

}  // namespace

int main() {
    test_parse_success();
    test_parse_failure();
    test_function_header_failure();
    return 0;
}
