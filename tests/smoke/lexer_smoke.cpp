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
        output << "import\n";
        output << "    Logger as Log from diagnostics.logger\n";
        output << "public type Port = UInt16\n";
        output << "public record User\n";
        output << "    private age: UInt8\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::Lexer lexer;
    auto result = lexer.lex(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.tokens.size() >= 22);
    assert(result.tokens[0].kind == orison::syntax::TokenKind::keyword_package);
    assert(result.tokens[0].line_start);
    assert(result.tokens[1].kind == orison::syntax::TokenKind::identifier);
    assert(result.tokens[2].kind == orison::syntax::TokenKind::dot);
    assert(result.tokens[3].kind == orison::syntax::TokenKind::identifier);
    bool saw_import = false;
    bool saw_as = false;
    bool saw_from = false;
    bool saw_public = false;
    bool saw_private = false;
    bool saw_type = false;
    bool saw_indent = false;
    bool saw_dedent = false;
    for (auto const& token : result.tokens) {
        if (token.kind == orison::syntax::TokenKind::keyword_import) {
            saw_import = true;
        }
        if (token.kind == orison::syntax::TokenKind::keyword_as) {
            saw_as = true;
        }
        if (token.kind == orison::syntax::TokenKind::keyword_from) {
            saw_from = true;
        }
        if (token.kind == orison::syntax::TokenKind::keyword_public) {
            saw_public = true;
        }
        if (token.kind == orison::syntax::TokenKind::keyword_private) {
            saw_private = true;
        }
        if (token.kind == orison::syntax::TokenKind::keyword_type) {
            saw_type = true;
        }
        if (token.kind == orison::syntax::TokenKind::indent) {
            saw_indent = true;
        }
        if (token.kind == orison::syntax::TokenKind::dedent) {
            saw_dedent = true;
        }
    }
    assert(saw_import);
    assert(saw_as);
    assert(saw_from);
    assert(saw_public);
    assert(saw_private);
    assert(saw_type);
    assert(saw_indent);
    assert(saw_dedent);
    return 0;
}
