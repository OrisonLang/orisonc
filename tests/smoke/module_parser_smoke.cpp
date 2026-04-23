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
        output << "    name: Text\n";
        output << "    values: DynamicArray<Maybe<Int32>>\n\n";
        output << "function main(input: shared.View<Byte>, count: Int32) -> Outcome<Int32, ParseError>\n";
        output << "    let label: Text = count\n";
        output << "    var total = 0\n";
        output << "    return total\n";
        output << "    finalize\n";
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
    assert(result.module.records.front().fields.size() == 2);
    assert(result.module.records.front().fields.front().name == "name");
    assert(result.module.records.front().fields.front().type.name == "Text");
    assert(result.module.records.front().fields[1].name == "values");
    assert(result.module.records.front().fields[1].type.name == "DynamicArray");
    assert(result.module.records.front().fields[1].type.generic_arguments.size() == 1);
    assert(result.module.records.front().fields[1].type.generic_arguments.front().name == "Maybe");
    assert(result.module.records.front().fields[1].type.generic_arguments.front().generic_arguments.size() == 1);
    assert(result.module.records.front().fields[1].type.generic_arguments.front().generic_arguments.front().name == "Int32");
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().name == "main");
    assert(result.module.functions.front().parameters.size() == 2);
    assert(result.module.functions.front().parameters.front().name == "input");
    assert(result.module.functions.front().parameters.front().type.name == "shared.View");
    assert(result.module.functions.front().parameters.front().type.generic_arguments.size() == 1);
    assert(result.module.functions.front().parameters.front().type.generic_arguments.front().name == "Byte");
    assert(result.module.functions.front().parameters[1].name == "count");
    assert(result.module.functions.front().parameters[1].type.name == "Int32");
    assert(result.module.functions.front().return_type.name == "Outcome");
    assert(result.module.functions.front().return_type.generic_arguments.size() == 2);
    assert(result.module.functions.front().return_type.generic_arguments.front().name == "Int32");
    assert(result.module.functions.front().return_type.generic_arguments[1].name == "ParseError");
    assert(result.module.functions.front().body_statements.size() == 4);
    assert(result.module.functions.front().body_statements[0].kind == orison::syntax::StatementKind::let_binding);
    assert(result.module.functions.front().body_statements[0].name == "label");
    assert(result.module.functions.front().body_statements[0].annotated_type.name == "Text");
    assert(result.module.functions.front().body_statements[0].expression.tokens.size() == 1);
    assert(result.module.functions.front().body_statements[0].expression.tokens.front() == "count");
    assert(result.module.functions.front().body_statements[1].kind == orison::syntax::StatementKind::var_binding);
    assert(result.module.functions.front().body_statements[1].name == "total");
    assert(result.module.functions.front().body_statements[1].expression.tokens.front() == "0");
    assert(result.module.functions.front().body_statements[2].kind == orison::syntax::StatementKind::return_statement);
    assert(result.module.functions.front().body_statements[2].expression.tokens.front() == "total");
    assert(result.module.functions.front().body_statements[3].kind == orison::syntax::StatementKind::expression_statement);
    assert(result.module.functions.front().body_statements[3].expression.tokens.front() == "finalize");
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

void test_missing_record_block_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_record_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.bad\n";
        output << "record User\n";
        output << "function main() -> Int32\n";
        output << "    0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    assert(result.diagnostics.entries().front().message == "record declaration requires an indented field block");
}

void test_binding_statement_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_binding_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.bad\n";
        output << "function main() -> Int32\n";
        output << "    let broken: Int32\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    assert(result.diagnostics.entries().front().message == "binding statement requires '=' before the initializer");
}

}  // namespace

int main() {
    test_parse_success();
    test_parse_failure();
    test_function_header_failure();
    test_missing_record_block_failure();
    test_binding_statement_failure();
    return 0;
}
