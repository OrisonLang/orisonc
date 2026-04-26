#include "orison/driver/compiler_app.hpp"

#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <filesystem>
#include <sstream>
#include <string_view>

namespace orison::driver {
namespace {

auto usage_text() -> std::string {
    return "usage: orisonc --version | --parse <file>";
}

auto render_type(orison::syntax::TypeSyntax const& type) -> std::string {
    std::string rendered = type.name;
    if (type.generic_arguments.empty()) {
        return rendered;
    }

    rendered += "<";
    for (std::size_t i = 0; i < type.generic_arguments.size(); ++i) {
        if (i > 0) {
            rendered += ", ";
        }
        rendered += render_type(type.generic_arguments[i]);
    }
    rendered += ">";
    return rendered;
}

auto render_visibility(orison::syntax::Visibility visibility) -> std::string_view {
    using orison::syntax::Visibility;
    switch (visibility) {
    case Visibility::public_visibility:
        return "public";
    case Visibility::package_visibility:
        return "package";
    case Visibility::private_visibility:
        return "private";
    }
    return "unknown";
}

auto render_where_constraint(orison::syntax::WhereConstraintSyntax const& constraint) -> std::string {
    std::string rendered = constraint.parameter_name + ": ";
    for (std::size_t i = 0; i < constraint.requirements.size(); ++i) {
        if (i > 0) {
            rendered += " + ";
        }
        rendered += render_type(constraint.requirements[i]);
    }
    return rendered;
}

auto render_statement_kind(orison::syntax::StatementKind kind) -> std::string_view {
    using orison::syntax::StatementKind;
    switch (kind) {
    case StatementKind::let_binding:
        return "let";
    case StatementKind::var_binding:
        return "var";
    case StatementKind::assignment_statement:
        return "assignment";
    case StatementKind::return_statement:
        return "return";
    case StatementKind::break_statement:
        return "break";
    case StatementKind::continue_statement:
        return "continue";
    case StatementKind::switch_statement:
        return "switch";
    case StatementKind::guard_statement:
        return "guard";
    case StatementKind::if_statement:
        return "if";
    case StatementKind::while_statement:
        return "while";
    case StatementKind::repeat_statement:
        return "repeat";
    case StatementKind::for_statement:
        return "for";
    case StatementKind::unsafe_statement:
        return "unsafe";
    case StatementKind::defer_statement:
        return "defer";
    case StatementKind::expression_statement:
        return "expression";
    }
    return "unknown";
}

auto render_expression(orison::syntax::ExpressionSyntax const& expression) -> std::string {
    using orison::syntax::ExpressionKind;
    switch (expression.kind) {
    case ExpressionKind::name:
    case ExpressionKind::integer_literal:
    case ExpressionKind::string_literal:
    case ExpressionKind::boolean_literal:
        return expression.text;
    case ExpressionKind::array_literal: {
        std::string rendered = "[";
        for (std::size_t i = 0; i < expression.arguments.size(); ++i) {
            if (i > 0) {
                rendered += ", ";
            }
            rendered += render_expression(expression.arguments[i]);
        }
        rendered += "]";
        return rendered;
    }
    case ExpressionKind::unary:
        if (expression.text == "not" || expression.text == "bit_not") {
            return expression.text + " " + render_expression(*expression.left);
        }
        return expression.text + render_expression(*expression.left);
    case ExpressionKind::cast:
        return render_expression(*expression.left) + " as " + expression.text;
    case ExpressionKind::call: {
        std::string rendered = render_expression(*expression.left);
        rendered += "(";
        for (std::size_t i = 0; i < expression.arguments.size(); ++i) {
            if (i > 0) {
                rendered += ", ";
            }
            rendered += render_expression(expression.arguments[i]);
        }
        rendered += ")";
        return rendered;
    }
    case ExpressionKind::member_access:
        return render_expression(*expression.left) + "." + expression.text;
    case ExpressionKind::null_safe_member_access:
        return render_expression(*expression.left) + "?." + expression.text;
    case ExpressionKind::index_access:
        return render_expression(*expression.left) + "[" + render_expression(expression.arguments.front()) + "]";
    case ExpressionKind::binary:
        return "(" + render_expression(*expression.left) + " " + expression.text + " " +
               render_expression(*expression.right) + ")";
    case ExpressionKind::ternary:
        return "(" + render_expression(*expression.left) + " ? " + render_expression(*expression.right) + " : " +
               render_expression(*expression.alternate) + ")";
    }
    return "";
}

}  // namespace

auto CompilerApp::run(std::span<char const* const> args) const -> CompileResult {
    if (args.size() > 1 && std::string_view(args[1]) == "--version") {
        return CompileResult {.exit_code = 0, .stdout_text = "orisonc 0.1.0-dev\n"};
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--parse") {
        auto maybe_source = source::SourceFile::read(std::filesystem::path(args[2]));
        if (!maybe_source.has_value()) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = "error: unable to read source file\n",
            };
        }

        syntax::ModuleParser parser;
        auto parse_result = parser.parse(*maybe_source);
        if (parse_result.diagnostics.has_errors()) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = parse_result.diagnostics.render(maybe_source->path().string()),
            };
        }

        std::ostringstream output;
        output << "parsed " << maybe_source->path().string() << '\n';
        output << "package " << parse_result.module.package_name << '\n';
        output << "top-level declarations: " << parse_result.module.top_level_declaration_count << '\n';
        output << "imports: " << parse_result.module.imports.size() << '\n';
        output << "constants: " << parse_result.module.constants.size() << '\n';
        output << "type aliases: " << parse_result.module.type_aliases.size() << '\n';
        output << "records: " << parse_result.module.records.size() << '\n';
        output << "choices: " << parse_result.module.choices.size() << '\n';
        output << "interfaces: " << parse_result.module.interfaces.size() << '\n';
        output << "implementations: " << parse_result.module.implementations.size() << '\n';
        output << "extensions: " << parse_result.module.extensions.size() << '\n';
        output << "functions: " << parse_result.module.functions.size() << '\n';
        if (!parse_result.module.imports.empty()) {
            output << "first import from: " << parse_result.module.imports.front().from_package << '\n';
        }
        if (!parse_result.module.constants.empty()) {
            output << "first constant type: " << render_type(parse_result.module.constants.front().type) << '\n';
            output << "first constant initializer: "
                   << render_expression(parse_result.module.constants.front().initializer) << '\n';
        }
        if (!parse_result.module.type_aliases.empty()) {
            output << "first type alias visibility: "
                   << render_visibility(parse_result.module.type_aliases.front().visibility) << '\n';
            output << "first type alias target: "
                   << render_type(parse_result.module.type_aliases.front().aliased_type) << '\n';
        }
        if (!parse_result.module.records.empty()) {
            output << "first record visibility: " << render_visibility(parse_result.module.records.front().visibility)
                   << '\n';
            output << "record fields: " << parse_result.module.records.front().fields.size() << '\n';
            if (!parse_result.module.records.front().fields.empty()) {
                output << "first field visibility: "
                       << render_visibility(parse_result.module.records.front().fields.front().visibility) << '\n';
                output << "first field type: "
                       << render_type(parse_result.module.records.front().fields.front().type) << '\n';
            }
        }
        if (!parse_result.module.choices.empty()) {
            output << "first choice visibility: " << render_visibility(parse_result.module.choices.front().visibility)
                   << '\n';
            output << "first choice variants: " << parse_result.module.choices.front().variants.size() << '\n';
            if (!parse_result.module.choices.front().variants.empty()) {
                output << "first choice payloads: "
                       << parse_result.module.choices.front().variants.front().payloads.size() << '\n';
            }
        }
        if (!parse_result.module.interfaces.empty()) {
            output << "first interface visibility: "
                   << render_visibility(parse_result.module.interfaces.front().visibility) << '\n';
            output << "first interface methods: " << parse_result.module.interfaces.front().methods.size() << '\n';
        }
        if (!parse_result.module.implementations.empty()) {
            output << "first implementation interface: "
                   << render_type(parse_result.module.implementations.front().interface_type) << '\n';
            output << "first implementation receiver: "
                   << render_type(parse_result.module.implementations.front().receiver_type) << '\n';
            output << "first implementation methods: " << parse_result.module.implementations.front().methods.size()
                   << '\n';
        }
        if (!parse_result.module.extensions.empty()) {
            output << "first extension receiver: " << render_type(parse_result.module.extensions.front().receiver_type)
                   << '\n';
            output << "first extension methods: " << parse_result.module.extensions.front().methods.size() << '\n';
            if (!parse_result.module.extensions.front().methods.empty()) {
                output << "first extension method visibility: "
                       << render_visibility(parse_result.module.extensions.front().methods.front().visibility) << '\n';
            }
        }
        if (!parse_result.module.functions.empty()) {
            output << "first function visibility: "
                   << render_visibility(parse_result.module.functions.front().visibility) << '\n';
            output << "function parameters: " << parse_result.module.functions.front().parameters.size() << '\n';
            output << "function return type: " << render_type(parse_result.module.functions.front().return_type)
                   << '\n';
            output << "function where constraints: "
                   << parse_result.module.functions.front().where_constraints.size() << '\n';
            if (!parse_result.module.functions.front().where_constraints.empty()) {
                output << "first function where constraint: "
                       << render_where_constraint(parse_result.module.functions.front().where_constraints.front())
                       << '\n';
            }
            output << "function body statements: " << parse_result.module.functions.front().body_statements.size()
                   << '\n';
            if (!parse_result.module.functions.front().body_statements.empty()) {
                auto const& first_statement = parse_result.module.functions.front().body_statements.front();
                output << "first statement kind: " << render_statement_kind(first_statement.kind) << '\n';
                output << "first statement expression: " << render_expression(first_statement.expression)
                       << '\n';
                output << "first statement nested count: " << first_statement.nested_statements.size() << '\n';
                output << "first statement alternate count: " << first_statement.alternate_statements.size() << '\n';
                output << "first statement switch cases: " << first_statement.switch_cases.size() << '\n';
                if (!first_statement.switch_cases.empty()) {
                    output << "first switch case pattern: "
                           << render_expression(first_statement.switch_cases.front().pattern) << '\n';
                    output << "first switch case statements: " << first_statement.switch_cases.front().statements.size()
                           << '\n';
                }
            }
        }
        return CompileResult {.exit_code = 0, .stdout_text = output.str()};
    }

    return CompileResult {.exit_code = 1, .stderr_text = usage_text() + "\n"};
}

}  // namespace orison::driver
