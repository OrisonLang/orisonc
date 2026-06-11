#include "orison/lowering/string_constants.hpp"

#include <cassert>

int main() {
    auto decoded = orison::lowering::decode_string_literal("\"line\\nquote\\\"slash\\\\zero\\0\"");
    assert(decoded == std::string("line\nquote\"slash\\zero\0", 23));
    assert(orison::lowering::encode_llvm_string_bytes(decoded) ==
           "line\\0Aquote\\22slash\\5Czero\\00\\00");

    auto module = orison::syntax::ModuleSyntax {};
    auto function = orison::syntax::FunctionSyntax {};
    function.name = "main";
    function.return_type = orison::syntax::TypeSyntax {.name = "Int32"};

    auto first = orison::syntax::StatementSyntax {};
    first.kind = orison::syntax::StatementKind::expression_statement;
    first.expression = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::string_literal,
        .text = "\"same\"",
    };
    auto duplicate = orison::syntax::StatementSyntax {};
    duplicate.kind = orison::syntax::StatementKind::expression_statement;
    duplicate.expression = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::string_literal,
        .text = "\"same\"",
    };
    auto second = orison::syntax::StatementSyntax {};
    second.kind = orison::syntax::StatementKind::expression_statement;
    second.expression = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::string_literal,
        .text = "\"other\"",
    };
    function.body_statements.push_back(std::move(first));
    function.body_statements.push_back(std::move(duplicate));
    function.body_statements.push_back(std::move(second));
    module.functions.push_back(std::move(function));

    auto table = orison::lowering::collect_string_constants(module);
    assert(table.constants.size() == 2);
    assert(table.constants[0].name == ".str.0");
    assert(table.constants[1].name == ".str.1");
    assert(table.find("\"same\"") == &table.constants[0]);
    assert(table.find("\"missing\"") == nullptr);
    return 0;
}
