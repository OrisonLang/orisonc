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
        output << "public choice ParseError\n";
        output << "    EmptyInput\n";
        output << "    InvalidDigit(value: UInt16)\n\n";
        output << "public interface Reader\n";
        output << "    function read(this: exclusive This, into: exclusive View<Byte>) -> Outcome<Int32, ParseError>\n\n";
        output << "implements Reader for FileReader\n";
        output << "    function read(this: exclusive This, into: exclusive View<Byte>) -> Outcome<Int32, ParseError>\n";
        output << "        return into.length()\n\n";
        output << "extend FileReader\n";
        output << "    public function reset(this: exclusive This) -> Unit\n";
        output << "        return count\n";
        output << "    private function ready(this: shared This) -> Bool\n";
        output << "        return count > 0\n\n";
        output << "package function main(input: shared.View<Byte>, count: Int32) -> Outcome<Int32, ParseError>\n";
        output << "    guard count > 0 else\n";
        output << "        return input.read(0)\n";
        output << "    if count < 10\n";
        output << "        let label: Text = input.read(count)\n";
        output << "        label = input.read(0)\n";
        output << "        sink(label.value)\n";
        output << "    else\n";
        output << "        return input.read(count)\n";
        output << "    var total = count + 1 * 2\n";
        output << "    total = total + 1\n";
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
    assert(result.module.top_level_declaration_count == 7);
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
    assert(result.module.choices.size() == 1);
    assert(result.module.choices.front().visibility == orison::syntax::Visibility::public_visibility);
    assert(result.module.choices.front().name == "ParseError");
    assert(result.module.choices.front().variants.size() == 2);
    assert(result.module.choices.front().variants[0].name == "EmptyInput");
    assert(result.module.choices.front().variants[0].payloads.empty());
    assert(result.module.choices.front().variants[1].name == "InvalidDigit");
    assert(result.module.choices.front().variants[1].payloads.size() == 1);
    assert(result.module.choices.front().variants[1].payloads[0].name == "value");
    assert(result.module.choices.front().variants[1].payloads[0].type.name == "UInt16");
    assert(result.module.interfaces.size() == 1);
    assert(result.module.interfaces.front().visibility == orison::syntax::Visibility::public_visibility);
    assert(result.module.interfaces.front().name == "Reader");
    assert(result.module.interfaces.front().methods.size() == 1);
    assert(result.module.interfaces.front().methods[0].name == "read");
    assert(result.module.interfaces.front().methods[0].parameters.size() == 2);
    assert(result.module.interfaces.front().methods[0].return_type.name == "Outcome");
    assert(result.module.implementations.size() == 1);
    assert(result.module.implementations.front().interface_type.name == "Reader");
    assert(result.module.implementations.front().receiver_type.name == "FileReader");
    assert(result.module.implementations.front().methods.size() == 1);
    assert(result.module.implementations.front().methods[0].name == "read");
    assert(result.module.implementations.front().methods[0].parameters.size() == 2);
    assert(result.module.implementations.front().methods[0].body_statements.size() == 1);
    assert(result.module.implementations.front().methods[0].body_statements[0].kind ==
           orison::syntax::StatementKind::return_statement);
    assert(result.module.implementations.front().methods[0].body_statements[0].expression.kind ==
           orison::syntax::ExpressionKind::call);
    assert(result.module.extensions.size() == 1);
    assert(result.module.extensions.front().receiver_type.name == "FileReader");
    assert(result.module.extensions.front().methods.size() == 2);
    assert(result.module.extensions.front().methods[0].visibility == orison::syntax::Visibility::public_visibility);
    assert(result.module.extensions.front().methods[0].name == "reset");
    assert(result.module.extensions.front().methods[0].body_statements.size() == 1);
    assert(result.module.extensions.front().methods[1].visibility == orison::syntax::Visibility::private_visibility);
    assert(result.module.extensions.front().methods[1].name == "ready");
    assert(result.module.extensions.front().methods[1].body_statements.size() == 1);
    assert(result.module.extensions.front().methods[1].body_statements[0].expression.kind ==
           orison::syntax::ExpressionKind::binary);
    assert(result.module.extensions.front().methods[1].body_statements[0].expression.text == ">");
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
    assert(result.module.functions.front().body_statements.size() == 5);
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
    assert(result.module.functions.front().body_statements[1].nested_statements.size() == 3);
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
    assert(result.module.functions.front().body_statements[1].nested_statements[1].kind ==
           orison::syntax::StatementKind::assignment_statement);
    assert(result.module.functions.front().body_statements[1].nested_statements[1].assignment_target.kind ==
           orison::syntax::ExpressionKind::name);
    assert(result.module.functions.front().body_statements[1].nested_statements[1].assignment_target.text == "label");
    assert(result.module.functions.front().body_statements[1].nested_statements[1].expression.kind ==
           orison::syntax::ExpressionKind::call);
    assert(result.module.functions.front().body_statements[1].nested_statements[2].kind ==
           orison::syntax::StatementKind::expression_statement);
    assert(result.module.functions.front().body_statements[1].nested_statements[2].expression.kind ==
           orison::syntax::ExpressionKind::call);
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
    assert(result.module.functions.front().body_statements[3].kind == orison::syntax::StatementKind::assignment_statement);
    assert(result.module.functions.front().body_statements[3].assignment_target.kind ==
           orison::syntax::ExpressionKind::name);
    assert(result.module.functions.front().body_statements[3].assignment_target.text == "total");
    assert(result.module.functions.front().body_statements[3].expression.kind ==
           orison::syntax::ExpressionKind::binary);
    assert(result.module.functions.front().body_statements[3].expression.text == "+");
    assert(result.module.functions.front().body_statements[4].kind == orison::syntax::StatementKind::return_statement);
    assert(result.module.functions.front().body_statements[4].expression.text == "total");
}

void test_choice_generic_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_choice_generic_success.or";
    {
        std::ofstream output(path);
        output << "package demo.choice\n";
        output << "choice List<T>\n";
        output << "    Empty\n";
        output << "    Node(head: T, tail: Box<List<T>>)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.choices.size() == 1);
    assert(result.module.choices.front().name == "List");
    assert(result.module.choices.front().generic_parameters.size() == 1);
    assert(result.module.choices.front().generic_parameters[0] == "T");
    assert(result.module.choices.front().variants.size() == 2);
    assert(result.module.choices.front().variants[1].payloads.size() == 2);
    assert(result.module.choices.front().variants[1].payloads[1].type.name == "Box");
}

void test_interface_generic_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_interface_generic_success.or";
    {
        std::ofstream output(path);
        output << "package demo.interfaces\n";
        output << "interface Iterator<T>\n";
        output << "    function next(this: exclusive This) -> Maybe<T>\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.interfaces.size() == 1);
    assert(result.module.interfaces.front().name == "Iterator");
    assert(result.module.interfaces.front().generic_parameters.size() == 1);
    assert(result.module.interfaces.front().generic_parameters[0] == "T");
    assert(result.module.interfaces.front().methods.size() == 1);
    assert(result.module.interfaces.front().methods[0].name == "next");
    assert(result.module.interfaces.front().methods[0].return_type.name == "Maybe");
    assert(result.module.interfaces.front().methods[0].return_type.generic_arguments.size() == 1);
    assert(result.module.interfaces.front().methods[0].return_type.generic_arguments[0].name == "T");
}

void test_implements_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_implements_success.or";
    {
        std::ofstream output(path);
        output << "package demo.impls\n";
        output << "implements Iterator<Byte> for BufferReader\n";
        output << "    function next(this: exclusive This) -> Maybe<Byte>\n";
        output << "        return this.read()\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.implementations.size() == 1);
    assert(result.module.implementations.front().interface_type.name == "Iterator");
    assert(result.module.implementations.front().interface_type.generic_arguments.size() == 1);
    assert(result.module.implementations.front().interface_type.generic_arguments[0].name == "Byte");
    assert(result.module.implementations.front().receiver_type.name == "BufferReader");
    assert(result.module.implementations.front().methods.size() == 1);
    assert(result.module.implementations.front().methods[0].return_type.name == "Maybe");
    assert(result.module.implementations.front().methods[0].body_statements.size() == 1);
}

void test_implements_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_implements_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.impls\n";
        output << "implements Reader for BufferReader\n";
        output << "    let broken = 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    auto diagnostics = result.diagnostics.entries();
    assert(!diagnostics.empty());
    assert(diagnostics.front().message == "implements members must be function declarations");
}

void test_extend_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_extend_success.or";
    {
        std::ofstream output(path);
        output << "package demo.extensions\n";
        output << "extend BufferReader\n";
        output << "    public function reset(this: exclusive This) -> Unit\n";
        output << "        return this.cursor\n";
        output << "    private function remaining(this: shared This) -> Int32\n";
        output << "        return this.length()\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.extensions.size() == 1);
    assert(result.module.extensions.front().receiver_type.name == "BufferReader");
    assert(result.module.extensions.front().methods.size() == 2);
    assert(result.module.extensions.front().methods[0].visibility == orison::syntax::Visibility::public_visibility);
    assert(result.module.extensions.front().methods[0].name == "reset");
    assert(result.module.extensions.front().methods[1].visibility == orison::syntax::Visibility::private_visibility);
    assert(result.module.extensions.front().methods[1].name == "remaining");
}

void test_extend_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_extend_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.extensions\n";
        output << "extend BufferReader\n";
        output << "    let broken = 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    auto diagnostics = result.diagnostics.entries();
    assert(!diagnostics.empty());
    assert(diagnostics.front().message == "extend members must be function declarations");
}

void test_where_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_where_success.or";
    {
        std::ofstream output(path);
        output << "package demo.constraints\n";
        output << "public function sort_copy<T>(items: shared View<T>) -> DynamicArray<T>\n";
        output << "where T: Ordered + Cloneable\n";
        output << "    return clone(items)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().name == "sort_copy");
    assert(result.module.functions.front().generic_parameters.size() == 1);
    assert(result.module.functions.front().generic_parameters.front() == "T");
    assert(result.module.functions.front().where_constraints.size() == 1);
    assert(result.module.functions.front().where_constraints.front().parameter_name == "T");
    assert(result.module.functions.front().where_constraints.front().requirements.size() == 2);
    assert(result.module.functions.front().where_constraints.front().requirements[0].name == "Ordered");
    assert(result.module.functions.front().where_constraints.front().requirements[1].name == "Cloneable");
    assert(result.module.functions.front().body_statements.size() == 1);
}

void test_where_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_where_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.constraints\n";
        output << "public function max<T>(left: T, right: T) -> T\n";
        output << "where T Ordered\n";
        output << "    return left\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    auto diagnostics = result.diagnostics.entries();
    assert(!diagnostics.empty());
    assert(diagnostics.front().message == "where clause requires ':' after the constrained parameter");
}

void test_assignment_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_assignment_success.or";
    {
        std::ofstream output(path);
        output << "package demo.assign\n";
        output << "function update(current: Int32) -> Int32\n";
        output << "    current = current + 1\n";
        output << "    this.value = current\n";
        output << "    return current\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 3);
    assert(result.module.functions.front().body_statements[0].kind == orison::syntax::StatementKind::assignment_statement);
    assert(result.module.functions.front().body_statements[0].assignment_target.kind ==
           orison::syntax::ExpressionKind::name);
    assert(result.module.functions.front().body_statements[0].assignment_target.text == "current");
    assert(result.module.functions.front().body_statements[1].kind == orison::syntax::StatementKind::assignment_statement);
    assert(result.module.functions.front().body_statements[1].assignment_target.kind ==
           orison::syntax::ExpressionKind::member_access);
    assert(result.module.functions.front().body_statements[1].assignment_target.text == "value");
    assert(result.module.functions.front().body_statements[1].assignment_target.left->text == "this");
}

void test_assignment_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_assignment_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.assign\n";
        output << "function update() -> Int32\n";
        output << "    current =\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    auto diagnostics = result.diagnostics.entries();
    assert(!diagnostics.empty());
    assert(diagnostics.front().message == "expected expression");
}

void test_break_continue_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_break_continue_success.or";
    {
        std::ofstream output(path);
        output << "package demo.loops\n";
        output << "function first_even(items: shared View<Int32>) -> Maybe<Int32>\n";
        output << "    for item in items\n";
        output << "        if item % 2 != 0\n";
        output << "            continue\n";
        output << "        break\n";
        output << "    return items.length()\n";
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
    assert(for_statement.nested_statements.size() == 2);
    assert(for_statement.nested_statements[0].kind == orison::syntax::StatementKind::if_statement);
    assert(for_statement.nested_statements[0].nested_statements.size() == 1);
    assert(for_statement.nested_statements[0].nested_statements[0].kind ==
           orison::syntax::StatementKind::continue_statement);
    assert(for_statement.nested_statements[1].kind == orison::syntax::StatementKind::break_statement);
}

void test_break_continue_standalone_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_break_continue_standalone_success.or";
    {
        std::ofstream output(path);
        output << "package demo.flow\n";
        output << "function stop() -> Int32\n";
        output << "    break\n";
        output << "    continue\n";
        output << "    return 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 3);
    assert(result.module.functions.front().body_statements[0].kind == orison::syntax::StatementKind::break_statement);
    assert(result.module.functions.front().body_statements[1].kind ==
           orison::syntax::StatementKind::continue_statement);
}

void test_repeat_statement_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_repeat_success.or";
    {
        std::ofstream output(path);
        output << "package demo.repeat\n";
        output << "function poll() -> Unit\n";
        output << "    repeat\n";
        output << "        step()\n";
        output << "        continue\n";
        output << "    while ready != false\n";
        output << "    return\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 2);
    auto const& repeat_statement = result.module.functions.front().body_statements[0];
    assert(repeat_statement.kind == orison::syntax::StatementKind::repeat_statement);
    assert(repeat_statement.nested_statements.size() == 2);
    assert(repeat_statement.nested_statements[0].kind == orison::syntax::StatementKind::expression_statement);
    assert(repeat_statement.nested_statements[1].kind == orison::syntax::StatementKind::continue_statement);
    assert(repeat_statement.expression.kind == orison::syntax::ExpressionKind::binary);
    assert(repeat_statement.expression.text == "!=");
    assert(repeat_statement.expression.left->text == "ready");
    assert(repeat_statement.expression.right->text == "false");
}

void test_repeat_statement_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_repeat_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.repeat\n";
        output << "function poll() -> Unit\n";
        output << "    repeat\n";
        output << "        step()\n";
        output << "    return\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    auto diagnostics = result.diagnostics.entries();
    assert(!diagnostics.empty());
    assert(diagnostics.front().message == "repeat statement requires a trailing while condition");
}

void test_unsafe_statement_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_unsafe_success.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "function scribble(addr: Address) -> Unit\n";
        output << "    unsafe\n";
        output << "        let p = Pointer(addr)\n";
        output << "        raw_write(p)\n";
        output << "    return\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 2);
    auto const& unsafe_statement = result.module.functions.front().body_statements[0];
    assert(unsafe_statement.kind == orison::syntax::StatementKind::unsafe_statement);
    assert(unsafe_statement.nested_statements.size() == 2);
    assert(unsafe_statement.nested_statements[0].kind == orison::syntax::StatementKind::let_binding);
    assert(unsafe_statement.nested_statements[1].kind == orison::syntax::StatementKind::expression_statement);
}

void test_unsafe_statement_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_unsafe_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.unsafe\n";
        output << "function scribble() -> Unit\n";
        output << "    unsafe\n";
        output << "    return\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    auto diagnostics = result.diagnostics.entries();
    assert(!diagnostics.empty());
    assert(diagnostics.front().message == "unsafe statement requires an indented block");
}

void test_boolean_literal_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_boolean_success.or";
    {
        std::ofstream output(path);
        output << "package demo.bools\n";
        output << "function classify(flag: Bool) -> Bool\n";
        output << "    if flag == false\n";
        output << "        return true\n";
        output << "    switch flag\n";
        output << "        true => false\n";
        output << "        default => true\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 2);
    auto const& if_statement = result.module.functions.front().body_statements[0];
    assert(if_statement.expression.kind == orison::syntax::ExpressionKind::binary);
    assert(if_statement.expression.right->kind == orison::syntax::ExpressionKind::boolean_literal);
    assert(if_statement.expression.right->text == "false");
    assert(if_statement.nested_statements[0].kind == orison::syntax::StatementKind::return_statement);
    assert(if_statement.nested_statements[0].expression.kind == orison::syntax::ExpressionKind::boolean_literal);
    assert(if_statement.nested_statements[0].expression.text == "true");
    auto const& switch_statement = result.module.functions.front().body_statements[1];
    assert(switch_statement.kind == orison::syntax::StatementKind::switch_statement);
    assert(switch_statement.switch_cases.size() == 2);
    assert(switch_statement.switch_cases[0].pattern.kind == orison::syntax::ExpressionKind::boolean_literal);
    assert(switch_statement.switch_cases[0].pattern.text == "true");
    assert(switch_statement.switch_cases[0].statements[0]->expression.kind ==
           orison::syntax::ExpressionKind::boolean_literal);
    assert(switch_statement.switch_cases[0].statements[0]->expression.text == "false");
}

void test_string_literal_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_string_success.or";
    {
        std::ofstream output(path);
        output << "package demo.strings\n";
        output << "function log_message() -> Text\n";
        output << "    let label: Text = \"sum\"\n";
        output << "    sink(\"hello\")\n";
        output << "    return \"done\"\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 3);
    auto const& let_statement = result.module.functions.front().body_statements[0];
    assert(let_statement.kind == orison::syntax::StatementKind::let_binding);
    assert(let_statement.expression.kind == orison::syntax::ExpressionKind::string_literal);
    assert(let_statement.expression.text == "\"sum\"");
    auto const& call_statement = result.module.functions.front().body_statements[1];
    assert(call_statement.kind == orison::syntax::StatementKind::expression_statement);
    assert(call_statement.expression.kind == orison::syntax::ExpressionKind::call);
    assert(call_statement.expression.arguments.size() == 1);
    assert(call_statement.expression.arguments[0].kind == orison::syntax::ExpressionKind::string_literal);
    assert(call_statement.expression.arguments[0].text == "\"hello\"");
    auto const& return_statement = result.module.functions.front().body_statements[2];
    assert(return_statement.kind == orison::syntax::StatementKind::return_statement);
    assert(return_statement.expression.kind == orison::syntax::ExpressionKind::string_literal);
    assert(return_statement.expression.text == "\"done\"");
}

void test_string_literal_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_string_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.strings\n";
        output << "function log_message() -> Text\n";
        output << "    return \"unterminated\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    auto diagnostics = result.diagnostics.entries();
    assert(!diagnostics.empty());
    assert(diagnostics.front().message == "unterminated string literal");
}

void test_hex_binary_integer_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_hex_binary_success.or";
    {
        std::ofstream output(path);
        output << "package demo.numbers\n";
        output << "function mask_values() -> UInt32\n";
        output << "    let mask = 0xFF\n";
        output << "    let bits = 0b1010_0001\n";
        output << "    return 0x2000_0000\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 3);
    assert(result.module.functions.front().body_statements[0].kind == orison::syntax::StatementKind::let_binding);
    assert(result.module.functions.front().body_statements[0].expression.kind ==
           orison::syntax::ExpressionKind::integer_literal);
    assert(result.module.functions.front().body_statements[0].expression.text == "0xFF");
    assert(result.module.functions.front().body_statements[1].kind == orison::syntax::StatementKind::let_binding);
    assert(result.module.functions.front().body_statements[1].expression.kind ==
           orison::syntax::ExpressionKind::integer_literal);
    assert(result.module.functions.front().body_statements[1].expression.text == "0b1010_0001");
    assert(result.module.functions.front().body_statements[2].kind ==
           orison::syntax::StatementKind::return_statement);
    assert(result.module.functions.front().body_statements[2].expression.kind ==
           orison::syntax::ExpressionKind::integer_literal);
    assert(result.module.functions.front().body_statements[2].expression.text == "0x2000_0000");
}

void test_cast_expression_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_cast_success.or";
    {
        std::ofstream output(path);
        output << "package demo.casts\n";
        output << "function checksum(b: Byte, total: UInt32) -> UInt32\n";
        output << "    let converted = b as UInt32\n";
        output << "    return total + b as UInt32\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 2);
    auto const& let_statement = result.module.functions.front().body_statements[0];
    assert(let_statement.expression.kind == orison::syntax::ExpressionKind::cast);
    assert(let_statement.expression.text == "UInt32");
    assert(let_statement.expression.left->kind == orison::syntax::ExpressionKind::name);
    assert(let_statement.expression.left->text == "b");
    auto const& return_statement = result.module.functions.front().body_statements[1];
    assert(return_statement.expression.kind == orison::syntax::ExpressionKind::binary);
    assert(return_statement.expression.text == "+");
    assert(return_statement.expression.right->kind == orison::syntax::ExpressionKind::cast);
    assert(return_statement.expression.right->text == "UInt32");
    assert(return_statement.expression.right->left->text == "b");
}

void test_cast_expression_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_cast_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.casts\n";
        output << "function checksum(b: Byte) -> UInt32\n";
        output << "    return b as\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    auto diagnostics = result.diagnostics.entries();
    assert(!diagnostics.empty());
    assert(diagnostics.front().message == "cast expression requires a target type after 'as'");
}

void test_index_expression_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_index_success.or";
    {
        std::ofstream output(path);
        output << "package demo.index\n";
        output << "function first(items: shared View<Byte>) -> Byte\n";
        output << "    let first_item = items[0]\n";
        output << "    sink(this.data[0])\n";
        output << "    return items[0]\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 3);
    auto const& let_statement = result.module.functions.front().body_statements[0];
    assert(let_statement.expression.kind == orison::syntax::ExpressionKind::index_access);
    assert(let_statement.expression.left->text == "items");
    assert(let_statement.expression.arguments.size() == 1);
    assert(let_statement.expression.arguments[0].kind == orison::syntax::ExpressionKind::integer_literal);
    assert(let_statement.expression.arguments[0].text == "0");
    auto const& call_statement = result.module.functions.front().body_statements[1];
    assert(call_statement.expression.kind == orison::syntax::ExpressionKind::call);
    assert(call_statement.expression.arguments.size() == 1);
    assert(call_statement.expression.arguments[0].kind == orison::syntax::ExpressionKind::index_access);
    assert(call_statement.expression.arguments[0].left->kind == orison::syntax::ExpressionKind::member_access);
    assert(call_statement.expression.arguments[0].left->text == "data");
    assert(call_statement.expression.arguments[0].left->left->text == "this");
    auto const& return_statement = result.module.functions.front().body_statements[2];
    assert(return_statement.expression.kind == orison::syntax::ExpressionKind::index_access);
    assert(return_statement.expression.left->text == "items");
}

void test_index_expression_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_index_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.index\n";
        output << "function first(items: shared View<Byte>) -> Byte\n";
        output << "    return items[0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    auto diagnostics = result.diagnostics.entries();
    assert(!diagnostics.empty());
    assert(diagnostics.front().message == "expected ']' after index expression");
}

void test_array_literal_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_array_literal_success.or";
    {
        std::ofstream output(path);
        output << "package demo.arrays\n";
        output << "function make_magic() -> DynamicArray<Byte>\n";
        output << "    let bytes = [0x7F, 0x45, 0x4C, 0x46]\n";
        output << "    return [0x41, 0x42]\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 2);
    auto const& let_statement = result.module.functions.front().body_statements[0];
    assert(let_statement.expression.kind == orison::syntax::ExpressionKind::array_literal);
    assert(let_statement.expression.arguments.size() == 4);
    assert(let_statement.expression.arguments[0].text == "0x7F");
    assert(let_statement.expression.arguments[3].text == "0x46");
    auto const& return_statement = result.module.functions.front().body_statements[1];
    assert(return_statement.expression.kind == orison::syntax::ExpressionKind::array_literal);
    assert(return_statement.expression.arguments.size() == 2);
    assert(return_statement.expression.arguments[0].text == "0x41");
    assert(return_statement.expression.arguments[1].text == "0x42");
}

void test_array_literal_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_array_literal_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.arrays\n";
        output << "function make_magic() -> DynamicArray<Byte>\n";
        output << "    return [0x7F, 0x45\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    auto diagnostics = result.diagnostics.entries();
    assert(!diagnostics.empty());
    assert(diagnostics.front().message == "expected ',' or ']' in array literal");
}

void test_word_boolean_operator_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_word_boolean_success.or";
    {
        std::ofstream output(path);
        output << "package demo.booleans\n";
        output << "function allowed(x: Int32, a: Bool, b: Bool, flag: Bool) -> Bool\n";
        output << "    let in_range = x >= 0 and x <= 100\n";
        output << "    let merged = a or b\n";
        output << "    if not flag\n";
        output << "        return false\n";
        output << "    return in_range\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 4);
    auto const& in_range = result.module.functions.front().body_statements[0];
    assert(in_range.expression.kind == orison::syntax::ExpressionKind::binary);
    assert(in_range.expression.text == "and");
    assert(in_range.expression.left->kind == orison::syntax::ExpressionKind::binary);
    assert(in_range.expression.left->text == ">=");
    assert(in_range.expression.right->kind == orison::syntax::ExpressionKind::binary);
    assert(in_range.expression.right->text == "<=");
    auto const& merged = result.module.functions.front().body_statements[1];
    assert(merged.expression.kind == orison::syntax::ExpressionKind::binary);
    assert(merged.expression.text == "or");
    auto const& if_statement = result.module.functions.front().body_statements[2];
    assert(if_statement.expression.kind == orison::syntax::ExpressionKind::unary);
    assert(if_statement.expression.text == "not");
    assert(if_statement.expression.left->text == "flag");
}

void test_named_bitwise_operator_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_named_bitwise_success.or";
    {
        std::ofstream output(path);
        output << "package demo.bits\n";
        output << "function mix(flags: UInt32, mask: UInt32, amount: UInt32) -> UInt32\n";
        output << "    let low = flags bit_and 0xFF\n";
        output << "    let combined = flags bit_or mask\n";
        output << "    let toggled = flags bit_xor mask\n";
        output << "    let moved = flags shift_left amount + 1\n";
        output << "    return bit_not flags\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 5);
    auto const& low = result.module.functions.front().body_statements[0];
    assert(low.expression.kind == orison::syntax::ExpressionKind::binary);
    assert(low.expression.text == "bit_and");
    assert(low.expression.left->text == "flags");
    assert(low.expression.right->text == "0xFF");
    auto const& combined = result.module.functions.front().body_statements[1];
    assert(combined.expression.kind == orison::syntax::ExpressionKind::binary);
    assert(combined.expression.text == "bit_or");
    auto const& toggled = result.module.functions.front().body_statements[2];
    assert(toggled.expression.kind == orison::syntax::ExpressionKind::binary);
    assert(toggled.expression.text == "bit_xor");
    auto const& moved = result.module.functions.front().body_statements[3];
    assert(moved.expression.kind == orison::syntax::ExpressionKind::binary);
    assert(moved.expression.text == "shift_left");
    assert(moved.expression.left->text == "flags");
    assert(moved.expression.right->kind == orison::syntax::ExpressionKind::binary);
    assert(moved.expression.right->text == "+");
    assert(moved.expression.right->left->text == "amount");
    assert(moved.expression.right->right->text == "1");
    auto const& return_statement = result.module.functions.front().body_statements[4];
    assert(return_statement.expression.kind == orison::syntax::ExpressionKind::unary);
    assert(return_statement.expression.text == "bit_not");
    assert(return_statement.expression.left->text == "flags");
}

void test_ternary_expression_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_ternary_success.or";
    {
        std::ofstream output(path);
        output << "package demo.ternary\n";
        output << "function absolute(x: Int64, enabled: Bool) -> Int64\n";
        output << "    let clamped = enabled or x < 0 ? -x : x\n";
        output << "    return x < 0 ? -x : x\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 2);
    auto const& let_statement = result.module.functions.front().body_statements[0];
    assert(let_statement.expression.kind == orison::syntax::ExpressionKind::ternary);
    assert(let_statement.expression.left->kind == orison::syntax::ExpressionKind::binary);
    assert(let_statement.expression.left->text == "or");
    assert(let_statement.expression.right->kind == orison::syntax::ExpressionKind::unary);
    assert(let_statement.expression.right->text == "-");
    assert(let_statement.expression.right->left->text == "x");
    assert(let_statement.expression.alternate->kind == orison::syntax::ExpressionKind::name);
    assert(let_statement.expression.alternate->text == "x");
    auto const& return_statement = result.module.functions.front().body_statements[1];
    assert(return_statement.expression.kind == orison::syntax::ExpressionKind::ternary);
    assert(return_statement.expression.left->kind == orison::syntax::ExpressionKind::binary);
    assert(return_statement.expression.left->text == "<");
    assert(return_statement.expression.left->left->text == "x");
    assert(return_statement.expression.left->right->text == "0");
    assert(return_statement.expression.right->kind == orison::syntax::ExpressionKind::unary);
    assert(return_statement.expression.right->text == "-");
    assert(return_statement.expression.alternate->kind == orison::syntax::ExpressionKind::name);
    assert(return_statement.expression.alternate->text == "x");
}

void test_ternary_expression_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_ternary_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.ternary\n";
        output << "function absolute(x: Int64) -> Int64\n";
        output << "    return x < 0 ? -x\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    auto diagnostics = result.diagnostics.entries();
    assert(!diagnostics.empty());
    assert(diagnostics.front().message == "expected ':' after ternary true branch");
}

void test_null_safe_member_access_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_null_safe_member_success.or";
    {
        std::ofstream output(path);
        output << "package demo.nullsafe\n";
        output << "function city_name(user: Maybe<User>) -> Maybe<Text>\n";
        output << "    let city = user?.profile?.address?.city\n";
        output << "    return city\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.module.functions.size() == 1);
    assert(result.module.functions.front().body_statements.size() == 2);
    auto const& let_statement = result.module.functions.front().body_statements[0];
    assert(let_statement.expression.kind == orison::syntax::ExpressionKind::null_safe_member_access);
    assert(let_statement.expression.text == "city");
    assert(let_statement.expression.left->kind == orison::syntax::ExpressionKind::null_safe_member_access);
    assert(let_statement.expression.left->text == "address");
    assert(let_statement.expression.left->left->kind == orison::syntax::ExpressionKind::null_safe_member_access);
    assert(let_statement.expression.left->left->text == "profile");
    assert(let_statement.expression.left->left->left->kind == orison::syntax::ExpressionKind::name);
    assert(let_statement.expression.left->left->left->text == "user");
}

void test_null_safe_member_access_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_null_safe_member_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.nullsafe\n";
        output << "function city_name(user: Maybe<User>) -> Maybe<Text>\n";
        output << "    return user?.\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    auto diagnostics = result.diagnostics.entries();
    assert(!diagnostics.empty());
    assert(diagnostics.front().message == "expected member name after '?.'");
}

void test_constructor_pattern_switch_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_constructor_pattern_success.or";
    {
        std::ofstream output(path);
        output << "package demo.patterns\n";
        output << "function sum_list(xs: shared List<Int64>, acc: Int64) -> Int64\n";
        output << "    switch xs\n";
        output << "        Empty => acc\n";
        output << "        Node(head, tail) => recur(tail.value, acc + head)\n";
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
    assert(switch_statement.expression.kind == orison::syntax::ExpressionKind::name);
    assert(switch_statement.expression.text == "xs");
    assert(switch_statement.switch_cases.size() == 2);
    assert(!switch_statement.switch_cases[0].is_default);
    assert(switch_statement.switch_cases[0].pattern.kind == orison::syntax::ExpressionKind::name);
    assert(switch_statement.switch_cases[0].pattern.text == "Empty");
    assert(switch_statement.switch_cases[0].statements.size() == 1);
    assert(switch_statement.switch_cases[0].statements[0] != nullptr);
    assert(switch_statement.switch_cases[0].statements[0]->kind == orison::syntax::StatementKind::expression_statement);
    assert(switch_statement.switch_cases[0].statements[0]->expression.text == "acc");
    assert(!switch_statement.switch_cases[1].is_default);
    assert(switch_statement.switch_cases[1].pattern.kind == orison::syntax::ExpressionKind::call);
    assert(switch_statement.switch_cases[1].pattern.left->kind == orison::syntax::ExpressionKind::name);
    assert(switch_statement.switch_cases[1].pattern.left->text == "Node");
    assert(switch_statement.switch_cases[1].pattern.arguments.size() == 2);
    assert(switch_statement.switch_cases[1].pattern.arguments[0].kind == orison::syntax::ExpressionKind::name);
    assert(switch_statement.switch_cases[1].pattern.arguments[0].text == "head");
    assert(switch_statement.switch_cases[1].pattern.arguments[1].kind == orison::syntax::ExpressionKind::name);
    assert(switch_statement.switch_cases[1].pattern.arguments[1].text == "tail");
    assert(switch_statement.switch_cases[1].statements.size() == 1);
    assert(switch_statement.switch_cases[1].statements[0] != nullptr);
    assert(switch_statement.switch_cases[1].statements[0]->kind == orison::syntax::StatementKind::expression_statement);
    assert(switch_statement.switch_cases[1].statements[0]->expression.kind == orison::syntax::ExpressionKind::call);
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

void test_choice_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_choice_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.bad\n";
        output << "choice ParseError\n";
        output << "    InvalidDigit(value UInt16)\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    assert(result.diagnostics.entries().front().message == "named type entry requires ':' after the name");
}

void test_interface_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_module_parser_interface_failure.or";
    {
        std::ofstream output(path);
        output << "package demo.bad\n";
        output << "interface Reader\n";
        output << "    read(this: exclusive This) -> Int32\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto result = parser.parse(*source_file);

    assert(result.diagnostics.has_errors());
    assert(result.diagnostics.entries().front().message == "interface members must be function declarations");
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
    test_choice_generic_success();
    test_interface_generic_success();
    test_implements_success();
    test_implements_failure();
    test_extend_success();
    test_extend_failure();
    test_where_success();
    test_where_failure();
    test_assignment_success();
    test_assignment_failure();
    test_break_continue_success();
    test_break_continue_standalone_success();
    test_repeat_statement_success();
    test_repeat_statement_failure();
    test_unsafe_statement_success();
    test_unsafe_statement_failure();
    test_boolean_literal_success();
    test_string_literal_success();
    test_string_literal_failure();
    test_hex_binary_integer_success();
    test_cast_expression_success();
    test_cast_expression_failure();
    test_index_expression_success();
    test_index_expression_failure();
    test_array_literal_success();
    test_array_literal_failure();
    test_word_boolean_operator_success();
    test_named_bitwise_operator_success();
    test_ternary_expression_success();
    test_ternary_expression_failure();
    test_null_safe_member_access_success();
    test_null_safe_member_access_failure();
    test_constructor_pattern_switch_success();
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
    test_choice_failure();
    test_interface_failure();
    test_switch_statement_failure();
    test_switch_block_case_failure();
    test_while_statement_failure();
    test_for_statement_failure();
    test_defer_statement_failure();
    return 0;
}
