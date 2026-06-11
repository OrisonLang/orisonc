#include "orison/lowering/string_constants.hpp"

#include <iomanip>
#include <sstream>

namespace orison::lowering {
namespace {

auto unquoted_text(std::string_view text) -> std::string_view {
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return text.substr(1, text.size() - 2);
    }
    return text;
}

void collect_expression_strings(
    syntax::ExpressionSyntax const& expression,
    StringConstantTable& table
);

void collect_statement_strings(
    syntax::StatementSyntax const& statement,
    StringConstantTable& table
) {
    collect_expression_strings(statement.expression, table);
    collect_expression_strings(statement.assignment_target, table);
    for (auto const& nested : statement.nested_statements) {
        collect_statement_strings(nested, table);
    }
    for (auto const& alternate : statement.alternate_statements) {
        collect_statement_strings(alternate, table);
    }
    for (auto const& switch_case : statement.switch_cases) {
        collect_expression_strings(switch_case.pattern, table);
        for (auto const& nested : switch_case.statements) {
            if (nested) {
                collect_statement_strings(*nested, table);
            }
        }
    }
}

void collect_expression_strings(
    syntax::ExpressionSyntax const& expression,
    StringConstantTable& table
) {
    if (expression.kind == syntax::ExpressionKind::string_literal &&
        !table.source_indices.contains(expression.text)) {
        auto bytes = decode_string_literal(expression.text);
        table.source_indices.emplace(expression.text, table.constants.size());
        table.constants.push_back(StringConstant {
            .name = ".str." + std::to_string(table.constants.size()),
            .bytes = bytes,
            .llvm_bytes = encode_llvm_string_bytes(bytes),
        });
    }
    if (expression.left) {
        collect_expression_strings(*expression.left, table);
    }
    if (expression.right) {
        collect_expression_strings(*expression.right, table);
    }
    if (expression.alternate) {
        collect_expression_strings(*expression.alternate, table);
    }
    for (auto const& argument : expression.arguments) {
        collect_expression_strings(argument, table);
    }
    for (auto const& statement : expression.nested_statements) {
        if (statement) {
            collect_expression_strings(statement->expression, table);
        }
    }
}

}  // namespace

auto StringConstantTable::find(std::string_view source_literal) const -> StringConstant const* {
    auto found = source_indices.find(std::string(source_literal));
    if (found == source_indices.end()) {
        return nullptr;
    }
    return &constants[found->second];
}

auto decode_string_literal(std::string_view source_literal) -> std::string {
    auto text = unquoted_text(source_literal);
    auto decoded = std::string {};
    decoded.reserve(text.size() + 1);
    for (auto index = std::size_t {0}; index < text.size(); ++index) {
        if (text[index] != '\\' || index + 1 == text.size()) {
            decoded.push_back(text[index]);
            continue;
        }

        auto escaped = text[++index];
        switch (escaped) {
        case 'n':
            decoded.push_back('\n');
            break;
        case 'r':
            decoded.push_back('\r');
            break;
        case 't':
            decoded.push_back('\t');
            break;
        case '0':
            decoded.push_back('\0');
            break;
        case '\\':
            decoded.push_back('\\');
            break;
        case '"':
            decoded.push_back('"');
            break;
        default:
            decoded.push_back(escaped);
            break;
        }
    }
    decoded.push_back('\0');
    return decoded;
}

auto encode_llvm_string_bytes(std::string_view bytes) -> std::string {
    auto output = std::ostringstream {};
    output << std::uppercase << std::hex << std::setfill('0');
    for (auto byte : bytes) {
        auto value = static_cast<unsigned char>(byte);
        if (value >= 0x20 && value <= 0x7e && byte != '"' && byte != '\\') {
            output << byte;
        } else {
            output << '\\' << std::setw(2) << static_cast<unsigned int>(value);
        }
    }
    return output.str();
}

auto collect_string_constants(syntax::ModuleSyntax const& module) -> StringConstantTable {
    auto table = StringConstantTable {};
    for (auto const& function : module.functions) {
        for (auto const& statement : function.body_statements) {
            collect_statement_strings(statement, table);
        }
    }
    return table;
}

}  // namespace orison::lowering
