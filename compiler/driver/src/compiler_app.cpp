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
        output << "records: " << parse_result.module.records.size() << '\n';
        output << "functions: " << parse_result.module.functions.size() << '\n';
        if (!parse_result.module.records.empty()) {
            output << "record fields: " << parse_result.module.records.front().fields.size() << '\n';
            if (!parse_result.module.records.front().fields.empty()) {
                output << "first field type: "
                       << render_type(parse_result.module.records.front().fields.front().type) << '\n';
            }
        }
        if (!parse_result.module.functions.empty()) {
            output << "function parameters: " << parse_result.module.functions.front().parameters.size() << '\n';
            output << "function return type: " << render_type(parse_result.module.functions.front().return_type)
                   << '\n';
            output << "function body statements: " << parse_result.module.functions.front().body_statement_count << '\n';
        }
        return CompileResult {.exit_code = 0, .stdout_text = output.str()};
    }

    return CompileResult {.exit_code = 1, .stderr_text = usage_text() + "\n"};
}

}  // namespace orison::driver
