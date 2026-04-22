#pragma once

#include <cstddef>
#include <string>

namespace orison::syntax {

enum class TokenKind {
    identifier,
    integer_literal,
    keyword_package,
    keyword_record,
    keyword_function,
    indent,
    dedent,
    left_paren,
    right_paren,
    comma,
    colon,
    dot,
    less,
    greater,
    arrow,
    newline,
    eof,
    unknown,
};

struct Token {
    TokenKind kind = TokenKind::unknown;
    std::string lexeme;
    std::size_t line = 0;
    std::size_t column = 0;
    std::size_t indent = 0;
    bool line_start = false;
};

}  // namespace orison::syntax
