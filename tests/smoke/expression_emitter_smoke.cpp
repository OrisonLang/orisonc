#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
    auto path = std::filesystem::temp_directory_path() / "orison_expression_emitter_smoke.or";
    {
        auto output = std::ofstream(path);
        output << "package demo.expression_emitter\n"
                  "\n"
                  "function choose(flag: Bool, left: UInt32, right: UInt32) -> UInt32\n"
                  "    flag ? left + 1 as UInt32 : right + 2 as UInt32\n"
                  "\n"
                  "extend UInt32\n"
                  "    function scale(this: shared This, amount: UInt32) -> UInt32\n"
                  "        this + amount\n"
                  "\n"
                  "function call_method(value: UInt32) -> UInt32\n"
                  "    value.scale(2 as UInt32)\n";
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
    auto failures = orison::lowering::LoweringFailures {};
    auto session = orison::lowering::FunctionLoweringSession {
        .state = state,
        .failures = failures,
    };
    state.immutable_bindings.emplace("flag", orison::lowering::LoweredExpression {
        .type = "i1",
        .value = "%flag",
    });
    state.immutable_bindings.emplace("left", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%left",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    state.immutable_bindings.emplace("right", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%right",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });

    auto output = std::ostringstream {};
    auto lowered = orison::lowering::lower_expression(
        parse_result.module.functions.front().body_statements.front().expression,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        session,
        output
    );
    assert(lowered.has_value());
    assert(lowered->type == "i32");
    assert(lowered->value == "%tmp2");
    assert(state.current_block == "ternary.merge.0");
    assert(
        output.str() ==
        "  br i1 %flag, label %ternary.then.0, label %ternary.else.0\n"
        "ternary.then.0:\n"
        "  %tmp0 = add i32 %left, 1\n"
        "  br label %ternary.merge.0\n"
        "ternary.else.0:\n"
        "  %tmp1 = add i32 %right, 2\n"
        "  br label %ternary.merge.0\n"
        "ternary.merge.0:\n"
        "  %tmp2 = phi i32 [%tmp0, %ternary.then.0], [%tmp1, %ternary.else.0]\n"
    );

    auto unknown = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "missing",
    };
    output = {};
    auto failed = orison::lowering::lower_expression(
        unknown,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        session,
        output
    );
    assert(!failed.has_value());
    assert(
        failures.expression.reason ==
        orison::lowering::ExpressionLoweringFailureReason::unknown_name
    );
    assert(
        orison::lowering::render_expression_lowering_failure(failures.expression) ==
        "unknown lowered name: missing"
    );

    auto call = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::call,
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(
            orison::syntax::ExpressionSyntax {
                .kind = orison::syntax::ExpressionKind::name,
                .text = "choose",
            }
        ),
    };
    output = {};
    failed = orison::lowering::lower_expression(
        call,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        session,
        output
    );
    assert(!failed.has_value());
    assert(
        failures.expression.reason ==
        orison::lowering::ExpressionLoweringFailureReason::call_arity_mismatch
    );
    assert(
        orison::lowering::render_expression_lowering_failure(failures.expression) ==
        "call arity mismatch: choose expects 3 arguments, got 0"
    );

    auto member_state = orison::lowering::FunctionLoweringState {};
    member_state.local_name_counts["value"] = 1;
    member_state.immutable_bindings.emplace("value", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%value",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    member_state.source_type_names.emplace("value", "UInt32");
    auto member_failures = orison::lowering::LoweringFailures {};
    auto member_session = orison::lowering::FunctionLoweringSession {
        .state = member_state,
        .failures = member_failures,
    };
    output = {};
    auto member_lowered = orison::lowering::lower_expression(
        parse_result.module.functions[1].body_statements.front().expression,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        member_session,
        output
    );
    assert(member_lowered.has_value());
    assert(member_lowered->type == "i32");
    assert(member_lowered->value == "%tmp0");
    assert(
        output.str() ==
        "  %tmp0 = call i32 @method.UInt32.scale(i32 %value, i32 2)\n"
    );

    auto missing_member_state = orison::lowering::FunctionLoweringState {};
    missing_member_state.immutable_bindings.emplace("value", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%value",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    auto missing_member_failures = orison::lowering::LoweringFailures {};
    auto missing_member_session = orison::lowering::FunctionLoweringSession {
        .state = missing_member_state,
        .failures = missing_member_failures,
    };
    output = {};
    auto missing_member_lowered = orison::lowering::lower_expression(
        parse_result.module.functions[1].body_statements.front().expression,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        missing_member_session,
        output
    );
    assert(!missing_member_lowered.has_value());
    assert(
        missing_member_session.failures.expression.reason ==
        orison::lowering::ExpressionLoweringFailureReason::unknown_member_call_receiver
    );
    assert(
        orison::lowering::render_expression_lowering_failure(
            missing_member_session.failures.expression
        ) == "unknown lowered member call receiver: value"
    );
    std::filesystem::remove(path);
    return 0;
}
