#include "orison/lowering/function_emitter.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

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
                  "    value\n"
                  "\n"
                  "extend UInt32\n"
                  "    function reset(this: shared This) -> Unit\n"
                  "        return\n"
                  "\n"
                  "function observe(value: UInt32) -> Unit\n"
                  "    return\n"
                  "\n"
                  "function call_then_return(value: UInt32) -> UInt32\n"
                  "    observe(value)\n"
                  "    value.reset()\n"
                  "    value\n"
                  "\n"
                  "function observe_then_return(value: UInt32) -> Unit\n"
                  "    observe(value)\n"
                  "    value.reset()\n"
                  "    return\n";
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

    auto call_then_return_ir = orison::lowering::emit_function(
        parse_result.module.functions[4],
        context.functions.at("call_then_return"),
        context,
        strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    assert(
        call_then_return_ir ==
        "define i32 @call_then_return(i32 %value) {\n"
        "entry:\n"
        "  call void @observe(i32 %value)\n"
        "  call void @method.UInt32.reset(i32 %value)\n"
        "  ret i32 %value\n"
        "}\n"
    );

    auto observe_then_return_ir = orison::lowering::emit_function(
        parse_result.module.functions[5],
        context.functions.at("observe_then_return"),
        context,
        strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    assert(
        observe_then_return_ir ==
        "define void @observe_then_return(i32 %value) {\n"
        "entry:\n"
        "  call void @observe(i32 %value)\n"
        "  call void @method.UInt32.reset(i32 %value)\n"
        "  ret void\n"
        "}\n"
    );

    auto control_flow_path = std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "tour_06_control_flow.or";
    auto control_flow_source = orison::source::SourceFile::read(control_flow_path);
    assert(control_flow_source.has_value());
    auto control_flow_parser = orison::syntax::ModuleParser {};
    auto control_flow_parse = control_flow_parser.parse(*control_flow_source);
    assert(!control_flow_parse.diagnostics.has_errors());
    diagnostics = {};
    auto control_flow_context = orison::lowering::build_lowering_context(control_flow_parse.module, diagnostics);
    assert(!diagnostics.has_errors());
    auto control_flow_strings = orison::lowering::collect_string_constants(control_flow_parse.module);
    auto classify_ir = orison::lowering::emit_function(
        control_flow_parse.module.functions.front(),
        control_flow_context.functions.at("classify"),
        control_flow_context,
        control_flow_strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    assert(classify_ir.find("guard.failure.") != std::string::npos);
    assert(classify_ir.find("ret i64 0") != std::string::npos);
    assert(classify_ir.find("switch i64") != std::string::npos);

    auto control_flow_ir = orison::lowering::emit_function(
        control_flow_parse.module.functions[1],
        control_flow_context.functions.at("loops"),
        control_flow_context,
        control_flow_strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    assert(control_flow_ir.find("repeat.body") != std::string::npos);
    assert(control_flow_ir.find("for.iteration") != std::string::npos);
    assert(control_flow_ir.find("ret void") != std::string::npos);

    auto unsafe_path = std::filesystem::temp_directory_path() / "orison_function_emitter_void_unsafe.or";
    {
        auto output = std::ofstream(unsafe_path);
        output << "package demo.function_emitter\n"
                  "\n"
                  "function observe(value: UInt32) -> Unit\n"
                  "    return\n"
                  "\n"
                  "function clear(value: UInt32) -> Unit\n"
                  "    unsafe\n"
                  "        observe(value)\n"
                  "    return\n";
    }
    auto unsafe_source = orison::source::SourceFile::read(unsafe_path);
    assert(unsafe_source.has_value());
    auto unsafe_parser = orison::syntax::ModuleParser {};
    auto unsafe_parse = unsafe_parser.parse(*unsafe_source);
    assert(!unsafe_parse.diagnostics.has_errors());
    diagnostics = {};
    auto unsafe_context = orison::lowering::build_lowering_context(unsafe_parse.module, diagnostics);
    assert(!diagnostics.has_errors());
    auto unsafe_strings = orison::lowering::collect_string_constants(unsafe_parse.module);
    auto unsafe_ir = orison::lowering::emit_function(
        unsafe_parse.module.functions[1],
        unsafe_context.functions.at("clear"),
        unsafe_context,
        unsafe_strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    assert(unsafe_ir.find("call void @observe") != std::string::npos);
    assert(unsafe_ir.find("ret void") != std::string::npos);
    std::filesystem::remove(unsafe_path);

    auto branchy_path = std::filesystem::temp_directory_path() / "orison_function_emitter_void_branchy.or";
    {
        auto output = std::ofstream(branchy_path);
        output << "package demo.function_emitter\n"
                  "\n"
                  "function observe(value: UInt32) -> Unit\n"
                  "    return\n"
                  "\n"
                  "function branchy(value: UInt32) -> Unit\n"
                  "    if value == 1 as UInt32\n"
                  "        observe(value)\n"
                  "    else\n"
                  "        observe(value)\n"
                  "    switch value\n"
                  "        2 as UInt32 => observe(value)\n"
                  "        default => observe(value)\n"
                  "    return\n";
    }
    auto branchy_source = orison::source::SourceFile::read(branchy_path);
    assert(branchy_source.has_value());
    auto branchy_parser = orison::syntax::ModuleParser {};
    auto branchy_parse = branchy_parser.parse(*branchy_source);
    assert(!branchy_parse.diagnostics.has_errors());
    diagnostics = {};
    auto branchy_context = orison::lowering::build_lowering_context(branchy_parse.module, diagnostics);
    assert(!diagnostics.has_errors());
    auto branchy_strings = orison::lowering::collect_string_constants(branchy_parse.module);
    auto branchy_ir = orison::lowering::emit_function(
        branchy_parse.module.functions[1],
        branchy_context.functions.at("branchy"),
        branchy_context,
        branchy_strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    assert(branchy_ir.find("br i1") != std::string::npos);
    assert(branchy_ir.find("switch i32") != std::string::npos);
    assert(branchy_ir.find("call void @observe") != std::string::npos);
    assert(branchy_ir.find("ret void") != std::string::npos);
    std::filesystem::remove(branchy_path);

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
