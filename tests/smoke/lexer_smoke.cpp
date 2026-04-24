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
        output << "function main(item: Int32) -> Int32\n";
        output << "    if item % 2 != 0\n";
        output << "        return 0\n";
        output << "    return 0\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::Lexer lexer;
    auto result = lexer.lex(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.tokens.size() >= 18);
    assert(result.tokens[0].kind == orison::syntax::TokenKind::keyword_package);
    assert(result.tokens[0].line_start);
    assert(result.tokens[1].kind == orison::syntax::TokenKind::identifier);
    assert(result.tokens[2].kind == orison::syntax::TokenKind::dot);
    assert(result.tokens[3].kind == orison::syntax::TokenKind::identifier);
    bool saw_percent = false;
    bool saw_bang_equal = false;
    bool saw_indent = false;
    bool saw_dedent = false;
    for (auto const& token : result.tokens) {
        if (token.kind == orison::syntax::TokenKind::percent) {
            saw_percent = true;
        }
        if (token.kind == orison::syntax::TokenKind::bang_equal) {
            saw_bang_equal = true;
        }
        if (token.kind == orison::syntax::TokenKind::indent) {
            saw_indent = true;
        }
        if (token.kind == orison::syntax::TokenKind::dedent) {
            saw_dedent = true;
        }
    }
    assert(saw_percent);
    assert(saw_bang_equal);
    assert(saw_indent);
    assert(saw_dedent);
    return 0;
}
