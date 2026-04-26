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
        output << "import\n";
        output << "    Logger as Log from diagnostics.logger\n";
        output << "    console from io\n\n";
        output << "public type UserId = UInt64\n\n";
        output << "public record User\n";
        output << "    public name: Text\n";
        output << "    private values: DynamicArray<Maybe<Int32>>\n\n";
        output << "package function main(input: shared.View<Byte>, count: Int32) -> Outcome<Int32, ParseError>\n";
        output << "    guard count > 0 else\n";
        output << "        return input.read(0)\n";
        output << "    if count < 10\n";
        output << "        let label: Text = input.read(count)\n";
        output << "        sink(label.value)\n";
        output << "    else\n";
        output << "        return input.read(count)\n";
        output << "    var total = count + 1 * 2\n";
        output << "    return total\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.package_name == "demo.app");
    assert(result.module.imports.size() == 2);
    assert(result.module.imports.front().name == "Logger");
    assert(result.module.imports.front().alias == "Log");
    assert(result.module.imports.front().from_package == "diagnostics.logger");
    assert(result.module.type_aliases.size() == 1);
    assert(result.module.type_aliases.front().visibility == orison::syntax::Visibility::public_visibility);
    assert(result.module.type_aliases.front().name == "UserId");
    assert(result.module.type_aliases.front().aliased_type.name == "UInt64");
    assert(result.module.top_level_declaration_count == 3);
    assert(result.module.records.size() == 1);
    assert(result.module.records.front().visibility == orison::syntax::Visibility::public_visibility);
    assert(result.module.records.front().name == "User");
    assert(result.module.records.front().fields.size() == 2);
    assert(result.module.records.front().fields.front().name == "name");
    assert(result.module.records.front().fields.front().visibility == orison::syntax::Visibility::public_visibility);
    assert(result.module.records.front().fields.front().type.name == "Text");
    assert(result.module.records.front().fields[1].name == "values");
    assert(result.module.records.front().fields[1].visibility == orison::syntax::Visibility::private_visibility);
    assert(result.module.records.front().fields[1].type.name == "DynamicArray");
    assert(result.module.records.front().fields[1].type.generic_arguments.size() == 1);
    assert(result.module.records.front().fields[1].type.generic_arguments.front().name == "Maybe");
    assert(result.module.records.front().fields[1].type.generic_arguments.front().generic_arguments.size() == 1);
    assert(result.module.records.front().fields[1].type.generic_arguments.front().generic_arguments.front().name == "Int32");
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().visibility == orison::syntax::Visibility::package_visibility);
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
    assert(result.module.functions.front().body_statements[0].kind == orison::syntax::StatementKind::guard_statement);
    assert(result.module.functions.front().body_statements[0].expression.kind == orison::syntax::ExpressionKind::binary);
    assert(result.module.functions.front().body_statements[0].expression.text == ">");
    assert(result.module.functions.front().body_statements[0].expression.left->text == "count");
    assert(result.module.functions.front().body_statements[0].expression.right->text == "0");
    assert(result.module.functions.front().body_statements[0].nested_statements.size() == 1);
    assert(result.module.functions.front().body_statements[0].alternate_statements.empty());
    assert(result.module.functions.front().body_statements[0].nested_statements[0].kind == orison::syntax::StatementKind::return_statement);
    assert(result.module.functions.front().body_statements[0].nested_statements[0].expression.kind == orison::syntax::ExpressionKind::call);
    assert(result.module.functions.front().body_statements[1].kind == orison::syntax::StatementKind::if_statement);
    assert(result.module.functions.front().body_statements[1].expression.kind == orison::syntax::ExpressionKind::binary);
    assert(result.module.functions.front().body_statements[1].expression.text == "<");
    assert(result.module.functions.front().body_statements[1].nested_statements.size() == 2);
    assert(result.module.functions.front().body_statements[1].alternate_statements.size() == 1);
    assert(result.module.functions.front().body_statements[1].nested_statements[0].kind == orison::syntax::StatementKind::let_binding);
    assert(result.module.functions.front().body_statements[1].nested_statements[0].name == "label");
    assert(result.module.functions.front().body_statements[1].nested_statements[0].annotated_type.name == "Text");
    assert(result.module.functions.front().body_statements[1].nested_statements[0].expression.kind == orison::syntax::ExpressionKind::call);
    assert(result.module.functions.front().body_statements[1].nested_statements[0].expression.left->kind == orison::syntax::ExpressionKind::member_access);
    assert(result.module.functions.front().body_statements[1].nested_statements[0].expression.left->text == "read");
    assert(result.module.functions.front().body_statements[1].nested_statements[0].expression.left->left->text == "input");
    assert(result.module.functions.front().body_statements[1].nested_statements[0].expression.arguments.size() == 1);
    assert(result.module.functions.front().body_statements[1].nested_statements[0].expression.arguments.front().text == "count");
    assert(result.module.functions.front().body_statements[1].nested_statements[1].kind == orison::syntax::StatementKind::expression_statement);
    assert(result.module.functions.front().body_statements[1].nested_statements[1].expression.kind == orison::syntax::ExpressionKind::call);
    assert(result.module.functions.front().body_statements[1].alternate_statements[0].kind == orison::syntax::StatementKind::return_statement);
    assert(result.module.functions.front().body_statements[1].alternate_statements[0].expression.kind == orison::syntax::ExpressionKind::call);
    assert(result.module.functions.front().body_statements[2].kind == orison::syntax::StatementKind::var_binding);
    assert(result.module.functions.front().body_statements[2].name == "total");
    assert(result.module.functions.front().body_statements[2].expression.kind == orison::syntax::ExpressionKind::binary);
    assert(result.module.functions.front().body_statements[2].expression.text == "+");
    assert(result.module.functions.front().body_statements[2].expression.left->text == "count");
    assert(result.module.functions.front().body_statements[2].expression.right->kind == orison::syntax::ExpressionKind::binary);
    assert(result.module.functions.front().body_statements[2].expression.right->text == "*");
    assert(result.module.functions.front().body_statements[2].expression.right->left->text == "1");
    assert(result.module.functions.front().body_statements[2].expression.right->right->text == "2");
    assert(result.module.functions.front().body_statements[3].kind == orison::syntax::StatementKind::return_statement);
    assert(result.module.functions.front().body_statements[3].expression.text == "total");
}

void test_switch_statement_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_switch_success.or";
    {
        std::ofstream output(path);
        output << "package demo.switches\n";
        output << "function classify(value: Int32) -> Int32\n";
        output << "    switch value >= 0\n";
        output << "        0 =>\n";
        output << "            let zero = value\n";
        output << "            return zero\n";
        output << "        classify(value) => return value\n";
        output << "        default => value + 1\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 1);
    auto const& switch_statement = result.module.functions.front().body_statements[0];
    assert(switch_statement.kind == orison::syntax::StatementKind::switch_statement);
    assert(switch_statement.expression.kind == orison::syntax::ExpressionKind::binary);
    assert(switch_statement.expression.text == ">=");
    assert(switch_statement.switch_cases.size() == 3);
    assert(!switch_statement.switch_cases[0].is_default);
    assert(switch_statement.switch_cases[0].pattern.kind == orison::syntax::ExpressionKind::integer_literal);
    assert(switch_statement.switch_cases[0].pattern.text == "0");
    assert(switch_statement.switch_cases[0].statements.size() == 2);
    assert(switch_statement.switch_cases[0].statements[0] != nullptr);
    assert(switch_statement.switch_cases[0].statements[0]->kind == orison::syntax::StatementKind::let_binding);
    assert(switch_statement.switch_cases[0].statements[0]->name == "zero");
    assert(switch_statement.switch_cases[0].statements[1] != nullptr);
    assert(switch_statement.switch_cases[0].statements[1]->kind == orison::syntax::StatementKind::return_statement);
    assert(switch_statement.switch_cases[0].statements[1]->expression.text == "zero");
    assert(!switch_statement.switch_cases[1].is_default);
    assert(switch_statement.switch_cases[1].pattern.kind == orison::syntax::ExpressionKind::call);
    assert(switch_statement.switch_cases[1].statements.size() == 1);
    assert(switch_statement.switch_cases[1].statements[0] != nullptr);
    assert(switch_statement.switch_cases[1].statements[0]->kind == orison::syntax::StatementKind::return_statement);
    assert(switch_statement.switch_cases[1].statements[0]->expression.text == "value");
    assert(switch_statement.switch_cases[2].is_default);
    assert(switch_statement.switch_cases[2].statements.size() == 1);
    assert(switch_statement.switch_cases[2].statements[0] != nullptr);
    assert(switch_statement.switch_cases[2].statements[0]->kind == orison::syntax::StatementKind::expression_statement);
    assert(switch_statement.switch_cases[2].statements[0]->expression.kind == orison::syntax::ExpressionKind::binary);
    assert(switch_statement.switch_cases[2].statements[0]->expression.text == "+");
}

void test_while_statement_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_while_success.or";
    {
        std::ofstream output(path);
        output << "package demo.loops\n";
        output << "function count_down(current: Int32) -> Int32\n";
        output << "    while current >= 0\n";
        output << "        sink(current)\n";
        output << "        var next = current - 1\n";
        output << "    return current\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 2);
    auto const& while_statement = result.module.functions.front().body_statements[0];
    assert(while_statement.kind == orison::syntax::StatementKind::while_statement);
    assert(while_statement.expression.kind == orison::syntax::ExpressionKind::binary);
    assert(while_statement.expression.text == ">=");
    assert(while_statement.nested_statements.size() == 2);
    assert(while_statement.nested_statements[0].kind == orison::syntax::StatementKind::expression_statement);
    assert(while_statement.nested_statements[0].expression.kind == orison::syntax::ExpressionKind::call);
    assert(while_statement.nested_statements[1].kind == orison::syntax::StatementKind::var_binding);
    assert(while_statement.nested_statements[1].name == "next");
    assert(while_statement.nested_statements[1].expression.kind == orison::syntax::ExpressionKind::binary);
    assert(while_statement.nested_statements[1].expression.text == "-");
}

void test_for_statement_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_for_success.or";
    {
        std::ofstream output(path);
        output << "package demo.loops\n";
        output << "function find_first(items: shared.View<Int32>) -> Int32\n";
        output << "    for item in items\n";
        output << "        sink(item)\n";
        output << "        return item\n";
        output << "    return 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 2);
    auto const& for_statement = result.module.functions.front().body_statements[0];
    assert(for_statement.kind == orison::syntax::StatementKind::for_statement);
    assert(for_statement.name == "item");
    assert(for_statement.expression.kind == orison::syntax::ExpressionKind::name);
    assert(for_statement.expression.text == "items");
    assert(for_statement.nested_statements.size() == 2);
    assert(for_statement.nested_statements[0].kind == orison::syntax::StatementKind::expression_statement);
    assert(for_statement.nested_statements[0].expression.kind == orison::syntax::ExpressionKind::call);
    assert(for_statement.nested_statements[1].kind == orison::syntax::StatementKind::return_statement);
    assert(for_statement.nested_statements[1].expression.text == "item");
}

void test_defer_statement_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_defer_success.or";
    {
        std::ofstream output(path);
        output << "package demo.cleanup\n";
        output << "function use_file(path: Text) -> Int32\n";
        output << "    defer\n";
        output << "        close(path)\n";
        output << "        flush(path)\n";
        output << "    return 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 2);
    auto const& defer_statement = result.module.functions.front().body_statements[0];
    assert(defer_statement.kind == orison::syntax::StatementKind::defer_statement);
    assert(defer_statement.nested_statements.size() == 2);
    assert(defer_statement.nested_statements[0].kind == orison::syntax::StatementKind::expression_statement);
    assert(defer_statement.nested_statements[0].expression.kind == orison::syntax::ExpressionKind::call);
    assert(defer_statement.nested_statements[1].kind == orison::syntax::StatementKind::expression_statement);
    assert(defer_statement.nested_statements[1].expression.kind == orison::syntax::ExpressionKind::call);
}

void test_equality_expression_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_equality_success.or";
    {
        std::ofstream output(path);
        output << "package demo.conditions\n";
        output << "function classify(enabled: Bool, ready: Bool) -> Int32\n";
        output << "    if enabled == false\n";
        output << "        return 0\n";
        output << "    while ready != false\n";
        output << "        return 1\n";
        output << "    return 2\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 3);
    auto const& if_statement = result.module.functions.front().body_statements[0];
    assert(if_statement.kind == orison::syntax::StatementKind::if_statement);
    assert(if_statement.expression.kind == orison::syntax::ExpressionKind::binary);
    assert(if_statement.expression.text == "==");
    assert(if_statement.expression.left->text == "enabled");
    assert(if_statement.expression.right->text == "false");
    auto const& while_statement = result.module.functions.front().body_statements[1];
    assert(while_statement.kind == orison::syntax::StatementKind::while_statement);
    assert(while_statement.expression.kind == orison::syntax::ExpressionKind::binary);
    assert(while_statement.expression.text == "!=");
    assert(while_statement.expression.left->text == "ready");
    assert(while_statement.expression.right->text == "false");
}

void test_modulo_expression_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_modulo_success.or";
    {
        std::ofstream output(path);
        output << "package demo.conditions\n";
        output << "function first_even(item: Int32) -> Int32\n";
        output << "    if item % 2 != 0\n";
        output << "        return 1\n";
        output << "    return 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 2);
    auto const& if_statement = result.module.functions.front().body_statements[0];
    assert(if_statement.kind == orison::syntax::StatementKind::if_statement);
    assert(if_statement.expression.kind == orison::syntax::ExpressionKind::binary);
    assert(if_statement.expression.text == "!=");
    assert(if_statement.expression.left->kind == orison::syntax::ExpressionKind::binary);
    assert(if_statement.expression.left->text == "%");
    assert(if_statement.expression.left->left->text == "item");
    assert(if_statement.expression.left->right->text == "2");
    assert(if_statement.expression.right->text == "0");
}

void test_unary_expression_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_unary_success.or";
    {
        std::ofstream output(path);
        output << "package demo.conditions\n";
        output << "function evaluate(inner: Int32) -> Int32\n";
        output << "    return -inner\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 1);
    auto const& return_statement = result.module.functions.front().body_statements[0];
    assert(return_statement.kind == orison::syntax::StatementKind::return_statement);
    assert(return_statement.expression.kind == orison::syntax::ExpressionKind::unary);
    assert(return_statement.expression.text == "-");
    assert(return_statement.expression.left->kind == orison::syntax::ExpressionKind::name);
    assert(return_statement.expression.left->text == "inner");
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

void test_if_statement_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_if_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.bad\n";
        output << "function main() -> Int32\n";
        output << "    if 1 + 2\n";
        output << "    return 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    assert(result.diagnostics.entries().front().message == "if statement requires an indented consequence block");
}

void test_else_statement_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_else_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.bad\n";
        output << "function main() -> Int32\n";
        output << "    else\n";
        output << "        return 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    assert(result.diagnostics.entries().front().message == "else must follow an if consequence block");
}

void test_guard_statement_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_guard_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.bad\n";
        output << "function main() -> Int32\n";
        output << "    guard 1 + 2\n";
        output << "        return 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    assert(result.diagnostics.entries().front().message == "guard statement requires 'else' before the failure block");
}

void test_switch_statement_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_switch_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.bad\n";
        output << "function main() -> Int32\n";
        output << "    switch 1\n";
        output << "        0 return 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    assert(result.diagnostics.entries().front().message == "switch case requires '=>'");
}

void test_switch_block_case_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_switch_block_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.bad\n";
        output << "function main() -> Int32\n";
        output << "    switch 1\n";
        output << "        0 =>\n";
        output << "        return 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    assert(result.diagnostics.entries().front().message == "switch case requires an indented consequence block");
}

void test_while_statement_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_while_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.bad\n";
        output << "function main() -> Int32\n";
        output << "    while 1\n";
        output << "    return 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    assert(result.diagnostics.entries().front().message == "while statement requires an indented body block");
}

void test_for_statement_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_for_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.bad\n";
        output << "function main(items: shared.View<Int32>) -> Int32\n";
        output << "    for item items\n";
        output << "        return 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    assert(result.diagnostics.entries().front().message == "for statement requires 'in' before the iterable expression");
}

void test_defer_statement_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_defer_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.bad\n";
        output << "function main() -> Int32\n";
        output << "    defer\n";
        output << "    return 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    assert(result.diagnostics.entries().front().message == "defer statement requires an indented cleanup block");
}

}  // namespace

int main() {
    test_parse_success();
    test_switch_statement_success();
    test_while_statement_success();
    test_for_statement_success();
    test_defer_statement_success();
    test_equality_expression_success();
    test_modulo_expression_success();
    test_unary_expression_success();
    test_parse_failure();
    test_function_header_failure();
    test_missing_record_block_failure();
    test_binding_statement_failure();
    test_if_statement_failure();
    test_else_statement_failure();
    test_guard_statement_failure();
    test_switch_statement_failure();
    test_switch_block_case_failure();
    test_while_statement_failure();
    test_for_statement_failure();
    test_defer_statement_failure();
    return 0;
}
