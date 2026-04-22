#include "orison/syntax/lexer.hpp"

#include <cctype>
#include <string_view>

namespace orison::syntax {
namespace {

auto keyword_kind(std::string_view text) -> TokenKind {
    if (text == "package") {
        return TokenKind::keyword_package;
    }
    if (text == "record") {
        return TokenKind::keyword_record;
    }
    if (text == "function") {
        return TokenKind::keyword_function;
    }
    return TokenKind::identifier;
}

}  // namespace

auto Lexer::lex(source::SourceFile const& source_file) const -> LexResult {
    LexResult result;
    auto const& input = source_file.content();

    std::size_t index = 0;
    std::size_t line = 1;
    std::size_t column = 1;
    bool at_line_start = true;
    std::size_t current_indent = 0;

    while (index < input.size()) {
        char ch = input[index];

        if (at_line_start) {
            current_indent = 0;
            while (index < input.size() && input[index] == ' ') {
                ++current_indent;
                ++index;
                ++column;
            }

            if (index < input.size() && input[index] == '\t') {
                result.diagnostics.error(line, "tabs are not permitted in indentation");
            }

            if (index >= input.size()) {
                break;
            }

            ch = input[index];
        }

        if (ch == '\r') {
            ++index;
            continue;
        }

        if (ch == '\n') {
            result.tokens.push_back(Token {
                .kind = TokenKind::newline,
                .lexeme = "\n",
                .line = line,
                .column = column,
                .indent = 0,
                .line_start = false,
            });

            ++index;
            ++line;
            column = 1;
            at_line_start = true;
            current_indent = 0;
            continue;
        }

        if (ch == '#') {
            while (index < input.size() && input[index] != '\n') {
                ++index;
                ++column;
            }
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            ++index;
            ++column;
            continue;
        }

        Token token {
            .kind = TokenKind::unknown,
            .lexeme = "",
            .line = line,
            .column = column,
            .indent = at_line_start ? current_indent : 0,
            .line_start = at_line_start,
        };
        at_line_start = false;

        if (std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_') {
            auto start = index;
            while (index < input.size()) {
                char part = input[index];
                if (std::isalnum(static_cast<unsigned char>(part)) == 0 && part != '_') {
                    break;
                }
                ++index;
                ++column;
            }

            token.lexeme = input.substr(start, index - start);
            token.kind = keyword_kind(token.lexeme);
            result.tokens.push_back(token);
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            auto start = index;
            while (index < input.size() && std::isdigit(static_cast<unsigned char>(input[index])) != 0) {
                ++index;
                ++column;
            }

            token.lexeme = input.substr(start, index - start);
            token.kind = TokenKind::integer_literal;
            result.tokens.push_back(token);
            continue;
        }

        if (ch == '-' && index + 1 < input.size() && input[index + 1] == '>') {
            token.kind = TokenKind::arrow;
            token.lexeme = "->";
            index += 2;
            column += 2;
            result.tokens.push_back(token);
            continue;
        }

        switch (ch) {
        case '(':
            token.kind = TokenKind::left_paren;
            break;
        case ')':
            token.kind = TokenKind::right_paren;
            break;
        case ',':
            token.kind = TokenKind::comma;
            break;
        case ':':
            token.kind = TokenKind::colon;
            break;
        case '.':
            token.kind = TokenKind::dot;
            break;
        case '<':
            token.kind = TokenKind::less;
            break;
        case '>':
            token.kind = TokenKind::greater;
            break;
        default:
            token.kind = TokenKind::unknown;
            token.lexeme = std::string(1, ch);
            result.tokens.push_back(token);
            result.diagnostics.error(line, "unexpected character in source");
            ++index;
            ++column;
            continue;
        }

        token.lexeme = std::string(1, ch);
        result.tokens.push_back(token);
        ++index;
        ++column;
    }

    result.tokens.push_back(Token {
        .kind = TokenKind::eof,
        .lexeme = "",
        .line = line,
        .column = column,
        .indent = 0,
        .line_start = at_line_start,
    });
    return result;
}

}  // namespace orison::syntax
