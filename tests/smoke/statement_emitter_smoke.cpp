#include "orison/lowering/expression_emitter.hpp"
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
#include <memory>
#include <optional>
#include <vector>

namespace {

auto reject_control_flow(
    orison::syntax::StatementSyntax const&,
    std::string_view,
    orison::lowering::IntegerSignedness,
    orison::lowering::LoweringEmissionContext const&,
    orison::lowering::FunctionLoweringSession&,
    orison::diagnostics::DiagnosticBag&,
    std::ostringstream&
) -> std::optional<orison::lowering::LoweredExpression> {
    return std::nullopt;
}

}  // namespace

int main() {
    auto path = std::filesystem::temp_directory_path() / "orison_statement_emitter_smoke.or";
    {
        auto output = std::ofstream(path);
        output << "package demo.statement_emitter\n"
                  "\n"
                  "function increment(input: UInt32) -> UInt32\n"
                  "    let value = input + 1 as UInt32\n"
                  "    value\n"
                  "\n"
                  "function mutate(input: UInt32) -> UInt32\n"
                  "    var total: UInt32 = input\n"
                  "    total = total + 1 as UInt32\n"
                  "    total\n"
                  "\n"
                  "function observe(input: UInt32) -> UInt32\n"
                  "    input\n"
                  "\n"
                  "function observe_unit(input: UInt32) -> Unit\n"
                  "    return\n"
                  "\n"
                  "function call_observe(input: UInt32) -> UInt32\n"
                  "    observe(input)\n"
                  "    observe_unit(input)\n"
                  "    input\n";
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

    auto mutable_state = orison::lowering::FunctionLoweringState {};
    mutable_state.local_name_counts["input"] = 1;
    mutable_state.immutable_bindings.emplace("input", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%input",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    auto mutable_failures = orison::lowering::LoweringFailures {};
    auto mutable_session = orison::lowering::FunctionLoweringSession {
        .state = mutable_state,
        .failures = mutable_failures,
    };
    auto const& mutable_statements = parse_result.module.functions[1].body_statements;
    output = {};
    assert(orison::lowering::lower_var_statement(
        mutable_statements[0],
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        mutable_session,
        diagnostics,
        output
    ));
    assert(orison::lowering::lower_assignment_statement(
        mutable_statements[1],
        context,
        mutable_session,
        diagnostics,
        output
    ));
    auto mutable_value = orison::lowering::lower_expression(
        mutable_statements[2].expression,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        mutable_session,
        output
    );
    assert(mutable_value.has_value());
    assert(mutable_value->value == "%tmp2");
    assert(mutable_state.source_type_names.at("total") == "UInt32");
    assert(
        output.str() ==
        "  %total.addr = alloca i32\n"
        "  store i32 %input, ptr %total.addr\n"
        "  %tmp0 = load i32, ptr %total.addr\n"
        "  %tmp1 = add i32 %tmp0, 1\n"
        "  store i32 %tmp1, ptr %total.addr\n"
        "  %tmp2 = load i32, ptr %total.addr\n"
    );

    auto call_state = orison::lowering::FunctionLoweringState {};
    call_state.local_name_counts["input"] = 1;
    call_state.immutable_bindings.emplace("input", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%input",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    auto call_failures = orison::lowering::LoweringFailures {};
    auto call_session = orison::lowering::FunctionLoweringSession {
        .state = call_state,
        .failures = call_failures,
    };
    auto const& call_statement = parse_result.module.functions[4].body_statements[0];
    diagnostics = {};
    output = {};
    assert(orison::lowering::lower_call_statement(
        call_statement,
        context,
        call_session,
        diagnostics,
        output
    ));
    assert(!diagnostics.has_errors());
    assert(output.str() == "  %tmp0 = call i32 @observe(i32 %input)\n");

    auto const& unit_call_statement = parse_result.module.functions[4].body_statements[1];
    diagnostics = {};
    output = {};
    assert(orison::lowering::lower_call_statement(
        unit_call_statement,
        context,
        call_session,
        diagnostics,
        output
    ));
    assert(!diagnostics.has_errors());
    assert(output.str() == "  call void @observe_unit(i32 %input)\n");

    auto unit_arity_mismatch = orison::syntax::StatementSyntax {};
    unit_arity_mismatch.kind = orison::syntax::StatementKind::expression_statement;
    unit_arity_mismatch.line = 6;
    unit_arity_mismatch.expression.kind = orison::syntax::ExpressionKind::call;
    unit_arity_mismatch.expression.left = std::make_unique<orison::syntax::ExpressionSyntax>();
    unit_arity_mismatch.expression.left->kind = orison::syntax::ExpressionKind::name;
    unit_arity_mismatch.expression.left->text = "observe_unit";
    call_failures = {};
    diagnostics = {};
    output = {};
    assert(!orison::lowering::lower_call_statement(
        unit_arity_mismatch,
        context,
        call_session,
        diagnostics,
        output
    ));
    assert(
        diagnostics.entries().front().message ==
        "lowering call statement failed: call arity mismatch: observe_unit expects 1 arguments, got 0"
    );
    assert(output.str().empty());

    auto unit_bad_argument = orison::syntax::StatementSyntax {};
    unit_bad_argument.kind = orison::syntax::StatementKind::expression_statement;
    unit_bad_argument.line = 7;
    unit_bad_argument.expression.kind = orison::syntax::ExpressionKind::call;
    unit_bad_argument.expression.left = std::make_unique<orison::syntax::ExpressionSyntax>();
    unit_bad_argument.expression.left->kind = orison::syntax::ExpressionKind::name;
    unit_bad_argument.expression.left->text = "observe_unit";
    unit_bad_argument.expression.arguments.push_back(orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "missing",
    });
    call_failures = {};
    diagnostics = {};
    output = {};
    assert(!orison::lowering::lower_call_statement(
        unit_bad_argument,
        context,
        call_session,
        diagnostics,
        output
    ));
    assert(
        diagnostics.entries().front().message ==
        "lowering call statement failed: unknown lowered name: missing"
    );
    assert(output.str().empty());

    auto immutable_assignment = orison::syntax::StatementSyntax {};
    immutable_assignment.kind = orison::syntax::StatementKind::assignment_statement;
    immutable_assignment.assignment_target.kind = orison::syntax::ExpressionKind::name;
    immutable_assignment.assignment_target.text = "input";
    diagnostics = {};
    output = {};
    assert(!orison::lowering::lower_assignment_statement(
        immutable_assignment,
        context,
        mutable_session,
        diagnostics,
        output
    ));
    assert(
        diagnostics.entries().front().message ==
        "lowering assignment target is not a mutable local"
    );
    assert(output.str().empty());

    auto block_state = orison::lowering::FunctionLoweringState {};
    block_state.local_name_counts["input"] = 1;
    block_state.immutable_bindings.emplace("input", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%input",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    auto block_failures = orison::lowering::LoweringFailures {};
    auto block_session = orison::lowering::FunctionLoweringSession {
        .state = block_state,
        .failures = block_failures,
    };
    diagnostics = {};
    output = {};
    auto block_value = orison::lowering::lower_value_statement_block(
        statements,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        block_session,
        diagnostics,
        output,
        reject_control_flow
    );
    assert(block_value.has_value());
    assert(block_value->value == "%value");
    assert(output.str() == "  %tmp0 = add i32 %input, 1\n  %value = add i32 0, %tmp0\n");

    auto pointer_owned_statements =
        std::vector<std::unique_ptr<orison::syntax::StatementSyntax>> {};
    auto pointer_owned_value = std::make_unique<orison::syntax::StatementSyntax>();
    pointer_owned_value->kind = orison::syntax::StatementKind::expression_statement;
    pointer_owned_value->expression.kind = orison::syntax::ExpressionKind::name;
    pointer_owned_value->expression.text = "input";
    pointer_owned_statements.push_back(std::move(pointer_owned_value));
    output = {};
    auto pointer_block_value = orison::lowering::lower_value_statement_block(
        pointer_owned_statements,
        "i32",
        orison::lowering::IntegerSignedness::unsigned_integer,
        context,
        block_session,
        diagnostics,
        output,
        reject_control_flow
    );
    assert(pointer_block_value.has_value());
    assert(pointer_block_value->value == "%input");
    assert(output.str().empty());

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

    auto non_call_statement = orison::syntax::StatementSyntax {};
    non_call_statement.kind = orison::syntax::StatementKind::expression_statement;
    non_call_statement.line = 5;
    non_call_statement.expression.kind = orison::syntax::ExpressionKind::name;
    non_call_statement.expression.text = "input";
    diagnostics = {};
    output = {};
    assert(!orison::lowering::lower_call_statement(
        non_call_statement,
        context,
        call_session,
        diagnostics,
        output
    ));
    assert(
        diagnostics.entries().front().message ==
        "lowering call statement requires a call expression"
    );

    auto unsupported_member_call_statement = orison::syntax::StatementSyntax {};
    unsupported_member_call_statement.kind = orison::syntax::StatementKind::expression_statement;
    unsupported_member_call_statement.line = 8;
    unsupported_member_call_statement.expression.kind = orison::syntax::ExpressionKind::call;
    unsupported_member_call_statement.expression.left = std::make_unique<orison::syntax::ExpressionSyntax>();
    unsupported_member_call_statement.expression.left->kind = orison::syntax::ExpressionKind::member_access;
    unsupported_member_call_statement.expression.left->text = "observe";
    diagnostics = {};
    output = {};
    assert(!orison::lowering::lower_call_statement(
        unsupported_member_call_statement,
        context,
        call_session,
        diagnostics,
        output
    ));
    assert(
        diagnostics.entries().front().message ==
        "lowering member call statement has unsupported receiver shape"
    );
    assert(output.str().empty());

    auto member_call_statement = orison::syntax::StatementSyntax {};
    member_call_statement.kind = orison::syntax::StatementKind::expression_statement;
    member_call_statement.line = 8;
    member_call_statement.expression.kind = orison::syntax::ExpressionKind::call;
    member_call_statement.expression.left = std::make_unique<orison::syntax::ExpressionSyntax>();
    member_call_statement.expression.left->kind = orison::syntax::ExpressionKind::member_access;
    member_call_statement.expression.left->text = "observe";
    member_call_statement.expression.left->left = std::make_unique<orison::syntax::ExpressionSyntax>();
    member_call_statement.expression.left->left->kind = orison::syntax::ExpressionKind::name;
    member_call_statement.expression.left->left->text = "receiver";
    diagnostics = {};
    output = {};
    assert(!orison::lowering::lower_call_statement(
        member_call_statement,
        context,
        call_session,
        diagnostics,
        output
    ));
    assert(
        diagnostics.entries().front().message ==
        "lowering member call receiver type is unknown"
    );
    assert(output.str().empty());

    call_state.source_type_names["receiver"] = "Device";
    diagnostics = {};
    output = {};
    assert(!orison::lowering::lower_call_statement(
        member_call_statement,
        context,
        call_session,
        diagnostics,
        output
    ));
    assert(
        diagnostics.entries().front().message ==
        "lowering member call target is unknown: Device.observe"
    );
    assert(output.str().empty());

    call_state.immutable_bindings.emplace("device", orison::lowering::LoweredExpression {
        .type = "i32",
        .value = "%device",
        .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
    });
    call_state.source_type_names["device"] = "UInt32";
    lowering.methods.push_back(orison::lowering::LoweredMethodSignature {
        .receiver_type_name = "UInt32",
        .method_name = "reset",
        .signature = orison::lowering::LoweredFunctionSignature {
            .return_type = "void",
            .parameter_types = {"i32"},
            .parameter_signedness = {orison::lowering::IntegerSignedness::unsigned_integer},
            .symbol_name = "method.UInt32.reset",
        },
    });
    auto direct_member_call_statement = orison::syntax::StatementSyntax {};
    direct_member_call_statement.kind = orison::syntax::StatementKind::expression_statement;
    direct_member_call_statement.line = 9;
    direct_member_call_statement.expression.kind = orison::syntax::ExpressionKind::call;
    direct_member_call_statement.expression.left =
        std::make_unique<orison::syntax::ExpressionSyntax>();
    direct_member_call_statement.expression.left->kind =
        orison::syntax::ExpressionKind::member_access;
    direct_member_call_statement.expression.left->text = "reset";
    direct_member_call_statement.expression.left->left =
        std::make_unique<orison::syntax::ExpressionSyntax>();
    direct_member_call_statement.expression.left->left->kind =
        orison::syntax::ExpressionKind::name;
    direct_member_call_statement.expression.left->left->text = "device";
    diagnostics = {};
    output = {};
    assert(orison::lowering::lower_call_statement(
        direct_member_call_statement,
        context,
        call_session,
        diagnostics,
        output
    ));
    assert(!diagnostics.has_errors());
    assert(output.str() == "  call void @method.UInt32.reset(i32 %device)\n");

    auto null_safe_member_call_statement = orison::syntax::StatementSyntax {};
    null_safe_member_call_statement.kind = orison::syntax::StatementKind::expression_statement;
    null_safe_member_call_statement.line = 10;
    null_safe_member_call_statement.expression.kind = orison::syntax::ExpressionKind::call;
    null_safe_member_call_statement.expression.left =
        std::make_unique<orison::syntax::ExpressionSyntax>();
    null_safe_member_call_statement.expression.left->kind =
        orison::syntax::ExpressionKind::null_safe_member_access;
    null_safe_member_call_statement.expression.left->text = "observe";
    null_safe_member_call_statement.expression.left->left =
        std::make_unique<orison::syntax::ExpressionSyntax>();
    null_safe_member_call_statement.expression.left->left->kind =
        orison::syntax::ExpressionKind::name;
    null_safe_member_call_statement.expression.left->left->text = "receiver";
    diagnostics = {};
    output = {};
    assert(!orison::lowering::lower_call_statement(
        null_safe_member_call_statement,
        context,
        call_session,
        diagnostics,
        output
    ));
    assert(
        diagnostics.entries().front().message ==
        "lowering null-safe member call statements is not yet supported"
    );
    assert(output.str().empty());

    lowering.methods.push_back(orison::lowering::LoweredMethodSignature {
        .receiver_type_name = "Device",
        .method_name = "observe",
        .signature = orison::lowering::LoweredFunctionSignature {
            .return_type = "void",
            .parameter_types = {"i32"},
            .parameter_signedness = {orison::lowering::IntegerSignedness::unsigned_integer},
            .symbol_name = "Device.observe",
        },
    });

    lowering.methods.push_back(orison::lowering::LoweredMethodSignature {
        .receiver_type_name = "Device",
        .method_name = "observe",
        .signature = orison::lowering::LoweredFunctionSignature {
            .return_type = "void",
            .parameter_types = {"i32"},
            .parameter_signedness = {orison::lowering::IntegerSignedness::unsigned_integer},
            .symbol_name = "Device.observe.duplicate",
        },
    });
    diagnostics = {};
    output = {};
    assert(!orison::lowering::lower_call_statement(
        member_call_statement,
        context,
        call_session,
        diagnostics,
        output
    ));
    assert(
        diagnostics.entries().front().message ==
        "lowering member call target is ambiguous: Device.observe"
    );
    assert(output.str().empty());

    auto loop_state = orison::lowering::FunctionLoweringState {};
    loop_state.loop_targets.push_back(orison::lowering::LoopTargets {
        .break_target = "while.exit.0",
        .continue_target = "while.condition.0",
    });
    auto loop_failures = orison::lowering::LoweringFailures {};
    auto loop_session = orison::lowering::FunctionLoweringSession {
        .state = loop_state,
        .failures = loop_failures,
    };
    auto break_statement = orison::syntax::StatementSyntax {};
    break_statement.kind = orison::syntax::StatementKind::break_statement;
    diagnostics = {};
    output = {};
    assert(orison::lowering::lower_loop_control_statement(
        break_statement,
        context,
        loop_session,
        diagnostics,
        output
    ));
    assert(output.str() == "  br label %while.exit.0\n");

    auto continue_statement = orison::syntax::StatementSyntax {};
    continue_statement.kind = orison::syntax::StatementKind::continue_statement;
    diagnostics = {};
    output = {};
    assert(orison::lowering::lower_loop_control_statement(
        continue_statement,
        context,
        loop_session,
        diagnostics,
        output
    ));
    assert(output.str() == "  br label %while.condition.0\n");

    std::filesystem::remove(path);
    return 0;
}
