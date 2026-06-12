#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/statement_emitter.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
    auto path = std::filesystem::temp_directory_path() / "orison_statement_emitter_smoke.or";
    {
        auto output = std::ofstream(path);
        output << "package demo.statement_emitter\n"
                  "\n"
                  "function increment(input: UInt32) -> UInt32\n"
                  "    let value = input + 1 as UInt32\n"
                  "    value\n";
    }

    auto source = orison::source::SourceFile::read(path);
    assert(source.has_value());
    auto parser = orison::syntax::ModuleParser {};
    auto parse_result = parser.parse(*source);
    assert(!parse_result.diagnostics.has_errors());

    auto diagnostics = orison::diagnostics::DiagnosticBag {};
    auto lowering = orison::lowering::build_lowering_context(parse_result.module, diagnostics);
    assert(!diagnostics.has_errors());
    auto strings = orison::lowering::collect_string_constants(parse_result.module);
    auto context = orison::lowering::LoweringEmissionContext {
        .lowering = lowering,
        .string_constants = strings,
    };
    auto state = orison::lowering::FunctionLoweringState {};
    state.local_name_counts["input"] = 1;
    state.immutable_bindings.emplace("input", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%input",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    auto failures = orison::lowering::LoweringFailures {};
    auto session = orison::lowering::FunctionLoweringSession {
        .state = state,
        .failures = failures,
    };

    auto const& statements = parse_result.module.functions.front().body_statements;
    assert(orison::lowering::value_expression_for(statements.front()) == nullptr);
    assert(orison::lowering::value_expression_for(statements.back()) == &statements.back().expression);

    auto output = std::ostringstream {};
    assert(orison::lowering::lower_let_statement(
        statements.front(),
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        session,
        diagnostics,
        output
    ));
    assert(!diagnostics.has_errors());
    assert(output.str() == "  %tmp0 = add i32 %input, 1\n  %value = add i32 0, %tmp0\n");
    assert(state.immutable_bindings.at("value").value == "%value");

    auto return_statement = orison::syntax::StatementSyntax {};
    return_statement.kind = orison::syntax::StatementKind::return_statement;
    assert(orison::lowering::value_expression_for(return_statement) == &return_statement.expression);

    auto unsupported = orison::syntax::StatementSyntax {};
    unsupported.kind = orison::syntax::StatementKind::let_binding;
    unsupported.name = "missing_value";
    unsupported.line = 4;
    unsupported.expression.kind = orison::syntax::ExpressionKind::name;
    unsupported.expression.text = "missing";
    diagnostics = {};
    output = {};
    assert(!orison::lowering::lower_let_statement(
        unsupported,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        session,
        diagnostics,
        output
    ));
    assert(
        diagnostics.entries().front().message ==
        "lowering does not yet support this let initializer: unknown lowered name: missing"
    );

    std::filesystem::remove(path);
    return 0;
}
