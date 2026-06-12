#include "orison/lowering/function_emitter.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>

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
