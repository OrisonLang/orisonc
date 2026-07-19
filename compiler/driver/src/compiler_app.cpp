#include "orison/driver/compiler_app.hpp"

#include "orison/link/host_linker.hpp"
#include "orison/link/host_runner.hpp"
#include "orison/pipeline/compile_pipeline.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace orison::driver {
namespace {

auto render_expression(orison::syntax::ExpressionSyntax const& expression) -> std::string;

auto render_report_lines(std::vector<std::string> const& lines) -> std::string {
    auto output = std::ostringstream {};
    for (auto const& line : lines) {
        output << line << '\n';
    }
    return output.str();
}

auto usage_text() -> std::string {
    return "usage: orisonc --version | run <file> | --parse <file> | --emit-llvm <file> | "
           "--semantic-planned-drops <file> | --semantic-drop-resolution <file> | "
           "--semantic-drop-diagnostics <file> | --semantic-drop-lowering-authorization <file> | "
           "--semantic-drop-summary <file> | --semantic-dynamic-array-descriptor-origins <file> | "
           "--planned-drops <file> | --planned-drop-actions <file> | --emitted-drops <file> | "
           "--drop-cleanup-authorization <file> | --drop-readiness <file> | "
           "--drop-readiness-summary <file> | --drop-readiness-relations <file> | "
           "--drop-readiness-blockers <file> | --drop-readiness-source-correlations <file> | "
           "--dynamic-array-descriptor-cleanup-plan <file> | "
           "--dynamic-array-cleanup-obligations <file> | "
           "--dynamic-array-cleanup-sequence-plan <file> | "
           "--dynamic-array-cleanup-sequence-verification <file> | "
           "--dynamic-array-cleanup-emission-gate <file> | "
           "--dynamic-array-cleanup-capability <file> | --dynamic-array-cleanup-audit <file> | "
           "--emit-object <file> -o <output> | --build <file> -o <executable>";
}

auto emit_llvm_report(
    std::filesystem::path const& source_path,
    pipeline::CompilePipelineOptions const& options,
    auto report_selector
) -> CompileResult {
    pipeline::CompilePipeline pipeline;
    auto result = pipeline.emit_llvm(source_path, options);
    if (result.has_errors()) {
        return CompileResult {
            .exit_code = 1,
            .stderr_text = std::move(result.error_text),
        };
    }

    return CompileResult {
        .exit_code = 0,
        .stdout_text = render_report_lines(report_selector(result)),
    };
}

auto emit_llvm_report(std::filesystem::path const& source_path, auto report_selector) -> CompileResult {
    return emit_llvm_report(source_path, pipeline::CompilePipelineOptions {}, report_selector);
}

auto dynamic_array_cleanup_report_options() -> pipeline::CompilePipelineOptions {
    return pipeline::CompilePipelineOptions {
        .test_only_enable_source_drop_lowering = true,
        .test_only_derive_dynamic_array_cleanup_from_semantics = true,
        .test_only_enable_dynamic_array_parameter_descriptors = true,
        .test_only_emit_bound_dynamic_array_parameter_cleanups = true,
    };
}

void append_report_lines(std::vector<std::string>& output, std::vector<std::string> const& lines) {
    output.insert(output.end(), lines.begin(), lines.end());
}

auto dynamic_array_cleanup_audit_report(pipeline::CompilePipelineResult const& result) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    append_report_lines(report, result.semantic_dynamic_array_descriptor_origin_report);
    append_report_lines(report, result.dynamic_array_descriptor_cleanup_plan_report);
    append_report_lines(report, result.dynamic_array_cleanup_obligation_report);
    append_report_lines(report, result.dynamic_array_cleanup_sequence_plan_report);
    append_report_lines(report, result.dynamic_array_cleanup_sequence_verification_report);
    append_report_lines(report, result.dynamic_array_cleanup_emission_gate_report);
    append_report_lines(report, result.dynamic_array_cleanup_emission_capability_report);
    return report;
}

auto analyze_report(std::filesystem::path const& source_path, auto report_selector) -> CompileResult {
    pipeline::CompilePipeline pipeline;
    auto result = pipeline.analyze(source_path);
    if (result.has_errors()) {
        return CompileResult {
            .exit_code = 1,
            .stderr_text = std::move(result.error_text),
        };
    }

    return CompileResult {
        .exit_code = 0,
        .stdout_text = render_report_lines(report_selector(result)),
    };
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

auto render_inline_statement(orison::syntax::StatementSyntax const& statement) -> std::string {
    switch (statement.kind) {
    case orison::syntax::StatementKind::let_binding:
        return "let " + statement.name + " = " + render_expression(statement.expression);
    case orison::syntax::StatementKind::var_binding:
        return "var " + statement.name + " = " + render_expression(statement.expression);
    case orison::syntax::StatementKind::assignment_statement:
        return render_expression(statement.assignment_target) + " = " + render_expression(statement.expression);
    case orison::syntax::StatementKind::return_statement:
        if (statement.expression.text.empty() && !statement.expression.left && !statement.expression.right &&
            !statement.expression.alternate && statement.expression.arguments.empty() &&
            statement.expression.nested_statements.empty()) {
            return "return";
        }
        return "return " + render_expression(statement.expression);
    case orison::syntax::StatementKind::expression_statement:
        return render_expression(statement.expression);
    default:
        return std::string(render_statement_kind(statement.kind));
    }
}

auto render_expression(orison::syntax::ExpressionSyntax const& expression) -> std::string {
    using orison::syntax::ExpressionKind;
    switch (expression.kind) {
    case ExpressionKind::name:
    case ExpressionKind::integer_literal:
    case ExpressionKind::float_literal:
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
    case ExpressionKind::task:
    case ExpressionKind::thread: {
        std::string rendered = expression.text + " { ";
        for (std::size_t i = 0; i < expression.nested_statements.size(); ++i) {
            if (i > 0) {
                rendered += "; ";
            }
            rendered += render_inline_statement(*expression.nested_statements[i]);
        }
        rendered += " }";
        return rendered;
    }
    case ExpressionKind::unary:
        if (expression.text == "not" || expression.text == "bit_not" || expression.text == "await") {
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

    if (args.size() == 3 && std::string_view(args[1]) == "run") {
        pipeline::CompilePipeline pipeline;
        auto result = pipeline.emit_object(std::filesystem::path(args[2]));
        if (result.has_errors()) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = std::move(result.error_text),
            };
        }

        link::HostRunner runner;
        auto run_result = runner.run(result.object_bytes, result.link_libraries);
        if (run_result.has_errors()) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = run_result.diagnostics.render(result.source_file->path().string()),
            };
        }
        return CompileResult {.exit_code = run_result.exit_code};
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--emit-llvm") {
        pipeline::CompilePipeline pipeline;
        auto result = pipeline.emit_llvm(std::filesystem::path(args[2]));
        if (result.has_errors()) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = std::move(result.error_text),
            };
        }
        return CompileResult {
            .exit_code = 0,
            .stdout_text = std::move(result.ir_text),
        };
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--planned-drops") {
        return emit_llvm_report(std::filesystem::path(args[2]), [](auto const& result) -> auto const& {
            return result.planned_drop_report;
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--semantic-planned-drops") {
        return analyze_report(std::filesystem::path(args[2]), [](auto const& result) -> auto const& {
            return result.semantic_planned_drop_report;
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--semantic-drop-resolution") {
        return analyze_report(std::filesystem::path(args[2]), [](auto const& result) -> auto const& {
            return result.semantic_drop_resolution_report;
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--semantic-drop-diagnostics") {
        return analyze_report(std::filesystem::path(args[2]), [](auto const& result) -> auto const& {
            return result.semantic_drop_diagnostic_report;
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--semantic-drop-lowering-authorization") {
        return analyze_report(std::filesystem::path(args[2]), [](auto const& result) -> auto const& {
            return result.semantic_drop_lowering_authorization_report;
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--semantic-drop-summary") {
        return analyze_report(std::filesystem::path(args[2]), [](auto const& result) -> auto const& {
            return result.semantic_drop_resolution_summary_report;
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--semantic-dynamic-array-descriptor-origins") {
        return analyze_report(std::filesystem::path(args[2]), [](auto const& result) -> auto const& {
            return result.semantic_dynamic_array_descriptor_origin_report;
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--planned-drop-actions") {
        return emit_llvm_report(std::filesystem::path(args[2]), [](auto const& result) -> auto const& {
            return result.planned_drop_action_report;
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--emitted-drops") {
        return emit_llvm_report(std::filesystem::path(args[2]), [](auto const& result) -> auto const& {
            return result.emitted_drop_declaration_report;
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--drop-cleanup-authorization") {
        return emit_llvm_report(std::filesystem::path(args[2]), [](auto const& result) -> auto const& {
            return result.drop_cleanup_authorization_report;
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--drop-readiness") {
        return emit_llvm_report(std::filesystem::path(args[2]), [](auto const& result) -> auto const& {
            return result.drop_readiness_snapshot_report;
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--drop-readiness-summary") {
        return emit_llvm_report(std::filesystem::path(args[2]), [](auto const& result) -> auto const& {
            return result.drop_readiness_summary_report;
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--drop-readiness-relations") {
        return emit_llvm_report(std::filesystem::path(args[2]), [](auto const& result) -> auto const& {
            return result.drop_readiness_relation_report;
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--drop-readiness-blockers") {
        return emit_llvm_report(std::filesystem::path(args[2]), [](auto const& result) -> auto const& {
            return result.drop_readiness_blocker_report;
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--drop-readiness-source-correlations") {
        return emit_llvm_report(std::filesystem::path(args[2]), [](auto const& result) -> auto const& {
            return result.drop_readiness_source_correlation_report;
        });
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--dynamic-array-descriptor-cleanup-plan") {
        return emit_llvm_report(
            std::filesystem::path(args[2]),
            dynamic_array_cleanup_report_options(),
            [](auto const& result) -> auto const& {
                return result.dynamic_array_descriptor_cleanup_plan_report;
            }
        );
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--dynamic-array-cleanup-obligations") {
        return emit_llvm_report(
            std::filesystem::path(args[2]),
            dynamic_array_cleanup_report_options(),
            [](auto const& result) -> auto const& {
                return result.dynamic_array_cleanup_obligation_report;
            }
        );
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--dynamic-array-cleanup-sequence-plan") {
        return emit_llvm_report(
            std::filesystem::path(args[2]),
            dynamic_array_cleanup_report_options(),
            [](auto const& result) -> auto const& {
                return result.dynamic_array_cleanup_sequence_plan_report;
            }
        );
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--dynamic-array-cleanup-sequence-verification") {
        return emit_llvm_report(
            std::filesystem::path(args[2]),
            dynamic_array_cleanup_report_options(),
            [](auto const& result) -> auto const& {
                return result.dynamic_array_cleanup_sequence_verification_report;
            }
        );
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--dynamic-array-cleanup-emission-gate") {
        return emit_llvm_report(
            std::filesystem::path(args[2]),
            dynamic_array_cleanup_report_options(),
            [](auto const& result) -> auto const& {
                return result.dynamic_array_cleanup_emission_gate_report;
            }
        );
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--dynamic-array-cleanup-capability") {
        return emit_llvm_report(
            std::filesystem::path(args[2]),
            dynamic_array_cleanup_report_options(),
            [](auto const& result) -> auto const& {
                return result.dynamic_array_cleanup_emission_capability_report;
            }
        );
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--dynamic-array-cleanup-audit") {
        return emit_llvm_report(
            std::filesystem::path(args[2]),
            dynamic_array_cleanup_report_options(),
            [](auto const& result) {
                return dynamic_array_cleanup_audit_report(result);
            }
        );
    }

    if (args.size() == 5 && std::string_view(args[1]) == "--emit-object" &&
        std::string_view(args[3]) == "-o") {
        pipeline::CompilePipeline pipeline;
        auto result = pipeline.emit_object(std::filesystem::path(args[2]));
        if (result.has_errors()) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = std::move(result.error_text),
            };
        }

        auto output_path = std::filesystem::path(args[4]);
        auto output = std::ofstream(output_path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!output) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = "error: unable to write object file\n",
            };
        }
        output.write(
            result.object_bytes.data(),
            static_cast<std::streamsize>(result.object_bytes.size())
        );
        output.close();
        if (!output) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = "error: unable to write object file\n",
            };
        }
        return CompileResult {.exit_code = 0};
    }

    if (args.size() == 5 && std::string_view(args[1]) == "--build" &&
        std::string_view(args[3]) == "-o") {
        pipeline::CompilePipeline pipeline;
        auto result = pipeline.emit_object(std::filesystem::path(args[2]));
        if (result.has_errors()) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = std::move(result.error_text),
            };
        }

        link::HostLinker linker;
        auto link_result = linker.link(
            result.object_bytes,
            std::filesystem::path(args[4]),
            result.link_libraries
        );
        if (link_result.has_errors()) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = link_result.diagnostics.render(result.source_file->path().string()),
            };
        }
        return CompileResult {.exit_code = 0};
    }

    if (args.size() == 3 && std::string_view(args[1]) == "--parse") {
        pipeline::CompilePipeline pipeline;
        auto result = pipeline.analyze(std::filesystem::path(args[2]));
        if (result.has_errors()) {
            return CompileResult {
                .exit_code = 1,
                .stderr_text = std::move(result.error_text),
            };
        }
        auto const& source_file = *result.source_file;
        auto const& parse_result = result.parse_result;

        std::ostringstream output;
        output << "parsed " << source_file.path().string() << '\n';
        output << "package " << parse_result.module.package_name << '\n';
        output << "top-level declarations: " << parse_result.module.top_level_declaration_count << '\n';
        output << "imports: " << parse_result.module.imports.size() << '\n';
        output << "foreign imports: " << parse_result.module.foreign_imports.size() << '\n';
        output << "foreign exports: " << parse_result.module.foreign_exports.size() << '\n';
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
        if (!parse_result.module.foreign_imports.empty()) {
            output << "first foreign import abi: " << parse_result.module.foreign_imports.front().abi << '\n';
            output << "first foreign import library: "
                   << (parse_result.module.foreign_imports.front().library_name.empty()
                           ? "<none>"
                           : parse_result.module.foreign_imports.front().library_name)
                   << '\n';
            output << "first foreign import functions: " << parse_result.module.foreign_imports.front().functions.size()
                   << '\n';
        }
        if (!parse_result.module.foreign_exports.empty()) {
            output << "first foreign export abi: " << parse_result.module.foreign_exports.front().abi << '\n';
            output << "first foreign export symbol: "
                   << (parse_result.module.foreign_exports.front().external_name.empty()
                           ? "<none>"
                           : parse_result.module.foreign_exports.front().external_name)
                   << '\n';
            output << "first foreign export function: "
                   << parse_result.module.foreign_exports.front().function.name << '\n';
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
            output << "first function async: "
                   << (parse_result.module.functions.front().is_async ? "true" : "false") << '\n';
            output << "first function unsafe: "
                   << (parse_result.module.functions.front().is_unsafe ? "true" : "false") << '\n';
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
