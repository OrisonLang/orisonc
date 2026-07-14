#include "orison/lowering/llvm_ir_emitter.hpp"
#include "orison/lowering/llvm_object_emitter.hpp"
#include "orison/lowering/llvm_ir_verifier.hpp"
#include "orison/semantics/drop_model.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {

auto lower_source(
    std::filesystem::path const& path,
    std::string_view source,
    orison::lowering::LlvmIrEmissionOptions const& options = {}
)
    -> orison::lowering::LlvmIrEmissionResult {
    {
        std::ofstream output(path);
        output << source;
    }

    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    auto semantic_result = analyzer.analyze(parse_result.module);
    assert(!semantic_result.has_errors());

    orison::lowering::LlvmIrEmitter emitter;
    return emitter.emit(parse_result.module, semantic_result, options);
}

void test_emit_constant_uint32_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_constant_uint32.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    1 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  ret i32 1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_carries_semantic_drop_lowering_authorization_metadata() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_semantic_drop_authorization_metadata.or";
    auto site = orison::semantics::PlannedDropSite {
        .source_type_name = "Payload",
        .abi_symbol_name = orison::semantics::drop_abi_symbol_name("Payload"),
        .owner_name = "payload",
        .site_line = 12,
    };
    auto authorization = orison::semantics::DropLoweringAuthorization {
        .site = site,
        .semantic_resolved = true,
        .source_drop_lowering_enabled = false,
        .authorized = false,
    };
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    1 as UInt32\n",
        orison::lowering::LlvmIrEmissionOptions {
            .semantic_drop_lowering_authorizations = {authorization},
        }
    );

    assert(!result.has_errors());
    assert(result.semantic_drop_lowering_authorizations.size() == 1);
    assert(result.semantic_drop_lowering_authorizations.front().site.abi_symbol_name == "__orison_drop.Payload");
    assert(result.semantic_drop_lowering_authorizations.front().semantic_resolved);
    assert(!result.semantic_drop_lowering_authorizations.front().source_drop_lowering_enabled);
    assert(!result.semantic_drop_lowering_authorizations.front().authorized);
    assert(result.ir_text.find("__orison_drop.Payload") == std::string::npos);
}

void test_emit_let_bound_uint32_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_let_bound_uint32.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    let value = 1 as UInt32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value = add i32 0, 1\n"
        "  ret i32 %value\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_mutable_uint32_assignment_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_mutable_uint32.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 1 as UInt32\n"
        "    value = value + 2 as UInt32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 1, ptr %value.addr\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = add i32 %tmp0, 2\n"
        "  store i32 %tmp1, ptr %value.addr\n"
        "  %tmp2 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp2\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_mutable_uint32_compound_assignment_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_mutable_uint32_compound.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 6 as UInt32\n"
        "    value += 2 as UInt32\n"
        "    value *= 3 as UInt32\n"
        "    value -= 4 as UInt32\n"
        "    value /= 2 as UInt32\n"
        "    value %= 5 as UInt32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 6, ptr %value.addr\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = add i32 %tmp0, 2\n"
        "  store i32 %tmp1, ptr %value.addr\n"
        "  %tmp2 = load i32, ptr %value.addr\n"
        "  %tmp3 = mul i32 %tmp2, 3\n"
        "  store i32 %tmp3, ptr %value.addr\n"
        "  %tmp4 = load i32, ptr %value.addr\n"
        "  %tmp5 = sub i32 %tmp4, 4\n"
        "  store i32 %tmp5, ptr %value.addr\n"
        "  %tmp6 = load i32, ptr %value.addr\n"
        "  %tmp7 = udiv i32 %tmp6, 2\n"
        "  store i32 %tmp7, ptr %value.addr\n"
        "  %tmp8 = load i32, ptr %value.addr\n"
        "  %tmp9 = urem i32 %tmp8, 5\n"
        "  store i32 %tmp9, ptr %value.addr\n"
        "  %tmp10 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp10\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_negative_int32_var_initializer_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_var_initializer.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> Int32\n"
        "    var value = -27 as Int32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = sub i32 0, 27\n"
        "  %value.addr = alloca i32\n"
        "  store i32 %tmp0, ptr %value.addr\n"
        "  %tmp1 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_negative_int32_let_initializer_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_let_initializer.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> Int32\n"
        "    let value = -27 as Int32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = sub i32 0, 27\n"
        "  %value = add i32 0, %tmp0\n"
        "  ret i32 %value\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_reject_negative_uint32_cast_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_cast.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    -1 as UInt32\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not yet support this return expression: unsupported cast: negative value to UInt32"
    );
}

void test_emit_negative_int32_cast_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_cast_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> Int32\n"
        "    -27 as Int32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = sub i32 0, 27\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_mutable_int32_compound_assignment_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_mutable_int32_compound.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> Int32\n"
        "    var value = -27 as Int32\n"
        "    value /= 4 as Int32\n"
        "    value %= 5 as Int32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = sub i32 0, 27\n"
        "  %value.addr = alloca i32\n"
        "  store i32 %tmp0, ptr %value.addr\n"
        "  %tmp1 = load i32, ptr %value.addr\n"
        "  %tmp2 = sdiv i32 %tmp1, 4\n"
        "  store i32 %tmp2, ptr %value.addr\n"
        "  %tmp3 = load i32, ptr %value.addr\n"
        "  %tmp4 = srem i32 %tmp3, 5\n"
        "  store i32 %tmp4, ptr %value.addr\n"
        "  %tmp5 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp5\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_reject_bool_compound_assignment() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_bool_compound_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> Bool\n"
        "    var value = true\n"
        "    value += false\n"
        "    value\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering compound assignment operator '+=' requires an integer assignment target"
    );
}

void test_reject_bool_aggregate_compound_assignment() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_bool_aggregate_compound_assignment.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Flags\n"
        "    enabled: Bool\n"
        "\n"
        "function main() -> Unit\n"
        "    var flags = Flags(true)\n"
        "    flags.enabled += false\n"
        "    return\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering compound assignment operator '+=' requires an integer assignment target"
    );
}

void test_emit_mutable_uint32_while_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_mutable_uint32_while.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 3 as UInt32\n"
        "        value = value + 1 as UInt32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
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
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_while_call_statement() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_while_call.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function observe(value: UInt32) -> UInt32\n"
        "    value\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 2 as UInt32\n"
        "        observe(value)\n"
        "        value = value + 1 as UInt32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @observe(i32 %value) {\n"
        "entry:\n"
        "  ret i32 %value\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 0, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.condition.0:\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = icmp ult i32 %tmp0, 2\n"
        "  br i1 %tmp1, label %while.body.0, label %while.exit.0\n"
        "while.body.0:\n"
        "  %tmp2 = load i32, ptr %value.addr\n"
        "  %tmp3 = call i32 @observe(i32 %tmp2)\n"
        "  %tmp4 = load i32, ptr %value.addr\n"
        "  %tmp5 = add i32 %tmp4, 1\n"
        "  store i32 %tmp5, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.exit.0:\n"
        "  %tmp6 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp6\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_while_unit_call_statement() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_while_unit_call.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 2 as UInt32\n"
        "        observe(value)\n"
        "        value = value + 1 as UInt32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define void @observe(i32 %value) {\n"
        "entry:\n"
        "  ret void\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 0, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.condition.0:\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = icmp ult i32 %tmp0, 2\n"
        "  br i1 %tmp1, label %while.body.0, label %while.exit.0\n"
        "while.body.0:\n"
        "  %tmp2 = load i32, ptr %value.addr\n"
        "  call void @observe(i32 %tmp2)\n"
        "  %tmp3 = load i32, ptr %value.addr\n"
        "  %tmp4 = add i32 %tmp3, 1\n"
        "  store i32 %tmp4, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.exit.0:\n"
        "  %tmp5 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp5\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_while_local_bindings() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_while_local_bindings.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 2 as UInt32\n"
        "        let step = 1 as UInt32\n"
        "        var local: UInt32 = value + step\n"
        "        value = local + step\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 0, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.condition.0:\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = icmp ult i32 %tmp0, 2\n"
        "  br i1 %tmp1, label %while.body.0, label %while.exit.0\n"
        "while.body.0:\n"
        "  %step = add i32 0, 1\n"
        "  %tmp2 = load i32, ptr %value.addr\n"
        "  %tmp3 = add i32 %tmp2, %step\n"
        "  %local.addr = alloca i32\n"
        "  store i32 %tmp3, ptr %local.addr\n"
        "  %tmp4 = load i32, ptr %local.addr\n"
        "  %tmp5 = add i32 %tmp4, %step\n"
        "  store i32 %tmp5, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.exit.0:\n"
        "  %tmp6 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp6\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_restore_outer_binding_after_while_local_shadow() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_while_local_shadow.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    let step = 5 as UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 1 as UInt32\n"
        "        let step = 1 as UInt32\n"
        "        value = step\n"
        "        break\n"
        "    step\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %step = add i32 0, 5\n"
        "  %value.addr = alloca i32\n"
        "  store i32 0, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.condition.0:\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = icmp ult i32 %tmp0, 1\n"
        "  br i1 %tmp1, label %while.body.0, label %while.exit.0\n"
        "while.body.0:\n"
        "  %step.1 = add i32 0, 1\n"
        "  store i32 %step.1, ptr %value.addr\n"
        "  br label %while.exit.0\n"
        "while.exit.0:\n"
        "  ret i32 %step\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_terminal_while_break() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_while_break.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 3 as UInt32\n"
        "        value = value + 1 as UInt32\n"
        "        break\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
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
        "  br label %while.exit.0\n"
        "while.exit.0:\n"
        "  %tmp4 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp4\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_terminal_while_continue() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_while_continue.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 3 as UInt32\n"
        "        value = value + 1 as UInt32\n"
        "        continue\n"
        "    value\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("  br label %while.condition.0\nwhile.exit.0:\n") != std::string::npos);
}

void test_emit_defer_cleanup_on_early_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_defer_return.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function cleanup_then_return(value: UInt32, flag: Bool) -> Unit\n"
        "    defer\n"
        "        observe(value)\n"
        "    if flag\n"
        "        return\n"
        "    return\n"
    );

    assert(!result.has_errors());
    auto const function_pos = result.ir_text.find("define void @cleanup_then_return");
    assert(function_pos != std::string::npos);
    auto const function_ir = result.ir_text.substr(function_pos);
    auto const first_call = function_ir.find("call void @observe(i32 %value)");
    assert(first_call != std::string::npos);
    auto const second_call = function_ir.find("call void @observe(i32 %value)", first_call + 1);
    assert(second_call != std::string::npos);
    auto const first_ret = function_ir.find("ret void");
    assert(first_ret != std::string::npos);
    auto const second_ret = function_ir.find("ret void", first_ret + 1);
    assert(second_ret != std::string::npos);
    assert(first_call < first_ret);
    assert(second_call < second_ret);
    std::filesystem::remove(path);
}

void test_emit_conditional_while_continue_and_break() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_conditional_while_control.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 5 as UInt32\n"
        "        value = value + 1 as UInt32\n"
        "        if value == 2 as UInt32\n"
        "            continue\n"
        "        if value == 4 as UInt32\n"
        "            break\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 0, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.condition.0:\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = icmp ult i32 %tmp0, 5\n"
        "  br i1 %tmp1, label %while.body.0, label %while.exit.0\n"
        "while.body.0:\n"
        "  %tmp2 = load i32, ptr %value.addr\n"
        "  %tmp3 = add i32 %tmp2, 1\n"
        "  store i32 %tmp3, ptr %value.addr\n"
        "  %tmp4 = load i32, ptr %value.addr\n"
        "  %tmp5 = icmp eq i32 %tmp4, 2\n"
        "  br i1 %tmp5, label %if.then.1, label %if.merge.1\n"
        "if.then.1:\n"
        "  br label %while.condition.0\n"
        "if.merge.1:\n"
        "  %tmp6 = load i32, ptr %value.addr\n"
        "  %tmp7 = icmp eq i32 %tmp6, 4\n"
        "  br i1 %tmp7, label %if.then.2, label %if.merge.2\n"
        "if.then.2:\n"
        "  br label %while.exit.0\n"
        "if.merge.2:\n"
        "  br label %while.condition.0\n"
        "while.exit.0:\n"
        "  %tmp8 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp8\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_partial_switch_in_while_body() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_partial_switch_in_while_body.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(value: UInt32) -> UInt32\n"
        "    var current = value\n"
        "    while current < 2 as UInt32\n"
        "        switch current\n"
        "            0 => break\n"
        "            default => current = current + 1 as UInt32\n"
        "        current = current + 1 as UInt32\n"
        "    current\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i32 %value) {\n"
        "entry:\n"
        "  %current.addr = alloca i32\n"
        "  store i32 %value, ptr %current.addr\n"
        "  br label %while.condition.0\n"
        "while.condition.0:\n"
        "  %tmp0 = load i32, ptr %current.addr\n"
        "  %tmp1 = icmp ult i32 %tmp0, 2\n"
        "  br i1 %tmp1, label %while.body.0, label %while.exit.0\n"
        "while.body.0:\n"
        "  %tmp2 = load i32, ptr %current.addr\n"
        "  switch i32 %tmp2, label %switch.default.1 [\n"
        "    i32 0, label %switch.case.1.0\n"
        "  ]\n"
        "switch.case.1.0:\n"
        "  br label %while.exit.0\n"
        "switch.default.1:\n"
        "  %tmp3 = load i32, ptr %current.addr\n"
        "  %tmp4 = add i32 %tmp3, 1\n"
        "  store i32 %tmp4, ptr %current.addr\n"
        "  br label %switch.merge.1\n"
        "switch.merge.1:\n"
        "  %tmp5 = load i32, ptr %current.addr\n"
        "  %tmp6 = add i32 %tmp5, 1\n"
        "  store i32 %tmp6, ptr %current.addr\n"
        "  br label %while.condition.0\n"
        "while.exit.0:\n"
        "  %tmp7 = load i32, ptr %current.addr\n"
        "  ret i32 %tmp7\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_terminating_while_if_else() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_terminating_while_if_else.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 1 as UInt32\n"
        "        if flag\n"
        "            break\n"
        "        else\n"
        "            continue\n"
        "    value\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("if.merge.1:") == std::string::npos);
    assert(result.ir_text.find("if.then.1:\n  br label %while.exit.0\n") != std::string::npos);
    assert(
        result.ir_text.find("if.else.1:\n  br label %while.condition.0\n") != std::string::npos
    );
}

void test_emit_nested_while_if_control() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_nested_while_if.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(first: Bool, second: Bool) -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 1 as UInt32\n"
        "        if first\n"
        "            if second\n"
        "                break\n"
        "        value = value + 1 as UInt32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("if.then.1:\n  br i1 %second, label %if.then.2") != std::string::npos);
    assert(result.ir_text.find("if.then.2:\n  br label %while.exit.0\n") != std::string::npos);
    assert(result.ir_text.find("if.merge.2:\n  br label %if.merge.1\n") != std::string::npos);
}

void test_emit_nested_while_loop_body() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_nested_while_loop.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var outer = 0 as UInt32\n"
        "    var inner = 0 as UInt32\n"
        "    while outer < 2 as UInt32\n"
        "        while inner < 3 as UInt32\n"
        "            inner = inner + 1 as UInt32\n"
        "        outer = outer + 1 as UInt32\n"
        "    outer + inner\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("while.condition.0:") != std::string::npos);
    assert(result.ir_text.find("while.condition.1:") != std::string::npos);
    assert(result.ir_text.find("br i1 %tmp1, label %while.body.0, label %while.exit.0") != std::string::npos);
    assert(result.ir_text.find("br i1 %tmp3, label %while.body.1, label %while.exit.1") != std::string::npos);
}

void test_emit_nested_repeat_in_while_body() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_nested_repeat_in_while.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var outer = 0 as UInt32\n"
        "    var inner = 0 as UInt32\n"
        "    while outer < 2 as UInt32\n"
        "        repeat\n"
        "            inner = inner + 1 as UInt32\n"
        "        while inner < 3 as UInt32\n"
        "        outer = outer + 1 as UInt32\n"
        "    outer + inner\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("while.condition.0:") != std::string::npos);
    assert(result.ir_text.find("repeat.body.1:") != std::string::npos);
    assert(result.ir_text.find("repeat.condition.1:") != std::string::npos);
    assert(result.ir_text.find("label %repeat.body.1, label %repeat.exit.1") != std::string::npos);
}

void test_emit_nested_for_in_while_body() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_nested_for_in_while.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(values: Array<UInt32, 2>) -> UInt32\n"
        "    var outer = 0 as UInt32\n"
        "    var total = 0 as UInt32\n"
        "    while outer < 1 as UInt32\n"
        "        for item in [1 as UInt32, 2 as UInt32]\n"
        "            total = total + item\n"
        "        for item in values\n"
        "            total = total + item\n"
        "        outer = outer + 1 as UInt32\n"
        "    total\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("while.condition.0:") != std::string::npos);
    assert(result.ir_text.find("for.iteration.1.0:") != std::string::npos);
    assert(result.ir_text.find("for.iteration.1.1:") != std::string::npos);
    assert(result.ir_text.find("for.iteration.2.0:") != std::string::npos);
    assert(result.ir_text.find("for.iteration.2.1:") != std::string::npos);
    assert(result.ir_text.find("extractvalue [2 x i32] %values, 0") != std::string::npos);
}

void test_reject_nonterminal_while_loop_control() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nonterminal_while_control.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 3 as UInt32\n"
        "        continue\n"
        "        value = value + 1 as UInt32\n"
        "    value\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not support statements after terminating loop control"
    );
}

void test_reject_unsupported_while_body_statement() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_unsupported_while_body.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 3 as UInt32\n"
        "        value\n"
        "    value\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering while body only supports local bindings, mutable-local assignments, "
        "call statements, loop control, nested if/switch statements, nested while/repeat/for statements, and unsafe blocks"
    );
}

void test_emit_scalar_extension_method_definition() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_scalar_extension_method.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function identity(this: shared This) -> UInt32\n"
        "        this\n"
        "\n"
        "extend Device\n"
        "    function scale(value: UInt32) -> UInt32\n"
        "        value + 1 as UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    0 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  ret i32 0\n"
        "}\n"
        "\n"
        "define i32 @method.UInt32.identity(i32 %this) {\n"
        "entry:\n"
        "  ret i32 %this\n"
        "}\n"
        "\n"
        "define i32 @method.Device.scale(i32 %value) {\n"
        "entry:\n"
        "  %tmp0 = add i32 %value, 1\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_scalar_member_call_expression() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_scalar_member_call.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function scale(this: shared This, amount: UInt32) -> UInt32\n"
        "        this + amount\n"
        "\n"
        "function main() -> UInt32\n"
        "    let value: UInt32 = 1 as UInt32\n"
        "    value.scale(2 as UInt32)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value = add i32 0, 1\n"
        "  %tmp0 = call i32 @method.UInt32.scale(i32 %value, i32 2)\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @method.UInt32.scale(i32 %this, i32 %amount) {\n"
        "entry:\n"
        "  %tmp0 = add i32 %this, %amount\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_scalar_member_call_statement() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_scalar_member_call_statement.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function reset(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value: UInt32 = 0 as UInt32\n"
        "    while value < 1 as UInt32\n"
        "        value.reset()\n"
        "        value = value + 1 as UInt32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 0, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.condition.0:\n"
        "  %tmp0 = load i32, ptr %value.addr\n"
        "  %tmp1 = icmp ult i32 %tmp0, 1\n"
        "  br i1 %tmp1, label %while.body.0, label %while.exit.0\n"
        "while.body.0:\n"
        "  %tmp2 = load i32, ptr %value.addr\n"
        "  call void @method.UInt32.reset(i32 %tmp2)\n"
        "  %tmp3 = load i32, ptr %value.addr\n"
        "  %tmp4 = add i32 %tmp3, 1\n"
        "  store i32 %tmp4, ptr %value.addr\n"
        "  br label %while.condition.0\n"
        "while.exit.0:\n"
        "  %tmp5 = load i32, ptr %value.addr\n"
        "  ret i32 %tmp5\n"
        "}\n"
        "\n"
        "define void @method.UInt32.reset(i32 %this) {\n"
        "entry:\n"
        "  ret void\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_scalar_call_statements() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_scalar_call_statements.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function reset(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function main() -> UInt32\n"
        "    let value: UInt32 = 1 as UInt32\n"
        "    observe(value)\n"
        "    value.reset()\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define void @observe(i32 %value) {\n"
        "entry:\n"
        "  ret void\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %value = add i32 0, 1\n"
        "  call void @observe(i32 %value)\n"
        "  call void @method.UInt32.reset(i32 %value)\n"
        "  ret i32 %value\n"
        "}\n"
        "\n"
        "define void @method.UInt32.reset(i32 %this) {\n"
        "entry:\n"
        "  ret void\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_scalar_unit_call_statements() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_scalar_unit_call_statements.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "extend UInt32\n"
        "    function reset(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function observe_then_return(value: UInt32) -> Unit\n"
        "    observe(value)\n"
        "    value.reset()\n"
        "    return\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define void @observe(i32 %value) {\n"
        "entry:\n"
        "  ret void\n"
        "}\n"
        "\n"
        "define void @observe_then_return(i32 %value) {\n"
        "entry:\n"
        "  call void @observe(i32 %value)\n"
        "  call void @method.UInt32.reset(i32 %value)\n"
        "  ret void\n"
        "}\n"
        "\n"
        "define void @method.UInt32.reset(i32 %this) {\n"
        "entry:\n"
        "  ret void\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_uint32_add_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_uint32_add.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    let left = 1 as UInt32\n"
        "    let right = 2 as UInt32\n"
        "    left + right\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %left = add i32 0, 1\n"
        "  %right = add i32 0, 2\n"
        "  %tmp0 = add i32 %left, %right\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_uint32_arithmetic_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_uint32_arithmetic.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    let left = 8 as UInt32\n"
        "    let right = 2 as UInt32\n"
        "    left - right * right / right + left % 3 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %left = add i32 0, 8\n"
        "  %right = add i32 0, 2\n"
        "  %tmp0 = mul i32 %right, %right\n"
        "  %tmp1 = udiv i32 %tmp0, %right\n"
        "  %tmp2 = sub i32 %left, %tmp1\n"
        "  %tmp3 = urem i32 %left, 3\n"
        "  %tmp4 = add i32 %tmp2, %tmp3\n"
        "  ret i32 %tmp4\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_int32_division_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_int32_division.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function divide(left: Int32, right: Int32) -> Int32\n"
        "    left / right\n"
        "\n"
        "function remainder(left: Int32, right: Int32) -> Int32\n"
        "    left % right\n"
        "\n"
        "function negate(value: Int32) -> Int32\n"
        "    -value\n"
        "\n"
        "function main() -> Int32\n"
        "    divide(8 as Int32, 2 as Int32) + remainder(9 as Int32, 4 as Int32) + negate(5 as Int32)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @divide(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = sdiv i32 %left, %right\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @remainder(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = srem i32 %left, %right\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @negate(i32 %value) {\n"
        "entry:\n"
        "  %tmp0 = sub i32 0, %value\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 @divide(i32 8, i32 2)\n"
        "  %tmp1 = call i32 @remainder(i32 9, i32 4)\n"
        "  %tmp2 = add i32 %tmp0, %tmp1\n"
        "  %tmp3 = call i32 @negate(i32 5)\n"
        "  %tmp4 = add i32 %tmp2, %tmp3\n"
        "  ret i32 %tmp4\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_uint32_comparison_returns() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_uint32_comparisons.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function is_equal(left: UInt32, right: UInt32) -> Bool\n"
        "    left == right\n"
        "\n"
        "function is_different(left: UInt32, right: UInt32) -> Bool\n"
        "    left != right\n"
        "\n"
        "function is_less(left: UInt32, right: UInt32) -> Bool\n"
        "    left < right\n"
        "\n"
        "function is_at_most(left: UInt32, right: UInt32) -> Bool\n"
        "    left <= right\n"
        "\n"
        "function is_greater(left: UInt32, right: UInt32) -> Bool\n"
        "    left > right\n"
        "\n"
        "function is_at_least(left: UInt32, right: UInt32) -> Bool\n"
        "    left >= right\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i1 @is_equal(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp eq i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_different(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp ne i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_less(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp ult i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_at_most(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp ule i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_greater(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp ugt i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_at_least(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp uge i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_int32_comparison_returns() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_int32_comparisons.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function is_less(left: Int32, right: Int32) -> Bool\n"
        "    left < right\n"
        "\n"
        "function is_at_most(left: Int32, right: Int32) -> Bool\n"
        "    left <= right\n"
        "\n"
        "function is_greater(left: Int32, right: Int32) -> Bool\n"
        "    left > right\n"
        "\n"
        "function is_at_least(left: Int32, right: Int32) -> Bool\n"
        "    left >= right\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i1 @is_less(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp slt i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_at_most(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp sle i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_greater(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp sgt i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @is_at_least(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp sge i32 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_boolean_expression_returns() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_boolean_expressions.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function enabled() -> Bool\n"
        "    true\n"
        "\n"
        "function disabled() -> Bool\n"
        "    false\n"
        "\n"
        "function invert(flag: Bool) -> Bool\n"
        "    not flag\n"
        "\n"
        "function allowed(left: Bool, right: Bool) -> Bool\n"
        "    left or right\n"
        "\n"
        "function same(left: Bool, right: Bool) -> Bool\n"
        "    left == right\n"
        "\n"
        "function different(left: Bool, right: Bool) -> Bool\n"
        "    left != right\n"
        "\n"
        "function in_range(value: UInt32) -> Bool\n"
        "    value >= 1 as UInt32 and value <= 10 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i1 @enabled() {\n"
        "entry:\n"
        "  ret i1 1\n"
        "}\n"
        "\n"
        "define i1 @disabled() {\n"
        "entry:\n"
        "  ret i1 0\n"
        "}\n"
        "\n"
        "define i1 @invert(i1 %flag) {\n"
        "entry:\n"
        "  %tmp0 = xor i1 %flag, true\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @allowed(i1 %left, i1 %right) {\n"
        "entry:\n"
        "  %tmp0 = or i1 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @same(i1 %left, i1 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp eq i1 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @different(i1 %left, i1 %right) {\n"
        "entry:\n"
        "  %tmp0 = icmp ne i1 %left, %right\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @in_range(i32 %value) {\n"
        "entry:\n"
        "  %tmp0 = icmp uge i32 %value, 1\n"
        "  %tmp1 = icmp ule i32 %value, 10\n"
        "  %tmp2 = and i1 %tmp0, %tmp1\n"
        "  ret i1 %tmp2\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_ternary_expression_returns() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_ternary_expressions.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(flag: Bool) -> UInt32\n"
        "    flag ? 1 as UInt32 : 2 as UInt32\n"
        "\n"
        "function choose_flag(flag: Bool) -> Bool\n"
        "    flag ? true : false\n"
        "\n"
        "function choose_nested(first: Bool, second: Bool) -> UInt32\n"
        "    first ? second ? 1 as UInt32 : 2 as UInt32 : 3 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %flag) {\n"
        "entry:\n"
        "  br i1 %flag, label %ternary.then.0, label %ternary.else.0\n"
        "ternary.then.0:\n"
        "  br label %ternary.merge.0\n"
        "ternary.else.0:\n"
        "  br label %ternary.merge.0\n"
        "ternary.merge.0:\n"
        "  %tmp0 = phi i32 [1, %ternary.then.0], [2, %ternary.else.0]\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i1 @choose_flag(i1 %flag) {\n"
        "entry:\n"
        "  br i1 %flag, label %ternary.then.0, label %ternary.else.0\n"
        "ternary.then.0:\n"
        "  br label %ternary.merge.0\n"
        "ternary.else.0:\n"
        "  br label %ternary.merge.0\n"
        "ternary.merge.0:\n"
        "  %tmp0 = phi i1 [1, %ternary.then.0], [0, %ternary.else.0]\n"
        "  ret i1 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @choose_nested(i1 %first, i1 %second) {\n"
        "entry:\n"
        "  br i1 %first, label %ternary.then.0, label %ternary.else.0\n"
        "ternary.then.0:\n"
        "  br i1 %second, label %ternary.then.1, label %ternary.else.1\n"
        "ternary.then.1:\n"
        "  br label %ternary.merge.1\n"
        "ternary.else.1:\n"
        "  br label %ternary.merge.1\n"
        "ternary.merge.1:\n"
        "  %tmp0 = phi i32 [1, %ternary.then.1], [2, %ternary.else.1]\n"
        "  br label %ternary.merge.0\n"
        "ternary.else.0:\n"
        "  br label %ternary.merge.0\n"
        "ternary.merge.0:\n"
        "  %tmp1 = phi i32 [%tmp0, %ternary.merge.1], [3, %ternary.else.0]\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_final_if_expression_returns() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_final_if_expressions.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(flag: Bool) -> UInt32\n"
        "    if flag\n"
        "        1 as UInt32\n"
        "    else\n"
        "        2 as UInt32\n"
        "\n"
        "function choose_return(flag: Bool) -> UInt32\n"
        "    if flag\n"
        "        return 3 as UInt32\n"
        "    else\n"
        "        return 4 as UInt32\n"
        "\n"
        "function choose_nested(first: Bool, second: Bool) -> UInt32\n"
        "    if first\n"
        "        second ? 5 as UInt32 : 6 as UInt32\n"
        "    else\n"
        "        7 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %flag) {\n"
        "entry:\n"
        "  br i1 %flag, label %if.then.0, label %if.else.0\n"
        "if.then.0:\n"
        "  br label %if.merge.0\n"
        "if.else.0:\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  %tmp0 = phi i32 [1, %if.then.0], [2, %if.else.0]\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @choose_return(i1 %flag) {\n"
        "entry:\n"
        "  br i1 %flag, label %if.then.0, label %if.else.0\n"
        "if.then.0:\n"
        "  br label %if.merge.0\n"
        "if.else.0:\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  %tmp0 = phi i32 [3, %if.then.0], [4, %if.else.0]\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @choose_nested(i1 %first, i1 %second) {\n"
        "entry:\n"
        "  br i1 %first, label %if.then.0, label %if.else.0\n"
        "if.then.0:\n"
        "  br i1 %second, label %ternary.then.1, label %ternary.else.1\n"
        "ternary.then.1:\n"
        "  br label %ternary.merge.1\n"
        "ternary.else.1:\n"
        "  br label %ternary.merge.1\n"
        "ternary.merge.1:\n"
        "  %tmp0 = phi i32 [5, %ternary.then.1], [6, %ternary.else.1]\n"
        "  br label %if.merge.0\n"
        "if.else.0:\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  %tmp1 = phi i32 [%tmp0, %ternary.merge.1], [7, %if.else.0]\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_final_if_branch_local_bindings() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_final_if_branch_locals.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(flag: Bool) -> UInt32\n"
        "    if flag\n"
        "        let value = 1 as UInt32\n"
        "        let result = value + 1 as UInt32\n"
        "        result\n"
        "    else\n"
        "        let value = 3 as UInt32\n"
        "        return value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %flag) {\n"
        "entry:\n"
        "  br i1 %flag, label %if.then.0, label %if.else.0\n"
        "if.then.0:\n"
        "  %value = add i32 0, 1\n"
        "  %tmp0 = add i32 %value, 1\n"
        "  %result = add i32 0, %tmp0\n"
        "  br label %if.merge.0\n"
        "if.else.0:\n"
        "  %value.1 = add i32 0, 3\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  %tmp1 = phi i32 [%result, %if.then.0], [%value.1, %if.else.0]\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_final_if_mutable_branch_prefixes() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_final_if_mutable_prefixes.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(flag: Bool) -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    if flag\n"
        "        value = 1 as UInt32\n"
        "        var local = 10 as UInt32\n"
        "        local + value\n"
        "    else\n"
        "        value = 2 as UInt32\n"
        "        var local = 20 as UInt32\n"
        "        local + value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %flag) {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 0, ptr %value.addr\n"
        "  br i1 %flag, label %if.then.0, label %if.else.0\n"
        "if.then.0:\n"
        "  store i32 1, ptr %value.addr\n"
        "  %local.addr = alloca i32\n"
        "  store i32 10, ptr %local.addr\n"
        "  %tmp0 = load i32, ptr %local.addr\n"
        "  %tmp1 = load i32, ptr %value.addr\n"
        "  %tmp2 = add i32 %tmp0, %tmp1\n"
        "  br label %if.merge.0\n"
        "if.else.0:\n"
        "  store i32 2, ptr %value.addr\n"
        "  %local.addr.1 = alloca i32\n"
        "  store i32 20, ptr %local.addr.1\n"
        "  %tmp3 = load i32, ptr %local.addr.1\n"
        "  %tmp4 = load i32, ptr %value.addr\n"
        "  %tmp5 = add i32 %tmp3, %tmp4\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  %tmp6 = phi i32 [%tmp2, %if.then.0], [%tmp5, %if.else.0]\n"
        "  ret i32 %tmp6\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_nested_final_if_returns() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_nested_final_if.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(first: Bool, second: Bool) -> UInt32\n"
        "    if first\n"
        "        let base = 1 as UInt32\n"
        "        if second\n"
        "            let value = base + 1 as UInt32\n"
        "            value\n"
        "        else\n"
        "            let value = base + 2 as UInt32\n"
        "            value\n"
        "    else\n"
        "        4 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %first, i1 %second) {\n"
        "entry:\n"
        "  br i1 %first, label %if.then.0, label %if.else.0\n"
        "if.then.0:\n"
        "  %base = add i32 0, 1\n"
        "  br i1 %second, label %if.then.1, label %if.else.1\n"
        "if.then.1:\n"
        "  %tmp0 = add i32 %base, 1\n"
        "  %value = add i32 0, %tmp0\n"
        "  br label %if.merge.1\n"
        "if.else.1:\n"
        "  %tmp1 = add i32 %base, 2\n"
        "  %value.1 = add i32 0, %tmp1\n"
        "  br label %if.merge.1\n"
        "if.merge.1:\n"
        "  %tmp2 = phi i32 [%value, %if.then.1], [%value.1, %if.else.1]\n"
        "  br label %if.merge.0\n"
        "if.else.0:\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  %tmp3 = phi i32 [%tmp2, %if.merge.1], [4, %if.else.0]\n"
        "  ret i32 %tmp3\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_final_integer_switch_returns() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_final_integer_switch.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(value: UInt32) -> UInt32\n"
        "    switch value\n"
        "        0 => 10 as UInt32\n"
        "        1 =>\n"
        "            let base = 20 as UInt32\n"
        "            base + 1 as UInt32\n"
        "        default =>\n"
        "            let base = 30 as UInt32\n"
        "            return base\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i32 %value) {\n"
        "entry:\n"
        "  switch i32 %value, label %switch.default.0 [\n"
        "    i32 0, label %switch.case.0.0\n"
        "    i32 1, label %switch.case.0.1\n"
        "  ]\n"
        "switch.case.0.0:\n"
        "  br label %switch.merge.0\n"
        "switch.case.0.1:\n"
        "  %base = add i32 0, 20\n"
        "  %tmp0 = add i32 %base, 1\n"
        "  br label %switch.merge.0\n"
        "switch.default.0:\n"
        "  %base.1 = add i32 0, 30\n"
        "  br label %switch.merge.0\n"
        "switch.merge.0:\n"
        "  %tmp1 = phi i32 [10, %switch.case.0.0], [%tmp0, %switch.case.0.1], [%base.1, %switch.default.0]\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_final_switch_mutable_case_prefixes() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_switch_mutable_prefixes.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(selector: UInt32) -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    switch selector\n"
        "        0 =>\n"
        "            value = 1 as UInt32\n"
        "            var local = 10 as UInt32\n"
        "            local + value\n"
        "        default =>\n"
        "            value = 2 as UInt32\n"
        "            var local = 20 as UInt32\n"
        "            local + value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i32 %selector) {\n"
        "entry:\n"
        "  %value.addr = alloca i32\n"
        "  store i32 0, ptr %value.addr\n"
        "  switch i32 %selector, label %switch.default.0 [\n"
        "    i32 0, label %switch.case.0.0\n"
        "  ]\n"
        "switch.case.0.0:\n"
        "  store i32 1, ptr %value.addr\n"
        "  %local.addr = alloca i32\n"
        "  store i32 10, ptr %local.addr\n"
        "  %tmp0 = load i32, ptr %local.addr\n"
        "  %tmp1 = load i32, ptr %value.addr\n"
        "  %tmp2 = add i32 %tmp0, %tmp1\n"
        "  br label %switch.merge.0\n"
        "switch.default.0:\n"
        "  store i32 2, ptr %value.addr\n"
        "  %local.addr.1 = alloca i32\n"
        "  store i32 20, ptr %local.addr.1\n"
        "  %tmp3 = load i32, ptr %local.addr.1\n"
        "  %tmp4 = load i32, ptr %value.addr\n"
        "  %tmp5 = add i32 %tmp3, %tmp4\n"
        "  br label %switch.merge.0\n"
        "switch.merge.0:\n"
        "  %tmp6 = phi i32 [%tmp2, %switch.case.0.0], [%tmp5, %switch.default.0]\n"
        "  ret i32 %tmp6\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_exhaustive_boolean_switch_returns() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_exhaustive_boolean_switch.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(flag: Bool) -> UInt32\n"
        "    switch flag\n"
        "        true => 1 as UInt32\n"
        "        false => 2 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %flag) {\n"
        "entry:\n"
        "  switch i1 %flag, label %switch.unreachable.0 [\n"
        "    i1 1, label %switch.case.0.0\n"
        "    i1 0, label %switch.case.0.1\n"
        "  ]\n"
        "switch.case.0.0:\n"
        "  br label %switch.merge.0\n"
        "switch.case.0.1:\n"
        "  br label %switch.merge.0\n"
        "switch.unreachable.0:\n"
        "  unreachable\n"
        "switch.merge.0:\n"
        "  %tmp0 = phi i32 [1, %switch.case.0.0], [2, %switch.case.0.1]\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_switch_nested_in_final_if() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_switch_nested_in_final_if.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(enabled: Bool, value: UInt32) -> UInt32\n"
        "    if enabled\n"
        "        switch value\n"
        "            0 => 1 as UInt32\n"
        "            default => 2 as UInt32\n"
        "    else\n"
        "        3 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %enabled, i32 %value) {\n"
        "entry:\n"
        "  br i1 %enabled, label %if.then.0, label %if.else.0\n"
        "if.then.0:\n"
        "  switch i32 %value, label %switch.default.1 [\n"
        "    i32 0, label %switch.case.1.0\n"
        "  ]\n"
        "switch.case.1.0:\n"
        "  br label %switch.merge.1\n"
        "switch.default.1:\n"
        "  br label %switch.merge.1\n"
        "switch.merge.1:\n"
        "  %tmp0 = phi i32 [1, %switch.case.1.0], [2, %switch.default.1]\n"
        "  br label %if.merge.0\n"
        "if.else.0:\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  %tmp1 = phi i32 [%tmp0, %switch.merge.1], [3, %if.else.0]\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_terminating_switch_nested_in_nonfinal_if() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_terminating_switch_nested_in_nonfinal_if.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(enabled: Bool, value: UInt32) -> UInt32\n"
        "    if enabled\n"
        "        switch value\n"
        "            0 => return 1 as UInt32\n"
        "            default => return 2 as UInt32\n"
        "    return 3 as UInt32\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("switch.merge") == std::string::npos);
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %enabled, i32 %value) {\n"
        "entry:\n"
        "  br i1 %enabled, label %if.then.0, label %if.merge.0\n"
        "if.then.0:\n"
        "  switch i32 %value, label %switch.default.1 [\n"
        "    i32 0, label %switch.case.1.0\n"
        "  ]\n"
        "switch.case.1.0:\n"
        "  ret i32 1\n"
        "switch.default.1:\n"
        "  ret i32 2\n"
        "if.merge.0:\n"
        "  ret i32 3\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_partial_switch_nested_in_nonfinal_if() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_partial_switch_nested_in_nonfinal_if.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(enabled: Bool, value: UInt32) -> UInt32\n"
        "    var current = value\n"
        "    if enabled\n"
        "        switch current\n"
        "            0 => return 1 as UInt32\n"
        "            default => current = current + 1 as UInt32\n"
        "    return 3 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %enabled, i32 %value) {\n"
        "entry:\n"
        "  %current.addr = alloca i32\n"
        "  store i32 %value, ptr %current.addr\n"
        "  br i1 %enabled, label %if.then.0, label %if.merge.0\n"
        "if.then.0:\n"
        "  %tmp0 = load i32, ptr %current.addr\n"
        "  switch i32 %tmp0, label %switch.default.1 [\n"
        "    i32 0, label %switch.case.1.0\n"
        "  ]\n"
        "switch.case.1.0:\n"
        "  ret i32 1\n"
        "switch.default.1:\n"
        "  %tmp1 = load i32, ptr %current.addr\n"
        "  %tmp2 = add i32 %tmp1, 1\n"
        "  store i32 %tmp2, ptr %current.addr\n"
        "  br label %switch.merge.1\n"
        "switch.merge.1:\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  ret i32 3\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_partial_switch_in_guard_failure() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_partial_switch_in_guard_failure.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(enabled: Bool, value: UInt32) -> UInt32\n"
        "    var current = value\n"
        "    guard enabled else\n"
        "        switch current\n"
        "            0 => return 1 as UInt32\n"
        "            default => current = current + 1 as UInt32\n"
        "    current\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @choose(i1 %enabled, i32 %value) {\n"
        "entry:\n"
        "  %current.addr = alloca i32\n"
        "  store i32 %value, ptr %current.addr\n"
        "  br i1 %enabled, label %guard.merge.0, label %guard.failure.0\n"
        "guard.failure.0:\n"
        "  %tmp0 = load i32, ptr %current.addr\n"
        "  switch i32 %tmp0, label %switch.default.1 [\n"
        "    i32 0, label %switch.case.1.0\n"
        "  ]\n"
        "switch.case.1.0:\n"
        "  ret i32 1\n"
        "switch.default.1:\n"
        "  %tmp1 = load i32, ptr %current.addr\n"
        "  %tmp2 = add i32 %tmp1, 1\n"
        "  store i32 %tmp2, ptr %current.addr\n"
        "  br label %switch.merge.1\n"
        "switch.merge.1:\n"
        "  br label %guard.merge.0\n"
        "guard.merge.0:\n"
        "  %tmp3 = load i32, ptr %current.addr\n"
        "  ret i32 %tmp3\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_terminating_unit_switch_nested_in_nonfinal_if() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_terminating_unit_switch_nested_in_nonfinal_if.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(enabled: Bool, value: UInt32) -> Unit\n"
        "    if enabled\n"
        "        switch value\n"
        "            0 => return\n"
        "            default => return\n"
        "    return\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("switch.merge") == std::string::npos);
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define void @choose(i1 %enabled, i32 %value) {\n"
        "entry:\n"
        "  br i1 %enabled, label %if.then.0, label %if.merge.0\n"
        "if.then.0:\n"
        "  switch i32 %value, label %switch.default.1 [\n"
        "    i32 0, label %switch.case.1.0\n"
        "  ]\n"
        "switch.case.1.0:\n"
        "  ret void\n"
        "switch.default.1:\n"
        "  ret void\n"
        "if.merge.0:\n"
        "  ret void\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_partial_unit_switch_nested_in_nonfinal_if() {
    auto path = std::filesystem::temp_directory_path() /
        "orison_lowering_partial_unit_switch_nested_in_nonfinal_if.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function choose(enabled: Bool, value: UInt32) -> Unit\n"
        "    var current = value\n"
        "    if enabled\n"
        "        switch current\n"
        "            0 => return\n"
        "            default => current = current + 1 as UInt32\n"
        "    return\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define void @choose(i1 %enabled, i32 %value) {\n"
        "entry:\n"
        "  %current.addr = alloca i32\n"
        "  store i32 %value, ptr %current.addr\n"
        "  br i1 %enabled, label %if.then.0, label %if.merge.0\n"
        "if.then.0:\n"
        "  %tmp0 = load i32, ptr %current.addr\n"
        "  switch i32 %tmp0, label %switch.default.1 [\n"
        "    i32 0, label %switch.case.1.0\n"
        "  ]\n"
        "switch.case.1.0:\n"
        "  ret void\n"
        "switch.default.1:\n"
        "  %tmp1 = load i32, ptr %current.addr\n"
        "  %tmp2 = add i32 %tmp1, 1\n"
        "  store i32 %tmp2, ptr %current.addr\n"
        "  br label %switch.merge.1\n"
        "switch.merge.1:\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  ret void\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_zero_argument_function_call_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_zero_argument_call.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function one() -> UInt32\n"
        "    1 as UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    one()\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @one() {\n"
        "entry:\n"
        "  ret i32 1\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 @one()\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_zero_argument_function_call_add_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_zero_argument_call_add.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function one() -> UInt32\n"
        "    1 as UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    one() + 2 as UInt32\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @one() {\n"
        "entry:\n"
        "  ret i32 1\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 @one()\n"
        "  %tmp1 = add i32 %tmp0, 2\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_single_uint32_parameter_function_call_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_single_uint32_parameter_call.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function increment(value: UInt32) -> UInt32\n"
        "    value + 1 as UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    increment(41 as UInt32)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @increment(i32 %value) {\n"
        "entry:\n"
        "  %tmp0 = add i32 %value, 1\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 @increment(i32 41)\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_negative_int32_call_argument_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_call_argument.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function identity(value: Int32) -> Int32\n"
        "    value\n"
        "\n"
        "function main() -> Int32\n"
        "    identity(-27 as Int32)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @identity(i32 %value) {\n"
        "entry:\n"
        "  ret i32 %value\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = sub i32 0, 27\n"
        "  %tmp1 = call i32 @identity(i32 %tmp0)\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_negative_int32_record_field_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_record_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record SignedBox\n"
        "    value: Int32\n"
        "\n"
        "function main() -> Int32\n"
        "    let box: SignedBox = SignedBox(-27 as Int32)\n"
        "    box.value\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("%record.SignedBox = type { i32 }") != std::string::npos);
    assert(result.ir_text.find(" = sub i32 0, 27") != std::string::npos);
    assert(result.ir_text.find(" = insertvalue %record.SignedBox undef, i32 %tmp") != std::string::npos);
    assert(result.ir_text.find(" = extractvalue %record.SignedBox %tmp") != std::string::npos);
    assert(result.ir_text.find("ret i32 %tmp") != std::string::npos);
}

void test_emit_negative_int32_array_element_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_negative_int32_array_element.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> Int32\n"
        "    let values: Array<Int32, 2> = [-27 as Int32, 4 as Int32]\n"
        "    values[0]\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find(" = sub i32 0, 27") != std::string::npos);
    assert(result.ir_text.find(" = insertvalue [2 x i32] undef, i32 %tmp") != std::string::npos);
    assert(result.ir_text.find(" = insertvalue [2 x i32] %tmp") != std::string::npos);
    assert(result.ir_text.find(" = extractvalue [2 x i32] %tmp") != std::string::npos);
    assert(result.ir_text.find("ret i32 %tmp") != std::string::npos);
}

void test_reject_negative_uint32_record_field_initializer() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_record_field.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UnsignedBox\n"
        "    value: UInt32\n"
        "\n"
        "function main() -> UInt32\n"
        "    let box: UnsignedBox = UnsignedBox(-1 as UInt32)\n"
        "    box.value\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message.find("unsupported cast: negative value to UInt32") !=
        std::string::npos
    );
}

void test_reject_negative_uint32_array_element_initializer() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_negative_uint32_array_element.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    let values: Array<UInt32, 2> = [-1 as UInt32, 4 as UInt32]\n"
        "    values[0]\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message.find("unsupported cast: negative value to UInt32") !=
        std::string::npos
    );
}

void test_emit_multi_uint32_parameter_function_call_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_multi_uint32_parameter_call.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function add(left: UInt32, right: UInt32) -> UInt32\n"
        "    left + right\n"
        "\n"
        "function main() -> UInt32\n"
        "    add(40 as UInt32, 2 as UInt32)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @add(i32 %left, i32 %right) {\n"
        "entry:\n"
        "  %tmp0 = add i32 %left, %right\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 @add(i32 40, i32 2)\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_c_foreign_call_with_string_literal() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_c_foreign_string_call.or";
    auto result = lower_source(
        path,
        "package demo.ffi\n"
        "\n"
        "package foreign \"c\"\n"
        "    function printf(format: Pointer<Byte>) -> Int32\n"
        "\n"
        "function main() -> Int32\n"
        "    printf(\"Hello world from Orison!\\n\")\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.ffi\n"
        "\n"
        "@.str.0 = private unnamed_addr constant [26 x i8] c\"Hello world from Orison!\\0A\\00\"\n"
        "\n"
        "declare i32 @printf(ptr, ...)\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 (ptr, ...) @printf(ptr @.str.0)\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_fixed_printf_adapter_with_integer_promotion() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_fixed_printf_adapter.or";
    auto result = lower_source(
        path,
        "package demo.ffi\n"
        "\n"
        "package foreign \"c\"\n"
        "    function print_checked(format: Pointer<Byte>, value: Int16) -> Int32 as \"printf\"\n"
        "\n"
        "function main() -> Int32\n"
        "    print_checked(\"Hello world from Orison!\\n\", 42 as Int16)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.ffi\n"
        "\n"
        "@.str.0 = private unnamed_addr constant [26 x i8] c\"Hello world from Orison!\\0A\\00\"\n"
        "\n"
        "declare i32 @printf(ptr, ...)\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = sext i16 42 to i32\n"
        "  %tmp1 = call i32 (ptr, ...) @printf(ptr @.str.0, i32 %tmp0)\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_fixed_printf_adapter_with_pointer_and_64_bit_arguments() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_fixed_printf_wide_arguments.or";
    auto result = lower_source(
        path,
        "package demo.ffi\n"
        "\n"
        "package foreign \"c\"\n"
        "    function print_checked(format: Pointer<Byte>, text: Pointer<Byte>, signed_value: Int64, unsigned_value: UInt64) -> Int32 as \"printf\"\n"
        "\n"
        "function main() -> Int32\n"
        "    print_checked(\"Orison\\n\", \"safe\", 42 as Int64, 84 as UInt64)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.ffi\n"
        "\n"
        "@.str.0 = private unnamed_addr constant [8 x i8] c\"Orison\\0A\\00\"\n"
        "@.str.1 = private unnamed_addr constant [5 x i8] c\"safe\\00\"\n"
        "\n"
        "declare i32 @printf(ptr, ...)\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 (ptr, ...) @printf(ptr @.str.0, ptr @.str.1, i64 42, i64 84)\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_fixed_printf_adapter_with_float_promotion() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_fixed_printf_float_arguments.or";
    auto result = lower_source(
        path,
        "package demo.ffi\n"
        "\n"
        "package foreign \"c\"\n"
        "    function print_checked(format: Pointer<Byte>, narrow: Float32, wide: Float64) -> Int32 as \"printf\"\n"
        "\n"
        "function main() -> Int32\n"
        "    print_checked(\"Orison\\n\", 1.5 as Float32, 2.5 as Float64)\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.ffi\n"
        "\n"
        "@.str.0 = private unnamed_addr constant [8 x i8] c\"Orison\\0A\\00\"\n"
        "\n"
        "declare i32 @printf(ptr, ...)\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = fpext float 1.5 to double\n"
        "  %tmp1 = call i32 (ptr, ...) @printf(ptr @.str.0, double %tmp0, double 2.5)\n"
        "  ret i32 %tmp1\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_reject_printf_adapter_with_invalid_fixed_prefix() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_invalid_printf_adapter.or";
    auto result = lower_source(
        path,
        "package demo.ffi\n"
        "\n"
        "package foreign \"c\"\n"
        "    function print_checked(value: Int32) -> Int32 as \"printf\"\n"
        "\n"
        "function main() -> Int32\n"
        "    print_checked(42 as Int32)\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "foreign symbol 'printf' does not match the required fixed C ABI prefix"
    );
}

void test_reject_printf_adapter_with_unsupported_trailing_type() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_unsupported_printf_argument.or";
    auto result = lower_source(
        path,
        "package demo.ffi\n"
        "\n"
        "package foreign \"c\"\n"
        "    function print_checked(format: Pointer<Byte>, value: Text) -> Int32 as \"printf\"\n"
        "\n"
        "function main() -> Int32\n"
        "    print_checked(\"Orison\\n\", \"unsafe representation\")\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "foreign symbol 'printf' parameter 'value' has no supported C variadic ABI representation"
    );
}

void test_emit_fixed_arity_c_foreign_call() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_fixed_arity_c_foreign_call.or";
    auto result = lower_source(
        path,
        "package demo.ffi\n"
        "\n"
        "package foreign \"c\"\n"
        "    function strcmp(left: Pointer<Byte>, right: Pointer<Byte>) -> Int32\n"
        "\n"
        "function main() -> Int32\n"
        "    strcmp(\"Orison\", \"Orison\")\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.ffi\n"
        "\n"
        "@.str.0 = private unnamed_addr constant [7 x i8] c\"Orison\\00\"\n"
        "\n"
        "declare i32 @strcmp(ptr, ptr)\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 @strcmp(ptr @.str.0, ptr @.str.0)\n"
        "  ret i32 %tmp0\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_emit_unsafe_function_identity_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_unsafe_function_identity.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "unsafe function unsafe_identity(value: UInt32) -> UInt32\n"
        "    value\n"
    );

    assert(!result.has_errors());
    auto expected = std::string {
        "; Orison LLVM IR scaffold\n"
        "; package demo.lowering\n"
        "\n"
        "define i32 @unsafe_identity(i32 %value) {\n"
        "entry:\n"
        "  ret i32 %value\n"
        "}\n"
        "\n"
    };
    assert(result.ir_text == expected);
}

void test_reject_unsupported_return_expression() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_unsupported_expression.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function same(left: Bool, right: Bool) -> Bool\n"
        "    left < right\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not yet support this return expression: unsupported operator: <"
    );
}

void test_reject_unsigned_unary_negation_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_unsigned_unary_negation.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function negate(value: UInt32) -> UInt32\n"
        "    -value\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not yet support this return expression: unsupported operator: -"
    );
}

void test_reject_boolean_unary_negation_return() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_boolean_unary_negation.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function negate(value: Bool) -> Bool\n"
        "    -value\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not yet support this return expression: unsupported operator: -"
    );
}

void test_emit_scalar_thread_join_expression() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_thread_expression.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function on_thread(value: Int64) -> Int64\n"
        "    let worker = thread\n"
        "        value + 1\n"
        "\n"
        "    worker.join()\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("declare ptr @__orison_thread_spawn(ptr, ptr, ptr, i64, ptr)") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_concurrency_spawn_failed()") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_thread_join(ptr)") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_concurrency_handle_destroy(ptr)") != std::string::npos);
    assert(
        result.ir_text.find(
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.on_thread.4.0"
        ) != std::string::npos
    );
    assert(
        result.ir_text.find(
            ", i64 8, ptr @__orison_thread_cleanup.on_thread.4.0)"
        ) != std::string::npos
    );
    assert(
        result.ir_text.find(
            "define private void @__orison_thread_cleanup.on_thread.4.0(ptr %environment) {\n"
            "entry:\n"
            "  ret void\n"
            "}"
        ) != std::string::npos
    );
    assert(result.ir_text.find("icmp eq ptr %worker, null") != std::string::npos);
    assert(
        result.ir_text.find(
            "call void @__orison_concurrency_spawn_failed()\n"
            "  unreachable\n"
            "worker.thread.spawn_ok.0:"
        ) != std::string::npos
    );
    assert(result.ir_text.find("call void @__orison_thread_join(ptr %worker)") != std::string::npos);
    assert(result.ir_text.find("load i64, ptr %worker.thread.result") != std::string::npos);
    assert(result.ir_text.find("call void @__orison_concurrency_handle_destroy(ptr %worker)") != std::string::npos);
    assert(result.ir_text.find("ret i64 %tmp") != std::string::npos);
}

void test_emit_abandoned_scalar_thread_cleanup() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_abandoned_thread_expression.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function on_thread(value: Int64) -> Int64\n"
        "    let worker = thread\n"
        "        value + 1\n"
        "\n"
        "    0\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("declare ptr @__orison_thread_spawn(ptr, ptr, ptr, i64, ptr)") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_concurrency_spawn_failed()") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_concurrency_handle_destroy(ptr)") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_thread_join(ptr)") == std::string::npos);
    assert(
        result.ir_text.find(
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.on_thread.4.0"
        ) != std::string::npos
    );
    assert(
        result.ir_text.find(
            ", i64 8, ptr @__orison_thread_cleanup.on_thread.4.0)"
        ) != std::string::npos
    );
    assert(result.ir_text.find("icmp eq ptr %worker, null") != std::string::npos);
    assert(
        result.ir_text.find(
            "call void @__orison_concurrency_spawn_failed()\n"
            "  unreachable\n"
            "worker.thread.spawn_ok.0:"
        ) != std::string::npos
    );
    assert(
        result.ir_text.find(
            "call void @__orison_concurrency_handle_destroy(ptr %worker)\n"
            "  ret i64 0"
        ) != std::string::npos
    );
}

void test_emit_scalar_task_await_expression() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_task_expression.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "async function on_task(value: Int64) -> Int64\n"
        "    let pending = task\n"
        "        value + 1\n"
        "\n"
        "    await pending\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("declare ptr @__orison_task_spawn(ptr, ptr, ptr, i64, ptr)") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_concurrency_spawn_failed()") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_task_await(ptr)") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_concurrency_handle_destroy(ptr)") != std::string::npos);
    assert(
        result.ir_text.find(
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.on_task.4.0"
        ) != std::string::npos
    );
    assert(
        result.ir_text.find(
            ", i64 8, ptr @__orison_task_cleanup.on_task.4.0)"
        ) != std::string::npos
    );
    assert(
        result.ir_text.find(
            "define private void @__orison_task_cleanup.on_task.4.0(ptr %environment) {\n"
            "entry:\n"
            "  ret void\n"
            "}"
        ) != std::string::npos
    );
    assert(result.ir_text.find("icmp eq ptr %pending, null") != std::string::npos);
    assert(
        result.ir_text.find(
            "call void @__orison_concurrency_spawn_failed()\n"
            "  unreachable\n"
            "pending.task.spawn_ok.0:"
        ) != std::string::npos
    );
    assert(result.ir_text.find("call void @__orison_task_await(ptr %pending)") != std::string::npos);
    assert(result.ir_text.find("load i64, ptr %pending.task.result") != std::string::npos);
    assert(result.ir_text.find("call void @__orison_concurrency_handle_destroy(ptr %pending)") != std::string::npos);
    assert(result.ir_text.find("ret i64 %tmp") != std::string::npos);
}

void test_emit_no_capture_thread_uses_null_cleanup() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_no_capture_thread_expression.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function on_thread() -> Int64\n"
        "    let worker = thread\n"
        "        41\n"
        "\n"
        "    worker.join()\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("declare ptr @__orison_thread_spawn(ptr, ptr, ptr, i64, ptr)") != std::string::npos);
    assert(
        result.ir_text.find(
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.on_thread.4.0"
        ) != std::string::npos
    );
    assert(result.ir_text.find(", i64 8, ptr null)") != std::string::npos);
    assert(result.ir_text.find("__orison_thread_cleanup.on_thread.4.0") == std::string::npos);
    assert(result.ir_text.find("call void @__orison_thread_join(ptr %worker)") != std::string::npos);
    assert(result.ir_text.find("ret i64 %tmp") != std::string::npos);
}

void test_emit_no_capture_task_uses_null_cleanup() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_no_capture_task_expression.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "async function on_task() -> Int64\n"
        "    let pending = task\n"
        "        41\n"
        "\n"
        "    await pending\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("declare ptr @__orison_task_spawn(ptr, ptr, ptr, i64, ptr)") != std::string::npos);
    assert(
        result.ir_text.find(
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.on_task.4.0"
        ) != std::string::npos
    );
    assert(result.ir_text.find(", i64 8, ptr null)") != std::string::npos);
    assert(result.ir_text.find("__orison_task_cleanup.on_task.4.0") == std::string::npos);
    assert(result.ir_text.find("call void @__orison_task_await(ptr %pending)") != std::string::npos);
    assert(result.ir_text.find("ret i64 %tmp") != std::string::npos);
}

void test_emit_record_thread_result_storage_size() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_record_thread_result.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Pair\n"
        "    public first: Int64\n"
        "    public second: Int64\n"
        "\n"
        "implements Transferable for Pair\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function on_thread() -> Pair\n"
        "    let worker = thread\n"
        "        Pair(1, 2)\n"
        "\n"
        "    worker.join()\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("%record.Pair = type { i64, i64 }") != std::string::npos);
    assert(result.ir_text.find("%worker.thread.result = alloca %record.Pair") != std::string::npos);
    assert(
        result.ir_text.find(
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.on_thread.12.0"
        ) != std::string::npos
    );
    assert(result.ir_text.find(", i64 16, ptr null)") != std::string::npos);
    assert(result.ir_text.find("store %record.Pair %tmp1, ptr %result_storage") != std::string::npos);
    assert(result.ir_text.find("load %record.Pair, ptr %worker.thread.result") != std::string::npos);
    assert(result.ir_text.find("ret %record.Pair %tmp") != std::string::npos);
}

void test_emit_record_task_result_storage_size() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_record_task_result.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Pair\n"
        "    public first: Int64\n"
        "    public second: Int64\n"
        "\n"
        "implements Shareable for Pair\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "async function on_task() -> Pair\n"
        "    let pending = task\n"
        "        Pair(1, 2)\n"
        "\n"
        "    await pending\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("%record.Pair = type { i64, i64 }") != std::string::npos);
    assert(result.ir_text.find("%pending.task.result = alloca %record.Pair") != std::string::npos);
    assert(
        result.ir_text.find(
            "call ptr @__orison_task_spawn(ptr @__orison_task_thunk.on_task.12.0"
        ) != std::string::npos
    );
    assert(result.ir_text.find(", i64 16, ptr null)") != std::string::npos);
    assert(result.ir_text.find("store %record.Pair %tmp1, ptr %result_storage") != std::string::npos);
    assert(result.ir_text.find("load %record.Pair, ptr %pending.task.result") != std::string::npos);
    assert(result.ir_text.find("ret %record.Pair %tmp") != std::string::npos);
}

void test_emit_record_capture_cleanup_field_address() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_record_capture_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "record OtherPayload\n"
        "    public value: Int64\n"
        "\n"
        "implements Transferable for Payload\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "implements Transferable for OtherPayload\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function on_thread(value: Int64) -> Int64\n"
        "    let payload: Payload = Payload(value)\n"
        "    let other: OtherPayload = OtherPayload(value)\n"
        "    let worker = thread\n"
        "        payload.value + other.value\n"
        "\n"
        "    worker.join()\n"
    );

    assert(!result.has_errors());
    assert(result.planned_drop_actions.size() == 2);
    assert(result.planned_drop_actions[0].capture_name == "payload");
    assert(result.planned_drop_actions[0].symbol_name == "__orison_drop.Payload");
    assert(result.planned_drop_actions[0].source_type_name == "Payload");
    assert(result.planned_drop_actions[0].field_index == 0);
    assert(result.planned_drop_actions[0].discovery_line == 20);
    assert(result.planned_drop_actions[1].capture_name == "other");
    assert(result.planned_drop_actions[1].symbol_name == "__orison_drop.OtherPayload");
    assert(result.planned_drop_actions[1].source_type_name == "OtherPayload");
    assert(result.planned_drop_actions[1].field_index == 1);
    assert(result.planned_drop_actions[1].discovery_line == 20);
    auto action_report = result.planned_drop_action_report();
    assert(action_report.size() == 2);
    assert(
        action_report[0] ==
        "planned drop action __orison_drop.Payload for capture payload: Payload field 0 "
        "discovered at line 20 (metadata only)"
    );
    assert(
        action_report[1] ==
        "planned drop action __orison_drop.OtherPayload for capture other: OtherPayload field 1 "
        "discovered at line 20 (metadata only)"
    );
    assert(result.planned_drop_declarations.size() == 2);
    assert(result.planned_drop_declarations[0].symbol_name == "__orison_drop.Payload");
    assert(result.planned_drop_declarations[0].source_type_name == "Payload");
    assert(result.planned_drop_declarations[0].discovery_line == 20);
    assert(!result.planned_drop_declarations[0].emit_declaration);
    assert(result.planned_drop_declarations[1].symbol_name == "__orison_drop.OtherPayload");
    assert(result.planned_drop_declarations[1].source_type_name == "OtherPayload");
    assert(result.planned_drop_declarations[1].discovery_line == 20);
    assert(!result.planned_drop_declarations[1].emit_declaration);
    auto report = result.planned_drop_report();
    assert(report.size() == 2);
    assert(
        report[0] ==
        "planned drop __orison_drop.Payload for Payload discovered at line 20 (metadata only)"
    );
    assert(
        report[1] ==
        "planned drop __orison_drop.OtherPayload for OtherPayload discovered at line 20 (metadata only)"
    );
    assert(result.ir_text.find("%record.Payload = type { i64 }") != std::string::npos);
    assert(result.ir_text.find("%record.OtherPayload = type { i64 }") != std::string::npos);
    assert(
        result.ir_text.find(
            "%worker.thread.env = alloca { %record.Payload, %record.OtherPayload }"
        ) != std::string::npos
    );
    assert(result.ir_text.find("store %record.Payload %tmp0, ptr %tmp2") != std::string::npos);
    assert(result.ir_text.find("store %record.OtherPayload %tmp1, ptr %tmp3") != std::string::npos);
    assert(
        result.ir_text.find(
            "call ptr @__orison_thread_spawn(ptr @__orison_thread_thunk.on_thread.20.0"
        ) != std::string::npos
    );
    assert(
        result.ir_text.find(
            ", i64 8, ptr @__orison_thread_cleanup.on_thread.20.0)"
        ) != std::string::npos
    );
    assert(
        result.ir_text.find(
            "define private void @__orison_thread_cleanup.on_thread.20.0(ptr %environment) {\n"
            "entry:\n"
            "  %cleanup.field.0 = getelementptr { %record.Payload, %record.OtherPayload }, ptr %environment, i32 0, i32 0\n"
            "  ; cleanup candidate payload: Payload field 0 drop __orison_drop.Payload\n"
            "  %cleanup.field.1 = getelementptr { %record.Payload, %record.OtherPayload }, ptr %environment, i32 0, i32 1\n"
            "  ; cleanup candidate other: OtherPayload field 1 drop __orison_drop.OtherPayload\n"
            "  ret void\n"
            "}"
        ) != std::string::npos
    );
    assert(result.ir_text.find("declare void @__orison_drop.Payload(ptr)") == std::string::npos);
    assert(result.ir_text.find("call void @__orison_drop.Payload(ptr") == std::string::npos);
    assert(result.ir_text.find("declare void @__orison_drop.OtherPayload(ptr)") == std::string::npos);
    assert(result.ir_text.find("call void @__orison_drop.OtherPayload(ptr") == std::string::npos);
}

void test_emit_same_type_record_capture_drop_metadata_dedupes() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_same_record_capture_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "implements Transferable for Payload\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function on_thread(value: Int64) -> Int64\n"
        "    let left: Payload = Payload(value)\n"
        "    let right: Payload = Payload(value)\n"
        "    let worker = thread\n"
        "        left.value + right.value\n"
        "\n"
        "    worker.join()\n"
    );

    assert(!result.has_errors());
    assert(result.planned_drop_actions.size() == 2);
    assert(result.planned_drop_actions[0].capture_name == "left");
    assert(result.planned_drop_actions[0].symbol_name == "__orison_drop.Payload");
    assert(result.planned_drop_actions[0].field_index == 0);
    assert(result.planned_drop_actions[0].discovery_line == 13);
    assert(result.planned_drop_actions[1].capture_name == "right");
    assert(result.planned_drop_actions[1].symbol_name == "__orison_drop.Payload");
    assert(result.planned_drop_actions[1].field_index == 1);
    assert(result.planned_drop_actions[1].discovery_line == 13);
    auto action_report = result.planned_drop_action_report();
    assert(action_report.size() == 2);
    assert(
        action_report[0] ==
        "planned drop action __orison_drop.Payload for capture left: Payload field 0 "
        "discovered at line 13 (metadata only)"
    );
    assert(
        action_report[1] ==
        "planned drop action __orison_drop.Payload for capture right: Payload field 1 "
        "discovered at line 13 (metadata only)"
    );
    assert(result.planned_drop_declarations.size() == 1);
    assert(result.planned_drop_declarations.front().symbol_name == "__orison_drop.Payload");
    assert(result.planned_drop_declarations.front().source_type_name == "Payload");
    assert(result.planned_drop_declarations.front().discovery_line == 13);
    assert(!result.planned_drop_declarations.front().emit_declaration);
    assert(result.emitted_drop_declaration_report().empty());
    assert(result.ir_text.find("%worker.thread.env = alloca { %record.Payload, %record.Payload }") != std::string::npos);
    assert(
        result.ir_text.find(
            "define private void @__orison_thread_cleanup.on_thread.13.0(ptr %environment) {\n"
            "entry:\n"
            "  %cleanup.field.0 = getelementptr { %record.Payload, %record.Payload }, ptr %environment, i32 0, i32 0\n"
            "  ; cleanup candidate left: Payload field 0 drop __orison_drop.Payload\n"
            "  %cleanup.field.1 = getelementptr { %record.Payload, %record.Payload }, ptr %environment, i32 0, i32 1\n"
            "  ; cleanup candidate right: Payload field 1 drop __orison_drop.Payload\n"
            "  ret void\n"
            "}"
        ) != std::string::npos
    );
    assert(result.ir_text.find("declare void @__orison_drop.Payload(ptr)") == std::string::npos);
    assert(result.ir_text.find("call void @__orison_drop.Payload(ptr") == std::string::npos);
}

void test_emit_allowed_record_capture_drop_abi_calls() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_allowed_record_capture_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "implements Transferable for Payload\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function on_thread(value: Int64) -> Int64\n"
        "    let left: Payload = Payload(value)\n"
        "    let right: Payload = Payload(value)\n"
        "    let worker = thread\n"
        "        left.value + right.value\n"
        "\n"
        "    worker.join()\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_declared_drop_source_type_allowlist = {"Payload"},
        }
    );

    assert(!result.has_errors());
    assert(result.planned_drop_declarations.size() == 1);
    assert(result.planned_drop_declarations.front().symbol_name == "__orison_drop.Payload");
    assert(result.planned_drop_declarations.front().source_type_name == "Payload");
    assert(result.planned_drop_declarations.front().discovery_line == 13);
    assert(result.planned_drop_declarations.front().emit_declaration);
    auto emitted_report = result.emitted_drop_declaration_report();
    assert(emitted_report.size() == 1);
    assert(emitted_report.front() == "planned drop __orison_drop.Payload for Payload discovered at line 13");
    auto readiness_summary_report = result.drop_readiness_summary_report();
    assert(readiness_summary_report.size() == 1);
    assert(
        readiness_summary_report.front() ==
        "drop readiness summary semantic authorized 0 blocked 0 emitted declarations 1 cleanup authorized 1 blocked 0"
    );
    assert(result.ir_text.find("declare void @__orison_drop.Payload(ptr)\n\n") != std::string::npos);
    auto readiness_report = result.drop_readiness_snapshot_report();
    assert(readiness_report.size() == 3);
    assert(
        readiness_report[0] ==
        "drop readiness snapshot semantic authorizations 0 emitted declarations 1 cleanup authorizations 1"
    );
    assert(readiness_report[1] == "emitted declaration readiness __orison_drop.Payload for Payload");
    assert(readiness_report[2] == "cleanup readiness __orison_thread_cleanup.on_thread.13.0 authorized");
    assert(
        result.ir_text.find(
            "define private void @__orison_thread_cleanup.on_thread.13.0(ptr %environment) {\n"
            "entry:\n"
            "  %cleanup.field.0 = getelementptr { %record.Payload, %record.Payload }, ptr %environment, i32 0, i32 0\n"
            "  ; cleanup candidate left: Payload field 0 drop __orison_drop.Payload\n"
            "  call void @__orison_drop.Payload(ptr %cleanup.field.0)\n"
            "  %cleanup.field.1 = getelementptr { %record.Payload, %record.Payload }, ptr %environment, i32 0, i32 1\n"
            "  ; cleanup candidate right: Payload field 1 drop __orison_drop.Payload\n"
            "  call void @__orison_drop.Payload(ptr %cleanup.field.1)\n"
            "  ret void\n"
            "}"
        ) != std::string::npos
    );
}

void test_emit_semantic_authorized_record_capture_drop_abi_calls() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_semantic_allowed_record_capture_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "implements Transferable for Payload\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function on_thread(value: Int64) -> Int64\n"
        "    let left: Payload = Payload(value)\n"
        "    let right: Payload = Payload(value)\n"
        "    let worker = thread\n"
        "        left.value + right.value\n"
        "\n"
        "    worker.join()\n",
        orison::lowering::LlvmIrEmissionOptions {
            .semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = orison::semantics::PlannedDropSite {
                        .source_type_name = "Payload",
                        .abi_symbol_name = "__orison_drop.Payload",
                        .owner_name = "left",
                        .site_line = 11,
                    },
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
        }
    );

    assert(!result.has_errors());
    assert(result.planned_drop_declarations.size() == 1);
    assert(result.planned_drop_declarations.front().symbol_name == "__orison_drop.Payload");
    assert(result.planned_drop_declarations.front().source_type_name == "Payload");
    assert(result.planned_drop_declarations.front().discovery_line == 13);
    assert(result.planned_drop_declarations.front().emit_declaration);
    auto emitted_report = result.emitted_drop_declaration_report();
    assert(emitted_report.size() == 1);
    assert(emitted_report.front() == "planned drop __orison_drop.Payload for Payload discovered at line 13");
    auto readiness_summary_report = result.drop_readiness_summary_report();
    assert(readiness_summary_report.size() == 1);
    assert(
        readiness_summary_report.front() ==
        "drop readiness summary semantic authorized 1 blocked 0 emitted declarations 1 cleanup authorized 1 blocked 0"
    );
    assert(result.ir_text.find("declare void @__orison_drop.Payload(ptr)\n\n") != std::string::npos);
    assert(
        result.ir_text.find(
            "define private void @__orison_thread_cleanup.on_thread.13.0(ptr %environment) {\n"
            "entry:\n"
            "  %cleanup.field.0 = getelementptr { %record.Payload, %record.Payload }, ptr %environment, i32 0, i32 0\n"
            "  ; cleanup candidate left: Payload field 0 drop __orison_drop.Payload\n"
            "  call void @__orison_drop.Payload(ptr %cleanup.field.0)\n"
            "  %cleanup.field.1 = getelementptr { %record.Payload, %record.Payload }, ptr %environment, i32 0, i32 1\n"
            "  ; cleanup candidate right: Payload field 1 drop __orison_drop.Payload\n"
            "  call void @__orison_drop.Payload(ptr %cleanup.field.1)\n"
            "  ret void\n"
            "}"
        ) != std::string::npos
    );
}

void test_reject_partial_record_capture_drop_abi_calls() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_partial_record_capture_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "record OtherPayload\n"
        "    public value: Int64\n"
        "\n"
        "implements Transferable for Payload\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "implements Transferable for OtherPayload\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function on_thread(value: Int64) -> Int64\n"
        "    let payload: Payload = Payload(value)\n"
        "    let other: OtherPayload = OtherPayload(value)\n"
        "    let worker = thread\n"
        "        payload.value + other.value\n"
        "\n"
        "    worker.join()\n",
        orison::lowering::LlvmIrEmissionOptions {
            .test_only_declared_drop_source_type_allowlist = {"Payload"},
        }
    );

    assert(!result.has_errors());
    assert(result.planned_drop_declarations.size() == 1);
    assert(result.planned_drop_declarations.front().symbol_name == "__orison_drop.Payload");
    assert(result.planned_drop_declarations.front().source_type_name == "Payload");
    assert(result.planned_drop_declarations.front().discovery_line == 20);
    assert(result.planned_drop_declarations.front().emit_declaration);
    assert(result.ir_text.find("declare void @__orison_drop.Payload(ptr)\n\n") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_drop.OtherPayload(ptr)") == std::string::npos);
    auto readiness_report = result.drop_readiness_snapshot_report();
    assert(readiness_report.size() == 3);
    assert(
        readiness_report[0] ==
        "drop readiness snapshot semantic authorizations 0 emitted declarations 1 cleanup authorizations 1"
    );
    assert(readiness_report[1] == "emitted declaration readiness __orison_drop.Payload for Payload");
    assert(
        readiness_report[2] ==
        "cleanup readiness __orison_thread_cleanup.on_thread.20.0 blocked semantic blockers 0 missing declarations 1"
    );
    assert(
        result.ir_text.find(
            "define private void @__orison_thread_cleanup.on_thread.20.0(ptr %environment) {\n"
            "entry:\n"
            "  %cleanup.field.0 = getelementptr { %record.Payload, %record.OtherPayload }, ptr %environment, i32 0, i32 0\n"
            "  ; cleanup candidate payload: Payload field 0 drop __orison_drop.Payload\n"
            "  %cleanup.field.1 = getelementptr { %record.Payload, %record.OtherPayload }, ptr %environment, i32 0, i32 1\n"
            "  ; cleanup candidate other: OtherPayload field 1 drop __orison_drop.OtherPayload\n"
            "  ret void\n"
            "}"
        ) != std::string::npos
    );
    assert(result.ir_text.find("call void @__orison_drop.Payload(ptr") == std::string::npos);
    assert(result.ir_text.find("call void @__orison_drop.OtherPayload(ptr") == std::string::npos);
}

void test_reject_partial_semantic_authorized_record_capture_drop_abi_calls() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_partial_semantic_record_capture_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record Payload\n"
        "    public value: Int64\n"
        "\n"
        "record OtherPayload\n"
        "    public value: Int64\n"
        "\n"
        "implements Transferable for Payload\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "implements Transferable for OtherPayload\n"
        "    function placeholder(this: shared This) -> Unit\n"
        "        return\n"
        "\n"
        "function on_thread(value: Int64) -> Int64\n"
        "    let payload: Payload = Payload(value)\n"
        "    let other: OtherPayload = OtherPayload(value)\n"
        "    let worker = thread\n"
        "        payload.value + other.value\n"
        "\n"
        "    worker.join()\n",
        orison::lowering::LlvmIrEmissionOptions {
            .semantic_drop_lowering_authorizations = {
                orison::semantics::DropLoweringAuthorization {
                    .site = orison::semantics::PlannedDropSite {
                        .source_type_name = "Payload",
                        .abi_symbol_name = "__orison_drop.Payload",
                        .owner_name = "payload",
                        .site_line = 18,
                    },
                    .semantic_resolved = true,
                    .source_drop_lowering_enabled = true,
                    .authorized = true,
                },
            },
        }
    );

    assert(!result.has_errors());
    assert(result.planned_drop_declarations.size() == 2);
    assert(result.planned_drop_declarations[0].symbol_name == "__orison_drop.Payload");
    assert(result.planned_drop_declarations[0].source_type_name == "Payload");
    assert(result.planned_drop_declarations[0].discovery_line == 20);
    assert(result.planned_drop_declarations[0].emit_declaration);
    assert(result.planned_drop_declarations[1].symbol_name == "__orison_drop.OtherPayload");
    assert(result.planned_drop_declarations[1].source_type_name == "OtherPayload");
    assert(result.planned_drop_declarations[1].discovery_line == 20);
    assert(!result.planned_drop_declarations[1].emit_declaration);
    auto emitted_report = result.emitted_drop_declaration_report();
    assert(emitted_report.size() == 1);
    assert(emitted_report.front() == "planned drop __orison_drop.Payload for Payload discovered at line 20");
    auto readiness_summary_report = result.drop_readiness_summary_report();
    assert(readiness_summary_report.size() == 1);
    assert(
        readiness_summary_report.front() ==
        "drop readiness summary semantic authorized 1 blocked 0 emitted declarations 1 cleanup authorized 0 blocked 1"
    );
    assert(result.ir_text.find("declare void @__orison_drop.Payload(ptr)\n\n") != std::string::npos);
    assert(result.ir_text.find("declare void @__orison_drop.OtherPayload(ptr)") == std::string::npos);
    assert(
        result.ir_text.find(
            "define private void @__orison_thread_cleanup.on_thread.20.0(ptr %environment) {\n"
            "entry:\n"
            "  %cleanup.field.0 = getelementptr { %record.Payload, %record.OtherPayload }, ptr %environment, i32 0, i32 0\n"
            "  ; cleanup candidate payload: Payload field 0 drop __orison_drop.Payload\n"
            "  %cleanup.field.1 = getelementptr { %record.Payload, %record.OtherPayload }, ptr %environment, i32 0, i32 1\n"
            "  ; cleanup candidate other: OtherPayload field 1 drop __orison_drop.OtherPayload\n"
            "  ret void\n"
            "}"
        ) != std::string::npos
    );
    assert(result.ir_text.find("call void @__orison_drop.Payload(ptr") == std::string::npos);
    assert(result.ir_text.find("call void @__orison_drop.OtherPayload(ptr") == std::string::npos);
}

void test_reject_unsupported_final_if_arm_expression() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_unsupported_final_if_arm.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function same(flag: Bool, left: Bool, right: Bool) -> Bool\n"
        "    if flag\n"
        "        left < right\n"
        "    else\n"
        "        false\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not yet support this final control-flow statement: "
        "if then arm lowering failed: unsupported operator: <"
    );
}

void test_reject_unsupported_final_switch_case_expression() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_unsupported_final_switch_case.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function same(flag: Bool, left: Bool, right: Bool) -> Bool\n"
        "    switch flag\n"
        "        true => left < right\n"
        "        false => false\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not yet support this final control-flow statement: "
        "switch case lowering failed: unsupported operator: <"
    );
}

void test_emit_nested_defer_cleanup_defers() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_nested_defer_cleanup.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
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
        "    return\n"
    );

    assert(!result.has_errors());
    auto const function_pos = result.ir_text.find("define void @cleanup_then_cleanup");
    assert(function_pos != std::string::npos);
    auto const function_ir = result.ir_text.substr(function_pos);
    auto const first_call = function_ir.find("call void @observe(i32 2)");
    assert(first_call != std::string::npos);
    auto const second_call = function_ir.find("call void @observe(i32 3)", first_call + 1);
    assert(second_call != std::string::npos);
    auto const third_call = function_ir.find("call void @observe(i32 1)", second_call + 1);
    assert(third_call != std::string::npos);
    auto const ret_void = function_ir.find("ret void");
    assert(ret_void != std::string::npos);
    assert(first_call < second_call);
    assert(second_call < third_call);
    assert(third_call < ret_void);
    std::filesystem::remove(path);
}

void test_emit_nested_defer_cleanup_multiple_defers() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nested_defer_cleanup_multi.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
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
        "    return\n"
    );

    assert(!result.has_errors());
    auto const function_pos = result.ir_text.find("define void @cleanup_then_cleanup");
    assert(function_pos != std::string::npos);
    auto const function_ir = result.ir_text.substr(function_pos);
    auto const first_call = function_ir.find("call void @observe(i32 2)");
    assert(first_call != std::string::npos);
    auto const second_call = function_ir.find("call void @observe(i32 4)", first_call + 1);
    assert(second_call != std::string::npos);
    auto const third_call = function_ir.find("call void @observe(i32 3)", second_call + 1);
    assert(third_call != std::string::npos);
    auto const fourth_call = function_ir.find("call void @observe(i32 1)", third_call + 1);
    assert(fourth_call != std::string::npos);
    auto const ret_void = function_ir.find("ret void");
    assert(ret_void != std::string::npos);
    assert(first_call < second_call);
    assert(second_call < third_call);
    assert(third_call < fourth_call);
    assert(fourth_call < ret_void);
    std::filesystem::remove(path);
}

void test_emit_nested_defer_cleanup_on_loop_control() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nested_defer_loop_control.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
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
        "    return\n"
    );

    assert(!result.has_errors());

    auto const break_function_pos = result.ir_text.find("define void @cleanup_on_break");
    assert(break_function_pos != std::string::npos);
    auto const break_function_ir = result.ir_text.substr(break_function_pos);
    auto const break_first_call = break_function_ir.find("call void @observe(i32 2)");
    assert(break_first_call != std::string::npos);
    auto const break_second_call = break_function_ir.find(
        "call void @observe(i32 3)",
        break_first_call + 1
    );
    assert(break_second_call != std::string::npos);
    auto const break_third_call = break_function_ir.find(
        "call void @observe(i32 1)",
        break_second_call + 1
    );
    assert(break_third_call != std::string::npos);
    auto const break_exit = break_function_ir.find("br label %while.exit.0");
    assert(break_exit != std::string::npos);
    assert(break_first_call < break_second_call);
    assert(break_second_call < break_third_call);
    assert(break_third_call < break_exit);

    auto const continue_function_pos = result.ir_text.find("define void @cleanup_on_continue");
    assert(continue_function_pos != std::string::npos);
    auto const continue_function_ir = result.ir_text.substr(continue_function_pos);
    auto const continue_first_call = continue_function_ir.find("call void @observe(i32 2)");
    assert(continue_first_call != std::string::npos);
    auto const continue_second_call = continue_function_ir.find(
        "call void @observe(i32 3)",
        continue_first_call + 1
    );
    assert(continue_second_call != std::string::npos);
    auto const continue_third_call = continue_function_ir.find(
        "call void @observe(i32 1)",
        continue_second_call + 1
    );
    assert(continue_third_call != std::string::npos);
    auto const continue_exit = continue_function_ir.find(
        "br label %while.condition.0",
        continue_third_call + 1
    );
    assert(continue_exit != std::string::npos);
    assert(continue_first_call < continue_second_call);
    assert(continue_second_call < continue_third_call);
    assert(continue_third_call < continue_exit);
    std::filesystem::remove(path);
}

void test_emit_nested_defer_cleanup_on_for_and_repeat() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nested_defer_for_repeat.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
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
        "    return\n"
    );

    assert(!result.has_errors());

    auto const for_function_pos = result.ir_text.find("define void @cleanup_on_for_continue");
    assert(for_function_pos != std::string::npos);
    auto const for_function_ir = result.ir_text.substr(for_function_pos);
    auto const for_first_call = for_function_ir.find("call void @observe(i32 2)");
    assert(for_first_call != std::string::npos);
    auto const for_second_call = for_function_ir.find("call void @observe(i32 3)", for_first_call + 1);
    assert(for_second_call != std::string::npos);
    auto const for_third_call = for_function_ir.find("call void @observe(i32 1)", for_second_call + 1);
    assert(for_third_call != std::string::npos);
    auto const for_continue_exit = for_function_ir.find("br label %for.iteration.0.1");
    assert(for_continue_exit != std::string::npos);
    assert(for_first_call < for_second_call);
    assert(for_second_call < for_third_call);
    assert(for_third_call < for_continue_exit);

    auto const repeat_function_pos = result.ir_text.find("define void @cleanup_on_repeat_break");
    assert(repeat_function_pos != std::string::npos);
    auto const repeat_function_ir = result.ir_text.substr(repeat_function_pos);
    auto const repeat_first_call = repeat_function_ir.find("call void @observe(i32 2)");
    assert(repeat_first_call != std::string::npos);
    auto const repeat_second_call = repeat_function_ir.find(
        "call void @observe(i32 3)",
        repeat_first_call + 1
    );
    assert(repeat_second_call != std::string::npos);
    auto const repeat_third_call = repeat_function_ir.find(
        "call void @observe(i32 1)",
        repeat_second_call + 1
    );
    assert(repeat_third_call != std::string::npos);
    auto const repeat_exit = repeat_function_ir.find("br label %repeat.exit.0");
    assert(repeat_exit != std::string::npos);
    assert(repeat_first_call < repeat_second_call);
    assert(repeat_second_call < repeat_third_call);
    assert(repeat_third_call < repeat_exit);
    std::filesystem::remove(path);
}

void test_emit_nested_defer_cleanup_in_unsafe_block() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nested_defer_unsafe.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
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
        "    return\n"
    );

    assert(!result.has_errors());

    auto const break_function_pos = result.ir_text.find("define void @cleanup_on_unsafe_break");
    assert(break_function_pos != std::string::npos);
    auto const break_function_ir = result.ir_text.substr(break_function_pos);
    auto const break_first_call = break_function_ir.find("call void @observe(i32 2)");
    assert(break_first_call != std::string::npos);
    auto const break_second_call = break_function_ir.find("call void @observe(i32 3)", break_first_call + 1);
    assert(break_second_call != std::string::npos);
    auto const break_third_call = break_function_ir.find("call void @observe(i32 1)", break_second_call + 1);
    assert(break_third_call != std::string::npos);
    auto const break_exit = break_function_ir.find("br label %while.exit.0");
    assert(break_exit != std::string::npos);
    assert(break_first_call < break_second_call);
    assert(break_second_call < break_third_call);
    assert(break_third_call < break_exit);

    auto const continue_function_pos = result.ir_text.find("define void @cleanup_on_unsafe_continue");
    assert(continue_function_pos != std::string::npos);
    auto const continue_function_ir = result.ir_text.substr(continue_function_pos);
    auto const continue_first_call = continue_function_ir.find("call void @observe(i32 2)");
    assert(continue_first_call != std::string::npos);
    auto const continue_second_call = continue_function_ir.find(
        "call void @observe(i32 3)",
        continue_first_call + 1
    );
    assert(continue_second_call != std::string::npos);
    auto const continue_third_call = continue_function_ir.find(
        "call void @observe(i32 1)",
        continue_second_call + 1
    );
    assert(continue_third_call != std::string::npos);
    auto const continue_exit = continue_function_ir.find(
        "br label %while.condition.0",
        continue_third_call + 1
    );
    assert(continue_exit != std::string::npos);
    assert(continue_first_call < continue_second_call);
    assert(continue_second_call < continue_third_call);
    assert(continue_third_call < continue_exit);
    std::filesystem::remove(path);
}

void test_emit_nested_defer_cleanup_in_for_and_repeat_unsafe_blocks() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nested_defer_for_repeat_unsafe.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
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
        "    return\n"
    );

    assert(!result.has_errors());

    auto const for_function_pos = result.ir_text.find("define void @cleanup_on_for_unsafe_continue");
    assert(for_function_pos != std::string::npos);
    auto const for_function_ir = result.ir_text.substr(for_function_pos);
    auto const for_first_call = for_function_ir.find("call void @observe(i32 2)");
    assert(for_first_call != std::string::npos);
    auto const for_second_call = for_function_ir.find("call void @observe(i32 3)", for_first_call + 1);
    assert(for_second_call != std::string::npos);
    auto const for_third_call = for_function_ir.find("call void @observe(i32 1)", for_second_call + 1);
    assert(for_third_call != std::string::npos);
    auto const for_exit = for_function_ir.find("br label %for.iteration.0.1");
    assert(for_exit != std::string::npos);
    assert(for_first_call < for_second_call);
    assert(for_second_call < for_third_call);
    assert(for_third_call < for_exit);

    auto const repeat_function_pos = result.ir_text.find("define void @cleanup_on_repeat_unsafe_break");
    assert(repeat_function_pos != std::string::npos);
    auto const repeat_function_ir = result.ir_text.substr(repeat_function_pos);
    auto const repeat_first_call = repeat_function_ir.find("call void @observe(i32 2)");
    assert(repeat_first_call != std::string::npos);
    auto const repeat_second_call = repeat_function_ir.find(
        "call void @observe(i32 3)",
        repeat_first_call + 1
    );
    assert(repeat_second_call != std::string::npos);
    auto const repeat_third_call = repeat_function_ir.find(
        "call void @observe(i32 1)",
        repeat_second_call + 1
    );
    assert(repeat_third_call != std::string::npos);
    auto const repeat_exit = repeat_function_ir.find("br label %repeat.exit.0");
    assert(repeat_exit != std::string::npos);
    assert(repeat_first_call < repeat_second_call);
    assert(repeat_second_call < repeat_third_call);
    assert(repeat_third_call < repeat_exit);
    std::filesystem::remove(path);
}

void test_emit_nonvoid_repeat_for_unsafe_prefixes() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_nonvoid_repeat_for_unsafe.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function main() -> UInt32\n"
        "    repeat\n"
        "        observe(1 as UInt32)\n"
        "    while false\n"
        "    for item in [2 as UInt32, 3 as UInt32]\n"
        "        observe(item)\n"
        "    unsafe\n"
        "        observe(4 as UInt32)\n"
        "    5 as UInt32\n"
    );

    assert(!result.has_errors());
    auto const main_pos = result.ir_text.find("define i32 @main()");
    assert(main_pos != std::string::npos);
    auto const main_ir = result.ir_text.substr(main_pos);
    assert(main_ir.find("repeat.body") != std::string::npos);
    assert(main_ir.find("for.iteration") != std::string::npos);
    assert(main_ir.find("call void @observe(i32 4)") != std::string::npos);
    assert(main_ir.find("ret i32 5") != std::string::npos);
    std::filesystem::remove(path);
}

void test_emit_repeat_body_partial_if_and_switch_flow() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_repeat_body_partial_if_switch.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(value: UInt32) -> Unit\n"
        "    var current = value\n"
        "    repeat\n"
        "        if current == 0 as UInt32\n"
        "            break\n"
        "        switch current\n"
        "            1 => continue\n"
        "            default => current = current + 1 as UInt32\n"
        "    while current < 3 as UInt32\n"
        "    return\n"
    );

    assert(!result.has_errors());
    auto const main_pos = result.ir_text.find("define void @main");
    assert(main_pos != std::string::npos);
    auto const main_ir = result.ir_text.substr(main_pos);
    assert(main_ir.find("repeat.body.0:") != std::string::npos);
    assert(main_ir.find("if.then.1:") != std::string::npos);
    assert(main_ir.find("if.merge.1:") != std::string::npos);
    assert(main_ir.find("switch.case.2.0:") != std::string::npos);
    assert(main_ir.find("switch.merge.2:") != std::string::npos);
    assert(main_ir.find("br label %repeat.exit.0") != std::string::npos);
    assert(main_ir.find("br label %repeat.condition.0") != std::string::npos);
    std::filesystem::remove(path);
}

void test_emit_for_body_partial_if_and_switch_flow() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_for_body_partial_if_switch.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function observe(value: UInt32) -> Unit\n"
        "    return\n"
        "\n"
        "function main() -> Unit\n"
        "    for item in [0 as UInt32, 1 as UInt32]\n"
        "        if item == 0 as UInt32\n"
        "            continue\n"
        "        switch item\n"
        "            1 => break\n"
        "            default => observe(item)\n"
        "    return\n"
    );

    assert(!result.has_errors());
    auto const main_pos = result.ir_text.find("define void @main");
    assert(main_pos != std::string::npos);
    auto const main_ir = result.ir_text.substr(main_pos);
    assert(main_ir.find("for.iteration.0.0:") != std::string::npos);
    assert(main_ir.find("for.iteration.0.1:") != std::string::npos);
    assert(main_ir.find("if.then.1:") != std::string::npos);
    assert(main_ir.find("if.merge.1:") != std::string::npos);
    assert(main_ir.find("switch.case.2.0:") != std::string::npos);
    assert(main_ir.find("switch.merge.2:") != std::string::npos);
    assert(main_ir.find("br label %for.iteration.0.1") != std::string::npos);
    assert(main_ir.find("br label %for.exit.0") != std::string::npos);
    std::filesystem::remove(path);
}

void test_reject_statements_after_terminating_nonvoid_statement() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_lowering_nonvoid_terminating_if.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main(flag: Bool) -> UInt32\n"
        "    if flag\n"
        "        return 4 as UInt32\n"
        "    else\n"
        "        return 5 as UInt32\n"
        "    6 as UInt32\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not yet support statements after a terminating non-Unit statement"
    );
    std::filesystem::remove(path);
}

void test_emit_raw_mmio_intrinsics() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_raw_mmio.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "record UartRegisters\n"
        "    data: UInt32\n"
        "    status: UInt32\n"
        "\n"
        "record Buffer\n"
        "    bytes: Array<Byte, 4>\n"
        "\n"
        "record Log\n"
        "    entries: Array<UartRegisters, 2>\n"
        "\n"
        "record Matrix\n"
        "    rows: Array<Array<Byte, 4>, 2>\n"
        "\n"
        "record Device\n"
        "    registers: UartRegisters\n"
        "    buffer: Buffer\n"
        "\n"
        "extend Device\n"
        "    function consume_word_pointer(this: shared This, pointer: Pointer<UInt32>) -> UInt32\n"
        "        0 as UInt32\n"
        "\n"
        "unsafe function read_word(address: Address) -> UInt32\n"
        "    let pointer: Pointer<UInt32> = Pointer(address)\n"
        "    raw_read(pointer)\n"
        "\n"
        "unsafe function read_next_word(base: Pointer<UInt32>) -> UInt32\n"
        "    let next = raw_offset(base, 1)\n"
        "    raw_read(next)\n"
        "\n"
        "unsafe function read_local_status_pointer() -> UInt32\n"
        "    var regs = UartRegisters(0 as UInt32, 1 as UInt32)\n"
        "    let pointer = Pointer(address_of(regs.status))\n"
        "    raw_read(pointer)\n"
        "\n"
        "unsafe function read_local_status_offset() -> UInt32\n"
        "    var regs = UartRegisters(0 as UInt32, 1 as UInt32)\n"
        "    let pointer = raw_offset(Pointer(address_of(regs.status)), 1 as UInt64)\n"
        "    raw_read(pointer)\n"
        "\n"
        "unsafe function consume_word_pointer(pointer: Pointer<UInt32>) -> UInt32\n"
        "    raw_read(pointer)\n"
        "\n"
        "unsafe function read_local_status_call_argument() -> UInt32\n"
        "    var regs = UartRegisters(0 as UInt32, 1 as UInt32)\n"
        "    consume_word_pointer(raw_offset(Pointer(address_of(regs.status)), 1 as UInt64))\n"
        "\n"
        "unsafe function read_local_status_member_argument() -> UInt32\n"
        "    let device = Device(UartRegisters(0 as UInt32, 1 as UInt32), Buffer([0 as Byte, 0 as Byte, 0 as Byte, 0 as Byte]))\n"
        "    var regs = UartRegisters(0 as UInt32, 1 as UInt32)\n"
        "    device.consume_word_pointer(raw_offset(Pointer(address_of(regs.status)), 1 as UInt64))\n"
        "\n"
        "unsafe function write_word(address: Address, value: UInt32) -> Unit\n"
        "    let pointer: Pointer<UInt32> = Pointer(address)\n"
        "    raw_write(pointer, value)\n"
        "\n"
        "public function clear(address: Address) -> Unit\n"
        "    unsafe\n"
        "        let pointer: Pointer<Byte> = Pointer(address)\n"
        "        volatile_write(pointer, 0 as Byte)\n"
        "\n"
        "unsafe function status_address(regs: Pointer<UartRegisters>) -> Address\n"
        "    address_of(regs.status)\n"
        "\n"
        "unsafe function nested_status_address(device: Pointer<Device>) -> Address\n"
        "    address_of(device.registers.status)\n"
        "\n"
        "unsafe function byte_address(buffer: Pointer<Buffer>, index: UInt64) -> Address\n"
        "    address_of(buffer.bytes[index])\n"
        "\n"
        "unsafe function nested_byte_address(device: Pointer<Device>, index: UInt64) -> Address\n"
        "    address_of(device.buffer.bytes[index])\n"
        "\n"
        "unsafe function entry_status_address(log: Pointer<Log>, index: UInt64) -> Address\n"
        "    address_of(log.entries[index].status)\n"
        "\n"
        "unsafe function read_entry_status_pointer(log: Pointer<Log>, index: UInt64) -> UInt32\n"
        "    let pointer = Pointer(address_of(log.entries[index].status))\n"
        "    raw_read(pointer)\n"
        "\n"
        "unsafe function write_entry_status_pointer(log: Pointer<Log>, index: UInt64, value: UInt32) -> Unit\n"
        "    let pointer = Pointer(address_of(log.entries[index].status))\n"
        "    raw_write(pointer, value)\n"
        "\n"
        "unsafe function volatile_read_entry_status_pointer(log: Pointer<Log>, index: UInt64) -> UInt32\n"
        "    let pointer = Pointer(address_of(log.entries[index].status))\n"
        "    volatile_read(pointer)\n"
        "\n"
        "unsafe function volatile_write_entry_status_pointer(log: Pointer<Log>, index: UInt64, value: UInt32) -> Unit\n"
        "    let pointer = Pointer(address_of(log.entries[index].status))\n"
        "    volatile_write(pointer, value)\n"
        "\n"
        "unsafe function read_entry_status_offset(log: Pointer<Log>, index: UInt64) -> UInt32\n"
        "    let pointer = raw_offset(Pointer(address_of(log.entries[index].status)), 1 as UInt64)\n"
        "    raw_read(pointer)\n"
        "\n"
        "unsafe function volatile_write_entry_status_offset(log: Pointer<Log>, index: UInt64, value: UInt32) -> Unit\n"
        "    let pointer = raw_offset(Pointer(address_of(log.entries[index].status)), 1 as UInt64)\n"
        "    volatile_write(pointer, value)\n"
        "\n"
        "unsafe function matrix_byte_address(matrix: Pointer<Matrix>, index: UInt64, inner: UInt64) -> Address\n"
        "    address_of(matrix.rows[index][inner])\n"
        "\n"
        "unsafe function local_status_address() -> Address\n"
        "    var regs = UartRegisters(0 as UInt32, 1 as UInt32)\n"
        "    address_of(regs.status)\n"
        "\n"
        "unsafe function local_byte_address(index: UInt64) -> Address\n"
        "    var buffer = Buffer([1, 2, 3, 4])\n"
        "    address_of(buffer.bytes[index])\n"
        "\n"
        "unsafe function local_entry_status_address(index: UInt64) -> Address\n"
        "    var log = Log([UartRegisters(1 as UInt32, 2 as UInt32), UartRegisters(3 as UInt32, 4 as UInt32)])\n"
        "    address_of(log.entries[index].status)\n"
        "\n"
        "unsafe function local_matrix_byte_address(index: UInt64, inner: UInt64) -> Address\n"
        "    var matrix = Matrix([[1, 2, 3, 4], [5, 6, 7, 8]])\n"
        "    address_of(matrix.rows[index][inner])\n"
        "\n"
        "unsafe function local_array_byte_address(index: UInt64) -> Address\n"
        "    var bytes: Array<Byte, 4> = [1, 2, 3, 4]\n"
        "    address_of(bytes[index])\n"
        "\n"
        "unsafe function reassign_local_status() -> Address\n"
        "    var regs = UartRegisters(0 as UInt32, 1 as UInt32)\n"
        "    regs = UartRegisters(2 as UInt32, 3 as UInt32)\n"
        "    address_of(regs.status)\n"
        "\n"
        "unsafe function reassign_local_array_byte(index: UInt64) -> Address\n"
        "    var bytes: Array<Byte, 4> = [1, 2, 3, 4]\n"
        "    bytes = [5, 6, 7, 8]\n"
        "    address_of(bytes[index])\n"
        "\n"
        "unsafe function assign_local_status() -> Unit\n"
        "    var regs = UartRegisters(0 as UInt32, 1 as UInt32)\n"
        "    regs.status = 4 as UInt32\n"
        "    return\n"
        "\n"
        "unsafe function compound_assign_local_status() -> Unit\n"
        "    var regs = UartRegisters(0 as UInt32, 1 as UInt32)\n"
        "    regs.status += 4 as UInt32\n"
        "    return\n"
        "\n"
        "unsafe function assign_local_array_byte(index: UInt64) -> Unit\n"
        "    var bytes: Array<Byte, 4> = [1, 2, 3, 4]\n"
        "    bytes[index] = 9\n"
        "    return\n"
        "\n"
        "unsafe function compound_assign_local_array_byte(index: UInt64) -> Unit\n"
        "    var bytes: Array<Byte, 4> = [1, 2, 3, 4]\n"
        "    bytes[index] += 9\n"
        "    return\n"
        "\n"
        "unsafe function assign_pointer_aggregate(regs: Pointer<UartRegisters>, buffer: Pointer<Buffer>, index: UInt64) -> Unit\n"
        "    regs.status = 4 as UInt32\n"
        "    buffer.bytes[index] = 7\n"
        "    return\n"
        "\n"
        "unsafe function compound_assign_pointer_aggregate(regs: Pointer<UartRegisters>, buffer: Pointer<Buffer>, index: UInt64) -> Unit\n"
        "    regs.status += 4 as UInt32\n"
        "    buffer.bytes[index] += 7\n"
        "    return\n"
        "\n"
        "unsafe function assign_nested_pointer_aggregate(log: Pointer<Log>, matrix: Pointer<Matrix>, index: UInt64, inner: UInt64) -> Unit\n"
        "    log.entries[index].status = 8 as UInt32\n"
        "    matrix.rows[index][inner] = 1 as Byte\n"
        "    return\n"
    );

    assert(!result.has_errors());
    assert(result.ir_text.find("%record.UartRegisters = type { i32, i32 }") != std::string::npos);
    assert(result.ir_text.find("%record.Buffer = type { [4 x i8] }") != std::string::npos);
    assert(result.ir_text.find("%record.Log = type { [2 x %record.UartRegisters] }") != std::string::npos);
    assert(result.ir_text.find("%record.Matrix = type { [2 x [4 x i8]] }") != std::string::npos);
    assert(
        result.ir_text.find("%record.Device = type { %record.UartRegisters, %record.Buffer }") !=
        std::string::npos
    );
    assert(result.ir_text.find("define i32 @read_word(i64 %address)") != std::string::npos);
    assert(result.ir_text.find("load i32, ptr") != std::string::npos);
    assert(result.ir_text.find("define void @write_word(i64 %address, i32 %value)") != std::string::npos);
    assert(result.ir_text.find("store i32 %value, ptr") != std::string::npos);
    assert(result.ir_text.find("define void @clear(i64 %address)") != std::string::npos);
    assert(result.ir_text.find("store volatile i8 0, ptr") != std::string::npos);
    assert(result.ir_text.find("define i64 @status_address(ptr %regs)") != std::string::npos);
    assert(
        result.ir_text.find("getelementptr %record.UartRegisters, ptr %regs, i32 0, i32 1") !=
        std::string::npos
    );
    assert(result.ir_text.find("define i64 @nested_status_address(ptr %device)") != std::string::npos);
    assert(
        result.ir_text.find("getelementptr %record.Device, ptr %device, i32 0, i32 0") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.UartRegisters, ptr %tmp") !=
        std::string::npos
    );
    assert(result.ir_text.find("define i64 @byte_address(ptr %buffer, i64 %index)") != std::string::npos);
    assert(
        result.ir_text.find("getelementptr %record.Buffer, ptr %buffer, i32 0, i32 0") !=
        std::string::npos
    );
    assert(result.ir_text.find("getelementptr [4 x i8], ptr %tmp") != std::string::npos);
    assert(
        result.ir_text.find("define i64 @nested_byte_address(ptr %device, i64 %index)") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.Device, ptr %device, i32 0, i32 1") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("define i64 @entry_status_address(ptr %log, i64 %index)") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.Log, ptr %log, i32 0, i32 0") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr [2 x %record.UartRegisters], ptr %tmp") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("define i32 @read_entry_status_pointer(ptr %log, i64 %index)") !=
        std::string::npos
    );
    assert(result.ir_text.find("load i32, ptr %pointer") != std::string::npos);
    assert(
        result.ir_text.find("define void @write_entry_status_pointer(ptr %log, i64 %index, i32 %value)") !=
        std::string::npos
    );
    assert(result.ir_text.find("store i32 %value, ptr") != std::string::npos);
    assert(
        result.ir_text.find("define i32 @volatile_read_entry_status_pointer(ptr %log, i64 %index)") !=
        std::string::npos
    );
    assert(result.ir_text.find("load volatile i32, ptr") != std::string::npos);
    assert(
        result.ir_text.find("define void @volatile_write_entry_status_pointer(ptr %log, i64 %index, i32 %value)") !=
        std::string::npos
    );
    assert(result.ir_text.find("store volatile i32 %value, ptr") != std::string::npos);
    assert(
        result.ir_text.find("define i32 @read_entry_status_offset(ptr %log, i64 %index)") !=
        std::string::npos
    );
    assert(result.ir_text.find("getelementptr i32, ptr") != std::string::npos);
    assert(
        result.ir_text.find("define void @volatile_write_entry_status_offset(ptr %log, i64 %index, i32 %value)") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("define i64 @matrix_byte_address(ptr %matrix, i64 %index, i64 %inner)") !=
        std::string::npos
    );
    assert(result.ir_text.find("define i64 @local_status_address()") != std::string::npos);
    assert(
        result.ir_text.find("%tmp0 = insertvalue %record.UartRegisters undef, i32 0, 0") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("%tmp1 = insertvalue %record.UartRegisters %tmp0, i32 1, 1") !=
        std::string::npos
    );
    assert(result.ir_text.find("%regs.addr = alloca %record.UartRegisters") != std::string::npos);
    assert(
        result.ir_text.find("store %record.UartRegisters %tmp1, ptr %regs.addr") != std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.UartRegisters, ptr %regs.addr, i32 0, i32 1") !=
        std::string::npos
    );
    assert(result.ir_text.find("define i64 @local_byte_address(i64 %index)") != std::string::npos);
    assert(result.ir_text.find("store %record.Buffer %tmp") != std::string::npos);
    assert(result.ir_text.find("define i64 @local_entry_status_address(i64 %index)") != std::string::npos);
    assert(result.ir_text.find("store %record.Log %tmp") != std::string::npos);
    assert(
        result.ir_text.find("define i64 @local_matrix_byte_address(i64 %index, i64 %inner)") !=
        std::string::npos
    );
    assert(result.ir_text.find("store %record.Matrix %tmp") != std::string::npos);
    assert(result.ir_text.find("define i64 @local_array_byte_address(i64 %index)") != std::string::npos);
    assert(result.ir_text.find("%bytes.addr = alloca [4 x i8]") != std::string::npos);
    assert(result.ir_text.find("store [4 x i8] %tmp") != std::string::npos);
    assert(result.ir_text.find("getelementptr [4 x i8], ptr %bytes.addr, i64 0, i64 %index") != std::string::npos);
    assert(result.ir_text.find("define i64 @reassign_local_status()") != std::string::npos);
    assert(result.ir_text.find("store %record.UartRegisters %tmp") != std::string::npos);
    assert(result.ir_text.find("define i64 @reassign_local_array_byte(i64 %index)") != std::string::npos);
    assert(result.ir_text.find("store [4 x i8] %tmp") != std::string::npos);
    assert(result.ir_text.find("define void @assign_local_status()") != std::string::npos);
    assert(result.ir_text.find("store i32 4, ptr %tmp") != std::string::npos);
    assert(result.ir_text.find("define void @compound_assign_local_status()") != std::string::npos);
    assert(result.ir_text.find(" = load i32, ptr %tmp") != std::string::npos);
    assert(result.ir_text.find(" = add i32 ") != std::string::npos);
    assert(result.ir_text.find("define void @assign_local_array_byte(i64 %index)") != std::string::npos);
    assert(result.ir_text.find("store i8 9, ptr %tmp") != std::string::npos);
    assert(result.ir_text.find("define void @compound_assign_local_array_byte(i64 %index)") != std::string::npos);
    assert(result.ir_text.find(" = load i8, ptr %tmp") != std::string::npos);
    assert(result.ir_text.find(" = add i8 ") != std::string::npos);
    assert(
        result.ir_text.find("define void @assign_pointer_aggregate(ptr %regs, ptr %buffer, i64 %index)") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.UartRegisters, ptr %regs, i32 0, i32 1") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.Buffer, ptr %buffer, i32 0, i32 0") !=
        std::string::npos
    );
    assert(result.ir_text.find("store i32 4, ptr %tmp") != std::string::npos);
    assert(result.ir_text.find("store i8 7, ptr %tmp") != std::string::npos);
    assert(
        result.ir_text.find("define void @compound_assign_pointer_aggregate(ptr %regs, ptr %buffer, i64 %index)") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("define void @assign_nested_pointer_aggregate(ptr %log, ptr %matrix, i64 %index, i64 %inner)") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.Log, ptr %log, i32 0, i32 0") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr [2 x %record.UartRegisters], ptr %tmp") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr %record.Matrix, ptr %matrix, i32 0, i32 0") !=
        std::string::npos
    );
    assert(
        result.ir_text.find("getelementptr [2 x [4 x i8]], ptr %tmp") != std::string::npos
    );
    assert(result.ir_text.find("store i32 8, ptr %tmp") != std::string::npos);
    assert(result.ir_text.find("store i8 1, ptr %tmp") != std::string::npos);
    assert(
        result.ir_text.find("getelementptr %record.Matrix, ptr %matrix, i32 0, i32 0") !=
        std::string::npos
    );
    assert(result.ir_text.find("getelementptr [2 x [4 x i8]], ptr %tmp") != std::string::npos);
    std::filesystem::remove(path);
}

void test_reject_malformed_generated_llvm_ir() {
    orison::lowering::LlvmIrVerifier verifier;
    auto diagnostics = verifier.verify(
        "define i32 @main() {\n"
        "entry:\n"
        "  ret i1 1\n"
        "}\n"
    );

    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(
        diagnostics.entries().front().message.find("generated LLVM IR failed to parse") != std::string::npos
    );
}

void test_reject_invalid_generated_llvm_module() {
    orison::lowering::LlvmIrVerifier verifier;
    auto diagnostics = verifier.verify(
        "define i32 @main(i1 %condition) {\n"
        "entry:\n"
        "  br i1 %condition, label %left, label %right\n"
        "left:\n"
        "  br label %merge\n"
        "right:\n"
        "  br label %merge\n"
        "merge:\n"
        "  %value = phi i32 [1, %left]\n"
        "  ret i32 %value\n"
        "}\n"
    );

    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(
        diagnostics.entries().front().message.find("generated LLVM IR failed verification") != std::string::npos
    );
}

void test_emit_native_object_file() {
    orison::lowering::LlvmObjectEmitter emitter;
    auto result = emitter.emit(
        "define i32 @main() {\n"
        "entry:\n"
        "  ret i32 42\n"
        "}\n"
    );

    assert(!result.has_errors());
    assert(!result.object_bytes.empty());
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_lowering_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    test_emit_constant_uint32_return();
    test_emit_carries_semantic_drop_lowering_authorization_metadata();
    test_emit_let_bound_uint32_return();
    test_emit_mutable_uint32_assignment_return();
    test_emit_mutable_uint32_compound_assignment_return();
    test_emit_negative_int32_var_initializer_return();
    test_emit_negative_int32_let_initializer_return();
    test_reject_negative_uint32_cast_return();
    test_emit_negative_int32_cast_return();
    test_emit_mutable_int32_compound_assignment_return();
    test_reject_bool_compound_assignment();
    test_reject_bool_aggregate_compound_assignment();
    test_emit_mutable_uint32_while_return();
    test_emit_while_call_statement();
    test_emit_while_unit_call_statement();
    test_emit_while_local_bindings();
    test_restore_outer_binding_after_while_local_shadow();
    test_emit_terminal_while_break();
    test_emit_terminal_while_continue();
    test_emit_defer_cleanup_on_early_return();
    test_emit_conditional_while_continue_and_break();
    test_emit_partial_switch_in_while_body();
    test_emit_terminating_while_if_else();
    test_emit_nested_while_if_control();
    test_emit_nested_while_loop_body();
    test_emit_nested_repeat_in_while_body();
    test_emit_nested_for_in_while_body();
    test_reject_nonterminal_while_loop_control();
    test_reject_unsupported_while_body_statement();
    test_emit_scalar_extension_method_definition();
    test_emit_scalar_member_call_expression();
    test_emit_scalar_member_call_statement();
    test_emit_scalar_call_statements();
    test_emit_scalar_unit_call_statements();
    test_emit_uint32_add_return();
    test_emit_uint32_arithmetic_return();
    test_emit_int32_division_return();
    test_emit_uint32_comparison_returns();
    test_emit_int32_comparison_returns();
    test_emit_boolean_expression_returns();
    test_reject_unsigned_unary_negation_return();
    test_reject_boolean_unary_negation_return();
    test_emit_ternary_expression_returns();
    test_emit_final_if_expression_returns();
    test_emit_final_if_branch_local_bindings();
    test_emit_final_if_mutable_branch_prefixes();
    test_emit_nested_final_if_returns();
    test_emit_final_integer_switch_returns();
    test_emit_final_switch_mutable_case_prefixes();
    test_emit_exhaustive_boolean_switch_returns();
    test_emit_switch_nested_in_final_if();
    test_emit_terminating_switch_nested_in_nonfinal_if();
    test_emit_partial_switch_nested_in_nonfinal_if();
    test_emit_partial_switch_in_guard_failure();
    test_emit_terminating_unit_switch_nested_in_nonfinal_if();
    test_emit_partial_unit_switch_nested_in_nonfinal_if();
    test_emit_zero_argument_function_call_return();
    test_emit_zero_argument_function_call_add_return();
    test_emit_single_uint32_parameter_function_call_return();
    test_emit_negative_int32_call_argument_return();
    test_emit_negative_int32_record_field_return();
    test_emit_negative_int32_array_element_return();
    test_reject_negative_uint32_record_field_initializer();
    test_reject_negative_uint32_array_element_initializer();
    test_emit_multi_uint32_parameter_function_call_return();
    test_emit_c_foreign_call_with_string_literal();
    test_emit_fixed_printf_adapter_with_integer_promotion();
    test_emit_fixed_printf_adapter_with_pointer_and_64_bit_arguments();
    test_emit_fixed_printf_adapter_with_float_promotion();
    test_reject_printf_adapter_with_invalid_fixed_prefix();
    test_reject_printf_adapter_with_unsupported_trailing_type();
    test_emit_fixed_arity_c_foreign_call();
    test_emit_unsafe_function_identity_return();
    test_reject_unsupported_return_expression();
    test_emit_scalar_thread_join_expression();
    test_emit_abandoned_scalar_thread_cleanup();
    test_emit_scalar_task_await_expression();
    test_emit_no_capture_thread_uses_null_cleanup();
    test_emit_no_capture_task_uses_null_cleanup();
    test_emit_record_thread_result_storage_size();
    test_emit_record_task_result_storage_size();
    test_emit_record_capture_cleanup_field_address();
    test_emit_same_type_record_capture_drop_metadata_dedupes();
    test_emit_allowed_record_capture_drop_abi_calls();
    test_emit_semantic_authorized_record_capture_drop_abi_calls();
    test_reject_partial_record_capture_drop_abi_calls();
    test_reject_partial_semantic_authorized_record_capture_drop_abi_calls();
    test_reject_unsupported_final_if_arm_expression();
    test_reject_unsupported_final_switch_case_expression();
    test_emit_nested_defer_cleanup_defers();
    test_emit_nested_defer_cleanup_multiple_defers();
    test_emit_nested_defer_cleanup_on_loop_control();
    test_emit_nested_defer_cleanup_on_for_and_repeat();
    test_emit_nested_defer_cleanup_in_unsafe_block();
    test_emit_nested_defer_cleanup_in_for_and_repeat_unsafe_blocks();
    test_emit_nonvoid_repeat_for_unsafe_prefixes();
    test_emit_repeat_body_partial_if_and_switch_flow();
    test_emit_for_body_partial_if_and_switch_flow();
    test_reject_statements_after_terminating_nonvoid_statement();
    test_emit_raw_mmio_intrinsics();
    test_reject_malformed_generated_llvm_ir();
    test_reject_invalid_generated_llvm_module();
    test_emit_native_object_file();
    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
