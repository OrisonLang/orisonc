#include "orison/lowering/llvm_ir_emitter.hpp"
#include "orison/lowering/llvm_object_emitter.hpp"
#include "orison/lowering/llvm_ir_verifier.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {

auto lower_source(std::filesystem::path const& path, std::string_view source)
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
    return emitter.emit(parse_result.module, semantic_result);
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

void test_reject_unsupported_while_body_statement() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_unsupported_while_body.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function main() -> UInt32\n"
        "    var value = 0 as UInt32\n"
        "    while value < 3 as UInt32\n"
        "        let increment = 1 as UInt32\n"
        "        value = value + increment\n"
        "    value\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering while body only supports mutable-local assignments"
    );
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
        "    left - right * right / right\n"
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
        "  ret i32 %tmp2\n"
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
        "function main() -> Int32\n"
        "    divide(8 as Int32, 2 as Int32)\n"
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
        "define i32 @main() {\n"
        "entry:\n"
        "  %tmp0 = call i32 @divide(i32 8, i32 2)\n"
        "  ret i32 %tmp0\n"
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

void test_reject_unsupported_return_expression() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_unsupported_expression.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function same(left: Bool, right: Bool) -> Bool\n"
        "    left == right\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not yet support this return expression: cannot infer operand type: =="
    );
}

void test_reject_unsupported_final_if_arm_expression() {
    auto path = std::filesystem::temp_directory_path() / "orison_lowering_unsupported_final_if_arm.or";
    auto result = lower_source(
        path,
        "package demo.lowering\n"
        "\n"
        "function same(flag: Bool, left: Bool, right: Bool) -> Bool\n"
        "    if flag\n"
        "        left == right\n"
        "    else\n"
        "        false\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not yet support this final control-flow statement: "
        "if then arm lowering failed: cannot infer operand type: =="
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
        "        true => left == right\n"
        "        false => false\n"
    );

    assert(result.has_errors());
    assert(result.diagnostics.entries().size() == 1);
    assert(
        result.diagnostics.entries().front().message ==
        "lowering does not yet support this final control-flow statement: "
        "switch case lowering failed: cannot infer operand type: =="
    );
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
    test_emit_constant_uint32_return();
    test_emit_let_bound_uint32_return();
    test_emit_mutable_uint32_assignment_return();
    test_emit_mutable_uint32_while_return();
    test_reject_unsupported_while_body_statement();
    test_emit_uint32_add_return();
    test_emit_uint32_arithmetic_return();
    test_emit_int32_division_return();
    test_emit_uint32_comparison_returns();
    test_emit_int32_comparison_returns();
    test_emit_boolean_expression_returns();
    test_emit_ternary_expression_returns();
    test_emit_final_if_expression_returns();
    test_emit_final_if_branch_local_bindings();
    test_emit_final_if_mutable_branch_prefixes();
    test_emit_nested_final_if_returns();
    test_emit_final_integer_switch_returns();
    test_emit_final_switch_mutable_case_prefixes();
    test_emit_exhaustive_boolean_switch_returns();
    test_emit_switch_nested_in_final_if();
    test_emit_zero_argument_function_call_return();
    test_emit_zero_argument_function_call_add_return();
    test_emit_single_uint32_parameter_function_call_return();
    test_emit_multi_uint32_parameter_function_call_return();
    test_emit_c_foreign_call_with_string_literal();
    test_emit_fixed_printf_adapter_with_integer_promotion();
    test_emit_fixed_printf_adapter_with_pointer_and_64_bit_arguments();
    test_emit_fixed_printf_adapter_with_float_promotion();
    test_reject_printf_adapter_with_invalid_fixed_prefix();
    test_reject_printf_adapter_with_unsupported_trailing_type();
    test_emit_fixed_arity_c_foreign_call();
    test_reject_unsupported_return_expression();
    test_reject_unsupported_final_if_arm_expression();
    test_reject_unsupported_final_switch_case_expression();
    test_reject_malformed_generated_llvm_ir();
    test_reject_invalid_generated_llvm_module();
    test_emit_native_object_file();
    return 0;
}
