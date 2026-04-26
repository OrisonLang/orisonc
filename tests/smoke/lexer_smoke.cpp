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
        output << "public interface Display\n";
        output << "    function display(this: shared This) -> Text\n";
        output << "implements Display for Screen\n";
        output << "    function display(this: shared This) -> Text\n";
        output << "        return this.name\n";
        output << "extend Screen\n";
        output << "    public function clear(this: exclusive This) -> Unit\n";
        output << "        return this.width\n";
        output << "package function fill<R>(source: exclusive R) -> Unit\n";
        output << "where R: Reader + Display\n";
        output << "    while source.ready()\n";
        output << "        continue\n";
        output << "    break\n";
        output << "    repeat\n";
        output << "        source.step()\n";
        output << "    while source.ready()\n";
        output << "    unsafe\n";
        output << "        source.write()\n";
        output << "    if true\n";
        output << "        return\n";
        output << "    let label: Text = \"ready\"\n";
        output << "    let mask = 0xFF\n";
        output << "    let bits = 0b1010_0001\n";
        output << "    return\n";
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::Lexer lexer;
    auto result = lexer.lex(*source_file);

    assert(!result.diagnostics.has_errors());
    assert(result.tokens.size() >= 24);
    assert(result.tokens[0].kind == orison::syntax::TokenKind::keyword_package);
    assert(result.tokens[0].line_start);
    assert(result.tokens[1].kind == orison::syntax::TokenKind::identifier);
    assert(result.tokens[2].kind == orison::syntax::TokenKind::dot);
    assert(result.tokens[3].kind == orison::syntax::TokenKind::identifier);
    bool saw_import = false;
    bool saw_as = false;
    bool saw_from = false;
    bool saw_public = false;
    bool saw_type = false;
    bool saw_interface = false;
    bool saw_implements = false;
    bool saw_extend = false;
    bool saw_where = false;
    bool saw_break = false;
    bool saw_continue = false;
    bool saw_repeat = false;
    bool saw_unsafe = false;
    bool saw_true = false;
    bool saw_false = false;
    bool saw_string = false;
    bool saw_hex = false;
    bool saw_binary = false;
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
        if (token.kind == orison::syntax::TokenKind::keyword_type) {
            saw_type = true;
        }
        if (token.kind == orison::syntax::TokenKind::keyword_interface) {
            saw_interface = true;
        }
        if (token.kind == orison::syntax::TokenKind::keyword_implements) {
            saw_implements = true;
        }
        if (token.kind == orison::syntax::TokenKind::keyword_extend) {
            saw_extend = true;
        }
        if (token.kind == orison::syntax::TokenKind::keyword_where) {
            saw_where = true;
        }
        if (token.kind == orison::syntax::TokenKind::keyword_break) {
            saw_break = true;
        }
        if (token.kind == orison::syntax::TokenKind::keyword_continue) {
            saw_continue = true;
        }
        if (token.kind == orison::syntax::TokenKind::keyword_repeat) {
            saw_repeat = true;
        }
        if (token.kind == orison::syntax::TokenKind::keyword_unsafe) {
            saw_unsafe = true;
        }
        if (token.kind == orison::syntax::TokenKind::keyword_true) {
            saw_true = true;
        }
        if (token.kind == orison::syntax::TokenKind::keyword_false) {
            saw_false = true;
        }
        if (token.kind == orison::syntax::TokenKind::string_literal) {
            saw_string = true;
        }
        if (token.kind == orison::syntax::TokenKind::integer_literal && token.lexeme == "0xFF") {
            saw_hex = true;
        }
        if (token.kind == orison::syntax::TokenKind::integer_literal && token.lexeme == "0b1010_0001") {
            saw_binary = true;
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
    assert(saw_type);
    assert(saw_interface);
    assert(saw_implements);
    assert(saw_extend);
    assert(saw_where);
    assert(saw_break);
    assert(saw_continue);
    assert(saw_repeat);
    assert(saw_unsafe);
    assert(saw_true);
    assert(!saw_false);
    assert(saw_string);
    assert(saw_hex);
    assert(saw_binary);
    assert(saw_indent);
    assert(saw_dedent);
    return 0;
}
