#include "orison/lowering/function_emitter.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>

int main() {
    auto path = std::filesystem::temp_directory_path() / "orison_function_emitter_smoke.or";
    {
        auto output = std::ofstream(path);
        output << "package demo.function_emitter\n"
                  "\n"
                  "function add(left: UInt32, right: UInt32) -> UInt32\n"
                  "    left + right\n"
                  "\n"
                  "function increment_mutable(input: UInt32) -> UInt32\n"
                  "    var value: UInt32 = input\n"
                  "    value = value + 1 as UInt32\n"
                  "    value\n"
                  "\n"
                  "function count_to_three() -> UInt32\n"
                  "    var value = 0 as UInt32\n"
                  "    while value < 3 as UInt32\n"
                  "        value = value + 1 as UInt32\n"
                  "    value\n";
    }

    auto source = orison::source::SourceFile::read(path);
    assert(source.has_value());
    auto parser = orison::syntax::ModuleParser {};
    auto parse_result = parser.parse(*source);
    assert(!parse_result.diagnostics.has_errors());

    auto diagnostics = orison::diagnostics::DiagnosticBag {};
    auto context = orison::lowering::build_lowering_context(parse_result.module, diagnostics);
    assert(!diagnostics.has_errors());
    auto strings = orison::lowering::collect_string_constants(parse_result.module);
    auto function_ir = orison::lowering::emit_function(
        parse_result.module.functions.front(),
        context.functions.at("add"),
        context,
        strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    assert(
        function_ir ==
        "define i32 @add(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = add i32 %left, %right\n"
        "  ret i32 %tmp0\n"
        "}\n"
    );

    auto mutable_ir = orison::lowering::emit_function(
        parse_result.module.functions[1],
        context.functions.at("increment_mutable"),
        context,
        strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    assert(
        mutable_ir ==
        "define i32 @increment_mutable(i32 %input) {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 %input, ptr %value.addr\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = add i32 %tmp0, 1\n"
        "  store i32 %tmp1, ptr %value.addr\n"
        "  %tmp2 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp2\n"
        "}\n"
    );

    auto while_ir = orison::lowering::emit_function(
        parse_result.module.functions[2],
        context.functions.at("count_to_three"),
        context,
        strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    assert(
        while_ir ==
        "define i32 @count_to_three() {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 0, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.condition.0:\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = icmp ult i32 %tmp0, 3\n"
        "  br i1 %tmp1, label %while.body.0, label %while.exit.0\n"
        "while.body.0:\n"
        "  %tmp2 = load i32, ptr %value.addr\n"
        "  %tmp3 = add i32 %tmp2, 1\n"
        "  store i32 %tmp3, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.exit.0:\n"
        "  %tmp4 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp4\n"
        "}\n"
    );

    auto method = orison::syntax::FunctionSyntax {
        .name = "scale",
        .parameters = {
            orison::syntax::ParameterSyntax {
                .name = "value",
                .type = orison::syntax::TypeSyntax {.name = "UInt32"},
            },
        },
        .return_type = orison::syntax::TypeSyntax {.name = "UInt32"},
    };
    method.body_statements.push_back(orison::syntax::StatementSyntax {
        .kind = orison::syntax::StatementKind::expression_statement,
        .expression = orison::syntax::ExpressionSyntax {
            .kind = orison::syntax::ExpressionKind::binary,
            .text = "+",
            .left = std::make_unique<orison::syntax::ExpressionSyntax>(
                orison::syntax::ExpressionSyntax {
                    .kind = orison::syntax::ExpressionKind::name,
                    .text = "value",
                }
            ),
            .right = std::make_unique<orison::syntax::ExpressionSyntax>(
                orison::syntax::ExpressionSyntax {
                    .kind = orison::syntax::ExpressionKind::integer_literal,
                    .text = "1",
                }
            ),
        },
    });
    auto method_signature = orison::lowering::LoweredFunctionSignature {
        .return_type = "i32",
        .return_signedness = orison::lowering::IntegerSignedness::unsigned_integer,
        .parameter_types = {"i32"},
        .parameter_signedness = {orison::lowering::IntegerSignedness::unsigned_integer},
        .symbol_name = "method.Device.scale",
    };
    diagnostics = {};
    auto method_ir = orison::lowering::emit_function(
        method,
        method_signature,
        context,
        strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    assert(
        method_ir ==
        "define i32 @method.Device.scale(i32 %value) {\n"
        "entry:\n"
        "  %tmp0 = add i32 %value, 1\n"
        "  ret i32 %tmp0\n"
        "}\n"
    );

    parse_result.module.functions.front().is_async = true;
    diagnostics = {};
    assert(
        orison::lowering::emit_function(
            parse_result.module.functions.front(),
            context.functions.at("add"),
            context,
            strings,
            diagnostics
        ).empty()
    );
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().front().message == "lowering does not yet support async functions");

    parse_result.module.functions.front().is_async = false;
    auto unsupported_return = context.functions.at("add");
    unsupported_return.return_type.clear();
    diagnostics = {};
    assert(
        orison::lowering::emit_function(
            parse_result.module.functions.front(),
            unsupported_return,
            context,
            strings,
            diagnostics
        ).empty()
    );
    assert(
        diagnostics.entries().front().message ==
        "lowering does not yet support this function return type"
    );

    auto unsupported_parameter = context.functions.at("add");
    unsupported_parameter.parameter_types.front().clear();
    diagnostics = {};
    assert(
        orison::lowering::emit_function(
            parse_result.module.functions.front(),
            unsupported_parameter,
            context,
            strings,
            diagnostics
        ).empty()
    );
    assert(
        diagnostics.entries().front().message ==
        "lowering does not yet support this function parameter type"
    );
    std::filesystem::remove(path);
    return 0;
}
