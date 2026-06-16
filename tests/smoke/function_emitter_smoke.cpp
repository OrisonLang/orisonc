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
                  "    return\n"
                  "\n"
                  "function switch_then_return(value: UInt32, flag: Bool) -> UInt32\n"
                  "    switch flag\n"
                  "        true => return value\n"
                  "        false => observe(value)\n"
                  "    value + 2 as UInt32\n"
                  "\n"
                  "function repeat_for_unsafe_then_return(value: UInt32) -> UInt32\n"
                  "    repeat\n"
                  "        observe(1 as UInt32)\n"
                  "    while false\n"
                  "    for item in [2 as UInt32, 3 as UInt32]\n"
                  "        observe(item)\n"
                  "    unsafe\n"
                  "        observe(value)\n"
                  "    value\n"
                  "\n"
                  "unsafe function unsafe_identity(value: UInt32) -> UInt32\n"
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

    auto switch_then_return_ir = orison::lowering::emit_function(
        parse_result.module.functions[6],
        context.functions.at("switch_then_return"),
        context,
        strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    assert(switch_then_return_ir.find("switch i1") != std::string::npos);
    assert(switch_then_return_ir.find("call void @observe") != std::string::npos);
    assert(switch_then_return_ir.find("ret i32") != std::string::npos);

    auto repeat_for_unsafe_ir = orison::lowering::emit_function(
        parse_result.module.functions[7],
        context.functions.at("repeat_for_unsafe_then_return"),
        context,
        strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    assert(repeat_for_unsafe_ir.find("repeat.body") != std::string::npos);
    assert(repeat_for_unsafe_ir.find("for.iteration") != std::string::npos);
    assert(repeat_for_unsafe_ir.find("call void @observe(i32 %value)") != std::string::npos);
    assert(repeat_for_unsafe_ir.find("ret i32 %value") != std::string::npos);

    auto unsafe_identity_ir = orison::lowering::emit_function(
        parse_result.module.functions.back(),
        context.functions.at("unsafe_identity"),
        context,
        strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    assert(
        unsafe_identity_ir ==
        "define i32 @unsafe_identity(i32 %value) {\n"
        "entry:\n"
        "  ret i32 %value\n"
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

    auto defer_return_path =
        std::filesystem::temp_directory_path() / "orison_function_emitter_defer_return.or";
    {
        auto output = std::ofstream(defer_return_path);
        output << "package demo.function_emitter\n"
                  "\n"
                  "function observe(value: UInt32) -> Unit\n"
                  "    return\n"
                  "\n"
                  "function cleanup_then_return(value: UInt32, flag: Bool) -> Unit\n"
                  "    defer\n"
                  "        observe(value)\n"
                  "    if flag\n"
                  "        return\n"
                  "    return\n";
    }
    auto defer_return_source = orison::source::SourceFile::read(defer_return_path);
    assert(defer_return_source.has_value());
    auto defer_return_parser = orison::syntax::ModuleParser {};
    auto defer_return_parse = defer_return_parser.parse(*defer_return_source);
    assert(!defer_return_parse.diagnostics.has_errors());
    diagnostics = {};
    auto defer_return_context = orison::lowering::build_lowering_context(
        defer_return_parse.module,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto defer_return_strings = orison::lowering::collect_string_constants(defer_return_parse.module);
    auto defer_return_ir = orison::lowering::emit_function(
        defer_return_parse.module.functions[1],
        defer_return_context.functions.at("cleanup_then_return"),
        defer_return_context,
        defer_return_strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto first_defer_call = defer_return_ir.find("call void @observe(i32 %value)");
    assert(first_defer_call != std::string::npos);
    auto second_defer_call = defer_return_ir.find(
        "call void @observe(i32 %value)",
        first_defer_call + 1
    );
    assert(second_defer_call != std::string::npos);
    auto first_ret_void = defer_return_ir.find("ret void");
    assert(first_ret_void != std::string::npos);
    auto second_ret_void = defer_return_ir.find("ret void", first_ret_void + 1);
    assert(second_ret_void != std::string::npos);
    assert(first_defer_call < first_ret_void);
    assert(second_defer_call < second_ret_void);
    std::filesystem::remove(defer_return_path);

    auto defer_break_path =
        std::filesystem::temp_directory_path() / "orison_function_emitter_defer_break.or";
    {
        auto output = std::ofstream(defer_break_path);
        output << "package demo.function_emitter\n"
                  "\n"
                  "function observe(value: UInt32) -> Unit\n"
                  "    return\n"
                  "\n"
                  "function cleanup_on_break(value: UInt32) -> Unit\n"
                  "    while value < 3 as UInt32\n"
                  "        defer\n"
                  "            observe(value)\n"
                  "        break\n"
                  "    return\n";
    }
    auto defer_break_source = orison::source::SourceFile::read(defer_break_path);
    assert(defer_break_source.has_value());
    auto defer_break_parser = orison::syntax::ModuleParser {};
    auto defer_break_parse = defer_break_parser.parse(*defer_break_source);
    assert(!defer_break_parse.diagnostics.has_errors());
    diagnostics = {};
    auto defer_break_context = orison::lowering::build_lowering_context(
        defer_break_parse.module,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto defer_break_strings = orison::lowering::collect_string_constants(defer_break_parse.module);
    auto defer_break_ir = orison::lowering::emit_function(
        defer_break_parse.module.functions[1],
        defer_break_context.functions.at("cleanup_on_break"),
        defer_break_context,
        defer_break_strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto break_call = defer_break_ir.find("call void @observe(i32 %value)");
    assert(break_call != std::string::npos);
    auto break_exit = defer_break_ir.find("br label %while.exit.0");
    assert(break_exit != std::string::npos);
    assert(break_call < break_exit);
    std::filesystem::remove(defer_break_path);

    auto defer_nested_path =
        std::filesystem::temp_directory_path() / "orison_function_emitter_defer_nested.or";
    {
        auto output = std::ofstream(defer_nested_path);
        output << "package demo.function_emitter\n"
                  "\n"
                  "function observe(value: UInt32) -> Unit\n"
                  "    return\n"
                  "\n"
                  "function cleanup_then_cleanup(value: UInt32) -> Unit\n"
                  "    defer\n"
                  "        observe(1 as UInt32)\n"
                  "    defer\n"
                  "        observe(2 as UInt32)\n"
                  "        defer\n"
                  "            observe(3 as UInt32)\n"
                  "    return\n";
    }
    auto defer_nested_source = orison::source::SourceFile::read(defer_nested_path);
    assert(defer_nested_source.has_value());
    auto defer_nested_parser = orison::syntax::ModuleParser {};
    auto defer_nested_parse = defer_nested_parser.parse(*defer_nested_source);
    assert(!defer_nested_parse.diagnostics.has_errors());
    diagnostics = {};
    auto defer_nested_context = orison::lowering::build_lowering_context(
        defer_nested_parse.module,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto defer_nested_strings = orison::lowering::collect_string_constants(defer_nested_parse.module);
    auto defer_nested_ir = orison::lowering::emit_function(
        defer_nested_parse.module.functions[1],
        defer_nested_context.functions.at("cleanup_then_cleanup"),
        defer_nested_context,
        defer_nested_strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto nested_function_pos = defer_nested_ir.find("define void @cleanup_then_cleanup");
    assert(nested_function_pos != std::string::npos);
    auto nested_function_ir = defer_nested_ir.substr(nested_function_pos);
    auto nested_first_call = nested_function_ir.find("call void @observe(i32 2)");
    assert(nested_first_call != std::string::npos);
    auto nested_second_call = nested_function_ir.find(
        "call void @observe(i32 3)",
        nested_first_call + 1
    );
    assert(nested_second_call != std::string::npos);
    auto nested_third_call = nested_function_ir.find(
        "call void @observe(i32 1)",
        nested_second_call + 1
    );
    assert(nested_third_call != std::string::npos);
    auto nested_ret_void = nested_function_ir.find("ret void");
    assert(nested_ret_void != std::string::npos);
    assert(nested_first_call < nested_second_call);
    assert(nested_second_call < nested_third_call);
    assert(nested_third_call < nested_ret_void);
    std::filesystem::remove(defer_nested_path);

    auto defer_nested_multi_path =
        std::filesystem::temp_directory_path() / "orison_function_emitter_defer_nested_multi.or";
    {
        auto output = std::ofstream(defer_nested_multi_path);
        output << "package demo.function_emitter\n"
                  "\n"
                  "function observe(value: UInt32) -> Unit\n"
                  "    return\n"
                  "\n"
                  "function cleanup_then_cleanup(value: UInt32) -> Unit\n"
                  "    defer\n"
                  "        observe(1 as UInt32)\n"
                  "    defer\n"
                  "        observe(2 as UInt32)\n"
                  "        defer\n"
                  "            observe(3 as UInt32)\n"
                  "        defer\n"
                  "            observe(4 as UInt32)\n"
                  "    return\n";
    }
    auto defer_nested_multi_source = orison::source::SourceFile::read(defer_nested_multi_path);
    assert(defer_nested_multi_source.has_value());
    auto defer_nested_multi_parser = orison::syntax::ModuleParser {};
    auto defer_nested_multi_parse = defer_nested_multi_parser.parse(*defer_nested_multi_source);
    assert(!defer_nested_multi_parse.diagnostics.has_errors());
    diagnostics = {};
    auto defer_nested_multi_context = orison::lowering::build_lowering_context(
        defer_nested_multi_parse.module,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto defer_nested_multi_strings =
        orison::lowering::collect_string_constants(defer_nested_multi_parse.module);
    auto defer_nested_multi_ir = orison::lowering::emit_function(
        defer_nested_multi_parse.module.functions[1],
        defer_nested_multi_context.functions.at("cleanup_then_cleanup"),
        defer_nested_multi_context,
        defer_nested_multi_strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto nested_multi_function_pos =
        defer_nested_multi_ir.find("define void @cleanup_then_cleanup");
    assert(nested_multi_function_pos != std::string::npos);
    auto nested_multi_function_ir = defer_nested_multi_ir.substr(nested_multi_function_pos);
    auto nested_multi_first_call = nested_multi_function_ir.find("call void @observe(i32 2)");
    assert(nested_multi_first_call != std::string::npos);
    auto nested_multi_second_call = nested_multi_function_ir.find(
        "call void @observe(i32 4)",
        nested_multi_first_call + 1
    );
    assert(nested_multi_second_call != std::string::npos);
    auto nested_multi_third_call = nested_multi_function_ir.find(
        "call void @observe(i32 3)",
        nested_multi_second_call + 1
    );
    assert(nested_multi_third_call != std::string::npos);
    auto nested_multi_fourth_call = nested_multi_function_ir.find(
        "call void @observe(i32 1)",
        nested_multi_third_call + 1
    );
    assert(nested_multi_fourth_call != std::string::npos);
    auto nested_multi_ret_void = nested_multi_function_ir.find("ret void");
    assert(nested_multi_ret_void != std::string::npos);
    assert(nested_multi_first_call < nested_multi_second_call);
    assert(nested_multi_second_call < nested_multi_third_call);
    assert(nested_multi_third_call < nested_multi_fourth_call);
    assert(nested_multi_fourth_call < nested_multi_ret_void);
    std::filesystem::remove(defer_nested_multi_path);

    auto defer_loop_control_path =
        std::filesystem::temp_directory_path() / "orison_function_emitter_defer_loop_control.or";
    {
        auto output = std::ofstream(defer_loop_control_path);
        output << "package demo.function_emitter\n"
                  "\n"
                  "function observe(value: UInt32) -> Unit\n"
                  "    return\n"
                  "\n"
                  "function cleanup_on_break(value: UInt32) -> Unit\n"
                  "    while value < 3 as UInt32\n"
                  "        defer\n"
                  "            observe(1 as UInt32)\n"
                  "        defer\n"
                  "            observe(2 as UInt32)\n"
                  "            defer\n"
                  "                observe(3 as UInt32)\n"
                  "        break\n"
                  "    return\n"
                  "\n"
                  "function cleanup_on_continue(value: UInt32) -> Unit\n"
                  "    while value < 3 as UInt32\n"
                  "        defer\n"
                  "            observe(1 as UInt32)\n"
                  "        defer\n"
                  "            observe(2 as UInt32)\n"
                  "            defer\n"
                  "                observe(3 as UInt32)\n"
                  "        continue\n"
                  "    return\n";
    }
    auto defer_loop_control_source = orison::source::SourceFile::read(defer_loop_control_path);
    assert(defer_loop_control_source.has_value());
    auto defer_loop_control_parser = orison::syntax::ModuleParser {};
    auto defer_loop_control_parse = defer_loop_control_parser.parse(*defer_loop_control_source);
    assert(!defer_loop_control_parse.diagnostics.has_errors());
    diagnostics = {};
    auto defer_loop_control_context = orison::lowering::build_lowering_context(
        defer_loop_control_parse.module,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto defer_loop_control_strings =
        orison::lowering::collect_string_constants(defer_loop_control_parse.module);

    auto loop_control_break_ir = orison::lowering::emit_function(
        defer_loop_control_parse.module.functions[1],
        defer_loop_control_context.functions.at("cleanup_on_break"),
        defer_loop_control_context,
        defer_loop_control_strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto loop_control_break_function_pos = loop_control_break_ir.find("define void @cleanup_on_break");
    assert(loop_control_break_function_pos != std::string::npos);
    auto loop_control_break_function_ir = loop_control_break_ir.substr(loop_control_break_function_pos);
    auto loop_control_break_first_call = loop_control_break_function_ir.find("call void @observe(i32 2)");
    assert(loop_control_break_first_call != std::string::npos);
    auto loop_control_break_second_call = loop_control_break_function_ir.find(
        "call void @observe(i32 3)",
        loop_control_break_first_call + 1
    );
    assert(loop_control_break_second_call != std::string::npos);
    auto loop_control_break_third_call = loop_control_break_function_ir.find(
        "call void @observe(i32 1)",
        loop_control_break_second_call + 1
    );
    assert(loop_control_break_third_call != std::string::npos);
    auto loop_control_break_exit = loop_control_break_function_ir.find("br label %while.exit.0");
    assert(loop_control_break_exit != std::string::npos);
    assert(loop_control_break_first_call < loop_control_break_second_call);
    assert(loop_control_break_second_call < loop_control_break_third_call);
    assert(loop_control_break_third_call < loop_control_break_exit);

    diagnostics = {};
    auto loop_control_continue_ir = orison::lowering::emit_function(
        defer_loop_control_parse.module.functions[2],
        defer_loop_control_context.functions.at("cleanup_on_continue"),
        defer_loop_control_context,
        defer_loop_control_strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto loop_control_continue_function_pos =
        loop_control_continue_ir.find("define void @cleanup_on_continue");
    assert(loop_control_continue_function_pos != std::string::npos);
    auto loop_control_continue_function_ir =
        loop_control_continue_ir.substr(loop_control_continue_function_pos);
    auto loop_control_continue_first_call =
        loop_control_continue_function_ir.find("call void @observe(i32 2)");
    assert(loop_control_continue_first_call != std::string::npos);
    auto loop_control_continue_second_call = loop_control_continue_function_ir.find(
        "call void @observe(i32 3)",
        loop_control_continue_first_call + 1
    );
    assert(loop_control_continue_second_call != std::string::npos);
    auto loop_control_continue_third_call = loop_control_continue_function_ir.find(
        "call void @observe(i32 1)",
        loop_control_continue_second_call + 1
    );
    assert(loop_control_continue_third_call != std::string::npos);
    auto loop_control_continue_exit = loop_control_continue_function_ir.find(
        "br label %while.condition.0",
        loop_control_continue_third_call + 1
    );
    assert(loop_control_continue_exit != std::string::npos);
    assert(loop_control_continue_first_call < loop_control_continue_second_call);
    assert(loop_control_continue_second_call < loop_control_continue_third_call);
    assert(loop_control_continue_third_call < loop_control_continue_exit);
    std::filesystem::remove(defer_loop_control_path);

    auto defer_for_repeat_path =
        std::filesystem::temp_directory_path() / "orison_function_emitter_defer_for_repeat.or";
    {
        auto output = std::ofstream(defer_for_repeat_path);
        output << "package demo.function_emitter\n"
                  "\n"
                  "function observe(value: UInt32) -> Unit\n"
                  "    return\n"
                  "\n"
                  "function cleanup_on_for_continue(value: UInt32) -> Unit\n"
                  "    for item in [0 as UInt32, 1 as UInt32, 2 as UInt32]\n"
                  "        defer\n"
                  "            observe(1 as UInt32)\n"
                  "        defer\n"
                  "            observe(2 as UInt32)\n"
                  "            defer\n"
                  "                observe(3 as UInt32)\n"
                  "        continue\n"
                  "    return\n"
                  "\n"
                  "function cleanup_on_repeat_break(value: UInt32) -> Unit\n"
                  "    repeat\n"
                  "        defer\n"
                  "            observe(1 as UInt32)\n"
                  "        defer\n"
                  "            observe(2 as UInt32)\n"
                  "            defer\n"
                  "                observe(3 as UInt32)\n"
                  "        break\n"
                  "    while value < 3 as UInt32\n"
                  "    return\n";
    }
    auto defer_for_repeat_source = orison::source::SourceFile::read(defer_for_repeat_path);
    assert(defer_for_repeat_source.has_value());
    auto defer_for_repeat_parser = orison::syntax::ModuleParser {};
    auto defer_for_repeat_parse = defer_for_repeat_parser.parse(*defer_for_repeat_source);
    assert(!defer_for_repeat_parse.diagnostics.has_errors());
    diagnostics = {};
    auto defer_for_repeat_context = orison::lowering::build_lowering_context(
        defer_for_repeat_parse.module,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto defer_for_repeat_strings =
        orison::lowering::collect_string_constants(defer_for_repeat_parse.module);

    auto for_continue_ir = orison::lowering::emit_function(
        defer_for_repeat_parse.module.functions[1],
        defer_for_repeat_context.functions.at("cleanup_on_for_continue"),
        defer_for_repeat_context,
        defer_for_repeat_strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto for_function_pos = for_continue_ir.find("define void @cleanup_on_for_continue");
    assert(for_function_pos != std::string::npos);
    auto for_function_ir = for_continue_ir.substr(for_function_pos);
    auto for_first_call = for_function_ir.find("call void @observe(i32 2)");
    assert(for_first_call != std::string::npos);
    auto for_second_call = for_function_ir.find("call void @observe(i32 3)", for_first_call + 1);
    assert(for_second_call != std::string::npos);
    auto for_third_call = for_function_ir.find("call void @observe(i32 1)", for_second_call + 1);
    assert(for_third_call != std::string::npos);
    auto for_continue_exit = for_function_ir.find("br label %for.iteration.0.1");
    assert(for_continue_exit != std::string::npos);
    assert(for_first_call < for_second_call);
    assert(for_second_call < for_third_call);
    assert(for_third_call < for_continue_exit);

    diagnostics = {};
    auto repeat_break_ir = orison::lowering::emit_function(
        defer_for_repeat_parse.module.functions[2],
        defer_for_repeat_context.functions.at("cleanup_on_repeat_break"),
        defer_for_repeat_context,
        defer_for_repeat_strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto repeat_function_pos = repeat_break_ir.find("define void @cleanup_on_repeat_break");
    assert(repeat_function_pos != std::string::npos);
    auto repeat_function_ir = repeat_break_ir.substr(repeat_function_pos);
    auto repeat_first_call = repeat_function_ir.find("call void @observe(i32 2)");
    assert(repeat_first_call != std::string::npos);
    auto repeat_second_call = repeat_function_ir.find(
        "call void @observe(i32 3)",
        repeat_first_call + 1
    );
    assert(repeat_second_call != std::string::npos);
    auto repeat_third_call = repeat_function_ir.find(
        "call void @observe(i32 1)",
        repeat_second_call + 1
    );
    assert(repeat_third_call != std::string::npos);
    auto repeat_exit = repeat_function_ir.find("br label %repeat.exit.0");
    assert(repeat_exit != std::string::npos);
    assert(repeat_first_call < repeat_second_call);
    assert(repeat_second_call < repeat_third_call);
    assert(repeat_third_call < repeat_exit);
    std::filesystem::remove(defer_for_repeat_path);

    auto defer_unsafe_loop_control_path = std::filesystem::temp_directory_path() /
        "orison_function_emitter_defer_unsafe_loop_control.or";
    {
        auto output = std::ofstream(defer_unsafe_loop_control_path);
        output << "package demo.function_emitter\n"
                  "\n"
                  "function observe(value: UInt32) -> Unit\n"
                  "    return\n"
                  "\n"
                  "function cleanup_on_unsafe_break(value: UInt32) -> Unit\n"
                  "    while value < 3 as UInt32\n"
                  "        unsafe\n"
                  "            defer\n"
                  "                observe(1 as UInt32)\n"
                  "            defer\n"
                  "                observe(2 as UInt32)\n"
                  "                defer\n"
                  "                    observe(3 as UInt32)\n"
                  "            break\n"
                  "    return\n"
                  "\n"
                  "function cleanup_on_unsafe_continue(value: UInt32) -> Unit\n"
                  "    while value < 3 as UInt32\n"
                  "        unsafe\n"
                  "            defer\n"
                  "                observe(1 as UInt32)\n"
                  "            defer\n"
                  "                observe(2 as UInt32)\n"
                  "                defer\n"
                  "                    observe(3 as UInt32)\n"
                  "            continue\n"
                  "    return\n";
    }
    auto defer_unsafe_loop_control_source =
        orison::source::SourceFile::read(defer_unsafe_loop_control_path);
    assert(defer_unsafe_loop_control_source.has_value());
    auto defer_unsafe_loop_control_parser = orison::syntax::ModuleParser {};
    auto defer_unsafe_loop_control_parse =
        defer_unsafe_loop_control_parser.parse(*defer_unsafe_loop_control_source);
    assert(!defer_unsafe_loop_control_parse.diagnostics.has_errors());
    diagnostics = {};
    auto defer_unsafe_loop_control_context = orison::lowering::build_lowering_context(
        defer_unsafe_loop_control_parse.module,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto defer_unsafe_loop_control_strings =
        orison::lowering::collect_string_constants(defer_unsafe_loop_control_parse.module);

    auto unsafe_break_ir = orison::lowering::emit_function(
        defer_unsafe_loop_control_parse.module.functions[1],
        defer_unsafe_loop_control_context.functions.at("cleanup_on_unsafe_break"),
        defer_unsafe_loop_control_context,
        defer_unsafe_loop_control_strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto unsafe_break_function_pos = unsafe_break_ir.find("define void @cleanup_on_unsafe_break");
    assert(unsafe_break_function_pos != std::string::npos);
    auto unsafe_break_function_ir = unsafe_break_ir.substr(unsafe_break_function_pos);
    auto unsafe_break_first_call = unsafe_break_function_ir.find("call void @observe(i32 2)");
    assert(unsafe_break_first_call != std::string::npos);
    auto unsafe_break_second_call = unsafe_break_function_ir.find(
        "call void @observe(i32 3)",
        unsafe_break_first_call + 1
    );
    assert(unsafe_break_second_call != std::string::npos);
    auto unsafe_break_third_call = unsafe_break_function_ir.find(
        "call void @observe(i32 1)",
        unsafe_break_second_call + 1
    );
    assert(unsafe_break_third_call != std::string::npos);
    auto unsafe_break_exit = unsafe_break_function_ir.find("br label %while.exit.0");
    assert(unsafe_break_exit != std::string::npos);
    assert(unsafe_break_first_call < unsafe_break_second_call);
    assert(unsafe_break_second_call < unsafe_break_third_call);
    assert(unsafe_break_third_call < unsafe_break_exit);

    diagnostics = {};
    auto unsafe_continue_ir = orison::lowering::emit_function(
        defer_unsafe_loop_control_parse.module.functions[2],
        defer_unsafe_loop_control_context.functions.at("cleanup_on_unsafe_continue"),
        defer_unsafe_loop_control_context,
        defer_unsafe_loop_control_strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto unsafe_continue_function_pos =
        unsafe_continue_ir.find("define void @cleanup_on_unsafe_continue");
    assert(unsafe_continue_function_pos != std::string::npos);
    auto unsafe_continue_function_ir =
        unsafe_continue_ir.substr(unsafe_continue_function_pos);
    auto unsafe_continue_first_call = unsafe_continue_function_ir.find("call void @observe(i32 2)");
    assert(unsafe_continue_first_call != std::string::npos);
    auto unsafe_continue_second_call = unsafe_continue_function_ir.find(
        "call void @observe(i32 3)",
        unsafe_continue_first_call + 1
    );
    assert(unsafe_continue_second_call != std::string::npos);
    auto unsafe_continue_third_call = unsafe_continue_function_ir.find(
        "call void @observe(i32 1)",
        unsafe_continue_second_call + 1
    );
    assert(unsafe_continue_third_call != std::string::npos);
    auto unsafe_continue_exit = unsafe_continue_function_ir.find(
        "br label %while.condition.0",
        unsafe_continue_third_call + 1
    );
    assert(unsafe_continue_exit != std::string::npos);
    assert(unsafe_continue_first_call < unsafe_continue_second_call);
    assert(unsafe_continue_second_call < unsafe_continue_third_call);
    assert(unsafe_continue_third_call < unsafe_continue_exit);
    std::filesystem::remove(defer_unsafe_loop_control_path);

    auto defer_unsafe_for_repeat_path = std::filesystem::temp_directory_path() /
        "orison_function_emitter_defer_unsafe_for_repeat.or";
    {
        auto output = std::ofstream(defer_unsafe_for_repeat_path);
        output << "package demo.function_emitter\n"
                  "\n"
                  "function observe(value: UInt32) -> Unit\n"
                  "    return\n"
                  "\n"
                  "function cleanup_on_for_unsafe_continue(value: UInt32) -> Unit\n"
                  "    for item in [0 as UInt32, 1 as UInt32, 2 as UInt32]\n"
                  "        unsafe\n"
                  "            defer\n"
                  "                observe(1 as UInt32)\n"
                  "            defer\n"
                  "                observe(2 as UInt32)\n"
                  "                defer\n"
                  "                    observe(3 as UInt32)\n"
                  "            continue\n"
                  "    return\n"
                  "\n"
                  "function cleanup_on_repeat_unsafe_break(value: UInt32) -> Unit\n"
                  "    repeat\n"
                  "        unsafe\n"
                  "            defer\n"
                  "                observe(1 as UInt32)\n"
                  "            defer\n"
                  "                observe(2 as UInt32)\n"
                  "                defer\n"
                  "                    observe(3 as UInt32)\n"
                  "            break\n"
                  "    while value < 3 as UInt32\n"
                  "    return\n";
    }
    auto defer_unsafe_for_repeat_source = orison::source::SourceFile::read(
        defer_unsafe_for_repeat_path
    );
    assert(defer_unsafe_for_repeat_source.has_value());
    auto defer_unsafe_for_repeat_parser = orison::syntax::ModuleParser {};
    auto defer_unsafe_for_repeat_parse = defer_unsafe_for_repeat_parser.parse(
        *defer_unsafe_for_repeat_source
    );
    assert(!defer_unsafe_for_repeat_parse.diagnostics.has_errors());
    diagnostics = {};
    auto defer_unsafe_for_repeat_context = orison::lowering::build_lowering_context(
        defer_unsafe_for_repeat_parse.module,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto defer_unsafe_for_repeat_strings =
        orison::lowering::collect_string_constants(defer_unsafe_for_repeat_parse.module);

    auto unsafe_for_continue_ir = orison::lowering::emit_function(
        defer_unsafe_for_repeat_parse.module.functions[1],
        defer_unsafe_for_repeat_context.functions.at("cleanup_on_for_unsafe_continue"),
        defer_unsafe_for_repeat_context,
        defer_unsafe_for_repeat_strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto unsafe_for_function_pos = unsafe_for_continue_ir.find(
        "define void @cleanup_on_for_unsafe_continue"
    );
    assert(unsafe_for_function_pos != std::string::npos);
    auto unsafe_for_function_ir = unsafe_for_continue_ir.substr(unsafe_for_function_pos);
    auto unsafe_for_first_call = unsafe_for_function_ir.find("call void @observe(i32 2)");
    assert(unsafe_for_first_call != std::string::npos);
    auto unsafe_for_second_call = unsafe_for_function_ir.find(
        "call void @observe(i32 3)",
        unsafe_for_first_call + 1
    );
    assert(unsafe_for_second_call != std::string::npos);
    auto unsafe_for_third_call = unsafe_for_function_ir.find(
        "call void @observe(i32 1)",
        unsafe_for_second_call + 1
    );
    assert(unsafe_for_third_call != std::string::npos);
    auto unsafe_for_exit = unsafe_for_function_ir.find("br label %for.iteration.0.1");
    assert(unsafe_for_exit != std::string::npos);
    assert(unsafe_for_first_call < unsafe_for_second_call);
    assert(unsafe_for_second_call < unsafe_for_third_call);
    assert(unsafe_for_third_call < unsafe_for_exit);

    diagnostics = {};
    auto unsafe_repeat_ir = orison::lowering::emit_function(
        defer_unsafe_for_repeat_parse.module.functions[2],
        defer_unsafe_for_repeat_context.functions.at("cleanup_on_repeat_unsafe_break"),
        defer_unsafe_for_repeat_context,
        defer_unsafe_for_repeat_strings,
        diagnostics
    );
    assert(!diagnostics.has_errors());
    auto unsafe_repeat_function_pos = unsafe_repeat_ir.find(
        "define void @cleanup_on_repeat_unsafe_break"
    );
    assert(unsafe_repeat_function_pos != std::string::npos);
    auto unsafe_repeat_function_ir = unsafe_repeat_ir.substr(unsafe_repeat_function_pos);
    auto unsafe_repeat_first_call = unsafe_repeat_function_ir.find("call void @observe(i32 2)");
    assert(unsafe_repeat_first_call != std::string::npos);
    auto unsafe_repeat_second_call = unsafe_repeat_function_ir.find(
        "call void @observe(i32 3)",
        unsafe_repeat_first_call + 1
    );
    assert(unsafe_repeat_second_call != std::string::npos);
    auto unsafe_repeat_third_call = unsafe_repeat_function_ir.find(
        "call void @observe(i32 1)",
        unsafe_repeat_second_call + 1
    );
    assert(unsafe_repeat_third_call != std::string::npos);
    auto unsafe_repeat_exit = unsafe_repeat_function_ir.find("br label %repeat.exit.0");
    assert(unsafe_repeat_exit != std::string::npos);
    assert(unsafe_repeat_first_call < unsafe_repeat_second_call);
    assert(unsafe_repeat_second_call < unsafe_repeat_third_call);
    assert(unsafe_repeat_third_call < unsafe_repeat_exit);
    std::filesystem::remove(defer_unsafe_for_repeat_path);

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
