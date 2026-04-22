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
        output << "record User\n";
        output << "    name: Text\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::Lexer lexer;
    auto result = lexer.lex(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.tokens.size() >= 8);
    assert(result.tokens[0].kind == orison::syntax::TokenKind::keyword_package);
    assert(result.tokens[0].line_start);
    assert(result.tokens[0].indent == 0);
    assert(result.tokens[1].kind == orison::syntax::TokenKind::identifier);
    assert(result.tokens[2].kind == orison::syntax::TokenKind::dot);
    assert(result.tokens[3].kind == orison::syntax::TokenKind::identifier);
    return 0;
}
