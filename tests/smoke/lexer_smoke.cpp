#include "orison/source/source_file.hpp"
#include "orison/syntax/lexer.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
    auto path = std::filesystem::temp_directory_path() / "orison_lexer_smoke.or";
    {
        std::ofstream output(path);
        output << "package demo.cli\n";
        output << "\n";
        output << "function main(value: Int32) -> Int32\n";
        output << "    while value >= 0\n";
        output << "        return value\n";
        output << "    return value\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::Lexer lexer;
    auto result = lexer.lex(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.tokens.size() >= 16);
    assert(result.tokens[0].kind == orison::syntax::TokenKind::keyword_package);
    assert(result.tokens[0].line_start);
    assert(result.tokens[1].kind == orison::syntax::TokenKind::identifier);
    assert(result.tokens[2].kind == orison::syntax::TokenKind::dot);
    assert(result.tokens[3].kind == orison::syntax::TokenKind::identifier);
    bool saw_while = false;
    bool saw_greater_equal = false;
    bool saw_indent = false;
    bool saw_dedent = false;
    for (auto const& token : result.tokens) {
        if (token.kind == orison::syntax::TokenKind::keyword_while) {
            saw_while = true;
        }
        if (token.kind == orison::syntax::TokenKind::greater_equal) {
            saw_greater_equal = true;
        }
        if (token.kind == orison::syntax::TokenKind::indent) {
            saw_indent = true;
        }
        if (token.kind == orison::syntax::TokenKind::dedent) {
            saw_dedent = true;
        }
    }
    assert(saw_while);
    assert(saw_greater_equal);
    assert(saw_indent);
    assert(saw_dedent);
    return 0;
}
